/*---- INCLUDES ----*/
#include "core/file_system.h"
#include "core/core_log.h"
#include "chashmap.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cstring>

/*---- CONFIGURATION ----*/
#define MAX_FILE_NAME 32                 /**< Max length (incl. '\0') of a single path component cached in a dentry. */
#define MAX_PATH_LEN  160                 /**< Max absolute path length the dentry cache can resolve. Longer paths still work, just bypass the cache. */
#define MAX_KEY       (MAX_FILE_NAME + 24) /**< "0x" + up to 16 hex digits (64-bit safe pointer) + ':' + name + '\0'. */
#define DENTRY_HASH_INITIAL_CAPACITY 32   /**< Initial bucket count for the dentry hashmap; it grows (chashmap_rehashing) as needed. */

/*---- TYPES ----*/

/**
 * @brief In-memory Directory Entry (dentry) node.
 *
 * Mirrors the LittleFS namespace as a tree kept entirely in RAM so repeated
 * path/directory resolution does not have to re-walk LittleFS's on-flash
 * directory blocks on every call. Every dentry additionally lives in
 * `s_dentry_table`, a hashmap keyed by "<parent pointer>:<name>", giving
 * O(1) average-case "does this name exist under this parent" lookups
 * instead of an O(children) scan of the sibling list.
 *
 * @note This node intentionally does NOT cache an open LittleFS `File`
 *       handle. `file_system_init()` mounts LittleFS with a hard cap on
 *       concurrently open files (`maxopenfiles`); holding one open per
 *       cached dentry would silently exhaust that pool and break unrelated
 *       opens elsewhere in the system. Only cheap POD metadata is cached
 *       here - flash remains the sole source of truth for file contents.
 */
typedef struct dentry {
    char name[MAX_FILE_NAME];  /**< Base name of this entry (no slashes), NUL-terminated. */
    bool is_dir;                /**< True if this entry represents a directory. */
    size_t size;                 /**< Cached file size in bytes (meaningless/stale for directories). */
    bool children_loaded;      /**< True once list_file() has performed a full resync of this directory's children. */
    bool mark;                  /**< Scratch bit used only during list_file()'s resync pass to detect stale children; always false when idle. */
    struct dentry *parent;     /**< Parent directory dentry; NULL only for the root. */
    struct dentry *child;       /**< First child in this directory (head of the sibling list), or NULL. */
    struct dentry *next;         /**< Next sibling under the same parent, or NULL. */
} dentry_t;

/*---- STATIC VARIABLES ----*/
/** @brief Copy of the base path passed to file_system_init(), kept for diagnostics. */
static char *s_basepath = NULL;
/** @brief Indicates whether the file system has been successfully mounted. */
static bool s_fs_mounted = false;
/** @brief Root of the in-memory dentry tree; always represents LittleFS's own "/" . */
static dentry_t *s_root_dentry = NULL;
/** @brief Hashmap of all cached dentries, keyed by "<parent pointer>:<name>". */
static chashmap_t *s_dentry_table = NULL;
/**
 * @brief Recursive mutex guarding every read/write of the dentry tree/table above.
 * @note Recursive because dentry_evict() calls itself while a lock may already be held
 *       by the public function that triggered the eviction.
 */
static SemaphoreHandle_t s_dentry_mutex = NULL;

/*---- STATIC HELPER FUNCTIONS (dentry cache) ----*/
/*
 * Locking convention: every low-level dentry_* helper below assumes the
 * caller already holds `s_dentry_mutex` via dentry_lock(). Only the public
 * file_system.h entry points (read_file, write_file, list_file, ...) take
 * the lock themselves, each around exactly the block that touches the
 * cache, so a single critical section always covers "resolve, then mutate"
 * without ever handing a dentry pointer to code running outside the lock.
 */

/**
 * @brief Acquire the dentry cache mutex.
 * @param timeout_ticks Max ticks to wait (default: block forever).
 * @return true if the lock was acquired (or the cache is not yet initialized, in which
 *         case there is nothing to protect); false on a timeout.
 */
static inline bool dentry_lock(TickType_t timeout_ticks = portMAX_DELAY)
{
    if (s_dentry_mutex == NULL) return false; // Cache not initialized (file_system_init not called yet) - nothing to guard.
    return xSemaphoreTakeRecursive(s_dentry_mutex, timeout_ticks) == pdTRUE;
}

/** @brief Release the dentry cache mutex previously acquired via dentry_lock(). */
static inline void dentry_unlock()
{
    if (s_dentry_mutex != NULL) xSemaphoreGiveRecursive(s_dentry_mutex);
}

/**
 * @brief Build the hashmap key identifying a child by its parent and name.
 * @param[out] out_buf  Destination buffer for the key string.
 * @param[in]  buf_len  Size of out_buf.
 * @param[in]  parent   Parent dentry (its address is part of the key).
 * @param[in]  name     Child's base name.
 */
static void dentry_make_key(char *out_buf, size_t buf_len, const dentry_t *parent, const char *name)
{
    snprintf(out_buf, buf_len, "%p:%s", (const void *)parent, name);
}

/**
 * @brief Insert (or overwrite the pointer for) a dentry's hashmap entry.
 * @param entry Dentry to index; its current parent/name form the key.
 * @note free_func on s_dentry_table is NULL, so an overwrite never frees the old
 *       payload automatically - callers must never re-store a pointer that is
 *       already the current value for that key (see dentry_move()).
 */
static void dentry_table_store(dentry_t *entry)
{
    if (!s_dentry_table || !entry) return;
    char key[MAX_KEY];
    dentry_make_key(key, sizeof(key), entry->parent, entry->name);
    chashmap_set(s_dentry_table, key, entry);
}

/**
 * @brief Look up a cached child dentry by parent and name.
 * @param parent Parent dentry to search under.
 * @param name   Base name of the child to find.
 * @return The cached dentry_t*, or NULL if not present in the cache.
 */
static dentry_t *dentry_table_lookup(const dentry_t *parent, const char *name)
{
    if (!s_dentry_table || !parent || !name) return NULL;
    char key[MAX_KEY];
    dentry_make_key(key, sizeof(key), parent, name);
    return (dentry_t *)chashmap_get(s_dentry_table, key);
}

/**
 * @brief Remove a dentry's hashmap entry without freeing the dentry itself.
 * @param entry Dentry whose (parent, name) key should be dropped from the table.
 */
static void dentry_table_evict(dentry_t *entry)
{
    if (!s_dentry_table || !entry) return;
    char key[MAX_KEY];
    dentry_make_key(key, sizeof(key), entry->parent, entry->name);
    chashmap_remove(s_dentry_table, key);
}

/**
 * @brief Allocate and initialize a new dentry node.
 * @param name   Base name (single path component) to copy in, truncated to fit.
 * @param is_dir Whether the new entry represents a directory.
 * @param parent Parent dentry, or NULL only when allocating the root.
 * @return Newly allocated dentry_t*, or NULL if the heap allocation failed.
 */
static dentry_t *dentry_alloc(const char *name, bool is_dir, dentry_t *parent)
{
    // calloc zero-initializes size/children_loaded/mark/child/next for us.
    dentry_t *entry = (dentry_t *)calloc(1, sizeof(dentry_t));
    if (!entry) return NULL;

    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->is_dir = is_dir;
    entry->parent = parent;
    return entry;
}

/**
 * @brief Link a newly allocated dentry as the head of its parent's child list.
 * @param parent Directory dentry gaining a child.
 * @param child  Dentry to link; its ->next is overwritten.
 */
static inline void dentry_link_child(dentry_t *parent, dentry_t *child)
{
    child->next = parent->child;
    parent->child = child;
}

/**
 * @brief Recursively evict a dentry and its entire cached subtree.
 *
 * Frees children bottom-up first (defensively - a directory reaching here
 * should already have zero cached children, since LittleFS refuses to
 * rmdir/overwrite a non-empty directory before we ever get called), then
 * unlinks `node` from its own parent's sibling list, drops its hashmap
 * entry, and frees it. After this call, `node` must never be dereferenced.
 *
 * @param node Dentry to evict; safe to pass NULL (no-op).
 */
static void dentry_evict(dentry_t *node)
{
    if (!node) return;

    dentry_t *child = node->child;
    while (child) {
        dentry_t *next_child = child->next; // Snapshot before recursion mutates the list.
        dentry_evict(child);
        child = next_child;
    }
    node->child = NULL;

    if (node->parent) {
        dentry_t *prev = NULL;
        dentry_t *sib = node->parent->child;
        while (sib) {
            if (sib == node) {
                if (prev) prev->next = sib->next; else node->parent->child = sib->next;
                break;
            }
            prev = sib;
            sib = sib->next;
        }
    }

    dentry_table_evict(node);
    free(node);
}

/**
 * @brief Evict every cached child of `node`, leaving `node` itself intact.
 * @param node Directory dentry whose children should be dropped (e.g. the root, after a format).
 */
static void dentry_evict_all_children(dentry_t *node)
{
    if (!node) return;
    dentry_t *child = node->child;
    while (child) {
        dentry_t *next_child = child->next;
        dentry_evict(child);
        child = next_child;
    }
    node->child = NULL;
}

/**
 * @brief Resolve the parent dentry of an absolute path's final component.
 *
 * Walks every "/"-separated component except the last. Each intermediate
 * component is first looked up in O(1) via the hashmap; on a miss, this
 * probes LittleFS directly for that single component (the only flash
 * access performed here) and memoizes the result so the next resolution
 * through the same directory is a pure cache hit. The final component is
 * intentionally left unresolved - callers decide whether it must already
 * exist (read/remove/rename-from) or may be freshly created (write/mkdir).
 *
 * @param[in]  path        Absolute path (must start with '/'), e.g. "/a/b/c.txt".
 * @param[out] out_name    Buffer receiving the final path component ("c.txt").
 * @param[in]  out_name_sz Size of out_name in bytes.
 * @return The resolved parent dentry, or NULL if the path is malformed, too
 *         long/deep to cache, or an intermediate component does not exist
 *         or is not a directory. NULL must be treated as "cache can't help
 *         here" rather than a filesystem error - the cache is a best-effort
 *         accelerator, never the source of truth.
 */
static dentry_t *dentry_resolve_parent(const char *path, char *out_name, size_t out_name_sz)
{
    if (!path || path[0] != '/' || !out_name || out_name_sz == 0 || !s_root_dentry) {
        return NULL;
    }

    size_t len = strlen(path);
    if (len == 0 || len >= MAX_PATH_LEN) {
        return NULL; // Too long to safely copy/probe - degrade gracefully to "not cacheable".
    }

    // Mutable working copy: each intermediate component is probed against
    // LittleFS by temporarily NUL-terminating this buffer right after its
    // trailing slash, so the buffer itself doubles as the exact path prefix
    // LittleFS expects, without a second allocation per component.
    char buf[MAX_PATH_LEN];
    memcpy(buf, path, len + 1);

    dentry_t *current = s_root_dentry;
    size_t component_start = 1; // buf[0] is the leading '/'.

    for (size_t i = 1; i <= len; i++) {
        if (buf[i] != '/' && buf[i] != '\0') {
            continue;
        }

        size_t comp_len = i - component_start;
        if (comp_len == 0) {
            // Repeated ("//") or trailing '/' - just skip the empty component.
            component_start = i + 1;
            continue;
        }
        if (comp_len >= MAX_FILE_NAME) {
            return NULL; // Component too long to fit a cached name - degrade gracefully.
        }

        bool is_last = (buf[i] == '\0');
        if (is_last) {
            memcpy(out_name, buf + component_start, comp_len);
            out_name[comp_len] = '\0';
            return current;
        }

        // Intermediate directory component.
        char saved = buf[i];
        buf[i] = '\0'; // buf[0..i) is now the exact LittleFS path up to this component.

        char name[MAX_FILE_NAME];
        memcpy(name, buf + component_start, comp_len);
        name[comp_len] = '\0';

        dentry_t *child = dentry_table_lookup(current, name);
        if (!child) {
            // Cache miss: ask LittleFS whether this directory actually exists.
            File probe = LittleFS.open(buf, "r", false);
            bool exists = (bool)probe;
            bool is_dir = exists && probe.isDirectory();
            if (!exists || !is_dir) {
                buf[i] = saved;
                return NULL;
            }
            child = dentry_alloc(name, true, current);
            if (!child) {
                buf[i] = saved;
                return NULL;
            }
            dentry_link_child(current, child);
            dentry_table_store(child);
        }

        buf[i] = saved; // Restore the separator before continuing the walk.
        current = child;
        component_start = i + 1;
    }

    return NULL; // No final component was found (e.g. path was just "/").
}

/**
 * @brief Resolve an absolute path to its dentry, creating a cache entry if missing.
 * @param path       Absolute path, e.g. "/config.json" or "/". Never touches flash
 *                    for the final component - callers only call this once they
 *                    already know (from a successful LittleFS call) that it exists.
 * @param is_dir_hint Type to assign if a new dentry has to be created here.
 * @return The resolved/created dentry, or NULL if the path can't be resolved
 *         (unknown parent) or allocation failed.
 */
static dentry_t *dentry_get_or_create(const char *path, bool is_dir_hint)
{
    if (!path) return NULL;
    if (strcmp(path, "/") == 0) return s_root_dentry;

    char name[MAX_FILE_NAME];
    dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
    if (!parent) return NULL;

    dentry_t *entry = dentry_table_lookup(parent, name);
    if (entry) return entry;

    entry = dentry_alloc(name, is_dir_hint, parent);
    if (!entry) return NULL;

    dentry_link_child(parent, entry);
    dentry_table_store(entry);
    return entry;
}

/**
 * @brief Evict the cached dentry for `path`, if any (no-op if uncached).
 * @param path Absolute path whose underlying flash entry no longer exists.
 */
static void dentry_evict_path(const char *path)
{
    char name[MAX_FILE_NAME];
    dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
    if (!parent) return;

    dentry_t *entry = dentry_table_lookup(parent, name);
    if (entry) dentry_evict(entry);
}

/**
 * @brief Move a cached dentry from one path's identity to another after a successful rename.
 *
 * Because the hashmap key is "<parent pointer>:<name>" rather than a full
 * path string, moving a directory is cheap: only the moved dentry's own
 * (parent, name) changes - every descendant keeps the same parent pointer
 * value, so their keys stay valid with no subtree walk required.
 *
 * @param path_from Absolute source path that used to identify this entry.
 * @param path_to   Absolute destination path LittleFS just renamed it to.
 */
static void dentry_move(const char *path_from, const char *path_to)
{
    char old_name[MAX_FILE_NAME];
    dentry_t *old_parent = dentry_resolve_parent(path_from, old_name, sizeof(old_name));
    dentry_t *entry = old_parent ? dentry_table_lookup(old_parent, old_name) : NULL;
    if (!entry) return; // Source was never cached - nothing to move.

    char new_name[MAX_FILE_NAME];
    dentry_t *new_parent = dentry_resolve_parent(path_to, new_name, sizeof(new_name));
    if (!new_parent) {
        // Destination can't be resolved in-cache (unlikely, since the rename
        // just succeeded on flash). Drop the stale entry rather than risk
        // leaving it reachable under an identity that no longer applies.
        dentry_evict(entry);
        return;
    }

    // Detach from the OLD (parent, name) hashmap key. free_func is NULL, so
    // this only unindexes `entry` - it does not free it.
    dentry_table_evict(entry);

    // Unlink from the old parent's sibling list.
    dentry_t *prev = NULL;
    dentry_t *sib = old_parent->child;
    while (sib) {
        if (sib == entry) {
            if (prev) prev->next = sib->next; else old_parent->child = sib->next;
            break;
        }
        prev = sib;
        sib = sib->next;
    }

    // Rewrite identity in place and re-index under the NEW key.
    strncpy(entry->name, new_name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->parent = new_parent;

    dentry_link_child(new_parent, entry);
    dentry_table_store(entry);
}

/*---- PUBLIC FUNCTIONS ----*/

/** @brief Human-readable description of a FsResult_t */
const char *file_system_strerror(FsResult_t result)
{
    switch (result) {
        case FS_OK:                   return "OK";
        case FS_ERR_NOT_MOUNTED:      return "File system is not mounted";
        case FS_ERR_NOT_FOUND:        return "File or directory not found";
        case FS_ERR_IS_DIRECTORY:     return "Path is a directory, not a file";
        case FS_ERR_NOT_A_DIRECTORY:  return "Path is a file, not a directory";
        case FS_ERR_ALREADY_EXISTS:   return "Path already exists";
        case FS_ERR_OPEN_FAILED:      return "Failed to open path";
        case FS_ERR_ALLOC_FAILED:     return "Memory allocation failed";
        case FS_ERR_WRITE_INCOMPLETE: return "Not all bytes were written";
        case FS_ERR_OPERATION_FAILED: return "File system operation failed";
        case FS_ERR_INVALID_ARG:      return "Invalid argument";
        default:                      return "Unknown file system error";
    }
}

/** @brief Initializes the file system (LittleFS) and its in-memory dentry cache. */
bool file_system_init(bool formatonfail, const char *basepath, uint8_t maxopenfiles)
{
    // 1. Mount the LittleFS partition.
    s_fs_mounted = LittleFS.begin(formatonfail, basepath, maxopenfiles);
    if (!s_fs_mounted) {
        LOG_ERROR_STR("File system initialized failed, can't mount file system");
        return false;
    }

    // 2. Keep a copy of the base path for diagnostics. Free any previous
    //    copy first so calling init() twice can never leak it.
    free(s_basepath);
    s_basepath = strdup(basepath);

    // 3. Create the cache mutex once. There is no concurrency to guard
    //    against yet (nothing else can call into this module before this
    //    function returns for the first time), so the setup below runs
    //    unlocked by design.
    if (s_dentry_mutex == NULL) {
        s_dentry_mutex = xSemaphoreCreateRecursiveMutex();
        if (s_dentry_mutex == NULL) {
            LOG_ERROR_STR("File system initialized failed, can't create dentry cache mutex");
            return false;
        }
    }

    // 4. Create the dentry hashmap once. free_func is NULL - dentry lifetime
    //    is managed explicitly by dentry_evict()/dentry_evict_all_children(),
    //    never implicitly by the hashmap (see dentry_table_store()'s note on
    //    why an automatic free-on-overwrite would be unsafe here).
    if (s_dentry_table == NULL) {
        s_dentry_table = chashmap_create(DENTRY_HASH_INITIAL_CAPACITY, NULL);
        if (s_dentry_table == NULL) {
            LOG_ERROR_STR("File system initialized failed, can't initialize dentry table");
            return false;
        }
    }

    // 5. Create the root dentry. It always represents LittleFS's own "/",
    //    independent of `basepath` (which only matters to the VFS mount
    //    call above - every LittleFS API call afterwards takes paths
    //    relative to "/").
    if (s_root_dentry == NULL) {
        s_root_dentry = dentry_alloc("/", true, NULL);
        if (s_root_dentry == NULL) {
            LOG_ERROR_STR("File system initialized failed, can't allocate root dentry");
            return false;
        }
    }

    return true;
}

/** @brief Formats the LittleFS partition and invalidates the entire dentry cache. */
bool file_system_format()
{
    bool ok = LittleFS.format();
    if (ok && dentry_lock()) {
        // Every path on flash just disappeared; every cached dentry (aside
        // from a fresh, empty root) would now describe nothing. Tear the
        // whole tree down rather than leave it silently stale.
        dentry_evict_all_children(s_root_dentry);
        dentry_unlock();
    }
    return ok;
}

/** @brief Gets the total size of the file system */
size_t file_system_get_size()
{
    return LittleFS.totalBytes();
}

/** @brief Gets the used size of the file system */
size_t file_system_get_used()
{
    return LittleFS.usedBytes();
}

/** @brief Reads the entire contents of a file into a dynamically allocated buffer. */
FsResult_t read_file(const char *path, char **out_data, unsigned int *out_bytes_read)
{
    // 1. Initialize out-pointers to safe defaults.
    if (out_data) *out_data = NULL;
    if (out_bytes_read) *out_bytes_read = 0;

    // 2. Validate input pointers (fail-fast with FS_ERR_INVALID_ARG).
    if (!path || !out_data || !out_bytes_read) return FS_ERR_INVALID_ARG;

    // 3. Verify LittleFS mount state.
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 4. Step 1 - In-memory cache validation (frontline gatekeeper): consult
    //    the dentry cache before ever touching Flash. Two known-negative
    //    cases can be answered purely from RAM, with zero flash access:
    //      a) The path is cached as a directory -> FS_ERR_IS_DIRECTORY.
    //      b) The parent's children are fully resynced (children_loaded) and
    //         this name is absent from them -> the file cannot exist on
    //         flash, so FS_ERR_NOT_FOUND without LittleFS.exists()/open().
    if (strcmp(path, "/") == 0) {
        return FS_ERR_IS_DIRECTORY; // Root is always a directory.
    }
    if (dentry_lock()) {
        char name[MAX_FILE_NAME];
        dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
        if (parent) {
            dentry_t *cached = dentry_table_lookup(parent, name);
            if (cached && cached->is_dir) {
                dentry_unlock();
                return FS_ERR_IS_DIRECTORY;
            }
            if (!cached && parent->children_loaded) {
                dentry_unlock();
                return FS_ERR_NOT_FOUND;
            }
        }
        dentry_unlock();
    }

    // 5. Step 2 - Flash I/O: the cache could not resolve this with certainty
    //    above, so open the file using RAII (the LittleFS File closes itself
    //    when out of scope).
    File file = LittleFS.open(path, "r", false);

    // 6. Disambiguate open failures (FS_ERR_NOT_FOUND vs FS_ERR_OPEN_FAILED).
    if (!file) {
        return LittleFS.exists(path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }

    // 7. Reject if the path turned out to be a directory (cache didn't know).
    if (file.isDirectory()) {
        return FS_ERR_IS_DIRECTORY;
    }

    size_t size = file.size();

    // 8. Allocate dynamic memory (prefer PSRAM if available, fallback to malloc).
    //    Allocation size: size + 1 byte for the null-terminator '\0'.
    //    NOTE: caller inherits ownership and must release it via free().
    #ifdef SYSTEM_USES_PSRAM
    char *buffer = (char*) ps_malloc(size + 1);
    #else
    char *buffer = (char*)malloc(size + 1);
    #endif

    if (!buffer) {
        return FS_ERR_ALLOC_FAILED;
    }

    // 9. Read flash data into the buffer and append the null-terminator.
    size_t readLen = file.readBytes(buffer, size);
    buffer[readLen] = '\0';

    // 10. Transfer memory ownership to the caller.
    *out_data = buffer;
    *out_bytes_read = readLen;

    // 11. Step 3 - Cache synchronization (write-through): now that this
    //     file's exact size is known, memoize it (or refresh a stale cached
    //     size) so a subsequent path resolution through it is an O(1) hit.
    if (dentry_lock()) {
        dentry_t *entry = dentry_get_or_create(path, false);
        if (entry) {
            entry->is_dir = false;
            entry->size = readLen;
        }
        dentry_unlock();
    }

    return FS_OK;
}

/** @brief Writes data to a file in LittleFS. */
FsResult_t write_file(const char *path, const char *data, unsigned int length, unsigned int *out_bytes_written)
{
    if (out_bytes_written) *out_bytes_written = 0;

    // 1. Validate inputs.
    if (!path || !data) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Step 1 - In-memory cache validation (frontline gatekeeper):
    //    a) Refuse writing to root outright - it is always a directory.
    //    b) Resolve the parent through the dentry cache. A NULL result means
    //       the parent directory does not exist (dentry_resolve_parent()
    //       already tried a single flash probe for any uncached intermediate
    //       directory), so fail fast without ever calling LittleFS.open().
    //    c) If the final component is already cached as a directory, reject
    //       immediately instead of opening it on flash just to find out.
    if (strcmp(path, "/") == 0) return FS_ERR_IS_DIRECTORY;

    if (dentry_lock()) {
        char name[MAX_FILE_NAME];
        dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
        if (!parent) {
            dentry_unlock();
            return FS_ERR_NOT_FOUND; // Parent directory doesn't exist.
        }
        dentry_t *existing = dentry_table_lookup(parent, name);
        if (existing && existing->is_dir) {
            dentry_unlock();
            return FS_ERR_IS_DIRECTORY;
        }
        dentry_unlock();
    }

    // 3. Step 2 - Flash I/O: open (creating/truncating) the file for writing.
    File file = LittleFS.open(path, "w", false);
    if (!file) return FS_ERR_OPEN_FAILED;
    if (file.isDirectory()) {
        return FS_ERR_IS_DIRECTORY; // Defensive: cache missed it, flash didn't.
    }

    size_t bytesWrite = file.write((const uint8_t*)data, length);

    if (out_bytes_written) *out_bytes_written = (unsigned int)bytesWrite;

    bool complete = (bytesWrite == length);

    // 4. Step 3 - Cache synchronization (write-through): only memoize the
    //    entry once the write actually succeeded in full - a partial write
    //    should not make a half-written file look trustworthy in the cache.
    if (complete && dentry_lock()) {
        dentry_t *entry = dentry_get_or_create(path, false);
        if (entry) {
            entry->is_dir = false;
            entry->size = bytesWrite;
        }
        dentry_unlock();
    }

    return complete ? FS_OK : FS_ERR_WRITE_INCOMPLETE;
}

/** @brief Iterates through a directory and serializes the file list into a flat JSON array string. */
FsResult_t list_file(const char *dir_path, char **out_json)
{
    // 1. Initialize *out_json = NULL; validate input parameters and mount state.
    if (out_json) *out_json = NULL;
    if (!dir_path || !out_json) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Open the directory using RAII; verify it is a valid directory.
    File root = LittleFS.open(dir_path);
    if (!root) {
        return LittleFS.exists(dir_path) ? FS_ERR_OPEN_FAILED : FS_ERR_NOT_FOUND;
    }
    if (!root.isDirectory()) {
        return FS_ERR_NOT_A_DIRECTORY;
    }

    // 3. Best-effort: resolve/create this directory's own dentry up front so
    //    the fresh flash listing below can be mirrored into the cache. Held
    //    for the whole traversal so the resync in step 5 sees a consistent
    //    snapshot instead of racing a concurrent write/remove.
    bool locked = dentry_lock();
    dentry_t *dir_entry = locked ? dentry_get_or_create(dir_path, true) : NULL;

    // 4. Sequentially traverse direct child entries via root.openNextFile()
    //    (non-recursive, lazy load) - this remains the authoritative source
    //    for what the JSON response reports; the cache only mirrors it.
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    File file = root.openNextFile();
    while (file) {
        JsonObject item = array.add<JsonObject>();

        // Extract basename (LittleFS on ESP32 sometimes includes full path).
        String fullname = file.name();
        int slashIdx = fullname.lastIndexOf('/');
        if (slashIdx >= 0) fullname = fullname.substring(slashIdx + 1);

        bool child_is_dir = file.isDirectory();
        size_t child_size = file.size();

        item["name"] = fullname;
        item["size"] = child_size;
        item["is_dir"] = child_is_dir;

        // Write-through: create-or-refresh this child's cache entry and
        // flag it as confirmed-present for the pruning pass in step 5.
        if (dir_entry) {
            dentry_t *child = dentry_table_lookup(dir_entry, fullname.c_str());
            if (!child) {
                child = dentry_alloc(fullname.c_str(), child_is_dir, dir_entry);
                if (child) {
                    dentry_link_child(dir_entry, child);
                    dentry_table_store(child);
                }
            }
            if (child) {
                child->is_dir = child_is_dir;
                child->size = child_size;
                child->mark = true;
            }
        }

        file = root.openNextFile();
    }

    // 5. Prune: any previously cached child NOT confirmed above no longer
    //    exists on flash (e.g. it was removed by something other than this
    //    module) - evict it so a later lookup can't resolve stale data.
    if (dir_entry) {
        dentry_t *prev = NULL;
        dentry_t *node = dir_entry->child;
        while (node) {
            dentry_t *next_node = node->next;
            if (!node->mark) {
                if (prev) prev->next = next_node; else dir_entry->child = next_node;
                dentry_table_evict(node);
                free(node);
            } else {
                node->mark = false; // Reset for the next resync.
                prev = node;
            }
            node = next_node;
        }
        dir_entry->children_loaded = true;
    }
    if (locked) dentry_unlock();

    // 6. Calculate the exact JSON string size via measureJson(doc) + 1 byte for '\0'.
    size_t jsonLen = measureJson(doc) + 1;

    // 7. Allocate dynamic memory on the heap via malloc() for the result string.
    char *jsonBuffer = (char*)malloc(jsonLen);
    if (!jsonBuffer) {
        return FS_ERR_ALLOC_FAILED;
    }

    // 8. Serialize JSON content into jsonBuffer and transfer ownership to the caller.
    serializeJson(doc, jsonBuffer, jsonLen);

    *out_json = jsonBuffer;
    return FS_OK;
}

/** @brief Removes a file from the file system. */
FsResult_t remove_file(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Step 1 - In-memory cache validation (frontline gatekeeper): fail
    //    fast on a known type mismatch (cached as a directory) or a
    //    confirmed absence (parent's children are fully resynced and this
    //    name isn't among them) without ever calling LittleFS.exists().
    if (dentry_lock()) {
        char name[MAX_FILE_NAME];
        dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
        if (parent) {
            dentry_t *cached = dentry_table_lookup(parent, name);
            if (cached && cached->is_dir) {
                dentry_unlock();
                return FS_ERR_IS_DIRECTORY;
            }
            if (!cached && parent->children_loaded) {
                dentry_unlock();
                return FS_ERR_NOT_FOUND;
            }
        }
        dentry_unlock();
    }

    // 3. Step 2 - Flash I/O: cache couldn't answer with certainty, confirm
    //    existence on flash before attempting removal.
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    bool ok = LittleFS.remove(path);

    // 4. Step 3 - Cache synchronization (write-through): the path is gone,
    //    so its cached dentry (if any) is now stale - evict it so nothing
    //    can resolve it again by mistake.
    if (ok && dentry_lock()) {
        dentry_evict_path(path);
        dentry_unlock();
    }

    return ok ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Renames a file in the file system. */
FsResult_t rename_file(const char *pathFrom, const char *pathTo)
{
    // Validate arguments and state.
    if (!pathFrom || !pathTo) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;
    if (!LittleFS.exists(pathFrom)) return FS_ERR_NOT_FOUND;

    // Attempt rename.
    bool ok = LittleFS.rename(pathFrom, pathTo);

    // Write-through: relocate the cached dentry (if any) to its new
    // identity in place, preserving its cached type/size instead of
    // dropping and lazily re-discovering them later.
    if (ok && dentry_lock()) {
        dentry_move(pathFrom, pathTo);
        dentry_unlock();
    }

    return ok ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Creates a directory. */
FsResult_t make_directory(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Step 1 - In-memory cache validation (frontline gatekeeper): fail
    //    fast if the cache already knows this path exists (either type - a
    //    conflicting file counts as "already exists" too), skipping
    //    LittleFS.exists().
    if (dentry_lock()) {
        char name[MAX_FILE_NAME];
        dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
        if (parent) {
            dentry_t *cached = dentry_table_lookup(parent, name);
            if (cached) {
                dentry_unlock();
                return FS_ERR_ALREADY_EXISTS;
            }
        }
        dentry_unlock();
    }

    // 3. Step 2 - Flash I/O: cache couldn't confirm absence with certainty,
    //    double-check on flash before creating.
    if (LittleFS.exists(path)) return FS_ERR_ALREADY_EXISTS;
    bool ok = LittleFS.mkdir(path);

    // 4. Step 3 - Cache synchronization (write-through): memoize the freshly
    //    created directory so the next path resolution through it is a
    //    cache hit instead of a flash probe. Force is_dir=true even on a
    //    cache hit, in case a stale ghost entry (e.g. left over from a race
    //    with another task) was resolved instead.
    if (ok && dentry_lock()) {
        dentry_t *entry = dentry_get_or_create(path, true);
        if (entry) entry->is_dir = true;
        dentry_unlock();
    }

    return ok ? FS_OK : FS_ERR_OPERATION_FAILED;
}

/** @brief Removes a directory. */
FsResult_t remove_dir(const char *path)
{
    // 1. Validate arguments and state.
    if (!path) return FS_ERR_INVALID_ARG;
    if (!s_fs_mounted) return FS_ERR_NOT_MOUNTED;

    // 2. Step 1 - In-memory cache validation (frontline gatekeeper): fail
    //    fast on a known type mismatch (cached as a plain file) or a
    //    confirmed absence (parent's children are fully resynced and this
    //    name isn't among them) without ever calling LittleFS.exists().
    if (dentry_lock()) {
        char name[MAX_FILE_NAME];
        dentry_t *parent = dentry_resolve_parent(path, name, sizeof(name));
        if (parent) {
            dentry_t *cached = dentry_table_lookup(parent, name);
            if (cached && !cached->is_dir) {
                dentry_unlock();
                return FS_ERR_NOT_A_DIRECTORY;
            }
            if (!cached && parent->children_loaded) {
                dentry_unlock();
                return FS_ERR_NOT_FOUND;
            }
        }
        dentry_unlock();
    }

    // 3. Step 2 - Flash I/O: cache couldn't answer with certainty, confirm
    //    existence on flash before attempting removal.
    if (!LittleFS.exists(path)) return FS_ERR_NOT_FOUND;
    bool ok = LittleFS.rmdir(path);

    // 4. Step 3 - Cache synchronization (write-through): the directory (and,
    //    defensively, any cached descendants - see dentry_evict()) is gone
    //    from flash; drop it.
    if (ok && dentry_lock()) {
        dentry_evict_path(path);
        dentry_unlock();
    }

    return ok ? FS_OK : FS_ERR_OPERATION_FAILED;
}

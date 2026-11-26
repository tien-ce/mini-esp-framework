/**
 * @file nvs_handle.cpp
 * @brief Script handle "nvs": methods and destructor of nvs_value_t.
 */

/*---- INCLUDES ----*/
#include "scripting/handle/storage/nvs_handle.h"
#include "core/core_log.h"
#include "include/tracked_memory.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static bool nvs_put(Preferences *prefs, const char *key, const value_t *v, size_t *written);
static bool nvs_write_dict_entry(const char *key, value_t *value, void *user_context);
static bool nvs_read_dict_entry(const char *key, value_t *value, void *user_context);
static void nvs_destroy(void *ptr);

static value_t *nvs_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *nvs_write(ti_handle_t handle, value_t **argv, int argc);
static value_t *nvs_write_dict(ti_handle_t handle, value_t **argv, int argc);
static value_t *nvs_read(ti_handle_t handle, value_t **argv, int argc);
static value_t *nvs_read_dict(ti_handle_t handle, value_t **argv, int argc);
static value_t *nvs_close(ti_handle_t handle, value_t **argv, int argc);

/*---- PARAMS ----*/

/** open(namespace). params[0] is self: the method is found through the handle's type, so it is always an nvs. */
static const param_t s_nvs_open_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("namespace"), };
/** write(key, value). value is int, float, bool or string, checked inside the method (VAL_ANY). */
static const param_t s_nvs_write_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("key"), PARAM_ANY("value"), };
/** write_dict(values), read_dict(values). */
static const param_t s_nvs_dict_params[] = { PARAM_HANDLE("self", NULL), PARAM_DICT("values"), };
/** read(key, default). default is int, float, bool or string and gives the type read, checked inside the method (VAL_ANY). */
static const param_t s_nvs_read_params[] = { PARAM_HANDLE("self", NULL), PARAM_STRING("key"), PARAM_ANY("default"), };
/** close(). */
static const param_t s_nvs_self_params[] = { PARAM_HANDLE("self", NULL), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_nvs_methods[] = {
    { "open",       VAL_BOOL, s_nvs_open_params,  2, nvs_open       },
    { "write",      VAL_INT,  s_nvs_write_params, 3, nvs_write      },
    { "write_dict", VAL_VOID, s_nvs_dict_params,  2, nvs_write_dict },
    { "read",       VAL_ANY,  s_nvs_read_params,  3, nvs_read       },   /* type of default */
    { "read_dict",  VAL_VOID, s_nvs_dict_params,  2, nvs_read_dict  },
    { "close",      VAL_VOID, s_nvs_self_params,  1, nvs_close      },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in nvs_handle.h */
const ti_handle_type_t g_nvs_handle_type = {
    "nvs",                                                          /* name */
    nvs_destroy,                                                    /* destroy */
    s_nvs_methods,                                                  /* methods */
    (int)(sizeof(s_nvs_methods) / sizeof(s_nvs_methods[0])),        /* method_count */
    NULL,                                                           /* get_attr: no attributes */
    NULL,                                                           /* set_attr: no attributes */
};

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Store one value under key, by its type.
 * @param prefs Opened namespace.
 * @param key Key name.
 * @param v Value: int, float, bool or string.
 * @param[out] written Bytes written by Preferences (0 on failure).
 * @return true when the type is supported, false when it is not (nothing written).
 */
static bool nvs_put(Preferences *prefs, const char *key, const value_t *v, size_t *written) {
    switch (v->type) {
        case VAL_INT:    *written = prefs->putInt(key, v->int_val);       return true;
        case VAL_FLOAT:  *written = prefs->putFloat(key, v->float_val);   return true;
        case VAL_BOOL:   *written = prefs->putBool(key, v->bool_val);     return true;
        case VAL_STRING: *written = prefs->putString(key, v->string_val); return true;
        default:         return false;
    }
}

/**
 * @brief val_dict_foreach callback of write_dict: store one entry, warn and skip when it cannot be stored.
 * @param user_context Opened Preferences of the nvs.
 */
static bool nvs_write_dict_entry(const char *key, value_t *value, void *user_context) {
    Preferences *prefs = (Preferences *)user_context;
    size_t written = 0;

    if (!nvs_put(prefs, key, value, &written)) {
        LOG_WARNING_STR("nvs.write_dict: key '%s' skipped, value must be int, float, bool or string", key);
    } else if (written == 0) {
        LOG_WARNING_STR("nvs.write_dict: key '%s' skipped, write failed", key);
    }
    return true;   /* always go on with the next key */
}

/**
 * @brief val_dict_foreach callback of read_dict: replace the entry's value (its default) with the stored one.
 * @details The value is changed in place, never with val_dict_set: setting a key during the iteration may
 *          rehash the map under the loop. The type stays the same, so the dict stays valid.
 * @param user_context Opened Preferences of the nvs.
 */
static bool nvs_read_dict_entry(const char *key, value_t *value, void *user_context) {
    Preferences *prefs = (Preferences *)user_context;

    switch (value->type) {
        case VAL_INT:   value->int_val   = prefs->getInt(key, value->int_val);     break;
        case VAL_FLOAT: value->float_val = prefs->getFloat(key, value->float_val); break;
        case VAL_BOOL:  value->bool_val  = prefs->getBool(key, value->bool_val);   break;
        case VAL_STRING: {
            String stored = prefs->getString(key, value->string_val ? value->string_val : "");
            char *copy = ti_raw_strdup(stored.c_str());
            if (copy == NULL) {
                LOG_WARNING_STR("nvs.read_dict: key '%s' skipped, out of memory", key);
                break;
            }
            ti_raw_free(value->string_val);
            value->string_val = copy;
            break;
        }
        default:
            LOG_WARNING_STR("nvs.read_dict: key '%s' skipped, value must be int, float, bool or string", key);
            break;
    }
    return true;   /* always go on with the next key */
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method nvs.open(string namespace): open the namespace read/write, created when it does not exist.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, namespace). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Bool: true when opened, false when Preferences refused it (e.g. name longer than 15 chars);
 *         NULL after raising an error.
 */
static value_t *nvs_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self = (nvs_value_t *)argv[0]->handle_val->ptr;
    const char *name  = argv[1]->string_val;

    // Step 1: this handle already holds a namespace (close it first)
    if (self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.open: a namespace is already open on this nvs, close it first");
        return NULL;
    }

    // Step 2: open read/write (false = not read-only), so a missing namespace is created
    self->is_open = self->prefs.begin(name, false);
    return val_new_bool(self->is_open);
}

/**
 * @brief Method nvs.write(string key, value): store an int, float, bool or string under key.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, key, value). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Int: bytes written by Preferences, 0 on failure; NULL after raising an error.
 */
static value_t *nvs_write(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self = (nvs_value_t *)argv[0]->handle_val->ptr;

    // Step 1: no namespace open yet
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.write: not open, call open(namespace) first");
        return NULL;
    }

    // Step 2: store by type
    size_t written = 0;
    if (!nvs_put(&self->prefs, argv[1]->string_val, argv[2], &written)) {
        ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "nvs.write: value must be int, float, bool or string");
        return NULL;
    }
    return val_new_int((int)written);
}

/**
 * @brief Method nvs.write_dict(dict values): store every entry (key = NVS key).
 * @details An entry that cannot be stored (container value or write failure) is logged as a warning and skipped.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, values). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error.
 */
static value_t *nvs_write_dict(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self = (nvs_value_t *)argv[0]->handle_val->ptr;

    // Step 1: no namespace open yet
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.write_dict: not open, call open(namespace) first");
        return NULL;
    }

    // Step 2: store every entry
    val_dict_foreach(argv[1]->dict_val, nvs_write_dict_entry, &self->prefs);
    return val_new_void();
}

/**
 * @brief Method nvs.read(string key, default): read the value stored under key, typed like default.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, key, default). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Value of the type of default (default itself when the key does not exist); NULL after raising an error.
 */
static value_t *nvs_read(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self   = (nvs_value_t *)argv[0]->handle_val->ptr;
    const char *key     = argv[1]->string_val;
    const value_t *def  = argv[2];

    // Step 1: no namespace open yet
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.read: not open, call open(namespace) first");
        return NULL;
    }

    // Step 2: read with the type of default
    value_t *result = NULL;
    switch (def->type) {
        case VAL_INT:    result = val_new_int(self->prefs.getInt(key, def->int_val));       break;
        case VAL_FLOAT:  result = val_new_float(self->prefs.getFloat(key, def->float_val)); break;
        case VAL_BOOL:   result = val_new_bool(self->prefs.getBool(key, def->bool_val));    break;
        case VAL_STRING: {
            String stored = self->prefs.getString(key, def->string_val ? def->string_val : "");
            result = val_new_string(stored.c_str());
            break;
        }
        default:
            ti_raise_error(handle, TI_ERR_TYPE_MISMATCH, "nvs.read: default must be int, float, bool or string");
            return NULL;
    }

    if (result == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "nvs.read: out of memory");
    }
    return result;
}

/**
 * @brief Method nvs.read_dict(dict values): every entry gets the value stored under its key.
 * @details The value of an entry is its default and gives the type read; a key not stored keeps its default.
 *          A container value is logged as a warning and skipped.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self, values). Owned by the interpreter; the values of the dict are updated in place.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error.
 */
static value_t *nvs_read_dict(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self = (nvs_value_t *)argv[0]->handle_val->ptr;

    // Step 1: no namespace open yet
    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.read_dict: not open, call open(namespace) first");
        return NULL;
    }

    // Step 2: update every entry in place
    val_dict_foreach(argv[1]->dict_val, nvs_read_dict_entry, &self->prefs);
    return val_new_void();
}

/**
 * @brief Method nvs.close(): close the namespace; the nvs can be opened again afterwards.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return Void; NULL after raising an error.
 */
static value_t *nvs_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    nvs_value_t *self = (nvs_value_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "nvs.close: nvs is not open");
        return NULL;
    }
    self->prefs.end();
    self->is_open = false;
    return val_new_void();
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the nvs handle type: runs when the last value pointing to the nvs is freed
 *        (out of scope, script ended or stopped). Closes the namespace if still open, then deletes the object.
 * @param ptr The nvs_value_t, allocated with new by NVS() (never NULL when called).
 */
static void nvs_destroy(void *ptr) {
    nvs_value_t *self = (nvs_value_t *)ptr;
    if (self->is_open) {
        self->prefs.end();
    }
    delete self;
}

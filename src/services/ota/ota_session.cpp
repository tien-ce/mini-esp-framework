#include "services/ota/ota_session.h"
#include "core/core_log.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/*                                  CONSTANTS                                 */
/* -------------------------------------------------------------------------- */

#define OTA_SECTOR_SIZE         4096u     /* Flash sector: chunk sizes are multiples of this */
#define OTA_CHUNK_MAX           16384u    /* Largest chunk without PSRAM */
#define OTA_CHUNK_MAX_PSRAM     32768u    /* Largest chunk with PSRAM */
#define OTA_SESSION_TIMEOUT_MS  30000u    /* No chunk activity for this long aborts the session */
#define OTA_TIMER_PERIOD_MS     5000u     /* How often the inactivity timer checks */
#define OTA_CHUNK_STEAL_MS      5000u     /* A half received chunk older than this may be replaced by a new request */

#define OTA_IMAGE_MAGIC         0xE9      /* First byte of every ESP firmware image */
#define OTA_HEADER_CHIP_ID_POS  12        /* Offset of the 16-bit chip id inside the image header */
#define OTA_HEADER_MIN_LEN      14        /* Bytes of the header needed to read the chip id */

/* Chip ids stored in the image header (esp_chip_id_t) and the matching name */
#if CONFIG_IDF_TARGET_ESP32S3
#define OTA_CHIP_ID   9
#define OTA_CHIP_NAME "esp32s3"
#elif CONFIG_IDF_TARGET_ESP32C3
#define OTA_CHIP_ID   5
#define OTA_CHIP_NAME "esp32c3"
#elif CONFIG_IDF_TARGET_ESP32S2
#define OTA_CHIP_ID   2
#define OTA_CHIP_NAME "esp32s2"
#else
#define OTA_CHIP_ID   0
#define OTA_CHIP_NAME "esp32"
#endif

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/** All state of the (single) OTA session. Zero initialised: state == OTA_IDLE. */
static struct {
    OtaState state;            /* Session state */
    char     id[9];            /* Session id, 8 hex digits */
    uint32_t total;            /* Declared firmware size */
    uint32_t offset;           /* Next byte expected = bytes already written to flash */
    uint32_t chunkSize;        /* Chunk size of this session */
    uint8_t *buf;              /* RAM buffer holding the chunk being received */
    int32_t  lastChunk;        /* Index of the last handled chunk, -1 if none */
    uint32_t lastActivityMs;   /* millis() of the last begin/chunk activity (for the timeout) */

    /* Chunk currently being received */
    const void *owner;         /* Request identity, nullptr when no chunk is in progress */
    uint32_t chunkOffset;      /* Offset the client claims for this chunk */
    uint32_t chunkCrc;         /* CRC-32 the client claims for this chunk */
    uint32_t chunkLen;         /* Expected body length */
    uint32_t chunkFilled;      /* Body bytes copied into buf so far */
    uint32_t chunkStartMs;     /* millis() when the chunk started */
    bool     chunkOverflow;    /* Pieces arrived out of order or too long */

    /* Last accepted chunk, used to recognise a chunk that is sent again */
    bool     hasLast;
    uint32_t lastOffset;
    uint32_t lastLen;
    uint32_t lastCrc;

    /* Last failure */
    bool     hasError;
    OtaError error;
} S;

static TimerHandle_t s_timeoutTimer = nullptr;   /* Periodic inactivity check */

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Returns the mutex that guards the session (created on first use).
 * @return Mutex handle.
 */
static SemaphoreHandle_t otaMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

/** Scoped lock for the session mutex. */
struct OtaLock {
    OtaLock()  { xSemaphoreTake(otaMutex(), portMAX_DELAY); }
    ~OtaLock() { xSemaphoreGive(otaMutex()); }
};

/**
 * @brief Computes the standard CRC-32 (zlib / PNG, polynomial 0xEDB88320) of a buffer.
 * @param data Buffer.
 * @param len Buffer length.
 * @return CRC-32 value.
 */
static uint32_t otaCrc32(const uint8_t *data, size_t len) {
    static const uint32_t table[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
        0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
    };
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        crc = (crc >> 4) ^ table[crc & 0x0F];
        crc = (crc >> 4) ^ table[crc & 0x0F];
    }
    return ~crc;
}

/**
 * @brief Fills an error using a printf-style message.
 * @param err Error to fill.
 * @param http HTTP status.
 * @param stage Stage name.
 * @param code Machine readable code.
 * @param retryable Whether the client may retry.
 * @param fmt printf format of the message.
 */
static void otaSetError(OtaError &err, uint16_t http, const char *stage, const char *code, bool retryable, const char *fmt, ...) {
    err.http = http;
    err.stage = stage;
    err.code = code;
    err.retryable = retryable;
    err.chunk = -1;
    err.offset = -1;
    err.expectedOffset = -1;

    va_list args;
    va_start(args, fmt);
    vsnprintf(err.message, sizeof(err.message), fmt, args);
    va_end(args);

    // The message goes into JSON by plain copy: make sure it cannot break the string
    for (char *p = err.message; *p; p++) {
        if (*p == '"' || *p == '\\') *p = '\'';
    }
}

/**
 * @brief Stops the inactivity timer.
 */
static void otaStopTimer() {
    if (s_timeoutTimer != nullptr) {
        xTimerStop(s_timeoutTimer, 0);
    }
}

/**
 * @brief Frees the chunk buffer and clears the chunk in progress.
 */
static void otaReleaseBuffer() {
    if (S.buf != nullptr) {
        free(S.buf);
        S.buf = nullptr;
    }
    S.owner = nullptr;
    S.chunkFilled = 0;
    S.chunkOverflow = false;
}

/**
 * @brief Ends the session as failed: aborts the flash update, frees RAM and remembers the error.
 * @param err Failure to remember (shown by GET /api/ota/status).
 */
static void otaFail(const OtaError &err) {
    Update.abort();
    otaReleaseBuffer();
    otaStopTimer();
    S.state = OTA_FAILED;
    S.hasError = true;
    S.error = err;
    LOG_ERROR_STR("[OTA] Session failed: stage=%s code=%s (%s)", err.stage, err.code, err.message);
}

/**
 * @brief Picks the chunk size from the largest free heap block.
 * @return Chunk size in bytes (multiple of 4096), or 0 when RAM is too low.
 */
static uint32_t otaPickChunkSize() {
    uint32_t cap = psramFound() ? OTA_CHUNK_MAX_PSRAM : OTA_CHUNK_MAX;
    // A quarter of the largest block: leaves room for the network stack and the other tasks
    uint32_t fit = (uint32_t)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 4);
    uint32_t size = (fit < cap) ? fit : cap;
    return size & ~(OTA_SECTOR_SIZE - 1u);
}

/**
 * @brief Tells whether a text is exactly 32 hexadecimal characters.
 * @param s Text to check.
 * @return true when it is a valid MD5 string.
 */
static bool otaIsMd5(const char *s) {
    if (s == nullptr || strlen(s) != 32) return false;
    for (int i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return true;
}

/**
 * @brief Checks that the session id is the open (or last) session.
 * @param session Id sent by the client.
 * @return true when it matches.
 */
static bool otaSessionMatches(const char *session) {
    return session != nullptr && S.id[0] != '\0' && strcmp(session, S.id) == 0;
}

/**
 * @brief Inactivity timer callback: aborts a session that received nothing for too long.
 * @param timer Timer handle (unused).
 */
static void otaTimeoutCallback(TimerHandle_t timer) {
    (void)timer;
    OtaLock lock;
    if (S.state != OTA_RECEIVING) {
        otaStopTimer();
        return;
    }
    if ((uint32_t)(millis() - S.lastActivityMs) >= OTA_SESSION_TIMEOUT_MS) {
        OtaError err;
        otaSetError(err, 408, "timeout", "session_timeout", false, "No chunk received for %u s", (unsigned)(OTA_SESSION_TIMEOUT_MS / 1000));
        otaFail(err);
    }
}

/**
 * @brief Checks the image header of the first chunk (magic byte and chip id).
 * @param data First bytes of the firmware.
 * @param len Number of bytes available.
 * @param err Filled when the header is not acceptable.
 * @return true when the header looks like a firmware for this chip.
 */
static bool otaCheckHeader(const uint8_t *data, size_t len, OtaError &err) {
    if (len < 1 || data[0] != OTA_IMAGE_MAGIC) {
        otaSetError(err, 400, "magic", "bad_magic", false, "Not an ESP firmware image");
        return false;
    }
    if (len < OTA_HEADER_MIN_LEN) {
        otaSetError(err, 400, "magic", "short_header", false, "Image header too short");
        return false;
    }
    uint16_t chipId = (uint16_t)(data[OTA_HEADER_CHIP_ID_POS] | (data[OTA_HEADER_CHIP_ID_POS + 1] << 8));
    if (chipId != OTA_CHIP_ID) {
        otaSetError(err, 400, "chip", "wrong_chip", false, "Firmware is built for another chip (id %u)", (unsigned)chipId);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void ota_error_make(OtaError &err, uint16_t http, const char *stage, const char *code, bool retryable, const char *message) {
    otaSetError(err, http, stage, code, retryable, "%s", message);
}

void ota_get_info(OtaInfo &info) {
    OtaLock lock;
    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    info.busy = (S.state == OTA_RECEIVING);
    info.maxSize = (part != nullptr) ? part->size : 0;
    info.chunkSize = otaPickChunkSize();
    info.chip = OTA_CHIP_NAME;
    info.freeHeap = ESP.getFreeHeap();
}

bool ota_begin(uint32_t size, const char *md5, char *sessionOut, uint32_t &chunkSize, OtaError &err) {
    OtaLock lock;

    // Step 1: only one session at a time (a stale one is removed by the inactivity timer)
    if (S.state == OTA_RECEIVING) {
        otaSetError(err, 409, "busy", "session_open", false, "Another OTA session is running");
        return false;
    }

    // Step 2: the size must fit the next OTA partition
    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (part == nullptr) {
        otaSetError(err, 500, "begin", "no_partition", false, "No OTA partition available");
        return false;
    }
    if (size == 0) {
        otaSetError(err, 400, "size", "bad_size", false, "Firmware size is missing or zero");
        return false;
    }
    if (size > part->size) {
        otaSetError(err, 413, "size", "too_large", false, "Firmware is larger than the OTA partition (%u bytes)", (unsigned)part->size);
        return false;
    }

    // Step 3: validate the optional MD5
    if (md5 != nullptr && md5[0] != '\0' && !otaIsMd5(md5)) {
        otaSetError(err, 400, "begin", "bad_md5", false, "MD5 must be 32 hex characters");
        return false;
    }

    // Step 4: allocate the chunk buffer; shrink the chunk size until it fits
    otaReleaseBuffer();
    uint32_t cs = otaPickChunkSize();
    uint8_t *buf = nullptr;
    while (cs >= OTA_SECTOR_SIZE) {
        buf = (uint8_t *)malloc(cs);
        if (buf != nullptr) break;
        cs = (cs / 2) & ~(OTA_SECTOR_SIZE - 1u);
    }
    if (buf == nullptr) {
        otaSetError(err, 503, "oom", "no_memory", true, "Not enough RAM for the chunk buffer");
        return false;
    }

    // Step 5: the timer that removes a session whose client has gone away
    if (s_timeoutTimer == nullptr) {
        s_timeoutTimer = xTimerCreate("ota_timeout", pdMS_TO_TICKS(OTA_TIMER_PERIOD_MS), pdTRUE, nullptr, otaTimeoutCallback);
    }
    if (s_timeoutTimer == nullptr) {
        free(buf);
        otaSetError(err, 503, "oom", "no_timer", true, "Could not create the session timer");
        return false;
    }

    // Step 6: an update left open by the old /doUpdate route would make Update.begin() refuse; drop it
    if (Update.isRunning()) {
        LOG_WARNING("[OTA] Dropping an unfinished update before starting a new session");
        Update.abort();
    }

    // Step 7: start the flash update with the exact size (end() then also verifies that everything arrived)
    if (!Update.begin(size)) {
        otaSetError(err, 500, "begin", "update_begin", false, "Update.begin failed: %s", Update.errorString());
        free(buf);
        return false;
    }
    if (md5 != nullptr && md5[0] != '\0') {
        Update.setMD5(md5);
    }

    // Step 8: fill the session
    memset(&S, 0, sizeof(S));
    S.state = OTA_RECEIVING;
    snprintf(S.id, sizeof(S.id), "%08x", (unsigned)esp_random());
    S.total = size;
    S.chunkSize = cs;
    S.buf = buf;
    S.lastChunk = -1;
    S.lastActivityMs = millis();

    xTimerStart(s_timeoutTimer, pdMS_TO_TICKS(100));

    strncpy(sessionOut, S.id, 9);
    chunkSize = cs;
    LOG_INFO_STR("[OTA] Session %s started: %u bytes, chunk %u bytes", S.id, (unsigned)size, (unsigned)cs);
    return true;
}

bool ota_chunk_start(const void *owner, const char *session, uint32_t offset, uint32_t crc, size_t bodyLen, OtaError &err) {
    OtaLock lock;

    // Step 1: the session must exist and be open
    if (!otaSessionMatches(session)) {
        otaSetError(err, 404, "session", "unknown_session", false, "Unknown or expired session");
        return false;
    }
    if (S.state != OTA_RECEIVING) {
        // Tell the client why its session is gone (for example a timeout) instead of a generic error
        if (S.state == OTA_FAILED && S.hasError) {
            err = S.error;
        } else {
            otaSetError(err, 404, "session", "session_closed", false, "Session is not receiving");
        }
        return false;
    }

    // Step 2: one chunk at a time; a half received chunk of a dead request may be replaced
    if (S.owner != nullptr && S.owner != owner) {
        if ((uint32_t)(millis() - S.chunkStartMs) < OTA_CHUNK_STEAL_MS) {
            otaSetError(err, 409, "busy", "chunk_in_progress", true, "Another chunk is still being received");
            return false;
        }
        S.owner = nullptr;
    }

    // Step 3: the offset must be the next expected one, or the last accepted chunk sent again
    uint32_t expectedLen = 0;
    if (offset == S.offset) {
        if (offset >= S.total) {
            otaSetError(err, 409, "offset", "already_complete", false, "All bytes were already received");
            err.offset = offset;
            err.expectedOffset = S.offset;
            return false;
        }
        uint32_t remaining = S.total - offset;
        expectedLen = (remaining < S.chunkSize) ? remaining : S.chunkSize;
    } else if (S.hasLast && offset == S.lastOffset) {
        expectedLen = S.lastLen;
    } else {
        otaSetError(err, 409, "offset", "bad_offset", true, "Chunk out of order, expected offset %u", (unsigned)S.offset);
        err.chunk = (int32_t)(offset / S.chunkSize);
        err.offset = offset;
        err.expectedOffset = S.offset;
        return false;
    }

    // Step 4: the body length must be exactly what this chunk should have (also protects the buffer size)
    if (bodyLen != expectedLen) {
        otaSetError(err, 400, "length", "bad_length", true, "Chunk length %u, expected %u", (unsigned)bodyLen, (unsigned)expectedLen);
        err.chunk = (int32_t)(offset / S.chunkSize);
        err.offset = offset;
        err.expectedOffset = S.offset;
        return false;
    }

    // Step 5: accept the chunk: remember its description and wait for the body pieces
    S.owner = owner;
    S.chunkOffset = offset;
    S.chunkCrc = crc;
    S.chunkLen = expectedLen;
    S.chunkFilled = 0;
    S.chunkOverflow = false;
    S.chunkStartMs = millis();
    S.lastActivityMs = millis();
    return true;
}

void ota_chunk_append(const void *owner, const uint8_t *data, size_t len, size_t index) {
    OtaLock lock;
    if (S.state != OTA_RECEIVING || S.owner != owner || S.buf == nullptr) return;

    // Pieces must arrive in order and never exceed the declared length
    if (index != S.chunkFilled || (size_t)S.chunkFilled + len > S.chunkLen) {
        S.chunkOverflow = true;
        return;
    }
    memcpy(S.buf + S.chunkFilled, data, len);
    S.chunkFilled += len;
    S.lastActivityMs = millis();
}

bool ota_chunk_commit(const void *owner, OtaProgress &prog, OtaError &err) {
    OtaLock lock;

    if (S.state != OTA_RECEIVING || S.owner != owner || S.buf == nullptr) {
        otaSetError(err, 409, "busy", "chunk_lost", true, "Chunk was replaced or the session ended");
        return false;
    }

    uint32_t chunkOffset = S.chunkOffset;
    int32_t chunkIndex = (int32_t)(chunkOffset / S.chunkSize);
    S.owner = nullptr;   // the chunk is no longer "in progress", whatever happens below

    // Step 1: all bytes must have arrived in order
    if (S.chunkOverflow || S.chunkFilled != S.chunkLen) {
        otaSetError(err, 400, "length", "bad_length", true, "Chunk %d body incomplete (%u of %u bytes)", (int)chunkIndex, (unsigned)S.chunkFilled, (unsigned)S.chunkLen);
        err.chunk = chunkIndex;
        err.offset = chunkOffset;
        err.expectedOffset = S.offset;
        return false;
    }

    // Step 2: integrity of this chunk. Nothing is written yet, so the client can simply send it again
    uint32_t crc = otaCrc32(S.buf, S.chunkLen);
    if (crc != S.chunkCrc) {
        otaSetError(err, 422, "crc", "crc_mismatch", true, "Chunk %d CRC mismatch", (int)chunkIndex);
        err.chunk = chunkIndex;
        err.offset = chunkOffset;
        err.expectedOffset = S.offset;
        return false;
    }

    // Step 3: a chunk that was already written (the client missed our answer): answer ok without writing twice
    if (chunkOffset != S.offset) {
        prog.offset = S.offset;
        prog.total = S.total;
        prog.percent = S.total ? (S.offset * 100.0f / S.total) : 0.0f;
        return true;
    }

    // Step 4: the first chunk carries the image header: refuse a wrong file before touching flash
    if (chunkOffset == 0 && !otaCheckHeader(S.buf, S.chunkLen, err)) {
        err.chunk = chunkIndex;
        err.offset = chunkOffset;
        otaFail(err);
        return false;
    }

    // Step 5: write to flash. A failure here cannot be retried, the update is aborted
    size_t written = Update.write(S.buf, S.chunkLen);
    if (written != S.chunkLen) {
        otaSetError(err, 500, "write", "flash_write", false, "Flash write failed at chunk %d: %s", (int)chunkIndex, Update.errorString());
        err.chunk = chunkIndex;
        err.offset = chunkOffset;
        otaFail(err);
        return false;
    }

    // Step 6: remember this chunk and move the expected offset forward
    S.hasLast = true;
    S.lastOffset = chunkOffset;
    S.lastLen = S.chunkLen;
    S.lastCrc = crc;
    S.lastChunk = chunkIndex;
    S.offset += S.chunkLen;
    S.lastActivityMs = millis();

    prog.offset = S.offset;
    prog.total = S.total;
    prog.percent = S.total ? (S.offset * 100.0f / S.total) : 0.0f;
    return true;
}

void ota_chunk_cancel(const void *owner) {
    OtaLock lock;
    if (S.owner != nullptr && S.owner == owner) {
        S.owner = nullptr;
        S.chunkFilled = 0;
        S.chunkOverflow = false;
    }
}

bool ota_end(const char *session, OtaError &err) {
    OtaLock lock;

    // Step 1: the session must be the open one
    if (!otaSessionMatches(session)) {
        otaSetError(err, 404, "session", "unknown_session", false, "Unknown or expired session");
        return false;
    }
    if (S.state != OTA_RECEIVING) {
        if (S.state == OTA_FAILED && S.hasError) {
            err = S.error;
        } else {
            otaSetError(err, 404, "session", "session_closed", false, "Session is not receiving");
        }
        return false;
    }

    // Step 2: every byte must have been written
    if (S.offset != S.total) {
        otaSetError(err, 409, "offset", "incomplete", true, "Only %u of %u bytes were received", (unsigned)S.offset, (unsigned)S.total);
        err.offset = S.offset;
        err.expectedOffset = S.offset;
        return false;
    }

    // Step 3: verify the whole image (size and MD5 if given) and select the new boot partition
    if (!Update.end()) {
        bool md5Error = (Update.getError() == UPDATE_ERROR_MD5);
        otaSetError(err, 500, md5Error ? "md5" : "end", md5Error ? "md5_mismatch" : "update_end", false, "Final check failed: %s", Update.errorString());
        otaFail(err);
        return false;
    }

    // Step 4: done
    otaReleaseBuffer();
    otaStopTimer();
    S.state = OTA_DONE;
    S.hasError = false;
    LOG_INFO_STR("[OTA] Session %s finished: %u bytes written", S.id, (unsigned)S.total);
    return true;
}

bool ota_abort(const char *session, OtaError &err) {
    OtaLock lock;

    if (session != nullptr && !otaSessionMatches(session)) {
        otaSetError(err, 404, "session", "unknown_session", false, "Unknown or expired session");
        return false;
    }
    if (S.state == OTA_RECEIVING) {
        Update.abort();
        otaReleaseBuffer();
        otaStopTimer();
        S.state = OTA_IDLE;
        S.hasError = false;
        LOG_WARNING("[OTA] Session aborted by the client");
    }
    return true;
}

void ota_get_status(OtaStatus &st) {
    OtaLock lock;
    st.state = S.state;
    st.offset = S.offset;
    st.total = S.total;
    st.percent = S.total ? (S.offset * 100.0f / S.total) : 0.0f;
    st.chunk = S.lastChunk;
    st.hasError = S.hasError;
    if (S.hasError) {
        st.error = S.error;
    }
}

bool ota_is_busy() {
    OtaLock lock;
    return S.state == OTA_RECEIVING;
}

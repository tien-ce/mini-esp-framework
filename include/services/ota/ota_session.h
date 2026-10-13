#ifndef OTA_SESSION_H
#define OTA_SESSION_H

#include <Arduino.h>

/*
 * Chunked OTA session (transport independent).
 *
 * The web layer cuts nothing and parses nothing here: it only forwards the request parameters and the raw
 * body pieces. The session owns the RAM buffer of one chunk, validates it (offset, length, CRC-32) and only
 * then writes it to flash, so a damaged chunk never reaches flash and can simply be sent again.
 *
 * Call order for one update:
 *   ota_begin()
 *   repeat per chunk: ota_chunk_start() -> ota_chunk_append() (once per received body piece) -> ota_chunk_commit()
 *   ota_end()
 * ota_abort() or the inactivity timeout can end a session at any time.
 */

/* -------------------------------------------------------------------------- */
/*                                   TYPES                                    */
/* -------------------------------------------------------------------------- */

/** State of the OTA session. */
enum OtaState {
    OTA_IDLE = 0,   /**< No session, nothing to report. */
    OTA_RECEIVING,  /**< A session is open and accepting chunks. */
    OTA_DONE,       /**< The last session finished successfully (device reboots). */
    OTA_FAILED      /**< The last session failed; see OtaStatus::error. */
};

/** Description of one failure, shaped like the JSON error answer of the API. */
struct OtaError {
    uint16_t    http;            /**< HTTP status to answer with. */
    const char *stage;           /**< Stage name (busy, size, magic, chip, oom, session, offset, length, crc, write, md5, end, timeout, begin). */
    const char *code;            /**< Short machine readable code. */
    char        message[80];     /**< Short English text. */
    int32_t     chunk;           /**< Chunk index, or -1 when not applicable. */
    int64_t     offset;          /**< Byte offset of the chunk, or -1 when not applicable. */
    int64_t     expectedOffset;  /**< Offset the device expects next, or -1 when not applicable. */
    bool        retryable;       /**< True when the client may send the same request again. */
};

/** Progress of the open session. */
struct OtaProgress {
    uint32_t offset;   /**< Next byte the device expects (bytes already written to flash). */
    uint32_t total;    /**< Declared size of the firmware. */
    float    percent;  /**< offset / total in percent, one decimal. */
};

/** Data answered by GET /api/ota/info. */
struct OtaInfo {
    bool        busy;       /**< True when a session is open. */
    uint32_t    maxSize;    /**< Size of the next OTA partition in bytes. */
    uint32_t    chunkSize;  /**< Chunk size a new session would use (0 when RAM is too low). */
    const char *chip;       /**< Chip name: esp32, esp32s3, esp32c3. */
    uint32_t    freeHeap;   /**< Free heap in bytes. */
};

/** Data answered by GET /api/ota/status. */
struct OtaStatus {
    OtaState    state;      /**< Current state. */
    uint32_t    offset;     /**< Bytes written so far. */
    uint32_t    total;      /**< Declared firmware size. */
    float       percent;    /**< Progress in percent. */
    int32_t     chunk;      /**< Index of the last handled chunk, or -1. */
    bool        hasError;   /**< True when error holds a failure. */
    OtaError    error;      /**< Last failure (valid when hasError). */
};

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Fills an OtaError with the given values (offsets and chunk are set to "not applicable").
 * @param err Error to fill.
 * @param http HTTP status.
 * @param stage Stage name (string literal).
 * @param code Machine readable code (string literal).
 * @param retryable Whether the client may retry.
 * @param message Short English text (copied, quotes are replaced).
 */
void ota_error_make(OtaError &err, uint16_t http, const char *stage, const char *code, bool retryable, const char *message);

/**
 * @brief Reports capacity and availability for a new session.
 * @param info Filled with the current values.
 */
void ota_get_info(OtaInfo &info);

/**
 * @brief Opens a session: checks the size, allocates the chunk buffer and starts the flash update.
 * @param size Exact firmware size in bytes.
 * @param md5 Lower-case hex MD5 of the whole file (32 characters), or nullptr to skip the check.
 * @param sessionOut Receives the session id (at least 9 bytes).
 * @param chunkSize Receives the chunk size to use for this session.
 * @param err Filled when the call fails.
 * @return true when the session is open.
 */
bool ota_begin(uint32_t size, const char *md5, char *sessionOut, uint32_t &chunkSize, OtaError &err);

/**
 * @brief Starts receiving one chunk (validates session, offset and body length before any data arrives).
 * @param owner Identity of the request that sends the chunk (used to match the later calls).
 * @param session Session id from ota_begin().
 * @param offset Byte position of the chunk in the file.
 * @param crc CRC-32 of the chunk bytes.
 * @param bodyLen Content length of the request body.
 * @param err Filled when the call fails.
 * @return true when the body should now be passed to ota_chunk_append().
 */
bool ota_chunk_start(const void *owner, const char *session, uint32_t offset, uint32_t crc, size_t bodyLen, OtaError &err);

/**
 * @brief Copies one received body piece into the chunk buffer.
 * @param owner Same identity as in ota_chunk_start().
 * @param data Piece data.
 * @param len Piece length.
 * @param index Offset of this piece inside the body.
 */
void ota_chunk_append(const void *owner, const uint8_t *data, size_t len, size_t index);

/**
 * @brief Validates the complete chunk (length, CRC-32, image header for the first chunk) and writes it to flash.
 * @param owner Same identity as in ota_chunk_start().
 * @param prog Receives the new progress on success.
 * @param err Filled when the call fails.
 * @return true when the chunk is accepted (or was an already accepted chunk sent again).
 */
bool ota_chunk_commit(const void *owner, OtaProgress &prog, OtaError &err);

/**
 * @brief Releases a chunk that was started but not committed (for example the client disconnected).
 * @param owner Same identity as in ota_chunk_start().
 */
void ota_chunk_cancel(const void *owner);

/**
 * @brief Finishes the session: checks that everything arrived, verifies the image (and MD5) and selects the new firmware.
 * @param session Session id from ota_begin().
 * @param err Filled when the call fails.
 * @return true when the new firmware is ready; the caller should reboot.
 */
bool ota_end(const char *session, OtaError &err);

/**
 * @brief Cancels the open session and frees its buffer.
 * @param session Session id, or nullptr to cancel whatever session is open.
 * @param err Filled when the call fails (wrong session id).
 * @return true when no session is open any more.
 */
bool ota_abort(const char *session, OtaError &err);

/**
 * @brief Copies the current state, progress and last error.
 * @param st Filled with the values.
 */
void ota_get_status(OtaStatus &st);

/**
 * @brief Tells whether a session is open (used to keep the legacy upload route out of the way).
 * @return true when a session is receiving.
 */
bool ota_is_busy();

#endif // OTA_SESSION_H

#ifndef CORE_RESET_LOG_H
#define CORE_RESET_LOG_H

#include <stdint.h>

#define CRASH_MAGIC             0xDEADBEEF /* Marks that the record holds a crash not yet written to flash */
#define CRASH_REASON_MAX_LEN    64
#define CRASH_TASK_NAME_MAX_LEN 16         /* configMAX_TASK_NAME_LEN: FreeRTOS keeps at most this */
#define CRASH_BACKTRACE_MAX     60         /* Same size as arduino_panic_info_t.backtrace */

#define RESET_LOG_DIR           "/log"
#define RESET_HISTORY_FILE      RESET_LOG_DIR "/before_shutdown.txt" /* Log lines leading up to the last reset */
#define CRASH_LOG_FILE          RESET_LOG_DIR "/crash.txt"           /* Details of the last crash */
#define CRASH_HISTORY_FILE      RESET_LOG_DIR "/before_crash.txt"    /* Log lines leading up to the last crash */

/**
 * @brief Last crash captured by the panic handler, kept in no-init memory across the reboot.
 */
typedef struct {
    uint32_t magic;                              /* CRASH_MAGIC when a crash is waiting to be saved */
    int      core;                               /* Core that triggered the panic */
    char     task[CRASH_TASK_NAME_MAX_LEN];      /* Task running on that core; for a TI script, its path */
    uint32_t pc;                                 /* Instruction address that faulted */
    uint32_t uptime_s;                           /* Seconds since boot: at boot vs. after running for days */
    uint32_t free_heap;                          /* Free internal heap at the crash */
    uint32_t min_free_heap;                      /* Lowest free internal heap since boot */
    char     reason[CRASH_REASON_MAX_LEN];       /* Exception string, e.g. "StoreProhibited" */
    uint32_t backtrace_len;                      /* Number of valid entries in backtrace */
    uint8_t  backtrace_corrupt;                  /* 1 when the stack walk hit a bad frame: trust only the first 2 entries */
    uint32_t backtrace[CRASH_BACKTRACE_MAX];     /* Backtrace PCs */
} crash_record_t;

/** @brief Registers the panic handler that copies crash info into the no-init record. */
void reset_log_init(void);

/**
 * @brief Saves what the previous run left behind, overwriting the old files:
 *        - any reset except power-on: the recent log history to RESET_HISTORY_FILE
 *        - a crash (panic handler ran): its details to CRASH_LOG_FILE and the same history to
 *          CRASH_HISTORY_FILE, which later non-crash resets do not overwrite
 * @note Needs the file system mounted. The crash record is cleared only after CRASH_LOG_FILE
 *       is written, so a failed write is retried on the next boot.
 */
void reset_log_save(void);

#endif // CORE_RESET_LOG_H

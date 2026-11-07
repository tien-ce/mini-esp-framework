#include "core/reset_log.h"
#include "core/core_log.h"
#include "core/file_system.h"
#include "core/info.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

/* Text of one crash record: header lines (incl. 64-char ELF SHA256), then up to
 * CRASH_BACKTRACE_MAX " 0xXXXXXXXX" entries */
#define CRASH_TEXT_MAX_LEN    (448 +CRASH_REASON_MAX_LEN + CRASH_BACKTRACE_MAX * 11)

/* History file: a short header (reset reason, firmware), then the whole log history */
#define HISTORY_TEXT_MAX_LEN  (128 + CORE_LOG_HISTORY_SIZE + 1)

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/* Not cleared on reboot, so the crash written here is still readable on the next boot */
static RTC_NOINIT_ATTR crash_record_t s_crash_record;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Panic hook called by Arduino's wrapped esp_panic_handler before the reboot.
 * @note Runs in panic context: only copy into RAM, no logging, Serial, malloc or file system.
 */
static void crash_panic_handler(arduino_panic_info_t *info, void *arg) {
    (void)arg;

    s_crash_record.core = info->core;
    s_crash_record.pc = (uint32_t)info->pc;

    /* Reads a hardware counter: no lock */
    s_crash_record.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);

    /* Both only read counters each heap keeps; never call anything that locks or walks the heap
     * here (e.g. heap_caps_get_largest_free_block), the crash may be inside malloc */
    s_crash_record.free_heap = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_crash_record.min_free_heap = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);

    /* The task that was running on the faulting core when it crashed */
    TaskHandle_t task = xTaskGetCurrentTaskHandleForCore(info->core);
    const char *task_name = task ? pcTaskGetName(task) : NULL;
    strncpy(s_crash_record.task, task_name ? task_name : "Unknown", sizeof(s_crash_record.task) - 1);
    s_crash_record.task[sizeof(s_crash_record.task) - 1] = '\0';

    const char *reason = info->reason ? info->reason : "Unknown";
    strncpy(s_crash_record.reason, reason, sizeof(s_crash_record.reason) - 1);
    s_crash_record.reason[sizeof(s_crash_record.reason) - 1] = '\0';

    uint32_t len = info->backtrace_len;
    if (len > CRASH_BACKTRACE_MAX) len = CRASH_BACKTRACE_MAX;
    for (uint32_t i = 0; i < len; i++) {
        s_crash_record.backtrace[i] = info->backtrace[i];
    }
    s_crash_record.backtrace_len = len;
    s_crash_record.backtrace_corrupt = info->backtrace_corrupt ? 1 : 0;

    /* Set last so a record is only marked valid once it is complete */
    s_crash_record.magic = CRASH_MAGIC;
}

/**
 * @brief Builds the history text: a short header, then the log lines leading up to the last reset.
 * @param text Destination buffer, NUL-terminated on return.
 * @param size Size of text.
 * @return Length of the text (excluding the NUL).
 */
static size_t build_history(char *text, size_t size) {
    int pos = snprintf(text, size, "Reset reason: %s\nFirmware: %s\n\n",
                       esp_info_get_reset_reason(), FW_VERSION_STR);
    if (pos > (int)size - 1) pos = size - 1;

    /* The previous run's last lines, up to the "----- boot -----" line of this run
     * (lines after it are from the current boot) */
    pos += (int)core_log_history_read(text + pos, size - pos);
    return (size_t)pos;
}

/**
 * @brief Writes the history text to a file.
 * @note The history is extra context: a failed write is only reported.
 */
static void save_history(const char *path, const char *text, size_t len) {
    FsResult_t ret = write_file(path, text, (unsigned int)len, NULL);
    if (ret != FS_OK) {
        LOG_ERROR_STR("Reset log: can't write %s: %s", path, file_system_strerror(ret));
    }
}

/**
 * @brief Writes the crash left in the no-init record to CRASH_LOG_FILE, then clears the record.
 */
static void save_crash(void) {
    /* The field comes from no-init memory, never index past the array */
    uint32_t len = s_crash_record.backtrace_len;
    if (len > CRASH_BACKTRACE_MAX) len = CRASH_BACKTRACE_MAX;

    /*
     * Identify the build: the board reboots into the firmware that crashed and saves at that
     * boot, so the running image's version and ELF SHA256 are the crashed one's. The SHA picks
     * the matching firmware.elf for addr2line.
     */
    char elf_sha[65];
    esp_app_get_elf_sha256(elf_sha, sizeof(elf_sha));

    /* Build the text; static so the boot task's stack is not used for it */
    uint32_t up = s_crash_record.uptime_s;

    static char text[CRASH_TEXT_MAX_LEN];
    int pos = snprintf(text, sizeof(text),
                       "Firmware: %s\nELF SHA256: %s\n"
                       "Uptime: %lud %02lu:%02lu:%02lu (%lu s)\n"
                       "Free heap: %lu B (min since boot %lu B)\n"
                       "Core: %d\nTask: %.*s\nReason: %s\nPC: 0x%08lX\nBacktrace:",
                       FW_VERSION_STR, elf_sha,
                       (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
                       (unsigned long)(up / 60 % 60), (unsigned long)(up % 60), (unsigned long)up,
                       (unsigned long)s_crash_record.free_heap,
                       (unsigned long)s_crash_record.min_free_heap,
                       s_crash_record.core,
                       CRASH_TASK_NAME_MAX_LEN, s_crash_record.task,
                       s_crash_record.reason,
                       (unsigned long)s_crash_record.pc);
    for (uint32_t i = 0; i < len && pos < (int)sizeof(text); i++) {
        pos += snprintf(text + pos, sizeof(text) - pos, " 0x%08lX",
                        (unsigned long)s_crash_record.backtrace[i]);
    }
    /* Entries 0-1 come from registers (PC, a0); later ones were read from a stack that was damaged */
    if (s_crash_record.backtrace_corrupt && pos < (int)sizeof(text)) {
        pos += snprintf(text + pos, sizeof(text) - pos,
                        " |<-CORRUPTED (trust only the first 2 addresses)");
    }
    if (pos < (int)sizeof(text)) {
        pos += snprintf(text + pos, sizeof(text) - pos, "\n");
    }
    if (pos > (int)sizeof(text) - 1) pos = sizeof(text) - 1;

    FsResult_t ret = write_file(CRASH_LOG_FILE, text, (unsigned int)pos, NULL);
    if (ret != FS_OK) {
        LOG_ERROR_STR("Reset log: can't write %s: %s", CRASH_LOG_FILE, file_system_strerror(ret));
        return;
    }

    /* Saved: clear the mark so the same crash is not written again */
    s_crash_record.magic = 0;
    LOG_ERROR_STR("Previous run crashed (%s), saved to %s", s_crash_record.reason, CRASH_LOG_FILE);
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void reset_log_init(void) {
    set_arduino_panic_handler(crash_panic_handler, NULL);
}

void reset_log_save(void) {
    /* Power-on: RTC memory holds garbage and there is no previous run to report */
    if (esp_reset_reason() == ESP_RST_POWERON) {
        s_crash_record.magic = 0;
        return;
    }

    /* write_file() creates the file but not its folder */
    FsResult_t ret = make_directory(RESET_LOG_DIR);
    if (ret != FS_OK && ret != FS_ERR_ALREADY_EXISTS) {
        LOG_ERROR_STR("Reset log: can't create %s: %s", RESET_LOG_DIR, file_system_strerror(ret));
        return;
    }

    /* Read the history first: lines logged from now on push the previous run's oldest ones out */
    static char history[HISTORY_TEXT_MAX_LEN];
    size_t history_len = build_history(history, sizeof(history));

    save_history(RESET_HISTORY_FILE, history, history_len);

    /* A crash keeps its own copy, so later restarts/brownouts don't overwrite the log before it */
    if (s_crash_record.magic == CRASH_MAGIC) {
        save_history(CRASH_HISTORY_FILE, history, history_len);
        save_crash();
    }
}

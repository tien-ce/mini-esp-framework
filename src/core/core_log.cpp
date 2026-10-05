/*---- INCLUDES ----*/
#include "core/core_log.h"
#include <cstring>

/*---- CONFIGURATION ----*/
/** @brief Max characters (incl. '\0') carried through the log queue per message; longer text is truncated. */
#define LOG_MSG_MAX_LEN 192

/**
 * @brief Max number of messages the queue can hold before it is "full".
 *
 * A FreeRTOS queue is a fixed-capacity ring buffer allocated once, up front,
 * by xQueueCreate() - it never grows. This is the number of LogMessage_t
 * slots reserved for that buffer. If producers log faster than the
 * dispatch task can drain them, the queue fills up and any further
 * core_log_print() calls drop their message instead of blocking (see the
 * xQueueSend() call at the bottom of this file).
 */
#define LOG_QUEUE_LENGTH 32

/**
 * @brief Stack size (in BYTES) for the dispatch task.
 * @note On vanilla FreeRTOS, task stack sizes are given in 32-bit WORDS.
 *       The ESP-IDF port that Arduino-ESP32 is built on redefines this to
 *       be BYTES instead - xTaskCreate() below takes this value as-is.
 *       4096 bytes is generous headroom for the String concatenation +
 *       Serial.println() work this task does (see core_log_dispatch()).
 */
#define LOG_TASK_STACK_SIZE 4096

/**
 * @brief Priority for the dispatch task.
 * @note On FreeRTOS, HIGHER numbers mean HIGHER priority (the opposite of
 *       "nice" values on Linux). tskIDLE_PRIORITY (0) is the lowest
 *       possible priority, reserved for the built-in idle task that only
 *       ever runs when nothing else is ready. We use idle+1 so the logger
 *       is still above pure idle (it will run and drain the queue), but
 *       below every real application task - logging must never preempt or
 *       starve actual work, only tidy up in the background.
 */
#define LOG_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/*---- TYPES ----*/
/**
 * @brief One buffered log entry, copied from a producer task into the queue.
 *
 * This is a plain fixed-size POD struct on purpose - it must NOT contain a
 * C++ String (or any other type with a heap pointer inside it). Here's why:
 * a FreeRTOS queue does not know or care what type it holds. xQueueSend()
 * and xQueueReceive() simply memcpy() sizeof(item) raw bytes in and out of
 * the queue's internal buffer. If the item were a String, that memcpy
 * would blindly duplicate the String's internal heap pointer - producing
 * TWO String objects (the producer's original, and the one memcpy'd into
 * the queue slot) that both believe they own and must free the SAME heap
 * buffer. Whichever one is destructed second frees already-freed memory.
 * A fixed `char[]` has no such pointer, so a raw byte copy is always safe,
 * and the producer side never has to allocate anything to build this.
 */
typedef struct {
    char text[LOG_MSG_MAX_LEN]; /**< NUL-terminated message text, truncated to fit. */
    LogLevel level;               /**< Severity level this message was logged at. */
} LogMessage_t;

/*---- STATIC VARIABLES ----*/
/** @brief Holds the current log level threshold to filter outgoing messages. */
static LogLevel currentLogLevel = LOG_LEVEL_DEBUG;

/**
 * @brief Handle to the log message queue - the single hand-off point between
 *        producer tasks (core_log_print) and the dispatch task.
 * @note NULL until core_log_init() successfully creates it. Every producer
 *       call checks this and falls back to synchronous printing if it's
 *       still NULL (e.g. a LOG() call that happens before setup() runs
 *       core_log_init(), or a queue/task creation failure).
 */
static QueueHandle_t logQueue = NULL;

/** @brief Handle to the dispatch task created by core_log_init(); NULL if it hasn't been created (yet, or ever). */
static TaskHandle_t logTaskHandle = NULL;

/** @brief Registered callbacks to forward log messages to external services (e.g., WebSockets). */
#define MAX_LOG_CALLBACKS 5
static LogOutputCallback logCallbacks[MAX_LOG_CALLBACKS] = {NULL};

/*---- STATIC HELPER FUNCTIONS ----*/

/**
 * @brief Perform the actual blocking work for one log message: a colored Serial
 *        line, then fan-out to every registered callback.
 *
 * @param text  NUL-terminated message text to output.
 * @param level Severity level the message was logged at (selects the ANSI color).
 *
 * @note This is only ever called from core_log_dispatch_task() below, and
 *       that task is the ONLY code in this entire file that touches
 *       Serial or walks logCallbacks[]. Because there is exactly one
 *       caller, by construction, there can never be two tasks racing to
 *       write Serial at once - so none of this needs a mutex. This is the
 *       whole point of the redesign: instead of many producer tasks
 *       fighting over a lock around slow I/O, there is one single-writer
 *       task and everyone else just drops a message in its mailbox.
 */
static void core_log_dispatch(const char *text, LogLevel level)
{
    switch (level) {
        case LOG_LEVEL_DEBUG:
            Serial.println(String(LOG_COLOR_BLUE) + text + LOG_COLOR_RESET);
            break;
        case LOG_LEVEL_INFO:
            Serial.println(String(LOG_COLOR_GREEN) + text + LOG_COLOR_RESET);
            break;
        case LOG_LEVEL_WARNING:
            Serial.println(String(LOG_COLOR_YELLOW) + text + LOG_COLOR_RESET);
            break;
        case LOG_LEVEL_ERROR:
            Serial.println(String(LOG_COLOR_RED) + text + LOG_COLOR_RESET);
            break;
        default:
            break;
    }

    // Forward to any registered external sinks (like WebSockets). The
    // LogOutputCallback signature takes a String, so we build exactly one
    // here - this heap allocation now happens only on this dedicated task,
    // never on whatever random task originally called LOG().
    String msgStr(text);
    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (logCallbacks[i] != NULL) {
            logCallbacks[i](msgStr, level);
        }
    }
}

/**
 * @brief Dispatch task body: the single consumer that drains the log queue forever.
 * @param pvParameters Unused (required by the FreeRTOS task-function signature).
 *
 * @note How the wait here works: xQueueReceive(logQueue, &entry, portMAX_DELAY)
 *       asks FreeRTOS to pop the oldest queued LogMessage_t into `entry`.
 *       If the queue is currently empty, this task is suspended - using
 *       ZERO CPU time - until either a producer pushes a new message
 *       (core_log_print's xQueueSend wakes this task up immediately) or
 *       portMAX_DELAY ticks pass (which, being the largest possible tick
 *       count, effectively means "wait forever"). This is why a dedicated
 *       task is cheap: it is asleep the vast majority of the time and only
 *       runs in short bursts exactly when there is real work to do.
 */
static void core_log_dispatch_task(void *pvParameters)
{
    (void)pvParameters; // Unused - this task takes no parameters.

    LogMessage_t entry;
    for (;;) {
        // Block here until a message arrives. xQueueReceive() returns
        // pdTRUE and fills `entry` when it does; it can only return
        // pdFALSE if the wait timed out, which never happens with
        // portMAX_DELAY, but we still check defensively rather than
        // assume the impossible.
        if (xQueueReceive(logQueue, &entry, portMAX_DELAY) == pdTRUE) {
            core_log_dispatch(entry.text, entry.level);
        }
    }
}

/*---- PUBLIC FUNCTIONS ----*/

/**
 * @brief Initializes the core logging system.
 */
void core_log_init()
{
    // Start hardware serial for standard log output.
    Serial.begin(115200);

    // Create the queue once (guard so a second core_log_init() call, if it
    // ever happens, doesn't leak/replace an already-running queue).
    // xQueueCreate(length, item_size) allocates, up front, enough memory
    // for `length` items of `item_size` bytes each, plus the queue's own
    // control structure. It returns NULL if that allocation fails (e.g.
    // heap exhaustion) - logQueue stays NULL and every core_log_print()
    // call below will keep using its synchronous fallback path forever.
    if (logQueue == NULL) {
        logQueue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(LogMessage_t));
    }

    // Start the single consumer task that owns all the blocking I/O.
    // xTaskCreate() parameters, in order:
    //   1. core_log_dispatch_task - the function the new task will run.
    //   2. "log_dispatch"         - a name, purely for debugging (visible
    //                               in tools like the FreeRTOS task list).
    //   3. LOG_TASK_STACK_SIZE    - how much stack memory to reserve for it
    //                               (bytes, on this ESP-IDF-based port).
    //   4. NULL                   - the `pvParameters` the task function
    //                               receives; unused here.
    //   5. LOG_TASK_PRIORITY      - its scheduling priority (see the
    //                               define's comment above).
    //   6. &logTaskHandle         - where to store a handle to the new
    //                               task, so we could later inspect/delete
    //                               it if needed.
    // It returns pdPASS on success, or an error code (e.g. if the stack
    // allocation for the new task fails) otherwise.
    if (logQueue != NULL && logTaskHandle == NULL) {
        BaseType_t created = xTaskCreate(core_log_dispatch_task, "log_dispatch",
                                          LOG_TASK_STACK_SIZE, NULL,
                                          LOG_TASK_PRIORITY, &logTaskHandle);
        if (created != pdPASS) {
            // Nothing will ever drain this queue - every future
            // core_log_print() call would just fill it up and then drop
            // messages forever. Tear it down so logQueue goes back to
            // NULL, which makes core_log_print() fall back to printing
            // synchronously instead (slower, but every message gets out).
            vQueueDelete(logQueue);
            logQueue = NULL;
            logTaskHandle = NULL;
            Serial.println("core_log_init: failed to create dispatch task, logging will be synchronous");
        }
    }
}

/**
 * @brief Sets the current log filtering level.
 */
void core_log_set_level(LogLevel level)
{
    currentLogLevel = level;
}

/**
 * @brief Retrieves the current log filtering level.
 */
LogLevel core_log_get_level()
{
    return currentLogLevel;
}

/**
 * @brief Registers a callback function for log output.
 */
void core_log_register_cb(LogOutputCallback cb)
{
    // Find an empty slot in the callback array and register the new callback
    for (int i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (logCallbacks[i] == NULL) {
            logCallbacks[i] = cb;
            return;
        }
    }
    LOG_ERROR_STR("Full callback slot for log output");
}

/**
 * @brief Submits a plain C-string message for asynchronous logging.
 */
void core_log_print(const char *msg, LogLevel level)
{
    if (msg == NULL) return;

    // Drop messages below the current threshold before they ever reach the
    // queue - filtering stays cheap and on the producer side, so filtered
    // messages never even take up a queue slot.
    if (level < currentLogLevel) return;

    // Fallback path: the queue/dispatch task isn't up yet - e.g. this is a
    // very early LOG() call before setup() runs core_log_init(), or
    // core_log_init()'s task creation failed earlier. Print synchronously,
    // right here on the caller's own task, so the message is never
    // silently lost (at the cost of this call now blocking on Serial I/O,
    // same as the old mutex-based implementation always did).
    if (logQueue == NULL) {
        Serial.println(msg);
        return;
    }

    // Build the fixed-size queue entry. We copy into entry.text (bounded,
    // truncating with strncpy) rather than queue the incoming `msg`
    // pointer itself, because `msg` may point at a caller's local stack
    // buffer (e.g. the _log_buf used by the LOG_*_STR macros) that will no
    // longer exist by the time the dispatch task gets around to reading
    // it. Copying now, while the caller's buffer is still valid, is what
    // makes this safe to call from any task's stack.
    LogMessage_t entry;
    entry.level = level;
    strncpy(entry.text, msg, sizeof(entry.text) - 1);
    entry.text[sizeof(entry.text) - 1] = '\0';

    // Hand the entry to the dispatch task via the queue.
    // xQueueSend(queue, &entry, ticksToWait) copies sizeof(entry) bytes
    // into the next free queue slot. The third argument is how many
    // FreeRTOS ticks the CALLING task is willing to block if the queue is
    // currently full (no free slot). We pass 0: "try once, right now, and
    // if there's no room, give up immediately rather than wait." This is
    // the crux of making logging non-blocking - a producer task must never
    // be made to wait on how fast Serial output happens elsewhere. If the
    // dispatch task genuinely cannot keep up and the queue is full, this
    // call returns pdFALSE and the message is simply dropped - an
    // intentional trade-off (lose a log line) in exchange for never
    // stalling real application logic.
    xQueueSend(logQueue, &entry, 0);
}

/**
 * @brief String overload of core_log_print(), kept for backward compatibility.
 */
void core_log_print(const String &msg, LogLevel level)
{
    // Thin forwarding shim: c_str() gives a NUL-terminated view of the
    // String's existing buffer (no extra copy here), which the const
    // char* overload above then copies into the fixed-size queue entry.
    core_log_print(msg.c_str(), level);
}

#include "core/log_task.h"
#include "core/config_manager.h"
#include "core/web_server_task.h"
#include "core/core_engine.h"
#include "built_in.h"
#include "TienInterpreter.h"
#include <Arduino.h>
#include <unordered_map>

/* -------------------------------------------------------------------------- */
/*                             DEFINES & CONSTANTS                            */
/* -------------------------------------------------------------------------- */

#define SERIAL_BAUDRATE    115200 

/* -------------------------------------------------------------------------- */
/*                            TYPES & STRUCTURES                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Custom Hash Struct for Arduino String:
 * std::unordered_map requires a hashing function to map keys to bucket indices.
 * Because std::unordered_map does not natively support Arduino's 'String' class,
 * we convert 'String' to 'const char*' via .c_str() and pass it to std::hash<std::string>.
 */
struct StringHash {
    std::size_t operator()(const String& s) const {
        return std::hash<std::string>{}(s.c_str());
    }
};

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static LogLevel currentLogLevel = LOG_LEVEL_DEBUG;

// Local Module Log Mutex Handle
static SemaphoreHandle_t logMutex = NULL;

// Local Command Queue Handle
static QueueHandle_t commandQueue = NULL;

// Command registry map storing command name and handler function pointer
static std::unordered_map<String, CommandHandlerFunc, StringHash> commandMap;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*                        INTERPRETER BUILT-IN FUNCTIONS                      */
/* -------------------------------------------------------------------------- */

/** @brief Built-in print function callback registered into TienInterpreter engine. */
static value_t *built_in_print(value_t **argv, int argc) {
    if (argc == 0) {
        LOG_INFO("");
        return init_val(VAL_NULL);
    }
    if (argv == NULL) {
        return init_val(VAL_NULL);
    }
    String buffer = "";
    for (int i = 0; i < argc; i++) {
        if (argv[i] == NULL) {
            buffer += "null";
            continue;
        }
        switch (argv[i]->type) {
            case VAL_STRING:
                buffer += (argv[i]->string_val ? String(argv[i]->string_val) : "null");
                break;
            case VAL_INT:
                buffer += String(argv[i]->int_val);
                break;
            case VAL_FLOAT:
                buffer += String(argv[i]->float_val, 2);
                break;
            case VAL_BOOL:
                buffer += (argv[i]->bool_val ? "true" : "false");
                break;
            case VAL_NULL:
                buffer += "null";
                break;
            default:
                ti_log("[ERROR] %s: Unexpected type %d\n", BUILTIN_PRINT, argv[i]->type);
                ti_fatal();
                break;
        }
    }
    LOG_INFO(buffer);
    return init_val(VAL_NULL);
}

/** @brief Command handler: Restarts system. */
static void esp32_restart(const String &arg) {
    LOG_INFO("Received restart command, logging and restarting system");
    LOG_INFO("System restarting........");
    vTaskDelay(1000);
    ESP.restart();
}

/** @brief Converts LogLevel enum value to string representation. */
static String levelToStr(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "LOG_LEVEL_DEBUG";
        case LOG_LEVEL_INFO:    return "LOG_LEVEL_INFO";
        case LOG_LEVEL_WARNING: return "LOG_LEVEL_WARNING";
        case LOG_LEVEL_ERROR:   return "LOG_LEVEL_ERROR";
        default:                return "LOG_LEVEL_UNKNOWN";
    }
}

/** @brief Command handler: Executes multiple semicolon-separated commands sequentially. */
static void backlogHandler(const String &arg) {
    if (arg.length() == 0) {
        LOG_WARNING("Backlog: No commands provided. Format: Backlog <cmd1:arg1>; <cmd2:arg2>; ...");
        return;
    }

    LOG_INFO("Processing Backlog: " + arg);

    int startIdx = 0;
    int len = arg.length();

    while (startIdx < len) {
        int semiIdx = arg.indexOf(';', startIdx);
        String subCmd = "";
        if (semiIdx != -1) {
            subCmd = arg.substring(startIdx, semiIdx);
            startIdx = semiIdx + 1;
        } else {
            subCmd = arg.substring(startIdx);
            startIdx = len;
        }

        subCmd.trim();
        if (subCmd.length() > 0) {
            LOG_DEBUG("Backlog posting sub-command: " + subCmd);
            postIncomingCommand(subCmd);
        }
    }
}

/** @brief Parses raw command string and executes matching registered handler. */
static void execute_cmd(const String &raw_cmd) {
    LOG_DEBUG("Raw command received: " + raw_cmd);

    String cleanCmd = raw_cmd;
    cleanCmd.trim();

    // Strip leading prompt characters (e.g. ">>> ") if copied from terminal
    while (cleanCmd.startsWith(">")) {
        cleanCmd = cleanCmd.substring(1);
        cleanCmd.trim();
    }

    // Strip ANSI escape codes (e.g. \033[0m)
    while (true) {
        int escIdx = cleanCmd.indexOf('\033');
        if (escIdx == -1) break;
        int mIdx = cleanCmd.indexOf('m', escIdx);
        if (mIdx != -1) {
            cleanCmd = cleanCmd.substring(0, escIdx) + cleanCmd.substring(mIdx + 1);
        } else {
            cleanCmd = cleanCmd.substring(0, escIdx);
        }
    }
    cleanCmd.trim();

    // Strip trailing semicolons
    while (cleanCmd.endsWith(";")) {
        cleanCmd = cleanCmd.substring(0, cleanCmd.length() - 1);
        cleanCmd.trim();
    }

    if (cleanCmd.length() == 0) return;

    // Check if the command starts with BACKLOG (with or without colon/space)
    String upperCmd = cleanCmd;
    upperCmd.toUpperCase();
    if (upperCmd.startsWith("BACKLOG")) {
        String backlogArgs = "";
        if (upperCmd.startsWith("BACKLOG:")) {
            backlogArgs = cleanCmd.substring(8);
        } else if (upperCmd.startsWith("BACKLOG ")) {
            backlogArgs = cleanCmd.substring(8);
        } else {
            backlogArgs = cleanCmd.substring(7);
        }
        backlogArgs.trim();
        backlogHandler(backlogArgs);
        return;
    }

    // If string contains multiple commands separated by ';', automatically process as backlog
    if (cleanCmd.indexOf(';') != -1) {
        backlogHandler(cleanCmd);
        return;
    }

    String name = "";
    String arg = "";

    // Find the first delimiter: either ':' or ' '
    int colonIdx = cleanCmd.indexOf(':');
    int spaceIdx = cleanCmd.indexOf(' ');

    int delimiterIdx = -1;
    if (colonIdx != -1 && spaceIdx != -1) {
        delimiterIdx = (colonIdx < spaceIdx) ? colonIdx : spaceIdx;
    } else if (colonIdx != -1) {
        delimiterIdx = colonIdx;
    } else if (spaceIdx != -1) {
        delimiterIdx = spaceIdx;
    }

    if (delimiterIdx != -1) {
        name = cleanCmd.substring(0, delimiterIdx);
        arg = cleanCmd.substring(delimiterIdx + 1);
    } else {
        name = cleanCmd;
        arg = "";
    }

    // Sanitize quotes and spaces
    name.trim();
    arg.trim();
    while (arg.endsWith(";")) {
        arg = arg.substring(0, arg.length() - 1);
        arg.trim();
    }
    if (name.startsWith("\"") && name.endsWith("\"")) name = name.substring(1, name.length() - 1);
    if (arg.startsWith("\"") && arg.endsWith("\""))   arg = arg.substring(1, arg.length() - 1);

    // Make command name uppercase for case-insensitive lookup
    name.toUpperCase();

    LOG_DEBUG("Parsed command name: '" + name + "', arg: '" + arg + "'");

    auto it = commandMap.find(name);
    if (it != commandMap.end()) {
        LOG_DEBUG("Executing handler for: " + name);
        it->second(arg);
    } else {
        LOG_WARNING("Command not found: " + name);
    }
}

/** @brief Command handler: Lists acceptable parameters for log level. */
static void listLogLevel(const String &arg) {
    LOG_INFO("Acceptable setLogLevel parameters:");
    LOG_INFO("  0 or DEBUG / LOG_DEBUG / LOG_LEVEL_DEBUG");
    LOG_INFO("  1 or INFO  / LOG_INFO  / LOG_LEVEL_INFO");
    LOG_INFO("  2 or WARNING / LOG_WARNING / LOG_LEVEL_WARNING");
    LOG_INFO("  3 or ERROR / LOG_ERROR / LOG_LEVEL_ERROR");
}

/** @brief Saves current log level setting to NVS storage. */
static void saveLogConfig() {
    if (config_get_lock()) {
        config_save_int("log", "level", (int32_t)currentLogLevel);
        config_release_lock();
    }
}

/** @brief Loads log level setting from NVS storage on startup. */
static void loadLogConfig() {
    register_config_module("log", "log");
    if (config_get_lock()) {
        int32_t savedLevel = config_read_int("log", "level", (int32_t)LOG_LEVEL_DEBUG);
        if (savedLevel >= (int32_t)LOG_LEVEL_DEBUG && savedLevel <= (int32_t)LOG_LEVEL_ERROR) {
            currentLogLevel = (LogLevel)savedLevel;
        }
        config_release_lock();
    }
}

/** @brief Command handler: Sets current log severity level. */
static void setLogLevel(const String &arg) {
    String cleanArg = arg;
    cleanArg.trim();
    cleanArg.toUpperCase();

    LogLevel level = currentLogLevel; // Default to existing level if invalid

    // Check for numeric string ("0" - "3")
    if (cleanArg.length() == 1 && cleanArg[0] >= '0' && cleanArg[0] <= '3') {
        level = (LogLevel)cleanArg.toInt();
    } 
    // Check for string level names
    else if (cleanArg == "LOG_LEVEL_DEBUG"   || cleanArg == "LOG_DEBUG"   || cleanArg == "DEBUG")   level = LOG_LEVEL_DEBUG;
    else if (cleanArg == "LOG_LEVEL_INFO"    || cleanArg == "LOG_INFO"    || cleanArg == "INFO")    level = LOG_LEVEL_INFO;
    else if (cleanArg == "LOG_LEVEL_WARNING" || cleanArg == "LOG_WARNING" || cleanArg == "WARNING") level = LOG_LEVEL_WARNING;
    else if (cleanArg == "LOG_LEVEL_ERROR"   || cleanArg == "LOG_ERROR"   || cleanArg == "ERROR")   level = LOG_LEVEL_ERROR;
    else {
        LOG_WARNING("Invalid log level arg: " + arg);
        return;
    }

    currentLogLevel = level;
    saveLogConfig();
    LOG_INFO("Set Log Level: " + String(currentLogLevel) + " (" + levelToStr(currentLogLevel) + ")");
}

/** @brief Command handler: Prints current log severity level. */
static void getLogLevel(const String &arg) {
    LOG_INFO("Current Log Level: " + String(levelToStr(currentLogLevel)));
}

#if !(defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT > 0))
/** @brief Polls HardwareSerial RX buffer for standard UART interfaces. */
static void processHardwareSerialInput() {
    static String serialBuf = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            serialBuf.trim();
            if (serialBuf.length() > 0) {
                postIncomingCommand(serialBuf);
                serialBuf = "";
            }
        } else {
            serialBuf += c;
        }
    }
}
#endif

/**
 * @brief Initializes Serial interface and creates logging mutex & command queue.
 */
void log_task_init(void) {
    if (logMutex == NULL) {
        logMutex = xSemaphoreCreateMutex();
    }
    if (commandQueue == NULL) {
        commandQueue = xQueueCreate(32, sizeof(CommandPacket));
    }
    Serial.begin(SERIAL_BAUDRATE);

#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT > 0)
    Serial.onEvent(ARDUINO_HW_CDC_RX_EVENT, [](void* arg, esp_event_base_t base, int32_t id, void* data) {
        static String serialBuf = "";
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                serialBuf.trim();
                if (serialBuf.length() > 0) {
                    postIncomingCommand(serialBuf);
                    serialBuf = "";
                }
            } else {
                serialBuf += c;
            }
        }
    });
#endif
}

/**
 * @brief Updates system mode to normal to enable Serial log output.
 */
static void setSerialLogReady() {
    CoreState_SetMode(SYS_NORMAL);
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Thread-safe logging function that outputs colored messages to Serial and Web sockets.
 * @param msg The log message string.
 * @param level The log severity level.
 */
void logPrint(const String &msg, LogLevel level) {
    if (level < currentLogLevel) 
        return;

    WebServerState_t webState = CoreState_GetWebServer();
    bool web_ready = (webState == WEB_STATE_LISTENING || webState == WEB_STATE_CLIENT_CONNECTED);

    if (logMutex != NULL && xSemaphoreTake(logMutex, portMAX_DELAY) == true) {
        switch (level) {
            case LOG_LEVEL_DEBUG:
                Serial.println(String(LOG_COLOR_BLUE) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_WHITE) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_INFO:
                Serial.println(String(LOG_COLOR_GREEN) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_GREEN) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_WARNING:
                Serial.println(String(LOG_COLOR_YELLOW) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_YELLOW) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_ERROR:
                Serial.println(String(LOG_COLOR_RED) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_RED) + msg + WEB_COLOR_RESET);
                break;

            default:
                break;
        }
        xSemaphoreGive(logMutex);
    } else {
        Serial.println(msg);
    }
}

/**
 * @brief Registers a command string and its handler function into the command map.
 * @param name The command identifier name string.
 * @param handler Function pointer callback to execute when the command is received.
 * @return true if successfully registered, false if command already exists.
 */
bool register_cmd(const String &name, CommandHandlerFunc handler) {
    String cleanName = name;
    cleanName.trim();
    cleanName.toUpperCase();

    LOG_DEBUG("Register cmd " + cleanName);

    // Check if command already exists
    if (commandMap.find(cleanName) != commandMap.end()) {
        LOG_WARNING("Can't register cmd: " + cleanName + ", already existed");
        return false;
    }

    commandMap[cleanName] = handler;
    LOG_INFO("Register success cmd: " + cleanName);
    return true;
}

/**
 * @brief Updates WebServer state to listening to enable Web WebSocket log output.
 */
void setWebLogReady() {
    CoreState_SetWebServer(WEB_STATE_LISTENING);
}

/**
 * @brief Thread-safe function to post incoming command from Serial or Web to vLogTask queue.
 * @param cmdText Command text string.
 */
void postIncomingCommand(const String &cmdText) {
    if (commandQueue == NULL) return;

    CommandPacket packet;
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.text, cmdText.c_str(), sizeof(packet.text) - 1);

    if (xQueueSend(commandQueue, &packet, 0) != pdPASS) {
        LOG_WARNING("Command queue full, dropping command: " + cmdText);
    }
}

/**
 * @brief FreeRTOS Task function body (Event-driven execution loop for log task).
 */
void vLogTask(void *pvParameters) {
    /* Wait until core engine is done */
    waiting_on_event(SYSTEM_EVENT, SYS_SETUP, portMAX_DELAY);
    CommandPacket packet;
    loadLogConfig();
    setSerialLogReady();
    LOG_INFO("Init log task done. Current level: " + levelToStr(currentLogLevel)); 
    register_cmd(CMD_SET_LOG_LEVEL, setLogLevel);
    register_cmd(CMD_GET_LOG_LEVEL, getLogLevel);
    register_cmd(CMD_LIST_LOG_LEVEL, listLogLevel);
    register_cmd(CMD_RESTART, esp32_restart);
    register_cmd(CMD_BACKLOG, backlogHandler);
    register_builtin_function(BUILTIN_PRINT, built_in_print);
    LOG_INFO("vLogTask started, sleeping until command arrives...");
    for (;;) {
#if !(defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT > 0))
        processHardwareSerialInput();
        TickType_t waitTicks = pdMS_TO_TICKS(50);
#else
        TickType_t waitTicks = portMAX_DELAY;
#endif

        // Sleep on commandQueue (event driven for USB CDC, 50ms polling for HardwareSerial)
        if (commandQueue != NULL && xQueueReceive(commandQueue, &packet, waitTicks) == pdPASS) {
            String cmdText = String(packet.text);

            LOG_DEBUG("vLogTask woke up! Received command: '" + cmdText + "' (Size: " + String(cmdText.length()) + " bytes)");

            execute_cmd(cmdText);
        }
    }
}



#include "log_task.h"
#include "web_server.h"
#include <Arduino.h>
#include <unordered_map>

/* -------------------------------------------------------------------------- */
/*                            STRUCTS & HASH HELPERS                          */
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
/*                               LOCAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

static LogLevel currentLogLevel = LOG_LEVEL_DEBUG;
static bool serial_ready = false;
static bool web_ready = false;

// Local Module Log Mutex Handle
static SemaphoreHandle_t logMutex = NULL;

// Local Command Queue Handle
static QueueHandle_t commandQueue = NULL;

// Command registry map storing command name and handler function pointer
static std::unordered_map<String, CommandHandlerFunc, StringHash> commandMap;

/* -------------------------------------------------------------------------- */
/*                            LOCAL HELPER FUNCTIONS                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Helper function to convert LogLevel enum values to string representations.
 * @param level The LogLevel enum value.
 * @return String representation of the log level.
 */
static String levelToStr(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG:   return "LOG_LEVEL_DEBUG";
        case LOG_LEVEL_INFO:    return "LOG_LEVEL_INFO";
        case LOG_LEVEL_WARNING: return "LOG_LEVEL_WARNING";
        case LOG_LEVEL_ERROR:   return "LOG_LEVEL_ERROR";
        default:                return "LOG_LEVEL_UNKNOWN";
    }
}

/**
 * @brief Parses raw command strings into command name and argument, then executes matching registered handler.
 * @param raw_cmd The raw command string received (e.g., "SET_LOG_LEVEL: DEBUG").
 */
static void execute_cmd(const String &raw_cmd) {
    LOG_DEBUG("Raw command received: " + raw_cmd);

    String cleanCmd = raw_cmd;
    cleanCmd.trim();

    String name = "";
    String arg = "";

    // Parse "NAME": "ARG" or plain "NAME"
    int colonIdx = cleanCmd.indexOf(':');
    if (colonIdx != -1) {
        name = cleanCmd.substring(0, colonIdx);
        arg = cleanCmd.substring(colonIdx + 1);
    } else {
        name = cleanCmd;
        arg = "";
    }

    // Sanitize quotes and spaces
    name.trim();
    arg.trim();
    if (name.startsWith("\"") && name.endsWith("\"")) name = name.substring(1, name.length() - 1);
    if (arg.startsWith("\"") && arg.endsWith("\""))   arg = arg.substring(1, arg.length() - 1);

    LOG_DEBUG("Parsed command name: '" + name + "', arg: '" + arg + "'");

    auto it = commandMap.find(name);
    if (it != commandMap.end()) {
        LOG_DEBUG("Executing handler for: " + name);
        it->second(arg);
    } else {
        LOG_WARNING("Command not found: " + name);
    }
}

/* -------------------------------------------------------------------------- */
/*                          COMMAND HANDLER FUNCTIONS                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Command handler: Lists all acceptable parameters for setting log level.
 * @param arg Command argument (unused).
 */
void listLogLevel(const String &arg) {
    LOG_INFO("Acceptable setLogLevel parameters:");
    LOG_INFO("  0 or DEBUG / LOG_DEBUG / LOG_LEVEL_DEBUG");
    LOG_INFO("  1 or INFO  / LOG_INFO  / LOG_LEVEL_INFO");
    LOG_INFO("  2 or WARNING / LOG_WARNING / LOG_LEVEL_WARNING");
    LOG_INFO("  3 or ERROR / LOG_ERROR / LOG_LEVEL_ERROR");
}

/**
 * @brief Command handler: Sets the system current log severity level.
 * @param arg Parameter string representing desired log level (e.g. "DEBUG", "1", "LOG_LEVEL_INFO").
 */
void setLogLevel(const String &arg) {
    String cleanArg = arg;
    cleanArg.trim();
    cleanArg.toUpperCase();

    LogLevel level = currentLogLevel; // Default to existing level if invalid

    // Check for numeric string ("0" - "3")
    if (cleanArg.length() == 1 && cleanArg[0] >= '0' && cleanArg[0] <= '3') {
        level = static_cast<LogLevel>(cleanArg.toInt());
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
    LOG_INFO("Set Log Level: " + String(currentLogLevel));
}

/**
 * @brief Command handler: Prints the current log severity level string.
 * @param arg Command argument (unused).
 */
void getLogLevel(const String &arg) {
    LOG_INFO("Current Log Level: " + String(levelToStr(currentLogLevel)));
}

/* -------------------------------------------------------------------------- */
/*                            GLOBAL API FUNCTIONS                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Thread-safe logging function that outputs colored messages to Serial and Web sockets.
 * @param msg The log message string.
 * @param level The log severity level.
 */
void logPrint(const String &msg, LogLevel level) {
    if (level < currentLogLevel) 
        return;
    if (logMutex != NULL && xSemaphoreTake(logMutex, portMAX_DELAY) == true) {
        switch (level) {
            case LOG_LEVEL_DEBUG:
                if (serial_ready)
                    Serial.println(String(LOG_COLOR_BLUE) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_WHITE) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_INFO:
                if (serial_ready)
                    Serial.println(String(LOG_COLOR_GREEN) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_GREEN) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_WARNING:
                if (serial_ready)
                    Serial.println(String(LOG_COLOR_YELLOW) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_YELLOW) + msg + WEB_COLOR_RESET);
                break;

            case LOG_LEVEL_ERROR:
                if (serial_ready)
                    Serial.println(String(LOG_COLOR_RED) + msg + LOG_COLOR_RESET);
                if (web_ready)
                    ws.textAll(String(WEB_COLOR_RED) + msg + WEB_COLOR_RESET);
                break;

            default:
                break;
        }
        xSemaphoreGive(logMutex);
    }
}

/**
 * @brief Initializes Serial interface, sets serial output ready, and registers default log commands.
 */
void initLogTask() {
    if (logMutex == NULL) {
        logMutex = xSemaphoreCreateMutex();
    }
    if (commandQueue == NULL) {
        commandQueue = xQueueCreate(10, sizeof(CommandPacket));
    }

    Serial.begin(SERIAL_BAUDRATE);
    Serial.onReceive([]() {
        static String serialBuf = "";
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                serialBuf.trim();
                if (serialBuf.length() > 0) {
                    postIncomingCommand(serialBuf, CMD_SOURCE_SERIAL);
                    serialBuf = "";
                }
            } else {
                serialBuf += c;
            }
        }
    });

    setSerialLogReady();
    LOG_INFO("Init log task done"); 
    String set_log_cmd = "SET_LOG_LEVEL";
    String get_log_cmd = "GET_LOG_LEVEL";
    String list_log_cmd = "LIST_LOG_LEVEL";
    register_cmd(set_log_cmd, setLogLevel);
    register_cmd(get_log_cmd, getLogLevel);
    register_cmd(list_log_cmd, listLogLevel);
}

/**
 * @brief Registers a command string and its handler function into the command map.
 * @param name The command identifier name string.
 * @param handler Function pointer callback to execute when the command is received.
 * @return true if successfully registered, false if command already exists.
 */
bool register_cmd(const String &name, CommandHandlerFunc handler) {
    LOG_DEBUG("Register cmd " + name);

    // Check if command already exists
    if (commandMap.find(name) != commandMap.end()) {
        LOG_WARNING("Can't register cmd: " + name + ", already existed");
        return false;
    }

    commandMap[name] = handler;
    LOG_INFO("Register success cmd: " + name);
    return true;
}

/**
 * @brief Enables Serial log output flag.
 */
void setSerialLogReady() {
    serial_ready = true;
}

/**
 * @brief Enables Web WebSocket log output flag.
 */
void setWebLogReady() {
    web_ready = true;
}

/**
 * @brief Thread-safe function to post incoming command from Serial or Web to vLogTask queue.
 * @param cmdText Command text string.
 * @param source Origin source (CMD_SOURCE_SERIAL or CMD_SOURCE_WEB).
 */
void postIncomingCommand(const String &cmdText, CommandSource source) {
    if (commandQueue == NULL) return;

    CommandPacket packet;
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.text, cmdText.c_str(), sizeof(packet.text) - 1);
    packet.source = source;

    String srcStr = (source == CMD_SOURCE_SERIAL) ? "SERIAL" : "WEB";
    LOG_DEBUG("Enqueueing command from [" + srcStr + "]: '" + cmdText + "' (Length: " + String(cmdText.length()) + ")");

    xQueueSend(commandQueue, &packet, 0);
}

/**
 * @brief FreeRTOS Task function body (Event-driven execution loop for log task).
 */
void vLogTask(void *pvParameters) {
    CommandPacket packet;
    LOG_INFO("vLogTask started, sleeping until command arrives...");

    for (;;) {
        // Sleep indefinitely on commandQueue (0% CPU usage while sleeping)
        if (commandQueue != NULL && xQueueReceive(commandQueue, &packet, portMAX_DELAY) == pdPASS) {
            String srcStr = (packet.source == CMD_SOURCE_SERIAL) ? "SERIAL" : "WEB";
            String cmdText = String(packet.text);

            LOG_DEBUG("vLogTask woke up! Received command from [" + srcStr + "]: '" + cmdText + "' (Size: " + String(cmdText.length()) + " bytes)");

            execute_cmd(cmdText);
        }
    }
}

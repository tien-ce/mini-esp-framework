#include "services/web/web_ws.h"
#include "services/web/web_server_task.h"
#include "core/core_log.h"
#include "services/logger/log_task.h"
#include "config.h"

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

AsyncWebSocket ws("/ws");

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief WebSocket event handler for "/ws" terminal console endpoint. */
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    // Logic: Handle client connected event
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
        client->text("=== Sensor Monitor Connected ===");
        return;
    }

    // Logic: Handle client disconnected event
    if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        return;
    }

    // Logic: Handle incoming data from web terminal
    if (type != WS_EVT_DATA) {
        return;
    }

    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!info || !info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) {
        return;
    }

    String msgStr = "";
    for (size_t i = 0; i < len; i++) {
        msgStr += (char)data[i];
    }
    msgStr.trim();
    if (msgStr.length() > 0) {
        postIncomingCommand(msgStr);
    }
}

/** @brief Log sink: forwards every log line (system and script output) to "/ws" clients, coloured by level. */
static void web_ws_log_sink(const String &msg, LogLevel level) {
    if (ws.count() == 0) return;

    switch (level) {
        case LOG_LEVEL_DEBUG:
            ws.textAll(String(WEB_COLOR_WHITE) + msg + WEB_COLOR_RESET);
            break;
        case LOG_LEVEL_INFO:
            ws.textAll(String(WEB_COLOR_GREEN) + msg + WEB_COLOR_RESET);
            break;
        case LOG_LEVEL_WARNING:
            ws.textAll(String(WEB_COLOR_YELLOW) + msg + WEB_COLOR_RESET);
            break;
        case LOG_LEVEL_ERROR:
            ws.textAll(String(WEB_COLOR_RED) + msg + WEB_COLOR_RESET);
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void web_ws_init(AsyncWebServer *server) {
    if (server == nullptr) return;
    ws.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws.onEvent(onWsEvent);
    server->addHandler(&ws);

    core_log_register_cb(web_ws_log_sink);
}

void web_ws_cleanup() {
    ws.cleanupClients();
}

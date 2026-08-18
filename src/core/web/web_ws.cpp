#include "core/web/web_ws.h"
#include "core/web_server_task.h"
#include "core/log_task.h"

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
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected\n", client->id());
        client->text("=== Sensor Monitor Connected ===");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            String msgStr = "";
            for (size_t i = 0; i < len; i++) {
                msgStr += (char)data[i];
            }
            msgStr.trim();
            if (msgStr.length() > 0) {
                postIncomingCommand(msgStr);
            }
        }
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
}

void web_ws_cleanup() {
    ws.cleanupClients();
}

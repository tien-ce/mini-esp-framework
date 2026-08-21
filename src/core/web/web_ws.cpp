#include "core/web/web_ws.h"
#include "core/web_server_task.h"
#include "core/log_task.h"
#include "core/ti_interpreter.h"

/* -------------------------------------------------------------------------- */
/*                              GLOBAL VARIABLES                              */
/* -------------------------------------------------------------------------- */

AsyncWebSocket ws("/ws");
AsyncWebSocket ws_tien("/ws_tien");

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

/** @brief WebSocket event handler for "/ws_tien" Tien script console endpoint. */
static void onWsTienEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                          void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("Tien WebSocket client #%u connected\n", client->id());
        client->text("=== Tien Script Interpreter Console Connected ===");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("Tien WebSocket client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            String msgStr = "";
            for (size_t i = 0; i < len; i++) {
                msgStr += (char)data[i];
            }
            msgStr.trim();
            if (msgStr.length() > 0) {
                // Echo command back to web console
                web_ws_tien_send("> " + msgStr);
                // Execute code via core/ti_interpreter.h API
                tien_run_script(msgStr.c_str());
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

    ws_tien.setAuthentication(getWebUsername().c_str(), getWebPassword().c_str());
    ws_tien.onEvent(onWsTienEvent);
    server->addHandler(&ws_tien);
}

void web_ws_cleanup() {
    ws.cleanupClients();
    ws_tien.cleanupClients();
}

void web_ws_tien_send(const String &msg) {
    if (ws_tien.count() > 0) {
        ws_tien.textAll(msg);
    }
}

extern "C" void tien_out_to_ws(const char *text) {
    if (text != nullptr) {
        web_ws_tien_send(String(text));
    }
}

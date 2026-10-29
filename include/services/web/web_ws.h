#ifndef WEB_WS_H
#define WEB_WS_H

#include <ESPAsyncWebServer.h>

/* HTML Color Wrappers for Web (WebSocket) */
#define WEB_COLOR_RESET   "</span>"
#define WEB_COLOR_RED     "<span style='color:#ff4d4d;'>"
#define WEB_COLOR_GREEN   "<span style='color:#00ff00;'>"
#define WEB_COLOR_YELLOW  "<span style='color:#ffff00;'>"
#define WEB_COLOR_CYAN    "<span style='color:#00ffff;'>"
#define WEB_COLOR_WHITE   "<span style='color:#ffffff;'>"
#define WEB_COLOR_GRAY    "<span style='color:#aaaaaa;'>"

/* -------------------------------------------------------------------------- */
/*                              EXTERNAL VARIABLES                            */
/* -------------------------------------------------------------------------- */

extern AsyncWebSocket ws;
extern AsyncWebSocket ws_tien;

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Initializes WebSocket terminal console endpoint and event handler. */
void web_ws_init(AsyncWebServer *server);

/** @brief Cleans up disconnected WebSocket clients to reclaim resources. */
void web_ws_cleanup();

/** @brief Sends log/output message to Tien Script Console WebSocket clients. */
void web_ws_tien_send(const String &msg);

#ifdef __cplusplus
extern "C" {
#endif

/** @brief C-linkage output bridge to broadcast interpreter text output to Tien WebSocket console. */
void tien_out_to_ws(const char *text);

#ifdef __cplusplus
}
#endif

#endif // WEB_WS_H

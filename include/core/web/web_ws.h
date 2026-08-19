#ifndef WEB_WS_H
#define WEB_WS_H

#include <ESPAsyncWebServer.h>

/* -------------------------------------------------------------------------- */
/*                              EXTERNAL VARIABLES                            */
/* -------------------------------------------------------------------------- */

extern AsyncWebSocket ws;

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Initializes WebSocket terminal console endpoint and event handler. */
void web_ws_init(AsyncWebServer *server);

/** @brief Cleans up disconnected WebSocket clients to reclaim resources. */
void web_ws_cleanup();

#endif // WEB_WS_H

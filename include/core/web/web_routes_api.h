#ifndef WEB_ROUTES_API_H
#define WEB_ROUTES_API_H

#include <ESPAsyncWebServer.h>

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Registers system, configuration, and telemetry REST API routes. */
void register_api_routes(AsyncWebServer *server);

#endif // WEB_ROUTES_API_H

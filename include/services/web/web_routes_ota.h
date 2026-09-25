#ifndef WEB_ROUTES_OTA_H
#define WEB_ROUTES_OTA_H

#include <ESPAsyncWebServer.h>

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Registers OTA firmware upload and update route ("/doUpdate"). */
void register_ota_routes(AsyncWebServer *server);

#endif // WEB_ROUTES_OTA_H

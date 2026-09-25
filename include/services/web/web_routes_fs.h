#ifndef WEB_ROUTES_FS_H
#define WEB_ROUTES_FS_H

#include <ESPAsyncWebServer.h>

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Registers LittleFS file system REST API routes according to API_FS_SPEC.md. */
void register_fs_routes(AsyncWebServer *server);

#endif // WEB_ROUTES_FS_H

#ifndef WEB_ROUTES_PAGES_H
#define WEB_ROUTES_PAGES_H

#include <ESPAsyncWebServer.h>

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Registers HTML dashboard and configuration page routes. */
void register_page_routes(AsyncWebServer *server);

#endif // WEB_ROUTES_PAGES_H

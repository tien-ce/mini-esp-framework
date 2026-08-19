#ifndef WEB_AUTH_H
#define WEB_AUTH_H

#include <ESPAsyncWebServer.h>

/** @brief Authenticates HTTP Basic Auth credentials for incoming web requests. */
bool web_authenticate(AsyncWebServerRequest *request);

#endif // WEB_AUTH_H

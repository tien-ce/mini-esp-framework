#ifndef WEB_SERVER_TASK_H
#define WEB_SERVER_TASK_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "core/web/web_auth.h"
#include "core/web/web_template.h"
#include "core/web/web_ws.h"
#include "core/web/web_routes_pages.h"
#include "core/web/web_routes_api.h"
#include "core/web/web_routes_ota.h"

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Gets web server port. */
uint16_t getWebPort();

/** @brief Gets web admin username. */
String getWebUsername();

/** @brief Gets web admin password. */
String getWebPassword();

/** @brief Updates web server configuration settings. */
void updateWebConfig(uint16_t port, const String &user, const String &pass);

/** @brief FreeRTOS task for web server monitoring and WebSocket cleanup. */
void vWebMonitorTask(void *pvParameters);

#endif // WEB_SERVER_TASK_H

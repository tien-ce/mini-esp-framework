#ifndef WEB_SERVER_TASK_H
#define WEB_SERVER_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "core/config_manager.h"
#include "html/main_html.h"
#include "html/info_html.h"
#include "html/tools_html.h"
#include "html/console_html.h"
#include "html/ota_html.h"

/* -------------------------------------------------------------------------- */
/*                              EXTERNAL VARIABLES                            */
/* -------------------------------------------------------------------------- */

extern AsyncWebSocket ws;

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

/** @brief Registers dynamic table element and returns unique ID. */
// Mark deprecated: registerElement is no longer needed in HTTP Polling pattern
[[deprecated("registerElement() is outdated and does nothing. UI elements are rendered dynamically via HTTP Polling.")]]
uint8_t registerElement(const String& label, const String& unit, const String& initialValue);

/**
 * @brief Appends an element update payload to a static JSON buffer for batch WebSocket transmission.
 * 
 * @param id The unique ID assigned during element registration.
 * @param newValue The updated value string to push to the client.
 */
[[deprecated("updateElementValue() is outdated and does nothing. Use updateElementValue to register key-value pairs.")]]
void updateElementValue(uint8_t id, const String& newValue);
/**
 * @brief New implementation of updateElementValue that uses key-value pairs for telemetry updates.
 */
void updateElementValue(const String& key, const String& newValue);

#endif // WEB_SERVER_TASK_H




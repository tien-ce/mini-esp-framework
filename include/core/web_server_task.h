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

/** @brief Gets web server port. */
uint16_t getWebPort();

/** @brief Gets web admin username. */
String getWebUsername();

/** @brief Gets web admin password. */
String getWebPassword();

/** @brief Updates web server configuration settings. */
void updateWebConfig(uint16_t port, const String &user, const String &pass);

extern AsyncWebSocket ws;

/** @brief FreeRTOS task for web server monitoring and WebSocket cleanup. */
void vWebMonitorTask(void *pvParameters);

/** @brief Registers dynamic table element and returns unique ID. */
uint8_t registerElement(const String& label, const String& unit, const String& initialValue);

/** @brief Pushes dynamic element update to WebSocket clients. */
void updateElementValue(uint8_t id, const String& newValue);
#endif // WEB_SERVER_TASK_H



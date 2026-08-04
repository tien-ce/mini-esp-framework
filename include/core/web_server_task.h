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

/**
 * @brief Loads web server configuration from LittleFS web_config.txt file.
 * Registers "web" module with config_manager if not already registered.
 * @param None
 * @return None
 */
void loadWebConfig();

/**
 * @brief Saves current in-memory web server configuration parameters to LittleFS.
 * @param None
 * @return None
 */
void saveWebConfig();

/**
 * @brief Gets current web server port in a thread-safe manner.
 * @param None
 * @return uint16_t Web server port number.
 */
uint16_t getWebPort();

/**
 * @brief Gets current web server admin username in a thread-safe manner.
 * @param None
 * @return String containing web admin username.
 */
String getWebUsername();

/**
 * @brief Gets current web server admin password in a thread-safe manner.
 * @param None
 * @return String containing web admin password.
 */
String getWebPassword();

/**
 * @brief Updates web server configuration settings and persists them to LittleFS.
 * @param port New web server HTTP port.
 * @param user New admin username string.
 * @param pass New admin password string.
 * @return None
 */
void updateWebConfig(uint16_t port, const String &user, const String &pass);

extern AsyncWebSocket ws;
/**
 * @brief AsyncWebSocket event handler for client connect, disconnect, and incoming data frames.
 * @param server Pointer to AsyncWebSocket instance.
 * @param client Pointer to AsyncWebSocketClient instance triggering the event.
 * @param type Event type identifier (e.g. WS_EVT_CONNECT, WS_EVT_DATA).
 * @param arg Pointer to event argument payload.
 * @param data Pointer to raw byte data array.
 * @param len Byte length of data buffer.
 * @return None
 */
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len);

/**
 * @brief Configures AsyncWebServer endpoints, WebSocket handlers, and authentication.
 * @param None
 * @return None
 */
void setupWebServer();

/**
 * @brief FreeRTOS task for web server maintenance and periodic WebSocket client cleanup.
 * @param pvParameters Pointer to FreeRTOS task parameters.
 * @return None
 */
void vWebMonitorTask(void *pvParameters);

#endif // WEB_SERVER_TASK_H



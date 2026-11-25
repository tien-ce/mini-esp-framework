#include "services/web/web_routes_pages.h"
#include "services/web/web_auth.h"
#include "html/main_html.h"
#include "html/info_html.h"
#include "html/tools_html.h"
#include "html/manage_file_system_html.h"
#include "html/console_html.h"
#include "html/ota_html.h"
#include "html/config_html.h"
#include "html/config_module_html.h"
#include "html/config_wifi_html.h"
#include "html/config_mqtt_html.h"
#include <Arduino.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Sends a gzip-compressed PROGMEM page after the auth check. */
static void sendGzPage(AsyncWebServerRequest *request, const uint8_t *data, size_t len) {
    if (!web_authenticate(request)) return;
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", data, len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

static void handleRoot(AsyncWebServerRequest *request)             { sendGzPage(request, MAIN_HTML_GZ, sizeof(MAIN_HTML_GZ)); }
static void handleInfo(AsyncWebServerRequest *request)             { sendGzPage(request, INFO_HTML_GZ, sizeof(INFO_HTML_GZ)); }
static void handleTools(AsyncWebServerRequest *request)            { sendGzPage(request, TOOLS_HTML_GZ, sizeof(TOOLS_HTML_GZ)); }
static void handleOta(AsyncWebServerRequest *request)              { sendGzPage(request, OTA_HTML_GZ, sizeof(OTA_HTML_GZ)); }
static void handleConfig(AsyncWebServerRequest *request)           { sendGzPage(request, CONFIG_HTML_GZ, sizeof(CONFIG_HTML_GZ)); }
static void handleConfigModule(AsyncWebServerRequest *request)     { sendGzPage(request, CONFIG_MODULE_HTML_GZ, sizeof(CONFIG_MODULE_HTML_GZ)); }
static void handleConfigWifi(AsyncWebServerRequest *request)       { sendGzPage(request, CONFIG_WIFI_HTML_GZ, sizeof(CONFIG_WIFI_HTML_GZ)); }
static void handleConfigMqtt(AsyncWebServerRequest *request)       { sendGzPage(request, CONFIG_MQTT_HTML_GZ, sizeof(CONFIG_MQTT_HTML_GZ)); }
static void handleManageFileSystem(AsyncWebServerRequest *request) { sendGzPage(request, MANAGE_FILE_SYSTEM_HTML_GZ, sizeof(MANAGE_FILE_SYSTEM_HTML_GZ)); }
static void handleConsole(AsyncWebServerRequest *request)          { sendGzPage(request, CONSOLE_HTML_GZ, sizeof(CONSOLE_HTML_GZ)); }

/** @brief Fallback handler for unmapped routes and HTTP 404 Not Found errors. */
static void handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_page_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    server->on("/", HTTP_GET, handleRoot);
    server->on("/info", HTTP_GET, handleInfo);
    server->on("/tools", HTTP_GET, handleTools);
    server->on("/manage_file_system", HTTP_GET, handleManageFileSystem);
    server->on("/console", HTTP_GET, handleConsole);
    server->on("/ota", HTTP_GET, handleOta);
    server->on("/config", HTTP_GET, handleConfig);
    server->on("/config-module", HTTP_GET, handleConfigModule);
    server->on("/config-wifi", HTTP_GET, handleConfigWifi);
    server->on("/wificonfig", HTTP_GET, handleConfigWifi);
    server->on("/config-mqtt", HTTP_GET, handleConfigMqtt);
    server->on("/mqttconfig", HTTP_GET, handleConfigMqtt);
    server->onNotFound(handleNotFound);
}

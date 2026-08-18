#include "core/web/web_routes_pages.h"
#include "core/web/web_auth.h"
#include "core/web/web_template.h"
#include "html/main_html.h"
#include "html/info_html.h"
#include "html/tools_html.h"
#include "html/manage_file_system_html.h"
#include "html/console_html.h"
#include "html/ota_html.h"
#include "html/config_html.h"
#include "html/config_module_html.h"
#include <Arduino.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Handles HTTP GET request for the root dashboard URL ("/"). */
static void handleRoot(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(MAIN_HTML));
}

/** @brief Handles HTTP GET request for the system information page ("/info"). */
static void handleInfo(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(INFO_HTML));
}

/** @brief Handles HTTP GET request for the tools overview page ("/tools"). */
static void handleTools(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(TOOLS_HTML));
}

/** @brief Handles HTTP GET request for LittleFS file system manager ("/manage_file_system" / "/tools/manage_file_system"). */
static void handleManageFileSystem(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(MANAGE_FILE_SYSTEM_HTML));
}

/** @brief Handles HTTP GET request for web console terminal page ("/console"). */
static void handleConsole(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(CONSOLE_HTML));
}

/** @brief Handles HTTP GET request for firmware OTA upgrade page ("/ota"). */
static void handleOta(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(OTA_HTML));
}

/** @brief Handles HTTP GET request for general WiFi configuration page ("/config"). */
static void handleConfig(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(CONFIG_HTML));
}

/** @brief Handles HTTP GET request for GPIO pin mapping configuration page ("/config-module"). */
static void handleConfigModule(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;
    request->send(200, "text/html", web_render_template(CONFIG_MODULE_HTML));
}

/** @brief Fallback handler for unmapped routes and HTTP 404 Not Found errors. */
static void handleNotFound(AsyncWebServerRequest *request) {
    Serial.printf("[Web Error] Not Found / Internal error on URL: %s\n", request->url().c_str());
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
    server->on("/tools/manage_file_system", HTTP_GET, handleManageFileSystem);
    server->on("/manage_file_system", HTTP_GET, handleManageFileSystem);
    server->on("/console", HTTP_GET, handleConsole);
    server->on("/ota", HTTP_GET, handleOta);
    server->on("/config", HTTP_GET, handleConfig);
    server->on("/config-module", HTTP_GET, handleConfigModule);
    server->onNotFound(handleNotFound);
}

#include "core/web_server_task.h"
#include "core/config_manager.h"
#include "core/log_task.h"
#include "core/core_engine.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static uint16_t web_port      = 0;
static String web_username    = "";
static String web_password    = "";
static AsyncWebServer *server = NULL;
static SemaphoreHandle_t webConfigMutex = NULL;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Initializes web server mutex. */
static void initWebMutex() {
    if (webConfigMutex == NULL) {
        webConfigMutex = xSemaphoreCreateMutex();
    }
}

/** @brief Saves current in-memory web server configuration parameters to NVS. */
static void saveWebConfig() {
    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            config_save_int("web", "port", web_port);
            config_save_string("web", "username", web_username);
            config_save_string("web", "password", web_password);
            config_release_lock();
        }
        xSemaphoreGive(webConfigMutex);
    }
}

/** @brief Loads web server configuration from NVS. */
static void loadWebConfig() {
    initWebMutex();
    register_config_module("web", "web");

    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            web_port = (uint16_t)config_read_int("web", "port", WEB_PORT);
            web_username = config_read_string("web", "username", WEB_USERNAME);
            web_password = config_read_string("web", "password", WEB_PASSWORD);
            config_release_lock();
        }
        xSemaphoreGive(webConfigMutex);
    }
    LOG_INFO("web config loaded from NVS successfully.");
}

/** @brief Configures AsyncWebServer endpoints, WebSocket handlers, and authentication. */
static void setupWebServer() {
    uint16_t port = getWebPort();
    if (server != NULL) {
        LOG_INFO("Cleaning up old server instance...");
        delete server;
        server = NULL;
    }
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
    server = new AsyncWebServer(port);

    // 1. WebSocket Handler
    web_ws_init(server);

    // 2. HTML Page Routes (includes 404 fallback)
    register_page_routes(server);

    // 3. System & Config REST APIs
    register_api_routes(server);

    // 5. OTA Firmware Update Route
    register_ota_routes(server);

    server->begin();
    setWebLogReady();
    Serial.println("Web Server started on port " + String(getWebPort()));
    Serial.println("Open: http://" + WiFi.localIP().toString() + ":" + String(getWebPort()));
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

uint16_t getWebPort() { 
    return web_port;
}

String getWebUsername() {
    return web_username;
}

String getWebPassword() {
    return web_password;
}

void updateWebConfig(uint16_t port, const String &user, const String &pass) {
    if (webConfigMutex != NULL && xSemaphoreTake(webConfigMutex, portMAX_DELAY) == pdTRUE) {
        web_port = port;
        web_username = user;
        web_password = pass;
        xSemaphoreGive(webConfigMutex);
    }
    saveWebConfig();
}

/**
 * @brief Web Server & System Maintenance Task
 * @priority 1 (Low Priority)
 * @core Core 1
 * 
 * Periodically cleans up inactive WebSocket clients and performs background maintenance.
 */
void vWebMonitorTask(void *pvParameters) {
    waiting_on_event(NETWORK_EVENT, NET_STATE_WIFI_STA, portMAX_DELAY);
    loadWebConfig();	
    setupWebServer();
    for (;;) {
        web_ws_cleanup();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

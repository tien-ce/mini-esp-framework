#include "core/web/web_routes_ota.h"
#include "core/web/web_auth.h"
#include "core/log_task.h"
#include <Arduino.h>
#include <Update.h>

#define OTA_RESTART_DELAY_MS 500

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Handles the final response for OTA firmware update ("/doUpdate"). */
static void handleOtaUpdateRequest(AsyncWebServerRequest *request) {
    if (!web_authenticate(request)) return;

    bool success = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", success ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);

    if (success) {
        xTaskCreate([](void *arg) {
            vTaskDelay(pdMS_TO_TICKS(OTA_RESTART_DELAY_MS));
            postIncomingCommand(CMD_RESTART);
            vTaskDelete(NULL);
        }, "ota_restart_task", 2048, NULL, 1, NULL);
    }
}

/** @brief Handles streaming chunks of binary firmware data during OTA update ("/doUpdate"). */
static void handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!web_authenticate(request)) return;

    if (!index) {
        LOG_INFO("OTA Update started, filename: " + filename);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            LOG_ERROR("OTA Begin Error!");
            Update.printError(Serial);
        }
    }

    if (Update.write(data, len) != len) {
        LOG_ERROR("OTA Write Error!");
        Update.printError(Serial);
    }

    if (final) {
        if (Update.end(true)) {
            LOG_INFO("OTA Update Success! Written bytes: " + String(index + len));
        } else {
            LOG_ERROR("OTA End Error!");
            Update.printError(Serial);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

void register_ota_routes(AsyncWebServer *server) {
    if (server == nullptr) return;

    server->on("/doUpdate", HTTP_POST, handleOtaUpdateRequest, handleOtaUpload);
}

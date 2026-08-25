#include "core/web/web_template.h"
#include "core/info.h"
#include "core/wifi_task.h"
#include "core/mqtt_task.h"
#include "core/pin_config.h"
#include "core/log_task.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/* For registering rows in dynamic table */
static String tableRowsHTML = "";

/* Buffer dynamic JSON for HTTP Polling responses */
static JsonDocument telemetryDoc;
static String telemetryJson = "{}";

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/** @brief Formats elapsed seconds into uptime string format (e.g. "0T00:01:23"). */
static String formatUptime(uint32_t seconds) {
    uint32_t days = seconds / 86400;
    seconds %= 86400;
    uint32_t hrs = seconds / 3600;
    seconds %= 3600;
    uint32_t mins = seconds / 60;
    uint32_t secs = seconds % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%uT%02u:%02u:%02u", days, hrs, mins, secs);
    return String(buf);
}

/** @brief Generates dynamic GPIO selector rows from BOARD_PINS and AVAILABLE_PIN_OPTIONS. */
static String generatePinRows() {
    String rows = "";
    for (size_t i = 0; i < BOARD_PIN_COUNT; i++) {
        const auto& pin = BOARD_PINS[i];
        String activeName = get_pin_name(pin.gpio);
        if (activeName.length() == 0) {
            activeName = pin.defaultOption;
        }

        if (pin.isFixed) {
            rows += "<div class='form-row'>";
            rows += "<label for='gpio" + String(pin.gpio) + "' class='gpio-red'>" + pin.label + "</label>";
            rows += "<select id='gpio" + String(pin.gpio) + "' name='gpio" + String(pin.gpio) + "' disabled>";
            rows += "<option value='-1' selected>" + String(pin.defaultOption) + "</option>";
            rows += "</select></div>";
        } else {
            rows += "<div class='form-row'>";
            rows += "<label for='gpio" + String(pin.gpio) + "'>" + pin.label + "</label>";
            rows += "<select id='gpio" + String(pin.gpio) + "' name='gpio" + String(pin.gpio) + "'>";

            for (size_t j = 0; j < AVAILABLE_PIN_OPTIONS_COUNT; j++) {
                const char* optName = AVAILABLE_PIN_OPTIONS[j];
                bool isSelected = activeName.equalsIgnoreCase(optName);
                String selectedAttr = isSelected ? " selected" : "";

                rows += "<option value='" + String(optName) + "'" + selectedAttr + ">" + String(optName) + "</option>";
            }

            rows += "</select></div>";
        }
    }
    return rows;
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

String web_render_template(const char* templateStr) {
    String page = String(templateStr);
    page.replace("%HEADER_TITLE%", "ESP32S3");
    page.replace("%HEADER_SUBTITLE%", "ESP mini framework " + String(FIRMWARE_VERSION));
    page.replace("%FOOTER_TEXT%", "ESP mini framework " + String(FIRMWARE_VERSION) + " by Văn Tiến");
    page.replace("%APP_VERSION%", String(FIRMWARE_VERSION));
    page.replace("%BUILD_DATE%", String(__DATE__) + " " + String(__TIME__));
    page.replace("%SDK_VERSION%", String(ESP.getSdkVersion()));
    String clientID = getWifiClientID();
    page.replace("%HOSTNAME%", clientID.length() > 0 ? clientID : ("tasmota-" + String(esp_info_get_mac_str())));
    page.replace("%CHIP_MODEL%", String(esp_info_get_model()));
    page.replace("%MAC_ADDR%", String(esp_info_get_mac_str()));
    page.replace("%FLASH_SIZE%", String(ESP.getFlashChipSize() / 1024) + " KB");
    page.replace("%SENSOR_TABLE_ROWS%", tableRowsHTML);
    page.replace("%GPIO_TABLE_ROWS%", generatePinRows());
    page.replace("%UPTIME%", formatUptime(millis() / 1000));
    page.replace("%IP_ADDR%", WiFi.localIP().toString());
    page.replace("%GATEWAY%", WiFi.gatewayIP().toString());
    page.replace("%SUBNET_MASK%", WiFi.subnetMask().toString());
    page.replace("%DNS_SERVER%", WiFi.dnsIP().toString());
    page.replace("%FREE_RAM%", String(esp_info_get_free_heap() / 1024.0, 1) + " KB");

    // WiFi placeholders
    page.replace("%WIFI_SSID%", getWifiSSID());
    page.replace("%WIFI_PASSWORD%", getWifiPassword());
    page.replace("%WIFI_CLIENT_ID%", getWifiClientID());
    page.replace("%STATIC_IP%", getWifiIP());
    page.replace("%STATIC_GATEWAY%", getWifiGateway());
    page.replace("%STATIC_SUBNET%", getWifiSubnet());
    page.replace("%STATIC_DNS1%", getWifiDNS1());

    // MQTT placeholders
    page.replace("%MQTT_SERVER%", getMqttServer());
    page.replace("%MQTT_PORT%", String(getMqttPort()));
    page.replace("%MQTT_USER%", getMqttUser());
    page.replace("%MQTT_PASSWORD%", getMqttPass());
    page.replace("%MQTT_TOPIC%", getMqttDataTopic());
    page.replace("%MQTT_RPC_TOPIC%", getMqttRpcTopic());
    page.replace("%MQTT_INTERVAL%", String(getMqttInterval()));

    return page;
}

void updateElementValue(const String& key, const String& newValue) {
    telemetryDoc[key] = newValue;
    telemetryJson = "";
    serializeJson(telemetryDoc, telemetryJson);
}

uint8_t registerElement(const String& label, const String& unit, const String& initialValue) {
    updateElementValue(label, initialValue);
    LOG_DEBUG("tableRowsHTML: " + tableRowsHTML);
    return 0;
}

void updateElementValue(uint8_t id, const String& newValue) {
    updateElementValue(String(id), newValue);
}

String web_get_telemetry_json() {
    return telemetryJson;
}

void web_clear_telemetry_json() {
    telemetryJson = "{}";
    telemetryDoc.clear();
}

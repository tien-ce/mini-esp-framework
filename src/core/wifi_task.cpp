#include "core/wifi_task.h"
#include "core/log_task.h"
#include "core/core_engine.h"
#include "core/chip_info.h"
#include "config.h"
#include <ESP32Ping.h>
#include <semphr.h>

static String wifi_ssid     = "";
static String wifi_password = "";

static SemaphoreHandle_t wifiConfigMutex = NULL;
static unsigned long previousWifiMillis = 0;
static const unsigned long wifiReconnectInterval = 30000;

/**
 * @brief Initializes the FreeRTOS mutex for protecting WiFi task configuration.
 * @param None
 * @return None
 */
static void initWifiMutex() {
    if (wifiConfigMutex == NULL) {
        wifiConfigMutex = xSemaphoreCreateMutex();
    }
}

void loadWifiConfig() {
    initWifiMutex();
    register_config_file("wifi", "wifi_config.txt");
    /* Init and save config */
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        String raw = read_config("wifi");
        if (raw.length() == 0) {
            /* Use default if config file is not found or empty */
            LOG_INFO("WiFi config file not found or empty. Creating default wifi_config.txt");
            wifi_ssid = WIFI_SSID;
            wifi_password = WIFI_PASSWORD;
            saveWifiConfig();
            return;
        }
        int pos = 0;
        while (pos < raw.length()) {
            int nextPos = raw.indexOf('\n', pos);
            if (nextPos == -1) nextPos = raw.length();
            String line = raw.substring(pos, nextPos);
            line.trim();
            pos = nextPos + 1;

            if (line.length() == 0) continue;
            int eqIdx = line.indexOf('=');
            if (eqIdx > 0) {
                String key = line.substring(0, eqIdx);
                String val = line.substring(eqIdx + 1);
                key.trim();
                val.trim();

                if (key.equalsIgnoreCase("ssid")) {
                    wifi_ssid = val;
                } else if (key.equalsIgnoreCase("pass")) {
                    wifi_password = val;
                }
            }
        }
        xSemaphoreGive(wifiConfigMutex);
    }
    LOG_INFO("WiFi config loaded successfully.");
}

void saveWifiConfig() {
    String content = "";
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        content += "ssid=" + wifi_ssid + "\n";
        content += "pass=" + wifi_password + "\n";
        xSemaphoreGive(wifiConfigMutex);
    }
    save_config("wifi", content);
}

String getWifiSSID() {
    String val = "";
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        val = wifi_ssid;
        xSemaphoreGive(wifiConfigMutex);
    }
    return val;
}

String getWifiPassword() {
    String val = "";
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        val = wifi_password;
        xSemaphoreGive(wifiConfigMutex);
    }
    return val;
}

void updateWifiConfig(const String &newSsid, const String &newPass) {
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        wifi_ssid = newSsid;
        wifi_password = newPass;
        xSemaphoreGive(wifiConfigMutex);
    }
    saveWifiConfig();
}

void updateConfig(const String &newSsid, const String &newPass) {
    updateWifiConfig(newSsid, newPass);
}

bool is_wifi_connected() {
    return (WiFi.status() == WL_CONNECTED);
}

wl_status_t get_wifi_link_status() {
    return WiFi.status();
}

int get_wifi_rssi() {
    return WiFi.RSSI();
}

/**
 * @brief Runs diagnostic tests (IP check, Gateway ping, Target Server ping) to isolate network issues.
 * @param targetIP Target server IP address to perform ping test against.
 * @return None
 */
static void diagnose_connection_issues(IPAddress targetIP) {
    LOG_INFO("--- Starting Network Diagnostics ---");

    // Check 1: Verify IP address assignment
    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        LOG_ERROR("FAIL: Invalid local IP address. DHCP or Static IP setup failed.");
        return;
    }

    // Check 2: Ping local Gateway
    IPAddress gateway = WiFi.gatewayIP();
    
    if (Ping.ping(gateway, 3)) {
        LOG_INFO("Pinging Gateway (" + gateway.toString() + ")... SUCCESS! Gateway responded.");
    } else {
        LOG_ERROR("Pinging Gateway (" + gateway.toString() + ")... FAIL! Gateway did not respond.");
        LOG_ERROR("  -> Diagnostic: Likely AP Isolation enabled on router, or wrong Subnet/Gateway configuration.");
        return;
    }

    // Check 3: Ping Target Server
    if (Ping.ping(targetIP, 3)) {
        LOG_INFO("Pinging Target Server (" + targetIP.toString() + ")... SUCCESS! Target server is reachable.");
    } else {
        LOG_ERROR("Pinging Target Server (" + targetIP.toString() + ")... FAIL! Target server did not respond.");
        LOG_ERROR("  -> Diagnostic: Server firewall blocking ICMP, device on wrong VLAN, or static route missing.");
    }
    LOG_INFO("-----------------------------------");
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    CoreState_SetNetwork(NET_STATE_DISCONNECTED);
    LOG_WARNING("WiFi disconnected! Reason: " + String(info.wifi_sta_disconnected.reason));
}

void setup_wifi() {
    /* Update state of network is connecting */
    CoreState_SetNetwork(NET_STATE_CONNECTING);
    WiFi.mode(WIFI_STA);
    
    // Read ESP32 MAC Address
    char macStr[18];
    esp_info_get_mac_str(macStr);
    LOG_INFO("--- WIFI Configuration ---");
    LOG_INFO("Device MAC Address: " + String(macStr));
    LOG_INFO("WiFi SSID: " + getWifiSSID());
    LOG_INFO("Static IP: " + String(STATIC_IP));
    LOG_INFO("----------------------------");

    if (USE_STATIC_IP) {
        IPAddress ip_addr, gw, sn, dns1;
        ip_addr.fromString(STATIC_IP);
        gw.fromString(STATIC_GATEWAY);
        sn.fromString(STATIC_SUBNET);
        dns1.fromString(STATIC_DNS1);

        if (!WiFi.config(ip_addr, gw, sn, dns1)) {
            LOG_ERROR("Static IP configuration failed!");
        } else {
            LOG_INFO("Using Static IP: " + String(STATIC_IP));
        }
    }

    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    String currentSsid = getWifiSSID();
    String currentPass = getWifiPassword();
    WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    LOG_INFO("Connecting to WiFi: " + currentSsid);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        CoreState_SetNetwork(NET_STATE_WIFI_STA);
        randomSeed(micros());
        LOG_INFO("WiFi connected successfully!");
        LOG_INFO("IP Address: " + WiFi.localIP().toString());
        LOG_INFO("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");
    } else {
        CoreState_SetNetwork(NET_STATE_DISCONNECTED);
        LOG_ERROR("WiFi connection FAILED!");
    }
    // 5. Final Validation and Diagnostics
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0)) {
        randomSeed(micros());
        LOG_INFO("WiFi connected successfully!");
        LOG_INFO("IP Address: " + WiFi.localIP().toString());
        LOG_INFO("Gateway: " + WiFi.gatewayIP().toString());
        LOG_INFO("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");

        // Run Ping Diagnostic against Gateway or Target Server
        IPAddress targetServer;
        targetServer.fromString(STATIC_GATEWAY); // Replace with specific target IP if needed
        diagnose_connection_issues(targetServer);

    } else {
        LOG_ERROR("WiFi connection FAILED!");
        LOG_ERROR("  -> Diagnostic: Verify SSID, Password, and signal strength (RSSI).");
    }
}

void vWifiTask(void *pvParameters) {
    /* Wait until log initialized) */
    waiting_on_event(SYSTEM_EVENT, MODE_NORMAL, portMAX_DELAY);
    setup_wifi();
    for (;;) {
        if (!is_wifi_connected()) {
            unsigned long currentMillis = millis();
            CoreState_SetNetwork(NET_STATE_DISCONNECTED);
            if (currentMillis - previousWifiMillis >= wifiReconnectInterval) {
                CoreState_SetNetwork(NET_STATE_CONNECTING);
                LOG_WARNING("WiFi DISCONNECTED! Reconnecting...");
                WiFi.disconnect();
                String currentSsid = getWifiSSID();
                String currentPass = getWifiPassword();
                WiFi.begin(currentSsid.c_str(), currentPass.c_str());
                vTaskDelay(pdMS_TO_TICKS(500));
                if (is_wifi_connected()) {
                    CoreState_SetNetwork(NET_STATE_WIFI_STA);
                    LOG_INFO("WIFI Reconnect Success");
                }
                else {
                    LOG_WARNING("WIFI Reconnect Failed");
                }
                previousWifiMillis = currentMillis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}



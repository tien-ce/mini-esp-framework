#include "wifi_task.h"
#include "log_task.h"
#include <ESP32Ping.h>
static unsigned long previousWifiMillis = 0;
static const unsigned long wifiReconnectInterval = 30000;
static void diagnose_connection_issues(IPAddress targetIP) {
    Serial.println("\n--- Starting Network Diagnostics ---");

    // Check 1: Verify IP address assignment
    if (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        Serial.println("FAIL: Invalid local IP address. DHCP or Static IP setup failed.");
        return;
    }

    // Check 2: Ping local Gateway
    IPAddress gateway = WiFi.gatewayIP();
    Serial.print("Pinging Gateway (");
    Serial.print(gateway);
    Serial.print(")... ");
    
    if (Ping.ping(gateway, 3)) {
        Serial.println("SUCCESS! Gateway responded.");
    } else {
        Serial.println("FAIL! Gateway did not respond.");
        Serial.println("  -> Diagnostic: Likely AP Isolation enabled on router, or wrong Subnet/Gateway configuration.");
        return;
    }

    // Check 3: Ping Target Server
    Serial.print("Pinging Target Server (");
    Serial.print(targetIP);
    Serial.print(")... ");

    if (Ping.ping(targetIP, 3)) {
        Serial.println("SUCCESS! Target server is reachable.");
    } else {
        Serial.println("FAIL! Target server did not respond.");
        Serial.println("  -> Diagnostic: Server firewall blocking ICMP, device on wrong VLAN, or static route missing.");
    }
    Serial.println("-----------------------------------\n");
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("WiFi disconnected! Reason: " + String(info.wifi_sta_disconnected.reason));
    LOG_WARNING("Attempting to reconnect...");
    
    String currentSsid = getWifiSSID();
    String currentPass = getWifiPassword();
    WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    
    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO("WiFi reconnected! Local IP: " + WiFi.localIP().toString());
    }
}

void setup_wifi() {
    WiFi.mode(WIFI_STA);
    
    // Read ESP32 MAC Address
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    
    Serial.println("\n--- WIFI Configuration ---");
    Serial.println("Device MAC Address: " + String(macStr));
    Serial.println("WiFi SSID: " + getWifiSSID());
    Serial.println("Static IP: " + String(STATIC_IP));
    Serial.println("Client ID: " + getClientID());
    Serial.println("API URL: " + getApiUrl() + getClientID());
    Serial.println("----------------------------\n");

    if (USE_STATIC_IP) {
        IPAddress ip_addr, gw, sn, dns1;
        ip_addr.fromString(STATIC_IP);
        gw.fromString(STATIC_GATEWAY);
        sn.fromString(STATIC_SUBNET);
        dns1.fromString(STATIC_DNS1);

        if (!WiFi.config(ip_addr, gw, sn, dns1)) {
            Serial.println("Static IP configuration failed!");
        } else {
            Serial.println("Using Static IP: " + String(STATIC_IP));
        }
    }

    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    String currentSsid = getWifiSSID();
    String currentPass = getWifiPassword();
    WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    Serial.print("Connecting to WiFi: ");
    Serial.println(currentSsid);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        randomSeed(micros());
        LOG_INFO("WiFi connected successfully!");
        LOG_INFO("IP Address: " + WiFi.localIP().toString());
        LOG_INFO("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");
    } else {
        LOG_ERROR("WiFi connection FAILED!");
    }
    // 5. Final Validation and Diagnostics
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0)) {
        randomSeed(micros());
        Serial.println("WiFi connected successfully!");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.println("Gateway: " + WiFi.gatewayIP().toString());
        Serial.println("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");

        // Run Ping Diagnostic against Gateway or Target Server
        IPAddress targetServer;
        targetServer.fromString(STATIC_GATEWAY); // Replace with specific target IP if needed
        diagnose_connection_issues(targetServer);

    } else {
        Serial.println("WiFi connection FAILED!");
        Serial.println("  -> Diagnostic: Verify SSID, Password, and signal strength (RSSI).");
    }
}

/**
 * @brief Dedicated FreeRTOS Task for WiFi State Monitoring & Auto-Reconnection
 * @priority 2 (Medium Priority)
 * @core Core 1
 */
void vWifiTask(void *pvParameters) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            unsigned long currentMillis = millis();
            if (currentMillis - previousWifiMillis >= wifiReconnectInterval) {
                LOG_WARNING("WiFi DISCONNECTED! Reconnecting...");
                WiFi.disconnect();

                String currentSsid = getWifiSSID();
                String currentPass = getWifiPassword();
                WiFi.begin(currentSsid.c_str(), currentPass.c_str());

                previousWifiMillis = currentMillis;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

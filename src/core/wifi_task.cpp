#include "core/wifi_task.h"
#include "core/dispatcher.h"
#include "core/log_task.h"
#include "core/core_engine.h"
#include "config.h"
#include "cmd.h"
#include <ESP32Ping.h>
#include <semphr.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

static String wifi_ssid      = "";
static String wifi_password  = "";
static String wifi_client_id = "";
static String wifi_ip        = "";
static String wifi_gateway   = "";
static String wifi_subnet    = "";
static String wifi_dns1      = "";

static SemaphoreHandle_t wifiConfigMutex = NULL;
static unsigned long previousWifiMillis = 0;
static const unsigned long wifiReconnectInterval = 30000;

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

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

/**
 * @brief Saves WiFi configuration settings to NVS.
 */
static void saveWifiConfig() {
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            config_save_string("wifi", "ssid", wifi_ssid);
            config_save_string("wifi", "pass", wifi_password);
            config_save_string("wifi", "client_id", wifi_client_id);
            config_save_string("wifi", "ip", wifi_ip);
            config_save_string("wifi", "gw", wifi_gateway);
            config_save_string("wifi", "sn", wifi_subnet);
            config_save_string("wifi", "dns", wifi_dns1);
            config_release_lock();
        }
        xSemaphoreGive(wifiConfigMutex);
    }
}

/**
 * @brief Loads WiFi module configuration from NVS.
 */
static void loadWifiConfig() {
    initWifiMutex();
    register_config_module("wifi", "wifi");

    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        if (config_get_lock()) {
            wifi_ssid      = config_read_string("wifi", "ssid", WIFI_SSID);
            wifi_password  = config_read_string("wifi", "pass", WIFI_PASSWORD);
            wifi_client_id = config_read_string("wifi", "client_id", WIFI_CLIENT_ID);
            wifi_ip        = config_read_string("wifi", "ip", STATIC_IP);
            wifi_gateway   = config_read_string("wifi", "gw", STATIC_GATEWAY);
            wifi_subnet    = config_read_string("wifi", "sn", STATIC_SUBNET);
            wifi_dns1      = config_read_string("wifi", "dns", STATIC_DNS1);
            config_release_lock();
        }
        xSemaphoreGive(wifiConfigMutex);
    }
    LOG_INFO("WiFi config loaded from NVS successfully.");
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

/**
 * @brief WiFi station disconnect event callback handler.
 */
static void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    CoreState_SetNetwork(NET_STATE_DISCONNECTED);
    LOG_WARNING("WiFi disconnected! Reason: " + String(info.wifi_sta_disconnected.reason));
}

/**
 * @brief Cấu hình phần cứng WiFi (chế độ Station), thiết lập IP tĩnh, đăng ký sự kiện và bắt đầu kết nối AP.
 * 
 * @details Quy trình bao gồm:
 *          1. Chuyển trạng thái sang NET_STATE_CONNECTING, cấu hình chế độ WIFI_STA và gán Hostname/Client ID.
 *          2. Áp dụng cấu hình IP tĩnh (nếu bật cờ USE_STATIC_IP).
 *          3. Đăng ký callback WiFiStationDisconnected để nhận diện sự kiện ngắt kết nối.
 *          4. Kích hoạt kết nối WiFi.begin() và áp dụng cơ chế Timeout Polling 20 giây để chờ đồng bộ.
 *          5. Khi kết nối thành công, kích hoạt chẩn đoán ICMP Ping tới Gateway để kiểm tra tính thông suốt mạng.
 */
static void setup_wifi() {
    /* Update state of network is connecting */
    CoreState_SetNetwork(NET_STATE_CONNECTING);
    WiFi.mode(WIFI_STA);
    
    String clientID = getWifiClientID();
    if (clientID.length() > 0) {
        WiFi.setHostname(clientID.c_str());
    }

    LOG_INFO("--- WIFI Configuration ---");
    LOG_INFO("WiFi SSID: " + getWifiSSID());
    LOG_INFO("Client ID: " + clientID);
    LOG_INFO("Static IP: " + getWifiIP());
    LOG_INFO("----------------------------");

    if (USE_STATIC_IP) {
        IPAddress ip_addr, gw, sn, dns1;
        ip_addr.fromString(getWifiIP());
        gw.fromString(getWifiGateway());
        sn.fromString(getWifiSubnet());
        dns1.fromString(getWifiDNS1());

        if (!WiFi.config(ip_addr, gw, sn, dns1)) {
            LOG_ERROR("Static IP configuration failed!");
        } else {
            LOG_INFO("Using Static IP: " + getWifiIP());
        }
    }

    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    String currentSsid = getWifiSSID();
    String currentPass = getWifiPassword();
    WiFi.begin(currentSsid.c_str(), currentPass.c_str());
    LOG_INFO("Connecting to WiFi: " + currentSsid);

    /*
     * CƠ CHẾ TIMEOUT POLLING 20 GIÂY (Bounded Polling Timeout Mechanism):
     * Vòng lặp thăm dò trạng thái kết nối định kỳ mỗi 500ms với biến đếm tối đa 40 lần thử (attempts < 40).
     * Tổng thời gian timeout tối đa: 40 * 500ms = 20,000ms = 20 giây.
     * Cơ chế này ngăn chặn tình trạng treo tác vụ (task blocking) vô hạn nếu Access Point (AP) không phản hồi,
     * suy hao tín hiệu nghiêm trọng hoặc sai thông tin xác thực mật khẩu.
     */
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        CoreState_SetNetwork(NET_STATE_WIFI_STA);
        dispatch_signal(SIG_WIFI_CONNECTED); 
        randomSeed(micros());
        LOG_INFO("WiFi connected successfully!");
        LOG_INFO("IP Address: " + WiFi.localIP().toString());
        LOG_INFO("Gateway: " + WiFi.gatewayIP().toString());
        LOG_INFO("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");

        /*
         * CƠ CHẾ CHẨN ĐOÁN MẠNG ICMP PING (Network Diagnostics via ICMP Echo Ping):
         * Sau khi stack IP sẵn sàng, hệ thống chủ động gửi 3 gói tin ICMP Echo Request tới địa chỉ Gateway IP.
         * Mục đích:
         * 1. Xác thực tính thông suốt của đường truyền mạng nội bộ (Layer 3 connectivity).
         * 2. Phát hiện sớm các lỗi cấu hình như AP Isolation trên Router, sai Subnet/Gateway, hoặc xung đột IP.
         */
        IPAddress targetServer;
        targetServer.fromString(getWifiGateway());
        diagnose_connection_issues(targetServer);
    } else {
        CoreState_SetNetwork(NET_STATE_DISCONNECTED);
        LOG_ERROR("WiFi connection FAILED!");
        LOG_ERROR("  -> Diagnostic: Verify SSID, Password, and signal strength (RSSI).");
    }
}

/** @brief Command handler: Gets or sets WiFi SSID. */
static void wifi_cmd_ssid(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_ssid = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_ssid = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi SSID updated to: " + wifi_ssid);
    } else {
        LOG_INFO("WiFi SSID: " + getWifiSSID());
    }
}

/** @brief Command handler: Gets or sets WiFi password. */
static void wifi_cmd_password(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_password = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_password = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Password updated.");
    } else {
        LOG_INFO("WiFi Password: " + getWifiPassword());
    }
}

/** @brief Command handler: Gets or sets device Client ID / Hostname. */
static void wifi_cmd_client_id(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.length() > 0) {
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_client_id = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_client_id = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Client ID updated to: " + wifi_client_id);
    } else {
        LOG_INFO("WiFi Client ID: " + getWifiClientID());
    }
}

/** @brief Command handler: Gets or sets static IP address. */
static void wifi_cmd_ip(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.indexOf(';') != -1) {
        cleanVal = cleanVal.substring(0, cleanVal.indexOf(';'));
        cleanVal.trim();
    }

    if (cleanVal.length() > 0) {
        IPAddress testIp;
        if (!testIp.fromString(cleanVal)) {
            LOG_ERROR("Invalid Static IP format: " + cleanVal);
            return;
        }
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_ip = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_ip = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Static IP updated to: " + wifi_ip);
    } else {
        LOG_INFO("WiFi Static IP: " + getWifiIP() + " (Current IP: " + WiFi.localIP().toString() + ")");
    }
}

/** @brief Command handler: Gets or sets static Gateway IP. */
static void wifi_cmd_gateway(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.indexOf(';') != -1) {
        cleanVal = cleanVal.substring(0, cleanVal.indexOf(';'));
        cleanVal.trim();
    }

    if (cleanVal.length() > 0) {
        IPAddress testIp;
        if (!testIp.fromString(cleanVal)) {
            LOG_ERROR("Invalid Gateway IP format: " + cleanVal);
            return;
        }
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_gateway = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_gateway = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Static Gateway updated to: " + wifi_gateway);
    } else {
        LOG_INFO("WiFi Static Gateway: " + getWifiGateway() + " (Current GW: " + WiFi.gatewayIP().toString() + ")");
    }
}

/** @brief Command handler: Gets or sets static Subnet mask. */
static void wifi_cmd_subnet(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.indexOf(';') != -1) {
        cleanVal = cleanVal.substring(0, cleanVal.indexOf(';'));
        cleanVal.trim();
    }

    if (cleanVal.length() > 0) {
        IPAddress testIp;
        if (!testIp.fromString(cleanVal)) {
            LOG_ERROR("Invalid Subnet mask format: " + cleanVal);
            return;
        }
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_subnet = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_subnet = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Static Subnet updated to: " + wifi_subnet);
    } else {
        LOG_INFO("WiFi Static Subnet: " + getWifiSubnet() + " (Current Subnet: " + WiFi.subnetMask().toString() + ")");
    }
}

/** @brief Command handler: Gets or sets static DNS1 server IP. */
static void wifi_cmd_dns(const String &arg) {
    String cleanVal = arg;
    cleanVal.trim();
    while (cleanVal.endsWith(";")) {
        cleanVal = cleanVal.substring(0, cleanVal.length() - 1);
        cleanVal.trim();
    }
    if (cleanVal.indexOf(';') != -1) {
        cleanVal = cleanVal.substring(0, cleanVal.indexOf(';'));
        cleanVal.trim();
    }

    if (cleanVal.length() > 0) {
        IPAddress testIp;
        if (!testIp.fromString(cleanVal)) {
            LOG_ERROR("Invalid DNS server IP format: " + cleanVal);
            return;
        }
        if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
            wifi_dns1 = cleanVal;
            xSemaphoreGive(wifiConfigMutex);
        } else {
            wifi_dns1 = cleanVal;
        }
        saveWifiConfig();
        LOG_INFO("WiFi Static DNS updated to: " + wifi_dns1);
    } else {
        LOG_INFO("WiFi Static DNS: " + getWifiDNS1() + " (Current DNS: " + WiFi.dnsIP().toString() + ")");
    }
}

/** @brief Command handler: Displays full WiFi configuration and current network status. */
static void wifi_cmd_status(const String &arg) {
    LOG_INFO("--- WiFi Configuration & Status ---");
    LOG_INFO("SSID: " + getWifiSSID());
    LOG_INFO("Client ID: " + getWifiClientID());
    LOG_INFO("Connected: " + String(is_wifi_connected() ? "YES" : "NO"));
    LOG_INFO("IP: " + WiFi.localIP().toString() + " (Configured: " + getWifiIP() + ")");
    LOG_INFO("Gateway: " + WiFi.gatewayIP().toString() + " (Configured: " + getWifiGateway() + ")");
    LOG_INFO("Subnet: " + WiFi.subnetMask().toString() + " (Configured: " + getWifiSubnet() + ")");
    LOG_INFO("DNS: " + WiFi.dnsIP().toString() + " (Configured: " + getWifiDNS1() + ")");
    LOG_INFO("RSSI: " + String(WiFi.RSSI()) + " dBm");
    LOG_INFO("------------------------------------");
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

String getWifiSSID() {
    return wifi_ssid;
}

String getWifiPassword() {
    return wifi_password;
}

String getWifiClientID() {
    return wifi_client_id;
}

String getWifiIP() {
    return wifi_ip;
}

String getWifiGateway() {
    return wifi_gateway;
}

String getWifiSubnet() {
    return wifi_subnet;
}

String getWifiDNS1() {
    return wifi_dns1;
}

void updateWifiConfig(const String &newSsid, const String &newPass) {
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        wifi_ssid = newSsid;
        wifi_password = newPass;
        if (config_get_lock()) {
            config_save_string("wifi", "ssid", wifi_ssid);
            config_save_string("wifi", "pass", wifi_password);
            config_release_lock();
        }
        xSemaphoreGive(wifiConfigMutex);
    }
}

void updateWifiClientID(const String &newClientId) {
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        wifi_client_id = newClientId;
        if (config_get_lock()) {
            config_save_string("wifi", "client_id", wifi_client_id);
            config_release_lock();
        }
        xSemaphoreGive(wifiConfigMutex);
    }
}

void updateWifiStaticIPConfig(const String &newIp, const String &newGw, const String &newSn, const String &newDns) {
    if (wifiConfigMutex != NULL && xSemaphoreTake(wifiConfigMutex, portMAX_DELAY) == pdTRUE) {
        wifi_ip = newIp;
        wifi_gateway = newGw;
        wifi_subnet = newSn;
        wifi_dns1 = newDns;
        if (config_get_lock()) {
            config_save_string("wifi", "ip", wifi_ip);
            config_save_string("wifi", "gw", wifi_gateway);
            config_save_string("wifi", "sn", wifi_subnet);
            config_save_string("wifi", "dns", wifi_dns1);
            config_release_lock();
        }
        xSemaphoreGive(wifiConfigMutex);
    }
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
 * @brief FreeRTOS task quản lý cấu hình WiFi và điều phối máy trạng thái tự động kết nối lại (Auto-Reconnect).
 * @param[in] pvParameters Tham số tác vụ FreeRTOS (không sử dụng).
 */
void vWifiTask(void *pvParameters) {
    /* Wait until log initialized */
    waiting_on_event(SYSTEM_EVENT, SYS_NORMAL, portMAX_DELAY);
    loadWifiConfig();

    // Register WiFi CLI / Web commands
    register_cmd(CMD_WIFI_SSID, wifi_cmd_ssid);
    register_cmd("WIFI_SSID", wifi_cmd_ssid);
    register_cmd(CMD_WIFI_PASSWORD, wifi_cmd_password);
    register_cmd("WIFI_PASS", wifi_cmd_password);
    register_cmd(CMD_WIFI_CLIENT_ID, wifi_cmd_client_id);
    register_cmd("CLIENT_ID", wifi_cmd_client_id);
    register_cmd("CLIENTID", wifi_cmd_client_id);
    register_cmd("DEVICE_ID", wifi_cmd_client_id);
    register_cmd("HOSTNAME", wifi_cmd_client_id);
    register_cmd(CMD_STATIC_IP, wifi_cmd_ip);
    register_cmd("STATIC_IP", wifi_cmd_ip);
    register_cmd(CMD_STATIC_GATEWAY, wifi_cmd_gateway);
    register_cmd("STATIC_GATEWAY", wifi_cmd_gateway);
    register_cmd(CMD_STATIC_SUBNET, wifi_cmd_subnet);
    register_cmd("STATIC_SUBNET", wifi_cmd_subnet);
    register_cmd(CMD_STATIC_DNS, wifi_cmd_dns);
    register_cmd("STATIC_DNS1", wifi_cmd_dns);
    register_cmd(CMD_WIFI_STATUS, wifi_cmd_status);

    setup_wifi();
    for (;;) {
        /*
         * MÁY TRẠNG THÁI TỰ ĐỘNG KẾT NỐI LẠI NON-BLOCKING 30 GIÂY (Non-blocking Reconnect State Machine):
         * 1. Chu kỳ thăm dò (Polling Interval): Task thức dậy mỗi 5000ms (5 giây) để kiểm tra is_wifi_connected().
         * 2. Nhận diện mất kết nối: Nếu mất kết nối, lập tức cập nhật trạng thái mạng sang NET_STATE_DISCONNECTED.
         * 3. Cơ chế Non-blocking Timer:
         *    Sử dụng hiệu số (currentMillis - previousWifiMillis >= wifiReconnectInterval) với wifiReconnectInterval = 30000ms (30s).
         *    - Tránh việc block task bằng vTaskDelay(30000), đảm bảo FreeRTOS scheduler vẫn có thể phân phối thời gian CPU
         *      cho các tiến trình khác và cho phép task phản ứng nhanh nếu trạng thái mạng thay đổi từ bên ngoài.
         * 4. Các bước chuyển trạng thái (State Transitions):
         *    - [State: DISCONNECTED] -> [State: CONNECTING]: Phát tín hiệu SIG_WIFI_DISCONNECTED để các subsystem (như MQTT)
         *      tạm dừng truyền thông hoặc đóng socket an toàn.
         *    - Thực hiện ngắt phiên cũ (WiFi.disconnect()) và yêu cầu kết nối lại (WiFi.begin()).
         *    - Chờ 500ms để stack mạng cập nhật trạng thái handshake ban đầu.
         *    - Nếu thành công: Chuyển sang [State: WIFI_STA] và ghi log.
         *    - Nếu thất bại: Giữ nguyên trạng thái ngắt kết nối, cập nhật previousWifiMillis để bắt đầu chu kỳ 30s kế tiếp.
         */
        if (!is_wifi_connected()) {
            unsigned long currentMillis = millis();
            CoreState_SetNetwork(NET_STATE_DISCONNECTED);
            if (currentMillis - previousWifiMillis >= wifiReconnectInterval) {
                CoreState_SetNetwork(NET_STATE_CONNECTING);
                dispatch_signal(SIG_WIFI_DISCONNECTED);
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




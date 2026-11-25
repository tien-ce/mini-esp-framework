#include "services/services.h"
#include "services/logger/log_task.h"
#include "services/wifi/wifi_task.h"
#include "services/web/web_server_task.h"
#include "services/mqtt/mqtt_task.h"

void services_start() {
    // 1. Task: Log & Command Processing Task (Priority 5)
    log_task_start();

    // 2. Task: WiFi State Monitoring & Auto-Reconnect Task (Priority 3)
    wifi_task_start();

    // 3. Task: Web Server & System Monitor Task (Priority 3)
    web_server_task_start();

    // 4. Task: MQTT Client Task (Priority 3)
    mqtt_task_start();
}

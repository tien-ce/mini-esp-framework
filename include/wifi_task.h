#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include "config_manager.h"
// WiFi Setup and Event Handler Declarations
void setup_wifi();
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

// WiFi FreeRTOS Task Declaration
void vWifiTask(void *pvParameters);

#endif // WIFI_TASK_H

#ifndef FREERTOS_TASKS_H
#define FREERTOS_TASKS_H

#include "Header_sensor.h"
#include "config_manager.h"
#include "web_server.h"
#include "log_task.h"
#include "wifi_task.h"

extern QueueHandle_t sensorQueue;

// Initialization helper for tasks & queues
void initFreeRTOSTasks();

#endif // FREERTOS_TASKS_H

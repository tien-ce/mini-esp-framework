#ifndef SERVICES_H
#define SERVICES_H

#include <Arduino.h>

/**
 * @brief Bootstrapping entry point for all framework background services
 * (Logger, WiFi, WebServer, MQTT).
 */
void services_start();

#endif // SERVICES_H

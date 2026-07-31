#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "config_manager.h"
#include "index_html.h"

// External Server & WebSocket Handles
extern AsyncWebServer server;
extern AsyncWebSocket ws;

// WebSocket Event Handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len);

// Setup Web Server Routes
void setupWebServer();

#endif // WEB_SERVER_H

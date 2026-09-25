#ifndef WEB_TEMPLATE_H
#define WEB_TEMPLATE_H

#include <Arduino.h>

/** @brief Renders HTML templates by replacing dynamic system placeholders. */
String web_render_template(const char* templateStr);

/** @brief Updates dynamic telemetry key-value pair for HTTP polling responses. */
void updateElementValue(const String& key, const String& newValue);

/** @brief Legacy helper for registering table elements. */
[[deprecated("registerElement() is outdated and does nothing. UI elements are rendered dynamically via HTTP Polling.")]]
uint8_t registerElement(const String& label, const String& unit, const String& initialValue);

/** @brief Legacy helper for updating element value by ID. */
[[deprecated("updateElementValue() is outdated and does nothing. Use updateElementValue to register key-value pairs.")]]
void updateElementValue(uint8_t id, const String& newValue);

/** @brief Retrieves the current buffered telemetry JSON payload string. */
String web_get_telemetry_json();

/** @brief Clears current telemetry JSON data buffer. */
void web_clear_telemetry_json();

#endif // WEB_TEMPLATE_H

#ifndef WEB_TEMPLATE_H
#define WEB_TEMPLATE_H

#include <Arduino.h>

/**
 * @brief Writes the GPIO pin mapping as JSON (shared option list + one entry per board pin).
 * @param out Destination stream, typically an AsyncResponseStream.
 */
void web_build_gpio_json(Print &out);

/** @brief Updates dynamic telemetry key-value pair for HTTP polling responses. */
void updateElementValue(const String& key, const String& newValue);

/** @brief Starts a telemetry poll: call once per HTTP poll, before the SIG_WEB_POLL dispatch. */
void web_begin_telemetry_poll();

/** @brief Builds the telemetry JSON from the table and removes keys not updated for too many polls. */
String web_get_telemetry_json();

#endif // WEB_TEMPLATE_H

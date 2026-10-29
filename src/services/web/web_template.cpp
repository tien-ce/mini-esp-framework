#include "services/web/web_template.h"
#include "core/info.h"
#include "services/wifi/wifi_task.h"
#include "services/mqtt/mqtt_task.h"
#include "core/pin_config.h"
#include "core/core_log.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <freertos/semphr.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              STATIC VARIABLES                              */
/* -------------------------------------------------------------------------- */

/* For registering rows in dynamic table */
static String tableRowsHTML = "";

/*
 * Telemetry table for HTTP polling. A key keeps its last value until it has not been updated for
 * TELEMETRY_MAX_AGE polls, so a poll where a script handler missed its window does not drop the row.
 */
#define TELEMETRY_MAX_KEYS   30   /* Rows the table can hold */
#define TELEMETRY_KEY_LEN    30   /* Key buffer size, including the terminator */
#define TELEMETRY_VALUE_LEN  32   /* Value buffer size, including the terminator */
#define TELEMETRY_MAX_AGE    3    /* Polls in a row a key may go without an update before it is removed */

typedef struct {
    char     key[TELEMETRY_KEY_LEN];      /* key[0] == '\0' marks a free slot */
    char     value[TELEMETRY_VALUE_LEN];
    uint32_t last_poll;                   /* s_poll_count when the key was last written */
} telemetry_entry_t;

static telemetry_entry_t s_telemetry[TELEMETRY_MAX_KEYS];
static uint32_t s_poll_count = 0;

/** @brief Returns the mutex guarding the telemetry table (written by driver/script tasks, read by the web task). */
static SemaphoreHandle_t telemetryMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}

/** Longest placeholder name (without the surrounding '%') the scanner will look for. */
#define TPL_MAX_TOKEN_LEN 20

/** Placeholder identifiers, in the same order as TPL_TOKENS. */
enum TplToken : uint8_t {
    TK_HEADER_TITLE,
    TK_HEADER_SUBTITLE,
    TK_FOOTER_TEXT,
    TK_APP_VERSION,
    TK_BUILD_DATE,
    TK_SDK_VERSION,
    TK_HOSTNAME,
    TK_CHIP_MODEL,
    TK_MAC_ADDR,
    TK_FLASH_SIZE,
    TK_SENSOR_TABLE_ROWS,
    TK_UPTIME,
    TK_IP_ADDR,
    TK_GATEWAY,
    TK_SUBNET_MASK,
    TK_DNS_SERVER,
    TK_FREE_RAM,
    TK_WIFI_SSID,
    TK_WIFI_CLIENT_ID,
    TK_STATIC_IP,
    TK_STATIC_GATEWAY,
    TK_STATIC_SUBNET,
    TK_STATIC_DNS1,
    TK_MQTT_SERVER,
    TK_MQTT_PORT,
    TK_MQTT_USER,
    TK_MQTT_TOPIC,
    TK_MQTT_RPC_TOPIC,
    TK_MQTT_INTERVAL,
    TK_COUNT
};

/** Placeholder names (without '%'), indexed by TplToken. */
static const char *const TPL_TOKENS[TK_COUNT] = {
    "HEADER_TITLE", "HEADER_SUBTITLE", "FOOTER_TEXT", "APP_VERSION", "BUILD_DATE",
    "SDK_VERSION", "HOSTNAME", "CHIP_MODEL", "MAC_ADDR", "FLASH_SIZE",
    "SENSOR_TABLE_ROWS", "UPTIME", "IP_ADDR", "GATEWAY", "SUBNET_MASK",
    "DNS_SERVER", "FREE_RAM", "WIFI_SSID", "WIFI_CLIENT_ID",
    "STATIC_IP", "STATIC_GATEWAY", "STATIC_SUBNET", "STATIC_DNS1", "MQTT_SERVER",
    "MQTT_PORT", "MQTT_USER", "MQTT_TOPIC", "MQTT_RPC_TOPIC",
    "MQTT_INTERVAL",
};

/**
 * Bounded output sink used by both passes.
 * buf == nullptr -> counting pass (only len grows); buf != nullptr -> emit pass (copy, never past cap).
 */
struct TplWriter {
    char  *buf;
    size_t cap;
    size_t len;
};

/**
 * Values that can change between the counting pass and the emit pass are sampled once here,
 * so both passes see identical text and the measured size stays exact.
 */
struct TplSnapshot {
    char uptime[24];
    char freeRam[20];
    char flashSize[20];
    char ip[16];
    char gateway[16];
    char subnet[16];
    char dns[16];
    char mqttPort[8];
    char mqttInterval[12];
};

/* -------------------------------------------------------------------------- */
/*                              STATIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Appends n bytes to the writer (count-only when no buffer, truncating at cap otherwise).
 * @param w Output writer.
 * @param s Source bytes.
 * @param n Number of bytes to append.
 */
static inline void tplEmitN(TplWriter &w, const char *s, size_t n) {
    // Copy only when a buffer is attached, and never beyond its capacity
    if (w.buf && w.len < w.cap) {
        size_t room = w.cap - w.len;
        memcpy(w.buf + w.len, s, n < room ? n : room);
    }
    // Always advance the logical length so the counting pass gets the exact total
    w.len += n;
}

/**
 * @brief Appends a NUL-terminated string to the writer.
 * @param w Output writer.
 * @param s Source string (nullptr is treated as empty).
 */
static inline void tplEmit(TplWriter &w, const char *s) {
    if (s) tplEmitN(w, s, strlen(s));
}

/**
 * @brief Appends an Arduino String to the writer.
 * @param w Output writer.
 * @param s Source string.
 */
static inline void tplEmit(TplWriter &w, const String &s) {
    tplEmitN(w, s.c_str(), s.length());
}

/**
 * @brief Appends a decimal integer to the writer.
 * @param w Output writer.
 * @param v Value to print.
 */
static inline void tplEmitInt(TplWriter &w, int v) {
    char tmp[16];
    int n = snprintf(tmp, sizeof(tmp), "%d", v);
    if (n > 0) tplEmitN(w, tmp, (size_t)n);
}

/**
 * @brief Formats an IPv4 address as dotted text into a fixed buffer.
 * @param out Destination buffer.
 * @param cap Destination capacity.
 * @param ip Address to format.
 */
static void tplFormatIp(char *out, size_t cap, const IPAddress &ip) {
    snprintf(out, cap, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/**
 * @brief Samples all fast-changing values(can change between 2 pharse (get length and copy value) once per render.
 * @param s Snapshot to fill.
 */
static void tplTakeSnapshot(TplSnapshot &s) {
    // Uptime as "<days>T<hh>:<mm>:<ss>"
    uint32_t seconds = millis() / 1000;
    uint32_t days = seconds / 86400;
    seconds %= 86400;
    uint32_t hrs = seconds / 3600;
    seconds %= 3600;
    snprintf(s.uptime, sizeof(s.uptime), "%uT%02u:%02u:%02u",
             (unsigned)days, (unsigned)hrs, (unsigned)(seconds / 60), (unsigned)(seconds % 60));

    // Memory and flash figures
    snprintf(s.freeRam, sizeof(s.freeRam), "%.1f KB", esp_info_get_free_heap() / 1024.0);
    snprintf(s.flashSize, sizeof(s.flashSize), "%u KB", (unsigned)(ESP.getFlashChipSize() / 1024));

    // Network addresses (no String temporaries)
    tplFormatIp(s.ip, sizeof(s.ip), WiFi.localIP());
    tplFormatIp(s.gateway, sizeof(s.gateway), WiFi.gatewayIP());
    tplFormatIp(s.subnet, sizeof(s.subnet), WiFi.subnetMask());
    tplFormatIp(s.dns, sizeof(s.dns), WiFi.dnsIP());

    // MQTT numeric settings
    snprintf(s.mqttPort, sizeof(s.mqttPort), "%u", (unsigned)getMqttPort());
    snprintf(s.mqttInterval, sizeof(s.mqttInterval), "%u", (unsigned)getMqttInterval());
}

/**
 * @brief Emits the value of one placeholder into the writer.
 * @param w Output writer.
 * @param tk Placeholder identifier.
 * @param snap Per-render snapshot of volatile values.
 */
static void tplEmitToken(TplWriter &w, TplToken tk, const TplSnapshot &snap) {
    // Dispatch on the placeholder id; each branch writes its value directly into the writer
    switch (tk) {
    case TK_HEADER_TITLE:    tplEmit(w, "ESP32S3"); break;
    case TK_HEADER_SUBTITLE: tplEmit(w, "ESP mini framework " FW_VERSION_STR); break;
    case TK_FOOTER_TEXT:     tplEmit(w, "ESP mini framework " FW_VERSION_STR " by Văn Tiến"); break;
    case TK_APP_VERSION:     tplEmit(w, FW_VERSION_STR); break;
    case TK_BUILD_DATE:      tplEmit(w, __DATE__ " " __TIME__); break;
    case TK_SDK_VERSION:     tplEmit(w, ESP.getSdkVersion()); break;
    case TK_HOSTNAME: {
        // Prefer the configured client id, fall back to "esp-mini-framework"
        String clientID = getWifiClientID();
        if (clientID.length() > 0) {
            tplEmit(w, clientID);
        } else {
            tplEmit(w, "esp-mini-framework-");
            tplEmit(w, esp_info_get_mac_str());
        }
        break;
    }
    case TK_CHIP_MODEL:        tplEmit(w, esp_info_get_model()); break;
    case TK_MAC_ADDR:          tplEmit(w, esp_info_get_mac_str()); break;
    case TK_FLASH_SIZE:        tplEmit(w, snap.flashSize); break;
    case TK_SENSOR_TABLE_ROWS: tplEmit(w, tableRowsHTML); break;
    case TK_UPTIME:            tplEmit(w, snap.uptime); break;
    case TK_IP_ADDR:           tplEmit(w, snap.ip); break;
    case TK_GATEWAY:           tplEmit(w, snap.gateway); break;
    case TK_SUBNET_MASK:       tplEmit(w, snap.subnet); break;
    case TK_DNS_SERVER:        tplEmit(w, snap.dns); break;
    case TK_FREE_RAM:          tplEmit(w, snap.freeRam); break;
    case TK_WIFI_SSID:         tplEmit(w, getWifiSSID()); break;
    case TK_WIFI_CLIENT_ID:    tplEmit(w, getWifiClientID()); break;
    case TK_STATIC_IP:         tplEmit(w, getWifiIP()); break;
    case TK_STATIC_GATEWAY:    tplEmit(w, getWifiGateway()); break;
    case TK_STATIC_SUBNET:     tplEmit(w, getWifiSubnet()); break;
    case TK_STATIC_DNS1:       tplEmit(w, getWifiDNS1()); break;
    case TK_MQTT_SERVER:       tplEmit(w, getMqttServer()); break;
    case TK_MQTT_PORT:         tplEmit(w, snap.mqttPort); break;
    case TK_MQTT_USER:         tplEmit(w, getMqttUser()); break;
    case TK_MQTT_TOPIC:        tplEmit(w, getMqttDataTopic()); break;
    case TK_MQTT_RPC_TOPIC:    tplEmit(w, getMqttRpcTopic()); break;
    case TK_MQTT_INTERVAL:     tplEmit(w, snap.mqttInterval); break;
    default: break;
    }
}

/**
 * @brief Looks up a placeholder name in TPL_TOKENS.
 * @param name Start of the name (not NUL-terminated).
 * @param len Length of the name.
 * @param out Receives the token id when found.
 * @return true when the name is a known placeholder.
 */
static bool tplFindToken(const char *name, size_t len, TplToken &out) {
    // Compare length first, then bytes, over the small fixed table
    for (uint8_t i = 0; i < TK_COUNT; i++) {
        if (strlen(TPL_TOKENS[i]) == len && memcmp(TPL_TOKENS[i], name, len) == 0) {
            out = (TplToken)i;
            return true;
        }
    }
    return false;
}

/**
 * @brief Runs one pass over the template, copying literal text and expanding placeholders.
 * @param w Output writer (counting or emitting).
 * @param tpl NUL-terminated template.
 * @param snap Per-render snapshot of volatile values.
 */
static void tplScan(TplWriter &w, const char *tpl, const TplSnapshot &snap) {
    const char *p = tpl;
    const char *litStart = tpl;   // start of the pending literal run

    // Single walk over the template; only '%' can start a placeholder
    while (*p) {
        if (*p != '%') { p++; continue; }

        // Find the closing '%' within the maximum token length
        const char *q = p + 1;
        while (*q && *q != '%' && (size_t)(q - p - 1) <= TPL_MAX_TOKEN_LEN) q++;

        // Expand only when the text between the '%' pair is a known name
        TplToken tk;
        if (*q == '%' && tplFindToken(p + 1, (size_t)(q - p - 1), tk)) {
            tplEmitN(w, litStart, (size_t)(p - litStart));   // flush literal before the token
            tplEmitToken(w, tk, snap);                       // write the resolved value
            p = q + 1;                                       // continue after the closing '%'
            litStart = p;
        } else {
            p++;                                             // lone/unknown '%': keep as literal
        }
    }

    // Flush the trailing literal run
    tplEmitN(w, litStart, (size_t)(p - litStart));
}

/* -------------------------------------------------------------------------- */
/*                              PUBLIC FUNCTIONS                              */
/* -------------------------------------------------------------------------- */

String web_render_template(const char* templateStr) {
    // Checking input
    if (templateStr == nullptr) return String();

    // Sample volatile values once so both passes agree
    TplSnapshot snap;
    tplTakeSnapshot(snap);

    // Pass 1: count the exact output size
    TplWriter counter = {nullptr, 0, 0};
    tplScan(counter, templateStr, snap);

    // Single allocation for the whole page
    char *out = (char *)malloc(counter.len + 1);
    if (out == nullptr) {
        LOG_ERROR_STR("web_render_template: out of memory (%u bytes)", (unsigned)(counter.len + 1));
        return String();
    }

    // Pass 2: emit into the buffer (bounded, cannot overflow even if a getter changed meanwhile)
    TplWriter emitter = {out, counter.len, 0};
    tplScan(emitter, templateStr, snap);
    out[emitter.len < counter.len ? emitter.len : counter.len] = '\0';

    // Hand the page to the caller as a String (one copy) and release the work buffer
    String page(out);
    free(out);
    return page;
}
/*
 * --------------------------------------------------------------------------
 * JSON Payload Schema generated by web_build_gpio_json()
 * --------------------------------------------------------------------------
 *
 * Root: JsonObject {}
 * │
 * ├── "options" : JsonArray [string, ...]
 * │     Shared list of all selectable pin functions (sent once for all pins).
 * │     Example: ["None", "Relay1", "Relay2", "Input1", "RS485_TX"]
 * │
 * └── "pins"    : JsonArray [JsonObject {}, ...]
 *       List of physical board pin configurations.
 *       Each item contains:
 *       ├── "gpio"  : (int)     GPIO pin number (e.g., 0, 6, 17)
 *       ├── "label" : (string)  Display text for the pin (e.g., "GPIO6")
 *       ├── "fixed" : (bool)   true: Hardware-fixed pin (disabled in UI)
 *       │                      false: Configurable pin (enabled dropdown)
 *       └── "value" : (string)  Active assigned function name
 *
 * --------------------------------------------------------------------------
 * Output Example:
 * --------------------------------------------------------------------------
 * {
 *   "options": ["None", "Relay1", "Relay2", "Input1"],
 *   "pins": [
 *     {"gpio": 6, "label": "GPIO6", "fixed": false, "value": "Relay1"},
 *     {"gpio": 9, "label": "GPIO9", "fixed": true,  "value": "SPI Flash"}
 *   ]
 * }
 * --------------------------------------------------------------------------
     */
void web_build_gpio_json(Print &out) {
    // Build a small document: shared option list once, then one entry per board pin
    JsonDocument doc;

    // Option names are shared by every selectable pin
    JsonArray options = doc["options"].to<JsonArray>();
    for (size_t i = 0; i < AVAILABLE_PIN_OPTIONS_COUNT; i++) {
        options.add(AVAILABLE_PIN_OPTIONS[i]);
    }

    // One entry per physical pin: current selection falls back to the board default
    JsonArray pins = doc["pins"].to<JsonArray>();
    for (size_t i = 0; i < BOARD_PIN_COUNT; i++) {
        const auto &pin = BOARD_PINS[i];
        String active = get_pin_name(pin.gpio);
        JsonObject o = pins.add<JsonObject>();
        o["gpio"] = pin.gpio;
        o["label"] = pin.label;
        o["fixed"] = pin.isFixed;
        o["value"] = (active.length() > 0 && !pin.isFixed) ? active.c_str() : pin.defaultOption;
    }

    // Stream straight to the caller (response stream), no intermediate String
    serializeJson(doc, out);
}

/* This function called from drives and scripts to update these value into the web */
void updateElementValue(const String& key, const String& newValue) {
    if (key.length() == 0) return;

    xSemaphoreTake(telemetryMutex(), portMAX_DELAY);
    telemetry_entry_t *slot = NULL;
    for (int i = 0; i < TELEMETRY_MAX_KEYS; i++) {
        if (strcmp(s_telemetry[i].key, key.c_str()) == 0) {
            slot = &s_telemetry[i];
            break;
        }
        if (slot == NULL && s_telemetry[i].key[0] == '\0') {
            slot = &s_telemetry[i];
        }
    }
    if (slot != NULL) {
        /* A key found by name is reused; otherwise slot is the first free entry */
        strlcpy(slot->key, key.c_str(), sizeof(slot->key));
        strlcpy(slot->value, newValue.c_str(), sizeof(slot->value));
        slot->last_poll = s_poll_count;
    }
    xSemaphoreGive(telemetryMutex());

    if (slot == NULL) {
        LOG_WARNING("[Web] Telemetry table is full, '" + key + "' dropped");
    }
}

/* Called at the start of each poll: ages are counted in polls */
void web_begin_telemetry_poll() {
    xSemaphoreTake(telemetryMutex(), portMAX_DELAY);
    s_poll_count++;
    xSemaphoreGive(telemetryMutex());
}

/* This function is called after all module done with SIG_WEB_POLL signal, it will check what is key is deprecated to remove by set key = '\0'*/
String web_get_telemetry_json() {
    JsonDocument doc;

    xSemaphoreTake(telemetryMutex(), portMAX_DELAY);
    for (int i = 0; i < TELEMETRY_MAX_KEYS; i++) {
        if (s_telemetry[i].key[0] == '\0') continue;

        /* Drop keys nobody has refreshed for too long (e.g. the script that owned them stopped) */
        if (s_poll_count - s_telemetry[i].last_poll > TELEMETRY_MAX_AGE) {
            s_telemetry[i].key[0] = '\0';
            continue;
        }
        doc[s_telemetry[i].key] = s_telemetry[i].value;
    }
    xSemaphoreGive(telemetryMutex());

    String json;
    if (doc.isNull()) {
        json = "{}";
    } else {
        serializeJson(doc, json);
    }
    return json;
}

/**
 * @file xsns_01_photoelectric.cpp
 * @brief Photoelectric sensor: pins and locks prepared at SIG_INIT. No CLI command: scripts use it through the
 *        hardware module.
 *
 * Filter of one sensor, moved once per SIG_10MS (only the transitions that change the state):
 *
 *            blocked                  blocked >= min_present
 *  CLEAR ───────────────► PENDING ──────────────────────► PRESENT
 *    ▲                       │                             │   ▲
 *    │        clear (spike)  │                       clear │   │ blocked
 *    ◄───────────────────────┘                             ▼   │
 *    │                                                     GAP ─┘
 *    └──────────── clear >= min_gap  →  on_object() ───────┘
 */

/*---- INCLUDES ----*/
#include "config.h"
#ifdef USE_PHOTO_SENSOR
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "core/core_engine.h"
#include "hardware/sensor/xsns_01_photoelectric.h"

/*---- STATIC VARIABLES ----*/

static PhotoSensor_t s_sensors[PHOTO_SENSOR_MAX];  /* Sensor n is s_sensors[n - 1] */
static bool          s_initialized = false;       /* Set at the end of a successful SIG_INIT */

/*---- PUBLIC FUNCTIONS ----*/

PhotoSensorStatus_t PhotoSensor_Start(uint8_t sensor, PhotoSensorCallback_t on_object, void *arg) {
    // Step 1: driver ready, valid arguments, the sensor has a pin
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    if (on_object == NULL) return PHOTO_SENSOR_ERR_INVALID_ARG;
    PhotoSensor_t *s = &s_sensors[sensor - 1];
    if (s->pin == GPIO_INVALID) return PHOTO_SENSOR_ERR_NO_PIN;

    // Step 2: take the slot; the filter starts from "no object"
    xSemaphoreTake(s->lock, portMAX_DELAY);
    if (s->in_use) {
        xSemaphoreGive(s->lock);
        return PHOTO_SENSOR_ERR_IN_USE;
    }
    s->state          = PHOTO_SENSOR_CLEAR;
    s->block_start_ms = 0;
    s->block_last_ms  = 0;
    s->on_object      = on_object;
    s->arg            = arg;
    s->in_use         = true;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

PhotoSensorStatus_t PhotoSensor_Stop(uint8_t sensor) {
    // Step 1: driver ready, valid sensor number
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    PhotoSensor_t *s = &s_sensors[sensor - 1];

    // Step 2: free the slot under the lock: a callback call in progress ends first, none starts after
    xSemaphoreTake(s->lock, portMAX_DELAY);
    if (!s->in_use) {
        xSemaphoreGive(s->lock);
        return PHOTO_SENSOR_ERR_NOT_IN_USE;
    }
    s->in_use         = false;
    s->on_object      = NULL;
    s->arg            = NULL;
    s->state          = PHOTO_SENSOR_CLEAR;
    s->min_present_ms = PHOTO_SENSOR_DEFAULT_MIN_PRESENT_MS;   // The next user starts from the defaults
    s->min_gap_ms     = PHOTO_SENSOR_DEFAULT_MIN_GAP_MS;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

PhotoSensorStatus_t PhotoSensor_SetMinPresent(uint8_t sensor, uint32_t min_present_ms) {
    // Step 1: driver ready, valid sensor number
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    PhotoSensor_t *s = &s_sensors[sensor - 1];

    // Step 2: write under the lock: the sampling uses it from the next tick
    xSemaphoreTake(s->lock, portMAX_DELAY);
    s->min_present_ms = min_present_ms;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

PhotoSensorStatus_t PhotoSensor_GetMinPresent(uint8_t sensor, uint32_t *min_present_ms) {
    // Step 1: driver ready, valid arguments
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    if (min_present_ms == NULL) return PHOTO_SENSOR_ERR_INVALID_ARG;
    PhotoSensor_t *s = &s_sensors[sensor - 1];

    // Step 2: read under the lock
    xSemaphoreTake(s->lock, portMAX_DELAY);
    *min_present_ms = s->min_present_ms;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

PhotoSensorStatus_t PhotoSensor_SetMinGap(uint8_t sensor, uint32_t min_gap_ms) {
    // Step 1: driver ready, valid sensor number
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    PhotoSensor_t *s = &s_sensors[sensor - 1];

    // Step 2: write under the lock: the sampling uses it from the next tick
    xSemaphoreTake(s->lock, portMAX_DELAY);
    s->min_gap_ms = min_gap_ms;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

PhotoSensorStatus_t PhotoSensor_GetMinGap(uint8_t sensor, uint32_t *min_gap_ms) {
    // Step 1: driver ready, valid arguments
    if (!s_initialized) return PHOTO_SENSOR_ERR_NOT_INIT;
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) return PHOTO_SENSOR_ERR_OUT_OF_RANGE;
    if (min_gap_ms == NULL) return PHOTO_SENSOR_ERR_INVALID_ARG;
    PhotoSensor_t *s = &s_sensors[sensor - 1];

    // Step 2: read under the lock
    xSemaphoreTake(s->lock, portMAX_DELAY);
    *min_gap_ms = s->min_gap_ms;
    xSemaphoreGive(s->lock);
    return PHOTO_SENSOR_OK;
}

const char *PhotoSensor_StatusToStr(PhotoSensorStatus_t status) {
    switch (status) {
        case PHOTO_SENSOR_OK:               return "OK";
        case PHOTO_SENSOR_ERR_NOT_INIT:     return "Photo sensor not initialized";
        case PHOTO_SENSOR_ERR_OUT_OF_RANGE: return "Photo sensor number out of range";
        case PHOTO_SENSOR_ERR_NO_PIN:       return "Photo sensor has no pin configured";
        case PHOTO_SENSOR_ERR_INVALID_ARG:  return "Invalid argument";
        case PHOTO_SENSOR_ERR_IN_USE:       return "Photo sensor already started";
        case PHOTO_SENSOR_ERR_NOT_IN_USE:   return "Photo sensor not started";
        default:                            return "Unknown error";
    }
}

/**
 * @brief Dispatcher entry of the photoelectric sensors.
 * @details SIG_INIT: create the lock of every sensor, then resolve and configure the input pin of every sensor
 *          that has one.
 * @param[in] signal Signal from the dispatcher.
 * @return true if the signal was handled, false otherwise (false at SIG_INIT removes the sensor from the dispatcher).
 */
bool Xsns1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT: {
            for (uint8_t i = 0; i < PHOTO_SENSOR_MAX; i++) {
                // Step 1: the lock that start / stop, the settings and the callback call of this sensor share
                s_sensors[i].lock = xSemaphoreCreateMutex();
                if (s_sensors[i].lock == NULL) {
                    LOG_ERROR("Photo sensor: cannot create the lock");
                    return false;
                }

                // Step 2: resolve and configure the pin if the sensor has one; the slot starts free
                String pin_name = "PHOTO" + String(i + 1) + "_IN";
                int8_t pin = is_use_name(pin_name.c_str()) ? get_gpio_by_name(pin_name.c_str()) : GPIO_INVALID;

                s_sensors[i].in_use         = false;
                s_sensors[i].pin            = pin;
                s_sensors[i].min_present_ms = PHOTO_SENSOR_DEFAULT_MIN_PRESENT_MS;
                s_sensors[i].min_gap_ms     = PHOTO_SENSOR_DEFAULT_MIN_GAP_MS;
                s_sensors[i].state          = PHOTO_SENSOR_CLEAR;
                s_sensors[i].on_object      = NULL;
                s_sensors[i].arg            = NULL;
                if (pin != GPIO_INVALID) {
                    pinMode(pin, INPUT);    // The board's input stage drives the level, no internal pull needed
                    LOG_INFO(pin_name + " on GPIO " + String(pin));
                }
            }

            // Step 3: last, so the locks and pins above are ready before anyone may use them
            s_initialized = true;
            return true;
        }
        case SIG_10MS: {
            for (uint8_t i = 0; i < PHOTO_SENSOR_MAX; i++) {
                PhotoSensor_t *s = &s_sensors[i];

                // Step 1: only started sensors are sampled (a sensor without a pin is never started)
                xSemaphoreTake(s->lock, portMAX_DELAY);
                if (!s->in_use) {
                    xSemaphoreGive(s->lock);
                    continue;
                }
                uint32_t now     = millis();    // Time from millis(), not tick counts: late ticks are merged
                bool     blocked = (digitalRead(s->pin) == PHOTO_SENSOR_BLOCKED_LEVEL);

                // Step 2: move the filter one sample
                switch (s->state) {
                    case PHOTO_SENSOR_CLEAR:
                        if (blocked) {
                            s->block_start_ms = now;
                            s->state          = PHOTO_SENSOR_PENDING;
                        }
                        break;
                    case PHOTO_SENSOR_PENDING:
                        if (!blocked) {
                            s->state = PHOTO_SENSOR_CLEAR;     // Spike: cleared before min_present_ms
                        } else if (now - s->block_start_ms >= s->min_present_ms) {
                            s->block_last_ms = now;
                            s->state         = PHOTO_SENSOR_PRESENT;
                        }
                        break;
                    case PHOTO_SENSOR_PRESENT:
                        if (blocked) {
                            s->block_last_ms = now;
                        } else {
                            s->state = PHOTO_SENSOR_GAP;    // block_last_ms stays: the last blocked sample
                        }
                        break;
                    case PHOTO_SENSOR_GAP:
                        if (blocked) {
                            s->block_last_ms = now;         // Gap inside the same object: merge
                            s->state         = PHOTO_SENSOR_PRESENT;
                        } else if (now - s->block_last_ms >= s->min_gap_ms) {
                            s->on_object(i + 1, s->block_last_ms - s->block_start_ms, s->arg);
                            s->state = PHOTO_SENSOR_CLEAR;
                        }
                        break;
                    default:
                        break;
                }

                // Step 3: release the slot for start / stop
                xSemaphoreGive(s->lock);
            }
            return true;
        }
        default:
            return false;
    }
}

#endif // USE_PHOTO_SENSOR

#ifndef PHOTOELECTRIC_HANDLE_H
#define PHOTOELECTRIC_HANDLE_H

/**
 * @file photoelectric_handle.h
 * @brief Script handle "photoelectric": one photoelectric sensor used by a TI script, built on top of
 *        xsns_01_photoelectric.
 *
 * Layering:
 *
 *     xsns_01_photoelectric  ->  photoelectric handle  ->  TI script (hardware.PE_SENSOR())
 *
 * The handle owns the sensor: the sensor is started by the script through the handle and stopped when the
 * last value pointing to the handle is destroyed (end of script included). There is no other owner.
 *
 * The filter settings (min_present_ms, min_gap_ms) are kept in the handle and given to the driver only by a
 * successful open(), so a handle never touches a sensor another handle holds. Setting them while open is an
 * error: close(), set, open() again.
 */

/*---- INCLUDES ----*/
#include <Arduino.h>
#include "TienInterpreter.h"
#include "hardware/sensor/xsns_01_photoelectric.h"

/*---- TYPES ----*/

/**
 * @brief Native object wrapped by a "photoelectric" handle value.
 */
typedef struct {
    uint8_t      sensor;          /**< Sensor number 1..PHOTO_SENSOR_MAX, 0 before open(). */
    bool         is_open;         /**< True while this handle holds the sensor in the driver. */
    uint32_t     min_present_ms;  /**< Attribute, given to the driver at open().
                                       PHOTO_SENSOR_DEFAULT_MIN_PRESENT_MS until set. */
    uint32_t     min_gap_ms;      /**< Attribute, given to the driver at open().
                                       PHOTO_SENSOR_DEFAULT_MIN_GAP_MS until set. */
    ti_handle_t  runtime;         /**< Script that opened the sensor: on_object is posted to it. */
    value_t     *on_object;       /**< Own copy of the script callback (val_copy), called with duration_ms.
                                       NULL before open(). */
} photoelectric_handle_t;

/*---- HANDLE TYPE ----*/

/**
 * @brief Handle type of a photoelectric value: name "photoelectric", destructor, methods and attributes.
 * @details Wrap a calloc'ed photoelectric_handle_t with val_new_handle(ptr, &g_photoelectric_handle_type).
 *          The value then owns it: the destructor stops the sensor if still open and frees it when the last
 *          value is freed.
 */
extern const ti_handle_type_t g_photoelectric_handle_type;

#endif // PHOTOELECTRIC_HANDLE_H

#ifndef XSNS_01_PHOTOELECTRIC_H
#define XSNS_01_PHOTOELECTRIC_H

/**
 * @file xsns_01_photoelectric.h
 * @brief Photoelectric sensor (e.g. E3F-R2C1, retro-reflective): turns the raw beam signal into "one object passed".
 *
 * Layering:
 *
 *     GPIO  ->  photo_sensor (sample + filter)  ->  hardware module  ->  TI script
 *
 * The sensor only reads the beam. It knows nothing about shoes or counts: it reports one object each time
 * the beam was blocked long enough and then clear long enough, with how long the object blocked it.
 * Whether that object is a shoe, a hand or two shoes stuck together is decided by the script.
 *
 * At the GPIO (after the board's input stage): PHOTO_SENSOR_BLOCKED_LEVEL = beam blocked (object present),
 * the other level = beam clear.
 *
 * Filter, per sensor:
 *   min_present_ms  The beam must stay blocked this long before it counts as an object (drops electrical spikes).
 *   min_gap_ms      The beam must stay clear this long before the object is over (merges the gaps inside one
 *                   object: laces, the waist of a shoe, two shoes of a pair).
 */

/*---- INCLUDES ----*/
#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*---- DEFINES ----*/

/** Number of photo sensors the board handles. */
#define PHOTO_SENSOR_MAX 1

/** Filter settings of a sensor after init and after every stop (not measured yet: tune on the line). */
#define PHOTO_SENSOR_DEFAULT_MIN_PRESENT_MS   30
#define PHOTO_SENSOR_DEFAULT_MIN_GAP_MS       200

/** GPIO level when the beam is blocked: LOW on the PLC MINI inputs (current flows, the optocoupler pulls the
 *  pin to GND). Change it for a board whose input stage does not invert. */
#define PHOTO_SENSOR_BLOCKED_LEVEL  LOW

/*---- TYPES ----*/

/** Result of the photo sensor functions. PHOTO_SENSOR_OK is 0, everything else is an error. */
typedef enum {
    PHOTO_SENSOR_OK = 0,            /**< Success. */
    PHOTO_SENSOR_ERR_NOT_INIT,      /**< The sampling task is not running. */
    PHOTO_SENSOR_ERR_OUT_OF_RANGE,  /**< Sensor number outside 1..PHOTO_SENSOR_MAX. */
    PHOTO_SENSOR_ERR_NO_PIN,        /**< The sensor has no input GPIO configured. */
    PHOTO_SENSOR_ERR_INVALID_ARG,   /**< NULL callback or output pointer. */
    PHOTO_SENSOR_ERR_IN_USE,        /**< The sensor is already started. */
    PHOTO_SENSOR_ERR_NOT_IN_USE,    /**< The sensor is not started. */
} PhotoSensorStatus_t;

/**
 * @brief Called once per object, from the sensor's sampling task, when the object is over.
 * @param sensor      Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param duration_ms From the first blocked sample to the last one, the merged gaps inside included.
 * @param arg         The arg given to PhotoSensor_Start, unchanged.
 */
typedef void (*PhotoSensorCallback_t)(uint8_t sensor, uint32_t duration_ms, void *arg);

/** @brief Filter state of one sensor. */
typedef enum {
    PHOTO_SENSOR_CLEAR = 0,     /**< No object. */
    PHOTO_SENSOR_PENDING,       /**< Beam blocked, but not yet for min_present_ms. */
    PHOTO_SENSOR_PRESENT,       /**< Object confirmed, beam still blocked. */
    PHOTO_SENSOR_GAP,           /**< Object confirmed, beam clear, but not yet for min_gap_ms. */
} PhotoSensorState_t;

/** @brief One photo sensor: its pin, its filter settings, its filter state and who to tell. */
typedef struct {
    SemaphoreHandle_t     lock;            /**< Guards every field below: start / stop, settings, sampling, callback. */
    bool                  in_use;          /**< Slot taken: the sampling task reads this sensor. */
    int8_t                pin;             /**< Input GPIO from pin option PHOTO<n>_IN, GPIO_INVALID if not configured. */

    /* Filter settings, given by the caller */
    uint32_t              min_present_ms;  /**< Blocked at least this long to start an object. */
    uint32_t              min_gap_ms;      /**< Clear at least this long to end an object. */

    /* Filter state, written by the sampling task only */
    PhotoSensorState_t    state;           /**< Where the current object is. */
    uint32_t              block_start_ms;  /**< millis() of the first blocked sample of the current object. */
    uint32_t              block_last_ms;   /**< millis() of the latest blocked sample so far (moves on while blocked). */

    PhotoSensorCallback_t on_object;       /**< Told once per object. NULL: nobody listens. */
    void                 *arg;             /**< Given back to on_object, never read by the driver. */
} PhotoSensor_t;

/*---- API ----*/

/**
 * @brief Start reporting objects of a sensor to a callback.
 * @details The filter starts from "no object", with the current min_present_ms / min_gap_ms (the defaults unless
 *          they were set). One user at a time: a second start fails until stop.
 * @param[in] sensor    Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param[in] on_object Called once per object from the sampling side. It must not call PhotoSensor_Stop.
 * @param[in] arg       Given back to on_object as is (e.g. the script handle). May be NULL.
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_Start(uint8_t sensor, PhotoSensorCallback_t on_object, void *arg);

/**
 * @brief Stop reporting objects of a sensor, and put its filter settings back to the defaults.
 * @details Shares the lock with the callback call: when it returns, the callback is not running and is never
 *          called again, so the caller may free what the callback uses.
 * @param[in] sensor Sensor number, 1..PHOTO_SENSOR_MAX.
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_Stop(uint8_t sensor);

/**
 * @brief Set how long the beam must stay blocked before it counts as an object.
 * @details Allowed before start and while started: the sampling uses the new value from its next tick.
 *          Stop puts it back to PHOTO_SENSOR_DEFAULT_MIN_PRESENT_MS.
 * @param[in] sensor         Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param[in] min_present_ms Milliseconds, 0 = an object starts at the next blocked sample.
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_SetMinPresent(uint8_t sensor, uint32_t min_present_ms);

/**
 * @brief Read the current min_present_ms of a sensor.
 * @param[in]  sensor         Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param[out] min_present_ms Receives the value, not NULL.
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_GetMinPresent(uint8_t sensor, uint32_t *min_present_ms);

/**
 * @brief Set how long the beam must stay clear before the object is over.
 * @details Allowed before start and while started: the sampling uses the new value from its next tick.
 *          Stop puts it back to PHOTO_SENSOR_DEFAULT_MIN_GAP_MS.
 * @param[in] sensor     Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param[in] min_gap_ms Milliseconds, 0 = the first clear sample ends the object (no merge).
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_SetMinGap(uint8_t sensor, uint32_t min_gap_ms);

/**
 * @brief Read the current min_gap_ms of a sensor.
 * @param[in]  sensor     Sensor number, 1..PHOTO_SENSOR_MAX.
 * @param[out] min_gap_ms Receives the value, not NULL.
 * @return PHOTO_SENSOR_OK or an error code.
 */
PhotoSensorStatus_t PhotoSensor_GetMinGap(uint8_t sensor, uint32_t *min_gap_ms);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *PhotoSensor_StatusToStr(PhotoSensorStatus_t status);

#endif // XSNS_01_PHOTOELECTRIC_H

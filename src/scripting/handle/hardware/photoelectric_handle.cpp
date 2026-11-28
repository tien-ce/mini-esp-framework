/**
 * @file photoelectric_handle.cpp
 * @brief Script handle "photoelectric": attributes, methods and destructor of photoelectric_handle_t.
 */

/*---- INCLUDES ----*/
#include "config.h"
#ifdef USE_PHOTO_SENSOR
#include "scripting/handle/hardware/photoelectric_handle.h"

/*---- STATIC FUNCTION PROTOTYPES ----*/

static void photoelectric_on_object(uint8_t sensor, uint32_t duration_ms, void *arg);
static PhotoSensorStatus_t photoelectric_stop_sensor(photoelectric_handle_t *self);
static void photoelectric_destroy(void *ptr);

static value_t *photoelectric_open(ti_handle_t handle, value_t **argv, int argc);
static value_t *photoelectric_close(ti_handle_t handle, value_t **argv, int argc);
static ti_status_t photoelectric_get_attr(void *ptr, const char *name, value_t **out);
static ti_status_t photoelectric_set_attr(void *ptr, const char *name, const value_t *value);

/*---- PARAMS ----*/

/** open(sensor, on_object). params[0] is self: the method is found through the handle's type, so it is always a
 *  photoelectric. */
static const param_t s_photoelectric_open_params[] = { PARAM_HANDLE("self", NULL), PARAM_INT("sensor"), PARAM_FUNC("on_object"), };
/** close(). */
static const param_t s_photoelectric_self_params[] = { PARAM_HANDLE("self", NULL), };

/*---- METHODS ----*/

static const ti_builtin_def_t s_photoelectric_methods[] = {
    { "open",  VAL_STRING, s_photoelectric_open_params, 3, photoelectric_open  },
    { "close", VAL_STRING, s_photoelectric_self_params, 1, photoelectric_close },
};

/*---- HANDLE TYPE ----*/

/* External linkage comes from the extern declaration in photoelectric_handle.h */
const ti_handle_type_t g_photoelectric_handle_type = {
    "photoelectric",                                                            /* name */
    photoelectric_destroy,                                                      /* destroy */
    s_photoelectric_methods,                                                    /* methods */
    (int)(sizeof(s_photoelectric_methods) / sizeof(s_photoelectric_methods[0])), /* method_count */
    photoelectric_get_attr,                                                     /* get_attr */
    photoelectric_set_attr,                                                     /* set_attr */
};

/*---- STATIC FUNCTIONS: DRIVER CALLBACK ----*/

/**
 * @brief Driver callback of every photoelectric handle: post on_object(duration_ms) to the script that opened it.
 * @details Runs in the dispatcher task with the sensor lock held: never call a PhotoSensor_ function here. The
 *          handle is stable: the destructor stops the sensor (same lock) before freeing it.
 * @param sensor      Sensor that saw the object (unused: arg already is the handle).
 * @param duration_ms How long the object blocked the beam.
 * @param arg         The photoelectric_handle_t given to PhotoSensor_Start.
 */
static void photoelectric_on_object(uint8_t sensor, uint32_t duration_ms, void *arg) {
    (void)sensor;
    const photoelectric_handle_t *self = (const photoelectric_handle_t *)arg;

    // ti_post_event takes the argument, also on failure; the function value is borrowed
    value_t *duration = val_new_int((int)duration_ms);
    if (duration == NULL) return;
    value_t *args[1] = { duration };
    ti_post_event(self->runtime, self->on_object, args, 1);
}

/*---- STATIC FUNCTIONS: HELPERS ----*/

/**
 * @brief Stop the sensor of an open handle and drop its callback; the handle object itself stays.
 * @details Order matters: PhotoSensor_Stop() waits for a running callback (sensor lock) and no callback starts
 *          after it, so the callback value is freed only when nothing can read it any more.
 * @param self An open handle (is_open == true).
 * @return Status of PhotoSensor_Stop().
 */
static PhotoSensorStatus_t photoelectric_stop_sensor(photoelectric_handle_t *self) {
    PhotoSensorStatus_t status = PhotoSensor_Stop(self->sensor);
    val_free(self->on_object);   // val_free(NULL) does nothing
    self->on_object = NULL;
    self->runtime   = TI_INVALID_HANDLE;
    self->is_open   = false;
    return status;
}

/*---- STATIC FUNCTIONS: METHODS ----*/

/**
 * @brief Method photoelectric.open(int sensor, func on_object): start the sensor, on_object(int duration_ms)
 *        is called once per object.
 * @param handle Calling runtime handle: on_object is posted to it, also used to raise script mistakes.
 * @param argv Arguments: (self, sensor, on_object). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of the driver; NULL after raising an error.
 */
static value_t *photoelectric_open(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    photoelectric_handle_t *self = (photoelectric_handle_t *)argv[0]->handle_val->ptr;
    int sensor = argv[1]->int_val;

    // Step 1: this handle already holds a sensor (close it first)
    if (self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "photoelectric.open: sensor %u is already open on this handle",
                       self->sensor);
        return NULL;
    }

    // Step 2: values the uint8_t cast would wrap, and the callback must be a defined function
    if (sensor < 1 || sensor > PHOTO_SENSOR_MAX) {
        ti_raise_error(handle, TI_ERR_VALUE_OUT_OF_RANGE, "photoelectric.open: sensor must be 1..%d, got %d",
                       PHOTO_SENSOR_MAX, sensor);
        return NULL;
    }
    if (val_func_name(argv[2]) == NULL) {
        ti_raise_error(handle, TI_ERR_INVALID_ARG, "photoelectric.open: expects a defined function");
        return NULL;
    }

    // Step 3: own copy of the callback (argv is freed after this call)
    value_t *func = val_copy(argv[2]);
    if (func == NULL) {
        ti_raise_error(handle, TI_ERR_NO_MEMORY, "photoelectric.open: out of memory");
        return NULL;
    }

    // Step 4: fill self before start (the driver may call back as soon as start returns)
    self->on_object = func;
    self->runtime   = handle;
    PhotoSensorStatus_t status = PhotoSensor_Start((uint8_t)sensor, photoelectric_on_object, self);

    // Step 5: the slot is ours only now, so only now it gets this handle's settings (an object needs at least
    // min_present_ms + min_gap_ms, so none is reported before this)
    if (status == PHOTO_SENSOR_OK) {
        status = PhotoSensor_SetMinPresent((uint8_t)sensor, self->min_present_ms);
        if (status == PHOTO_SENSOR_OK) {
            status = PhotoSensor_SetMinGap((uint8_t)sensor, self->min_gap_ms);
        }
        if (status != PHOTO_SENSOR_OK) {
            PhotoSensor_Stop((uint8_t)sensor);
        }
    }

    // Step 6: keep the sensor on success, undo on failure
    if (status == PHOTO_SENSOR_OK) {
        self->sensor  = (uint8_t)sensor;
        self->is_open = true;
    } else {
        val_free(func);
        self->on_object = NULL;
        self->runtime   = TI_INVALID_HANDLE;
    }
    return val_new_string(PhotoSensor_StatusToStr(status));
}

/**
 * @brief Method photoelectric.close(): stop the sensor and drop on_object. The handle can be opened again.
 * @param handle Calling runtime handle, used to raise script mistakes.
 * @param argv Arguments: (self). Owned by the interpreter, only read.
 * @param argc Argument count (unused).
 * @return String "OK" or the error text of the driver; NULL after raising an error.
 */
static value_t *photoelectric_close(ti_handle_t handle, value_t **argv, int argc) {
    (void)argc;
    photoelectric_handle_t *self = (photoelectric_handle_t *)argv[0]->handle_val->ptr;

    if (!self->is_open) {
        ti_raise_error(handle, TI_ERR_INVALID_VALUE, "photoelectric.close: sensor is not open");
        return NULL;
    }
    return val_new_string(PhotoSensor_StatusToStr(photoelectric_stop_sensor(self)));
}

/*---- STATIC FUNCTIONS: ATTRIBUTES ----*/

/**
 * @brief Read photoelectric.sensor, photoelectric.min_present_ms or photoelectric.min_gap_ms (the handle's values,
 *        readable open or not).
 * @param ptr The photoelectric_handle_t.
 * @param name Attribute name written in the script.
 * @param out Receives a new int value owned by the caller (only on TI_OK).
 * @return TI_OK, TI_ERR_UNDEFINED for an unknown name, or TI_ERR_NO_MEMORY.
 */
static ti_status_t photoelectric_get_attr(void *ptr, const char *name, value_t **out) {
    const photoelectric_handle_t *self = (const photoelectric_handle_t *)ptr;
    int v;

    if (strcmp(name, "sensor") == 0) {
        v = (int)self->sensor;
    } else if (strcmp(name, "min_present_ms") == 0) {
        v = (int)self->min_present_ms;
    } else if (strcmp(name, "min_gap_ms") == 0) {
        v = (int)self->min_gap_ms;
    } else {
        return TI_ERR_UNDEFINED;
    }

    *out = val_new_int(v);
    return (*out != NULL) ? TI_OK : TI_ERR_NO_MEMORY;
}

/**
 * @brief Write photoelectric.min_present_ms or photoelectric.min_gap_ms: kept in the handle, given to the driver
 *        by the next open().
 * @param ptr The photoelectric_handle_t.
 * @param name Attribute name written in the script.
 * @param value New value: int >= 0.
 * @return TI_OK; TI_ERR_INVALID_VALUE while open (close first); TI_ERR_TYPE_MISMATCH, TI_ERR_VALUE_OUT_OF_RANGE;
 *         TI_ERR_READ_ONLY for sensor; TI_ERR_UNDEFINED for an unknown name.
 */
static ti_status_t photoelectric_set_attr(void *ptr, const char *name, const value_t *value) {
    photoelectric_handle_t *self = (photoelectric_handle_t *)ptr;

    // The settings reach the driver only at open(): a change while open would never be used
    if (self->is_open) return TI_ERR_INVALID_VALUE;

    if (strcmp(name, "min_present_ms") == 0) {
        if (value->type != VAL_INT) return TI_ERR_TYPE_MISMATCH;
        if (value->int_val < 0) return TI_ERR_VALUE_OUT_OF_RANGE;
        self->min_present_ms = (uint32_t)value->int_val;
        return TI_OK;
    }
    if (strcmp(name, "min_gap_ms") == 0) {
        if (value->type != VAL_INT) return TI_ERR_TYPE_MISMATCH;
        if (value->int_val < 0) return TI_ERR_VALUE_OUT_OF_RANGE;
        self->min_gap_ms = (uint32_t)value->int_val;
        return TI_OK;
    }
    if (strcmp(name, "sensor") == 0) {
        return TI_ERR_READ_ONLY;
    }
    return TI_ERR_UNDEFINED;
}

/*---- STATIC FUNCTIONS: DESTRUCTOR ----*/

/**
 * @brief Destructor of the photoelectric handle type: runs when the last value pointing to the handle is freed
 *        (out of scope, script ended or stopped). Stops the sensor if still open, then frees the object.
 * @param ptr The photoelectric_handle_t, allocated with calloc by PE_SENSOR() (never NULL when called).
 */
static void photoelectric_destroy(void *ptr) {
    photoelectric_handle_t *self = (photoelectric_handle_t *)ptr;
    if (self->is_open) {
        photoelectric_stop_sensor(self);
    }
    free(self);
}

#endif // USE_PHOTO_SENSOR

#ifndef XDRV_02_RELAY_H
#define XDRV_02_RELAY_H

/**
 * @file xdrv_02_relay.h
 * @brief Relay driver: public API used by other layers (e.g. the script module "hardware").
 *
 * The driver resolves and configures the relay pins at SIG_INIT; callers only use the relay number
 * (1..MAX_RELAYS), never the pin.
 */

/*---- INCLUDES ----*/
#include <stdint.h>
#include <stdbool.h>

/*---- DEFINES ----*/

/** Number of relays the driver handles (Relay1..Relay<MAX_RELAYS>). */
#define MAX_RELAYS 3

/*---- TYPES ----*/

/** Result of the relay functions. RELAY_OK is 0, everything else is an error. */
typedef enum {
    RELAY_OK = 0,               /**< Success. */
    RELAY_ERR_NOT_INIT,         /**< The driver has not run SIG_INIT yet. */
    RELAY_ERR_OUT_OF_RANGE,     /**< Relay number outside 1..MAX_RELAYS. */
    RELAY_ERR_NO_PIN,           /**< The relay has no GPIO configured (Relay<n> not used). */
    RELAY_ERR_INVALID_ARG,      /**< NULL output pointer. */
} RelayStatus_t;

/*---- API ----*/

/**
 * @brief Read the current output level of a relay.
 * @param[in]  relay Relay number, 1..MAX_RELAYS.
 * @param[out] state Receives true when the relay pin is HIGH (ON). Not written on error.
 * @return RELAY_OK or an error code.
 */
RelayStatus_t Relay_GetState(uint8_t relay, bool *state);

/**
 * @brief Switch a relay on or off.
 * @param[in] relay Relay number, 1..MAX_RELAYS.
 * @param[in] state True drives the relay pin HIGH (ON), false LOW (OFF).
 * @return RELAY_OK or an error code.
 */
RelayStatus_t Relay_SetState(uint8_t relay, bool state);

/**
 * @brief Readable text for a status code (logs, and the string returned to scripts).
 * @param[in] status Status code.
 * @return Constant string, never NULL.
 */
const char *Relay_StatusToStr(RelayStatus_t status);

#endif // XDRV_02_RELAY_H

#ifndef DRIVER_SHARED_H
#define DRIVER_SHARED_H

/**
 * @file driver_shared.h
 * @brief Resources shared by several drivers.
 *
 * UART lock: the serial drivers (raw UART, Modbus) take a free hardware UART when they open a port (the first of
 * Serial1, Serial2 whose IDF driver is not installed) and start it with begin(). Holding this lock from the check
 * to the end of begin() (and around end() at close) keeps two drivers from taking the same UART.
 * The lock exists before setup(): no init call is needed.
 */

/*---- INCLUDES ----*/
#include <HardwareSerial.h>

/*---- API ----*/

/**
 * @brief Take the UART lock, waiting as long as another driver holds it.
 * @details Hold it only around "find a free UART + begin()" or "end()": never around a transfer.
 */
void DriverShared_UartLock(void);

/**
 * @brief Give the UART lock back. Only the task that took it may give it.
 */
void DriverShared_UartUnlock(void);

/**
 * @brief First hardware UART that nobody started (its IDF driver is not installed): Serial1, then Serial2.
 * @details Call with the UART lock held, up to the end of begin(): otherwise another driver may find the same UART
 *          free before this one started it.
 * @return The UART, or NULL when every one is in use.
 */
HardwareSerial *DriverShared_FindFreeUart(void);

#endif // DRIVER_SHARED_H

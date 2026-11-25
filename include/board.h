#ifndef BOARD_H
#define BOARD_H

/*
 * Board selection: each env in platformio.ini sets exactly one -DBOARD=BOARD_<name>.
 * Compare with "#if BOARD == BOARD_<name>", never "#ifdef": the preprocessor only compares
 * integers, so every board gets a unique number here and BOARD_NAME is derived from it.
 */
#define BOARD_PLC_MINI   1
#define BOARD_RS485_CAN  2

#if !defined(BOARD)
    #error "BOARD is not set (platformio.ini: -DBOARD=BOARD_PLC_MINI)"
#elif BOARD == BOARD_PLC_MINI
    #define BOARD_NAME   "PLC_MINI"
#elif BOARD == BOARD_RS485_CAN
    #define BOARD_NAME   "RS485_CAN"
#else
    /* Also caught: a misspelled name, which #if reads as 0 */
    #error "Unknown BOARD, add it to board.h"
#endif

#endif // BOARD_H

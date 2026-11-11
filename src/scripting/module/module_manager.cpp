/**
 * @file module_manager.cpp
 * @brief Register every script module.
 */

/*---- INCLUDES ----*/
#include "scripting/module/module_manager.h"

/*---- PUBLIC FUNCTIONS ----*/

void modules_init(void) {
    hardware_module_init();
}

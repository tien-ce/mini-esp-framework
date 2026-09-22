#include "config.h"
#ifdef USE_EXAMPLE
#include "core/core_engine.h"
#include <Arduino.h>
// Override weak Xdrv1 function
bool Xdrv1(Signal_t signal) {
    switch (signal) {
        case SIG_INIT:
            LOG_DEBUG("[Xdrv1] Initialized");
        case SIG_1SEC:
            LOG_DEBUG("[Xdrv1] Logging every 1 second...");
            return true; // Handled

        default:
            return false; // Not handled
    }
}

#endif // USE_EXAMPLE

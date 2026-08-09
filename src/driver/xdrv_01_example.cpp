#include <Arduino.h>
#include "core/dispatcher.h"
// Override weak Xdrv1 function
bool Xdrv1(Signal_t signal) {
    switch (signal) {
        case SIG_1SEC:
            Serial.println("[Xdrv1] Logging every 1 second...");
            return true; // Handled

        default:
            return false; // Not handled
    }
}

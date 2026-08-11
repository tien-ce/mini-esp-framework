#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <Arduino.h>

/**
 * @brief Operator types for rule evaluation.
 */
typedef enum {
    OP_NONE = 0,
    OP_GREATER,      // >
    OP_LESS,         // <
    OP_EQUAL,        // ==
    OP_GREATER_EQUAL,// >=
    OP_LESS_EQUAL,   // <=
    OP_NOT_EQUAL     // !=
} RuleOperator_t;

/**
 * @brief Parsed Rule structure stored in RAM memory.
 */
struct Rule_t{
    String trigger_event;   // e.g., "Autonics_PV"
    RuleOperator_t op;      // e.g., OP_GREATER
    float threshold;        // e.g., 45.5
    String target_var;      // e.g., "%var1%" if threshold is dynamic
    bool is_dynamic_thresh; // true if threshold comes from variable
    String action_cmd;      // e.g., "RELAY1: ON"
    bool active;            // Rule enable status

    // Default Constructor
    Rule_t() 
        : trigger_event(""), 
          op(OP_NONE), 
          threshold(0.0f), 
          target_var(""), 
          is_dynamic_thresh(false), 
          action_cmd(""), 
          active(false) {}
};

/**
 * @brief Initializes Rule Engine and registers CMD command with vLogTask.
 */
void rule_engine_init(void);

/**
 * @brief Public API to notify Rule Engine of an incoming event/telemetry change.
 * @param event The event key/name to match (e.g., "Autonics_PV")
 * @param value The live numerical data value from the event source
 */
void rule_on_event(const String &event, float value);
#endif // RULE_ENGINE_H
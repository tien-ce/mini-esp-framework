#include "core/rule_engine.h"
#include "core/log_task.h"
#include "core/config_manager.h"
#define MAX_RULES 10
static Rule_t s_current_rule;
static Rule_t s_rules[MAX_RULES] = {};
static SemaphoreHandle_t ruleConfigMutex = NULL;

static void initRuleMutex() {
    if (ruleConfigMutex == NULL) {
        ruleConfigMutex = xSemaphoreCreateMutex();
    }
}


/**
 * @brief Helper function to convert operator string to enum type.
 */
static RuleOperator_t parse_operator(const String &op_str) {
    if (op_str == ">")  return OP_GREATER;
    if (op_str == "<")  return OP_LESS;
    if (op_str == "==" || op_str == "=") return OP_EQUAL;
    if (op_str == ">=") return OP_GREATER_EQUAL;
    if (op_str == "<=") return OP_LESS_EQUAL;
    if (op_str == "!=") return OP_NOT_EQUAL;
    return OP_NONE;
}

/**
 * @brief Helper function to parse a raw string line into a Rule_t structure.
 * Format: ON <Event> <Op> <Val> DO <Cmd>|<Active>
 * Example: ON Autonics_PV > 45.5 DO RELAY1: ON|1
 */
static bool parse_rule_string(const String &line, Rule_t &out_rule) {
    String input = line;
    input.trim();

    if (input.length() == 0) return false;

    // 1. Separate Active status delimiter '|' from line
    int pipe_idx = input.lastIndexOf('|');
    bool is_active = true;
    String rule_part = input;

    if (pipe_idx != -1) {
        rule_part = input.substring(0, pipe_idx);
        String active_str = input.substring(pipe_idx + 1);
        active_str.trim();
        is_active = (active_str == "1" || active_str.equalsIgnoreCase("true"));
    }

    // 2. Locate "ON " and " DO " keywords
    int on_idx = rule_part.indexOf("ON ");
    int do_idx = rule_part.indexOf(" DO ");

    if (on_idx == -1 || do_idx == -1 || do_idx <= on_idx) return false;

    String condition_part = rule_part.substring(on_idx + 3, do_idx);
    String action_part = rule_part.substring(do_idx + 4);
    condition_part.trim();
    action_part.trim();

    // 3. Extract <Event>, <Op>, and <Threshold/Variable>
    int first_space = condition_part.indexOf(' ');
    int second_space = condition_part.indexOf(' ', first_space + 1);

    if (first_space == -1 || second_space == -1) return false;

    String event_str = condition_part.substring(0, first_space);
    String op_str = condition_part.substring(first_space + 1, second_space);
    String val_str = condition_part.substring(second_space + 1);

    event_str.trim();
    op_str.trim();
    val_str.trim();

    // 4. Fill output Rule_t structure
    out_rule.trigger_event = event_str;
    out_rule.op = parse_operator(op_str);
    out_rule.action_cmd = action_part;
    out_rule.active = is_active;

    if (val_str.startsWith("%") && val_str.endsWith("%")) {
        out_rule.is_dynamic_thresh = true;
        out_rule.target_var = val_str;
        out_rule.threshold = 0.0f;
    } else {
        out_rule.is_dynamic_thresh = false;
        out_rule.target_var = "";
        out_rule.threshold = val_str.toFloat();
    }

    return true;
}

/**
 * @brief Saves current in-memory rules array to LittleFS rule_config.txt file.
 */
static void saveRulesConfig() {
    String content = "";
    
    if (ruleConfigMutex != NULL && xSemaphoreTake(ruleConfigMutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < MAX_RULES; i++) {
            String op_str = "";
            switch (s_rules[i].op) {
                case OP_GREATER:       op_str = ">";  break;
                case OP_LESS:          op_str = "<";  break;
                case OP_EQUAL:         op_str = "=="; break;
                case OP_GREATER_EQUAL: op_str = ">="; break;
                case OP_LESS_EQUAL:    op_str = "<="; break;
                case OP_NOT_EQUAL:     op_str = "!="; break;
                default: break;
            }

            String val_str = s_rules[i].is_dynamic_thresh ? s_rules[i].target_var : String(s_rules[i].threshold);

            content += "ON " + s_rules[i].trigger_event + " " + op_str + " " + val_str + 
                       " DO " + s_rules[i].action_cmd + "|" + String(s_rules[i].active ? 1 : 0) + "\n";
        }
        xSemaphoreGive(ruleConfigMutex);
    }

    save_config("rule", content);
}

/**
 * @brief Command handler for parsing Rule DSL input text.
 */
/**
 * @brief Command handler for RULE: <index> <CMD|OFF>
 * Examples:
 *   RULE: 0 ON Autonics_PV > 45.5 DO RELAY1: ON
 *   RULE: 0 OFF
 */
static void cmd_rule_handler(const String &args) {
    String input = args;
    input.trim();

    if (input.length() == 0) {
        LOG_WARNING("[RuleEngine] Empty rule command!");
        return;
    }

    // 1. Extract rule index from the first space delimiter
    int first_space = input.indexOf(' ');
    if (first_space == -1) {
        LOG_ERROR("[RuleEngine] Invalid format! Expected: RULE: <index> <CMD|OFF>");
        return;
    }

    int index = input.substring(0, first_space).toInt();
    if (index < 0 || index >= MAX_RULES) {
        LOG_ERROR("[RuleEngine] Invalid Rule index: " + String(index));
        return;
    }

    String payload = input.substring(first_space + 1);
    payload.trim();

    // 2. If payload is OFF, set active state to false
    if (payload.equalsIgnoreCase("OFF")) {
        s_rules[index].active = false;
        LOG_INFO("[RuleEngine] Rule " + String(index) + " set to OFF");
    } 
    // 3. Otherwise parse rule string and store to corresponding index
    else {
        Rule_t parsed_rule;
        if (!parse_rule_string(payload, parsed_rule)) {
            LOG_ERROR("[RuleEngine] Failed to parse rule payload for index " + String(index));
            return;
        }

        s_rules[index] = parsed_rule;
        s_rules[index].active = true;

        LOG_INFO("[RuleEngine] Rule " + String(index) + " updated successfully");
    }

    // 4. Save updated rules array to NVS/Flash
    saveRulesConfig();
}

/**
 * @brief Loads rules configuration from NVS/Flash into s_rules array.
 */
static void loadRulesConfig() {
    register_config_file("rule", "rule_config.txt");
    initRuleMutex();

    String raw = read_config("rule");
    if (raw.length() == 0) {
        LOG_INFO("Rule config file not found or empty.");
        return;
    }

    // Reset memory array before loading
    for (uint8_t i = 0; i < MAX_RULES; i++) {
        s_rules[i] = Rule_t();
    }

    int pos = 0;
    uint8_t index = 0;

    while (pos < raw.length() && index < MAX_RULES) {
        int nextpos = raw.indexOf('\n', pos);
        if (nextpos == -1) nextpos = raw.length();
        String line = raw.substring(pos, nextpos);
        line.trim();
        pos = nextpos + 1;

        if (line.length() == 0) continue;

        Rule_t parsed_rule;
        if (parse_rule_string(line, parsed_rule)) {
            s_rules[index] = parsed_rule;
            index++;
        } else {
            LOG_WARNING("Failed to parse rule line at index " + String(index) + ": " + line);
        }
    }

    LOG_INFO("Rule config loaded into memory successfully. Total loaded: " + String(index));
}

/**
 * @brief Register command handler to vLogTask system.
 */
void rule_engine_init(void) {
    loadRulesConfig();
    register_cmd("RULE", cmd_rule_handler);
    LOG_INFO("[RuleEngine] Module initialized and command 'RULE' registered");
}


void rule_on_event(const String &event, float value) {
    LOG_DEBUG("[RuleEngine] Processing event: " + event + " with value: " + String(value, 2));

    for (uint8_t i = 0; i < MAX_RULES; i++) {
        // Skip inactive or uninitialized rules
        if (!s_rules[i].active || s_rules[i].trigger_event.length() == 0) {
            continue;
        }

        // Check if event name matches
        if (s_rules[i].trigger_event.equalsIgnoreCase(event)) {
            LOG_DEBUG("[RuleEngine] Matched rule at index " + String(i) + " for event: " + event);

            float target_threshold = 0.0f;

            if (s_rules[i].is_dynamic_thresh) {
                LOG_WARNING("[RuleEngine] Dynamic threshold variable (" + s_rules[i].target_var + ") evaluation not fully resolved, using 0.0");
                target_threshold = 0.0f;
            } else {
                target_threshold = s_rules[i].threshold;
            }

            // Evaluate condition
            bool condition_met = false;
            switch (s_rules[i].op) {
                case OP_GREATER:       condition_met = (value > target_threshold);  break;
                case OP_LESS:          condition_met = (value < target_threshold);  break;
                case OP_EQUAL:         condition_met = (value == target_threshold); break;
                case OP_GREATER_EQUAL: condition_met = (value >= target_threshold); break;
                case OP_LESS_EQUAL:    condition_met = (value <= target_threshold); break;
                case OP_NOT_EQUAL:     condition_met = (value != target_threshold); break;
                default:
                    LOG_WARNING("[RuleEngine] Invalid operator for rule index " + String(i));
                    break;
            }

            LOG_DEBUG("[RuleEngine] Rule " + String(i) + " eval: " + String(value, 2) + " vs " + String(target_threshold, 2) + " -> " + (condition_met ? "MATCHED" : "NOT MATCHED"));

            // Dispatch command if condition evaluates to true
            if (condition_met) {
                LOG_INFO("[RuleEngine] Rule " + String(i) + " triggered! Executing: " + s_rules[i].action_cmd);
                postIncomingCommand(s_rules[i].action_cmd);
            }
        }
    }
}
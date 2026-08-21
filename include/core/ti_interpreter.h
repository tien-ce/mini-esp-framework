#ifndef CORE_TIEN_INTERPRETER_H
#define CORE_TIEN_INTERPRETER_H

#include <Arduino.h>

/** @brief Executes a Tien script from a LittleFS file path. */
void tien_run_file(const char *path);

/** @brief Executes a Tien script directly from a source code string buffer. */
void tien_run_script(const char *source_code);

/** @brief Initializes the Tien interpreter module and registers CLI commands. */
void tien_init(void);

#endif // CORE_TIEN_INTERPRETER_H

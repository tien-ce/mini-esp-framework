#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

/**
 * @file module_manager.h
 * @brief Script modules ('import <name>;'): one init function per module, all called by modules_init().
 */

/**
 * @brief Register every script module. Call at startup, before any runtime is created.
 */
void modules_init(void);

void hardware_module_init(void);
void storage_module_init(void);

#endif // MODULE_MANAGER_H

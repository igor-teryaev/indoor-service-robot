#ifndef MOTION_WATCHDOG_H
#define MOTION_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

bool motion_watchdog_init(uint32_t timeout_ms);
bool motion_watchdog_refresh(uint32_t now_ms);
void motion_watchdog_disarm(void);
bool motion_watchdog_is_armed(void);
bool motion_watchdog_expired(uint32_t now_ms);

#endif
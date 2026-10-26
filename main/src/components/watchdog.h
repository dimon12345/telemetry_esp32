#pragma once

#include <stdint.h>

void watchdog_init(uint32_t timeout_ms);
void watchdog_feed();

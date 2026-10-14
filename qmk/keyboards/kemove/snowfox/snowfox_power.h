// Copyright 2026 Michael Loukeris (@milouk)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Sleep on battery: 0 = 3 minutes (default), 1 = off, 2 = 1, 3 = 5, 4 = 10 minutes.
#define SNOWFOX_SLEEP_SETTINGS 5

void     snowfox_power_task(void);
uint8_t  snowfox_sleep_setting(void);
void     snowfox_set_sleep_setting(uint8_t setting);
bool     snowfox_power_asleep(void);
bool     snowfox_on_battery(void);
void     snowfox_power_force_battery(bool on);
uint16_t snowfox_scan_rate(void);
uint16_t snowfox_battery_raw(void);
uint8_t  snowfox_battery_percent(void);
void     snowfox_factory_reset_key(bool pressed);
void     snowfox_factory_reset(void);

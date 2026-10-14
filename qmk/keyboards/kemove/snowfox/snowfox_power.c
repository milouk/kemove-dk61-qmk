// Copyright 2026 Michael Loukeris (@milouk)
// SPDX-License-Identifier: GPL-2.0-or-later

// Battery level, sleep on battery and the factory reset key.

#include "snowfox.h"
#include "snowfox_power.h"
#include "sled17341.h"
#include "usb_util.h"

// The battery is read on AD4 (PIO0_15). Kemove's software maps these raw
// 10-bit readings to 0% and 100%.
#define BATTERY_ADC_CHANNEL 4
#define BATTERY_RAW_EMPTY 0x222
#define BATTERY_RAW_FULL 0x280
#define BATTERY_SAMPLE_MS 10000

// Lights off after this long without typing when no USB host is connected.
// Setting 0 is the default of 3 minutes, as the stock firmware.
static const uint8_t sleep_minutes[SNOWFOX_SLEEP_SETTINGS] = {3, 0, 1, 5, 10};

#define FACTORY_RESET_HOLD_MS 3000

// On battery, the main loop pauses this long between scans once no key has
// been pressed for IDLE_AFTER_MS, so the chip sleeps most of the time.
#define IDLE_AFTER_MS 500
#define IDLE_PAUSE_MS 1

static bool     force_battery;
static uint16_t scan_rate;
static uint16_t battery_raw;
static bool     asleep;
static bool     lights_were_on;
static bool     reset_held;
static uint16_t reset_pressed_at;

static uint16_t battery_read_raw(void) {
    LPC_SYSCON->PDRUNCFG &= ~SYSCON_PDRUNCFG_ADC_PD;
    LPC_SYSCON->SYSAHBCLKCTRL |= SYSCON_SYSAHBCLKCTRL_ADC;

    // 48 MHz / 12 = 4 MHz ADC clock, start now.
    LPC_ADC->CR = (1U << BATTERY_ADC_CHANNEL) | (11U << 8) | (1U << 24);
    uint32_t data;
    uint16_t tries = 1000;
    do {
        data = LPC_ADC->DR[BATTERY_ADC_CHANNEL];
    } while (!(data & (1U << 31)) && --tries);
    LPC_ADC->CR = 0;

    return tries ? (data >> 6) & 0x3FF : 0;
}

bool snowfox_on_battery(void) {
    return force_battery || !usb_connected_state();
}

void snowfox_power_force_battery(bool on) {
    force_battery = on;
}

uint16_t snowfox_scan_rate(void) {
    return scan_rate;
}

static void scan_rate_task(void) {
    static uint16_t scans;
    static uint16_t since;
    scans++;
    if (timer_elapsed(since) >= 1000) {
        scan_rate = scans;
        scans     = 0;
        since     = timer_read();
    }
}

static void idle_task(void) {
    if (snowfox_on_battery() && last_input_activity_elapsed() > IDLE_AFTER_MS) {
        chThdSleepMilliseconds(IDLE_PAUSE_MS);
    }
}

uint16_t snowfox_battery_raw(void) {
    return battery_raw;
}

uint8_t snowfox_battery_percent(void) {
    if (battery_raw <= BATTERY_RAW_EMPTY) {
        return 0;
    }
    if (battery_raw >= BATTERY_RAW_FULL) {
        return 100;
    }
    return (uint32_t)(battery_raw - BATTERY_RAW_EMPTY) * 100 / (BATTERY_RAW_FULL - BATTERY_RAW_EMPTY);
}

static void battery_task(void) {
    static uint16_t last;
    if (battery_raw != 0 && timer_elapsed(last) < BATTERY_SAMPLE_MS) {
        return;
    }
    last = timer_read();

    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8; i++) {
        sum += battery_read_raw();
    }
    battery_raw = sum / 8;
}

uint8_t snowfox_sleep_setting(void) {
    snowfox_config_t config = snowfox_config_read();
    return config.sleep < SNOWFOX_SLEEP_SETTINGS ? config.sleep : 0;
}

void snowfox_set_sleep_setting(uint8_t setting) {
    snowfox_config_t config = snowfox_config_read();
    config.sleep            = setting < SNOWFOX_SLEEP_SETTINGS ? setting : 0;
    snowfox_config_write(config);
}

static void sleep_task(void) {
    static uint8_t  setting = UINT8_MAX;
    static uint16_t checked;
    if (setting == UINT8_MAX || timer_elapsed(checked) > 1000) {
        setting = snowfox_sleep_setting();
        checked = timer_read();
    }
    uint32_t sleep_ms = sleep_minutes[setting] * 60000UL;
    bool     idle     = sleep_ms > 0 && snowfox_on_battery() && last_input_activity_elapsed() > sleep_ms;

    if (idle && !asleep) {
        asleep         = true;
        lights_were_on = rgb_matrix_is_enabled();
        rgb_matrix_disable_noeeprom();
        sled_off();
    } else if (!idle && asleep) {
        asleep = false;
        sled_on();
        if (lights_were_on) {
            rgb_matrix_enable_noeeprom();
        }
    }
}

void snowfox_factory_reset(void) {
    eeconfig_disable();
    soft_reset_keyboard();
}

static void factory_reset_task(void) {
    if (reset_held && timer_elapsed(reset_pressed_at) >= FACTORY_RESET_HOLD_MS) {
        reset_held = false;
        snowfox_factory_reset();
    }
}

void snowfox_power_task(void) {
    scan_rate_task();
    battery_task();
    sleep_task();
    factory_reset_task();
    idle_task();
}

bool snowfox_power_asleep(void) {
    return asleep;
}

void snowfox_factory_reset_key(bool pressed) {
    reset_held = pressed;
    if (pressed) {
        reset_pressed_at = timer_read();
    }
}

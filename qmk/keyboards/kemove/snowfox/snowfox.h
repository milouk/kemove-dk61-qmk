// Copyright 2020 Yaotian Feng / Codetector
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"
#include "ch.h"
#include "hal.h"
#include "sled17341.h"
#include "board.h"

#define SF_LED_STEP 0x40

enum SNOWFOX_KEYCODES {
    // QK_KB_0 so that VIA can address these as custom keycodes.
    SNOWFOX_LED_ON = QK_KB_0,
    SNOWFOX_LED_OFF,
    SNOWFOX_LED_NEXT, // Next Profile
    SNOWFOX_LED_PUP, // Profile Specific action Up
    SNOWFOX_LED_PDN, // Profile Specific action Down
    SNOWFOX_LED_BUP, // Brightness Up
    SNOWFOX_LED_BDN, // Brightness Down
    SNOWFOX_BLE_CONN,
    SNOWFOX_BLE_DISCOVER,
    SNOWFOX_BLE_DISCONN,
    SNOWFOX_BLE_KB1,
    SNOWFOX_BLE_KB2,
    SNOWFOX_BLE_KB3,
    SNOWFOX_DEBUG,
    SNOWFOX_LED_TOGGLE,
    SNOWFOX_SAFE_RANGE,
};

// Settings kept in the keyboard's EEPROM word.
typedef union {
    uint32_t raw;
    struct {
        bool    active : 1;
        uint8_t profile : 7;
        uint8_t brightness;
        uint8_t ble_slot : 2; // last Bluetooth slot used plus one, 0 for none
    };
} snowfox_config_t;

extern THD_WORKING_AREA(waLEDThread, 128);
THD_FUNCTION(LEDThread, arg);

extern SerialConfig serialCfg;
extern thread_t *ble_thread;

extern const SPIConfig spi1Config;
extern thread_t *led_thread;
extern uint8_t led_brightness;

void snowfox_early_init(void);
void snowfox_early_led_init(void);

void snowfox_led_on(void);
void snowfox_led_off(void);
void snowfox_led_next(void);
void snowfox_led_toggle(void);
void snowfox_led_save(void);
void snowfox_led_load(void);


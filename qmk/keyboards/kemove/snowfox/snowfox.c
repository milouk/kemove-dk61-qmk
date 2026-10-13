// Copyright 2020 Yaotian Feng / Codetector
// Copyright 2026 Michael Loukeris (@milouk)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "snowfox.h"
#include "snowfox_ble.h"
#include "eeprom_lpc11uxx.h"
thread_t *led_thread = NULL;

SerialConfig serialCfg = {
    9600
};

bool dip_switch_update_kb(uint8_t index, bool active) {
if (!dip_switch_update_user(index, active)) { return false; }
  switch (index) {
    case 0:
      // v1.5: DIP on = Windows Layer, DIP off = Mac Layer
      default_layer_set(1UL << (active ? 0 : 1));
      break;
  }
  return true;
}

void matrix_init_kb(void) {
    snowfox_early_led_init();
    led_thread = chThdCreateStatic(waLEDThread, sizeof(waLEDThread), NORMALPRIO, LEDThread, NULL);
}

void keyboard_post_init_kb(void) {
    snowfox_led_load();
    keyboard_post_init_user();
}

void mcu_reset(void) {
    eeprom_lpc11uxx_flush();
    NVIC_SystemReset();
}

void bootloader_jump(void) {
    eeprom_lpc11uxx_flush();
    *((volatile uint32_t*) 0x100001F0) = 0xDEADBEEF;
    __asm__ __volatile__("dsb");
    SCB->AIRCR = 0x05FA0004; // Sys Reset
    __asm__ __volatile__("dsb");
    while(1) {}
}

// Hold a Bluetooth slot key at least this long to pair instead of switching.
#define SNOWFOX_BLE_PAIR_HOLD_MS 3000

static bool process_ble_slot_key(uint16_t keycode, keyrecord_t *record) {
    static uint16_t pressed_at[BLE_KEYBOARD_SIZE];
    uint8_t         slot = keycode - SNOWFOX_BLE_KB1;

    if (record->event.pressed) {
        pressed_at[slot] = timer_read();
        return false;
    }

    ble_cmdq->put(PICK_KEYBOARD1 + slot);
    ble_cmdq->put(timer_elapsed(pressed_at[slot]) >= SNOWFOX_BLE_PAIR_HOLD_MS ? DISCOVER : CONNECT);

    snowfox_config_t config = {.raw = eeconfig_read_kb()};
    if (config.ble_slot != slot + 1) {
        config.ble_slot = slot + 1;
        eeconfig_update_kb(config.raw);
    }
    return false;
}

static bool process_led_key(uint16_t keycode) {
    switch (keycode) {
        case SNOWFOX_LED_ON:
            snowfox_led_on();
            return false;
        case SNOWFOX_LED_OFF:
            snowfox_led_off();
            return false;
        case SNOWFOX_LED_TOGGLE:
            snowfox_led_toggle();
            return false;
        case SNOWFOX_LED_NEXT:
            snowfox_led_next();
            return false;
        case SNOWFOX_LED_PUP:
            return false;
        case SNOWFOX_LED_PDN:
            return false;
        case SNOWFOX_LED_BUP:
            if ((0xFF - led_brightness) >= SF_LED_STEP) {
                led_brightness += SF_LED_STEP;
            } else {
                led_brightness = 0xFF;
            }
            return false;
        case SNOWFOX_LED_BDN:
            if (led_brightness > (0x10 + SF_LED_STEP)) {
                led_brightness -= SF_LED_STEP;
            } else {
                led_brightness = 0x10;
            }
            return false;
    }
    return false;
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (keycode >= SNOWFOX_BLE_KB1 && keycode <= SNOWFOX_BLE_KB3) {
        return process_ble_slot_key(keycode, record);
    }

    if (record->event.pressed && ((keycode >= SNOWFOX_LED_ON && keycode <= SNOWFOX_LED_BDN) || keycode == SNOWFOX_LED_TOGGLE)) {
        bool handled = process_led_key(keycode);
        snowfox_led_save();
        return handled;
    }

    if (record->event.pressed) {
        switch (keycode) {
            case SNOWFOX_BLE_CONN:
                ble_cmdq->put(CONNECT);
                return false;
            case SNOWFOX_BLE_DISCOVER:
                ble_cmdq->put(DISCOVER);
                return false;
            case SNOWFOX_BLE_DISCONN:
                ble_cmdq->put(DROP_CONN);
                return false;
            case SNOWFOX_DEBUG:
                return false;
            default:
                break;
        }
    }
    return true;
}


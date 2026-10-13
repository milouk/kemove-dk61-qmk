// Copyright 2021 Yaotian Feng / Codetector
// SPDX-License-Identifier: GPL-2.0-or-later

#include "snowfox.h"
#include "snowfox_led.h"
#include "snowfox_ble.h"
#include <string.h>

#define ANIME_PERIOD      50

const SPIConfig spi1Config = {
  .clock_divider = 1, // No Division
  .clock_prescaler = 24, // To 2MHz
  .clock_rate = 4, // Divide 2 again to be 1MHz
  .data_size = 8, // 8 bits per transfer
};

uint8_t led_brightness = 0x10;
uint32_t anime_frame = 0;

static mutex_t led_profile_mutex;

typedef rgb_color_t (*led_update_fn)(uint8_t);

// ========= BEGIN PROFILES =========

static rgb_color_t led_profile_cycle_colormap(uint8_t ind) {
    static hsv_color_t hsv = {.h = 0, .s = 0xFF, .v = 0xFF};
    hsv.h = (anime_frame >> 2) & 0xFF;
    hsv.v = led_brightness;

    return hsv_to_rgb(hsv);
}

static rgb_color_t led_profile_white_colormap(uint8_t ind) {
    return hsv_to_rgb((hsv_color_t) {
        .h = 0, .s = 0, .v = led_brightness
    });
}

static rgb_color_t led_profile_yellow_colormap(uint8_t ind) {
    return hsv_to_rgb((hsv_color_t) {
        .h = 42, .s = 255, .v = led_brightness
    });
}

static rgb_color_t led_profile_green_colormap(uint8_t ind) {
    return hsv_to_rgb((hsv_color_t) {
        .h = 85, .s = 255, .v = led_brightness
    });
}

static rgb_color_t led_profile_cyan_colormap(uint8_t ind) {
    return hsv_to_rgb((hsv_color_t) {
        .h = 127, .s = 255, .v = led_brightness
    });
}

// ========= END PROFILES ===========

const led_update_fn led_profiles[] = {
    led_profile_cycle_colormap,
    led_profile_white_colormap,
    led_profile_green_colormap,
    led_profile_yellow_colormap,
    led_profile_cyan_colormap,
};

static uint8_t current_profile = 0;
const uint8_t num_profiles = sizeof(led_profiles) / sizeof(led_profiles[0]);
static bool led_active = false;

bool led_update_kb(led_t led_state) {
    bool res = led_update_user(led_state);
    if (res) {
        status_conf[SLED_STATUS_CONF_NUM_LOCK].enable = led_state.num_lock;
        status_conf[SLED_STATUS_CONF_CAPS_LOCK].enable = led_state.caps_lock;
        status_conf[SLED_STATUS_CONF_SCROLL_LOCK].enable = led_state.scroll_lock;
    }
    return res;
}

static inline void sled_ble_clear(sled_status_conf_t* conf) {
        conf->enable = false;
        conf->blink = false;
}

static void sled_ble_discover(sled_status_conf_t* conf) {
    conf->enable = true;
    conf->blink = true;
    conf->rgb = (rgb_color_t) {.r=0, .g=255, .b=255};
}


static void sled_ble_connecting(sled_status_conf_t* conf) {
    conf->enable = true;
    conf->blink = true;
    conf->rgb = (rgb_color_t) {.r=255, .g=0, .b=255};
}

// After connecting, the slot key shows blue this long and then returns to normal.
#define BLE_CONNECTED_SHOW_MS 3000

static sled_status_conf_t *ble_connected_conf = NULL;
static uint16_t            ble_connected_at;

static void sled_ble_active(sled_status_conf_t* conf) {
    conf->enable = true;
    conf->blink = false;
    conf->rgb = (rgb_color_t) {.r=0, .g=255, .b=255};
}

static sled_status_conf_t* ble_sled_getconf(ble_keyboard_t kb) {
    switch (kb) {
        case BLE_KEYBOARD1:
            return &status_conf[SLED_STATUS_CONF_BLE1];

        case BLE_KEYBOARD2:
            return &status_conf[SLED_STATUS_CONF_BLE2];

        case BLE_KEYBOARD3:
            return &status_conf[SLED_STATUS_CONF_BLE3];

        default:
            return NULL;
    }
}

bool ble_update_kb(ble_handle_t* ble_state) {
    sled_status_conf_t *active = ble_sled_getconf(ble_state->keyboard);
    sled_status_conf_t *sweeper;

    for (ble_keyboard_t kb=BLE_KEYBOARD1; kb < BLE_KEYBOARD_SIZE; kb++) {
        sweeper = ble_sled_getconf(kb);

        if (sweeper == active) {
            switch (ble_state->state) {
                case DISCOVERING:
                    sled_ble_discover(sweeper);
                    break;
                case CONNECTING:
                    sled_ble_connecting(sweeper);
                    break;
                case CONNECTED:
                    sled_ble_active(sweeper);
                    ble_connected_at   = timer_read();
                    ble_connected_conf = sweeper;
                    break;
                default:
                    sled_ble_clear(sweeper);
            }
        } else {
            sled_ble_clear(sweeper);
        }
    }

    return true;
}

void static status_led_relay(void) {
    sled_status_conf_t *connected = ble_connected_conf;
    if (connected != NULL && timer_elapsed(ble_connected_at) > BLE_CONNECTED_SHOW_MS) {
        if (connected->enable && !connected->blink) {
            sled_ble_clear(connected);
        }
        ble_connected_conf = NULL;
    }

    for (uint8_t i=0; i < SLED_STATUS_CONF_SIZE; i++) {
        sled_status_conf_t* conf = &status_conf[i];

        if (conf->enable) {
            sled_set_cache_blink(conf->pos, conf->blink);
            sled_set_cache_color(conf->pos, conf->rgb);
        } else {
            sled_set_cache_blink(conf->pos, false);
        }
    }
}

void reset_anime_frame(void) {
    anime_frame = 0;
}

void snowfox_early_led_init(void) {
    sled_early_init();
    chMtxObjectInit(&led_profile_mutex);
}

THD_WORKING_AREA(waLEDThread, 128);
THD_FUNCTION(LEDThread, arg) {
    (void)arg;
    chRegSetThreadName("LEDThread");
    sled_init();
    while(1) {
        chThdSleepMilliseconds(ANIME_PERIOD);

        if (led_active) {
            led_update_fn colormap = led_profiles[current_profile];

            for (uint8_t i=0; i<MATRIX_ROWS; i++) {
                for (uint8_t j=0; j<MATRIX_COLS; j++) {
                    uint8_t ind = keypos_led_map[i][j];
                    sled_set_cache_color(ind, (*colormap)(ind));
                }
            }
        } else {
            sled_cache_init();
        }

        status_led_relay();
        sled_apply();
        anime_frame++;
    }
}

void snowfox_led_next(void) {
    chMtxLock(&led_profile_mutex);
    current_profile = (current_profile + 1) % num_profiles;
    chMtxUnlock(&led_profile_mutex);
}

void suspend_power_down_kb(void) {
#if ENABLE_SLEEP_LED == TRUE
    if (led_active && !ble_custom_is_connected()) {
        sled_off();
    }
#endif
}

void suspend_wakeup_init_kb(void) {
#if ENABLE_SLEEP_LED == TRUE
    if (led_active) {
        sled_on();
    }
#endif
}

void snowfox_led_on(void) {
    if (led_active) {
        snowfox_led_next();
    } else {
        led_active = true;
    }
}

void snowfox_led_off(void) {
    led_active = false;
}

void snowfox_led_toggle(void) {
    if (led_active) {
        snowfox_led_off();
    } else {
        led_active = true;
    }
}

void snowfox_led_save(void) {
    snowfox_config_t config = {.raw = eeconfig_read_kb()};
    config.active     = led_active;
    config.profile    = current_profile;
    config.brightness = led_brightness;
    eeconfig_update_kb(config.raw);
}

void snowfox_led_load(void) {
    snowfox_config_t config = {.raw = eeconfig_read_kb()};

    chMtxLock(&led_profile_mutex);
    current_profile = config.profile % num_profiles;
    chMtxUnlock(&led_profile_mutex);
    led_brightness = config.brightness < 0x10 ? 0x10 : config.brightness;
    led_active     = config.active;
}

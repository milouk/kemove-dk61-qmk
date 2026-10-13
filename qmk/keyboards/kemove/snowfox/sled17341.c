// Copyright 2020 Yaotian Feng / Codetector
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sled17341.h"
#include "hal.h"
#include "ch.h"
#include "board.h"
#include "quantum.h"
#include "snowfox.h"
#include <string.h>
#include QMK_KEYBOARD_H

led_regs_t led_memcache[CTRLER_COUNT] = {0};

sled_status_conf_t status_conf[SLED_STATUS_CONF_SIZE] = {
    [SLED_STATUS_CONF_NUM_LOCK] = {
        .pos = KC_NO
    },
    [SLED_STATUS_CONF_CAPS_LOCK] = {
        .pos = 81,
        .enable = false,
        .blink = false,
        .rgb = {.r = 255, .g = 0, .b = 0},
    },
    [SLED_STATUS_CONF_SCROLL_LOCK] = {
        .pos = 155,
        .enable = false,
        .blink = false,
        .rgb = {.r = 255, .g = 0, .b = 0},
    },
    [SLED_STATUS_CONF_BLE1] = {
        .pos = 116,
        .enable = false,
        .blink = false,
        .rgb = {.r = 255, .g = 0, .b = 255},
    },
    [SLED_STATUS_CONF_BLE2] = {
        .pos = 119,
        .enable = false,
        .blink = false,
        .rgb = {.r = 255, .g = 0, .b = 255},
    },
    [SLED_STATUS_CONF_BLE3] = {
        .pos = 146,
        .enable = false,
        .blink = false,
        .rgb = {.r = 255, .g = 0, .b = 255},
    }
};

const ioline_t led_spi_lines[CTRLER_COUNT] = {LINE_LED1_CS, LINE_LED2_CS};

const uint8_t keypos_led_map[MATRIX_ROWS][MATRIX_COLS] = LAYOUT_ansi_61(
     2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  36,  37, 189, 190,
    46,  47,  72,  73,  74,  75,  76,  77,  80, 151, 152, 153, 185, 154,
    81,  82,  83, 108, 109, 110, 111, 112, 113, 182, 155, 181,      180,
   114, 116, 119, 146, 147, 148, 149, 150, 186, 191, 216,      187,
    38,  41,  42,            43,                 45,  44,   115,    188
);

const uint8_t ledpos_phy_map[PHY_ROWS][PHY_COLS] = {
    {    2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  36,  37, 189, 190},
    {   46,  47,  72,  73,  74,  75,  76,  77,  80, 151, 152, 153, 185, 154},
    {   81,  82,  83, 108, 109, 110, 111, 112, 113, 182, 155, 181, 180, 180},
    {  114, 114, 116, 119, 146, 147, 148, 149, 150, 186, 191, 216, 187, 187},
    {   38,  41,  42,  43,  43,  43,  43,  43,  43,  43,  45,  44, 115, 188}
};

static void spi_write(sled_spiio_t handle) {
    palClearLine(handle.line);

    uint8_t header = handle.frame | SLED_REGS_SANITY | SLED_OPERATION_WRITE;
    spiStartSend(&SPID1, sizeof(header), &header);
    spiStartSend(&SPID1, sizeof(handle.offset), &handle.offset);
    spiStartSend(&SPID1, handle.length, handle.buffer);

    palSetLine(handle.line);
}

static inline void sled_configure(ioline_t line, sled_fn_t operation, uint8_t value) {
    sled_spiio_t handle = {
        .line = line,
        .frame = SLED_REGS_FRAME_FUNC,
        .offset = operation,
        .length = sizeof(value),
        .buffer = &value};
    spi_write(handle);
}

static sled_pos_t sled_matrix_pos(uint8_t led_ind) {
    sled_pos_t ret = {0};
    ret.ind = led_ind % CTRL_SLOTS;
    ret.ctrler = led_ind / CTRL_SLOTS;
    ret.bit = ret.ind % 8;
    ret.byte = ret.ind / 8;

    return ret;
}

static void sled_ctrl_init(void) {
    for (uint8_t cs=0; cs < CTRLER_COUNT; cs++) {
        sled_configure(led_spi_lines[cs], SLED_SHUTDOWN, SLED_POWER_OFF_SW);
        sled_configure(led_spi_lines[cs], SLED_PICTURE_DISPLAY, SLED_MATRIX_TYPE_2);

        sled_configure(led_spi_lines[cs], SLED_DISPLAY_OPTION,
            SLED_BLINK_ACTIVE_MAX |
            SLED_BLINK_ENABLE |
            SLED_BLINK_FRAME_75);

        // Constant current control: value * 0.5 + 8 mA
        sled_configure(led_spi_lines[cs], SLED_CURRENT_CTRL, SLED_CONST_CURRENT_ENABLE | 25);
/*
        sled_configure(led_spi_lines[cs], SLED_STAGGERED_DELAY,
            (0 << SLED_STD1_OFFSET) |
            (1 << SLED_STD2_OFFSET) |
            (2 << SLED_STD3_OFFSET) |
            (3 << SLED_STD4_OFFSET));
*/
        sled_configure(led_spi_lines[cs], SLED_SLEW_RATE, SLED_SLEW_RATE_ENABLE);

        sled_configure(led_spi_lines[cs], SLED_VAF_1,
            (SLED_VAF_BASELINE << SLED_VAF1_OFFSET) |
            (SLED_VAF_BASELINE << SLED_VAF2_OFFSET));
        sled_configure(led_spi_lines[cs], SLED_VAF_2,
            (SLED_VAF_BASELINE << SLED_VAF3_OFFSET) |
            SLED_FVAF_FTIME_ENABLE);
    }
}

/*
static void sled_find_indicator(void) {
    status_indicator.caps_lock = sled_find_index(KC_CAPSLOCK);
    status_indicator.scrl_lock = sled_find_index(KC_SCROLLLOCK);
    status_indicator.num_lock  = sled_find_index(KC_NUMLOCK);
    status_indicator.ble_ports[0] = sled_find_index(SNOWFOX_BLE_KB1);
    status_indicator.ble_ports[1] = sled_find_index(SNOWFOX_BLE_KB2);
    status_indicator.ble_ports[2] = sled_find_index(SNOWFOX_BLE_KB3);
}
*/

void sled_cache_init(void) {
    for (uint8_t page=0; page < CTRLER_COUNT; page++) {
        memset(led_memcache[page].enable, 0xFF, sizeof(led_memcache[page].enable));
        memset(led_memcache[page].blink,  0x00, sizeof(led_memcache[page].blink ));
        memset(led_memcache[page].pwm,    0x00, sizeof(led_memcache[page].pwm   ));
    }
}

void sled_apply(void) {
    for (uint8_t cs=0; cs < CTRLER_COUNT; cs++) {
        sled_spiio_t handle = {
            .line = led_spi_lines[cs],
            .frame = SLED_REGS_FRAME_1,
            .offset = 0,
            .length = sizeof(led_regs_t),
            .buffer = (uint8_t*) &led_memcache[cs]
        };
        spi_write(handle);
    }
}

void sled_set_cache_color(uint8_t led_base_index, rgb_color_t rgb) {
    if (led_base_index == KC_NO) {
        return;
    }

    const uint8_t offsets[] = {COLOR_R, COLOR_G, COLOR_B};

    for (uint8_t ci=0; ci < sizeof(offsets); ci++) {
        sled_pos_t pos = sled_matrix_pos(led_base_index + offsets[ci]);
        led_memcache[pos.ctrler].pwm[pos.ind] = rgb.raw[ci];
    }
}

void sled_set_cache_blink(uint8_t led_base_index, bool enable) {
    if (led_base_index == KC_NO) {
        return;
    }

    const uint8_t offsets[] = {COLOR_R, COLOR_G, COLOR_B};

    for (uint8_t ci=0; ci < sizeof(offsets); ci++) {
        sled_pos_t pos = sled_matrix_pos(led_base_index + offsets[ci]);

        if (enable) {
            led_memcache[pos.ctrler].blink[pos.byte] |= (1 << pos.bit);
        } else {
            led_memcache[pos.ctrler].blink[pos.byte] &= ~(1 << pos.bit);
        }
    }
}

void sled_early_init(void) {
    palSetLine(LINE_LED1_CS);
    palSetLine(LINE_LED2_CS);

    spiStart(&SPID1, &spi1Config);
}

void sled_init(void) {
  sled_ctrl_init();
  sled_cache_init();
  sled_on();
}

void sled_on(void) {
  sled_configure(LINE_LED1_CS, SLED_SHUTDOWN, SLED_POWER_NORMAL);
  sled_configure(LINE_LED2_CS, SLED_SHUTDOWN, SLED_POWER_NORMAL);
}

void sled_off(void) {
  sled_configure(LINE_LED1_CS, SLED_SHUTDOWN, SLED_POWER_OFF_SW);
  sled_configure(LINE_LED2_CS, SLED_SHUTDOWN, SLED_POWER_OFF_SW);
}

rgb_color_t hsv_to_rgb(hsv_color_t hsv) {
    rgb_color_t rgb;
    uint8_t region, remainder, p, q, t;

    if (hsv.s == 0)
    {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6;

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

// hsv_color_t RgbToHsv(rgb_color_t rgb)
// {
//     hsv_color_t hsv;
//     unsigned char rgbMin, rgbMax;

//     rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
//     rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

//     hsv.v = rgbMax;
//     if (hsv.v == 0)
//     {
//         hsv.h = 0;
//         hsv.s = 0;
//         return hsv;
//     }

//     hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
//     if (hsv.s == 0)
//     {
//         hsv.h = 0;
//         return hsv;
//     }

//     if (rgbMax == rgb.r)
//         hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
//     else if (rgbMax == rgb.g)
//         hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
//     else
//         hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

//     return hsv;
// }

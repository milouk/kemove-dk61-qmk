// Copyright 2020 Yaotian Feng / Codetector
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <hal.h>
#include "ch.h"
#include "keycode.h"


#define PHY_ROWS        5
#define PHY_COLS       14

// SLED17341 Header Byte Definition
#define SLED_REGS_FRAME_1          (0x0 << 0)
#define SLED_REGS_FRAME_2          (0x1 << 0)
#define SLED_REGS_FRAME_FUNC       (0xb << 0)
#define SLED_REGS_FRAME_DETECT     (0xc << 0)
#define SLED_REGS_FRAME_LOCATION   (0xd << 0)
#define SLED_REGS_SANITY           (0x2 << 4)

#define SLED_OPERATION_WRITE       (0x0 << 7)
#define SLED_OPERATION_READ        (0x1 << 7)


// SLED17341 Function Register Value
// 00h: Config
#define SLED_ENABLE                0b00001000
#define SLED_ADC_ENABLE            0b00010000
#define SLED_SYNC_MASTER           0b01000000
#define SLED_SYNC_SLAVE            0b10000000
// 01h: Picture Display
#define SLED_MATRIX_TYPE_1         0b00000000
#define SLED_MATRIX_TYPE_2         0b00001000
#define SLED_MATRIX_TYPE_3         0b00010000
#define SLED_MATRIX_TYPE_4         0b00011000
// 05h: Display Option
#define SLED_BLINK_ACTIVE_327ms    0b00000001
#define SLED_BLINK_ACTIVE_MAX      0b00000111
#define SLED_BLINK_ENABLE          0b00001000
#define SLED_BLINK_FRAME_300       0b00000000
#define SLED_BLINK_FRAME_600       0b01000000
#define SLED_BLINK_FRAME_75        0b10000000
#define SLED_BLINK_FRAME_150       0b11000000
// 06h: Audio Synchronization
#define SLED_AUDIO_SYNC_ENABLE     0b00000001
// 08h: Breath Control 1
#define SLED_FADEIN_BASE2          0b00000001
#define SLED_FADEIN_MAX            0b00000111
#define SLED_FADEOUT_BASE2         0b00010000
#define SLED_FADEOUT_MAX           0b01110000
// 09h: Breath Control 2
#define SLED_EXTINGWISH_BASE2      0b00000001
#define SLED_EXTINGWISH_MAX        0b00000111
#define SLED_BREATH_ENABLE         0b00010000
#define SLED_BREATH_CONTINUOUS     0b00100000
// 0Ah: Shutdown
#define SLED_POWER_OFF_SW          0b00000000
#define SLED_POWER_NORMAL          0b00000001
// 0Bh: AGC (Audio Gain Control)
#define SLED_AGC_3dB               0b00000001
#define SLED_AGC_MAX               0b00000111
#define SLED_AGC_ENABLE            0b00001000
#define SLED_AGC_SLOW              0b00000000
#define SLED_AGC_FAST              0b00010000
// 0Dh: Staggered Delay (0->3: Zero delay->Max delay)
#define SLED_STD1_OFFSET           0x0
#define SLED_STD2_OFFSET           0x2
#define SLED_STD3_OFFSET           0x4
#define SLED_STD4_OFFSET           0x6
// 0Eh: Slew rate
#define SLED_SLEW_RATE_ENABLE      0b00000001
// 0Fh: Constant current control
#define SLED_CONST_CURRENT_ENABLE  0b10000000
#define SLED_CONST_CURRENT_SAFE    0b00110001
#define SLED_CONST_CURRENT_MAX     0b00111111
// 14h: VAF 1
#define SLED_VAF_BASELINE          0b100
#define SLED_VAF1_OFFSET           0x0
#define SLED_VAF2_OFFSET           0x4
// 15h: VAF2
#define SLED_VAF3_OFFSET           0x0
#define SLED_FTIME_OFFSET          0x3
#define SLED_FVAF_ENABLE           0b00000000
#define SLED_FVAF_FTIME_ENABLE     0b01000000
#define SLED_FVAF_DISABLE          0b10000000


// LED Wiring Traits
#define CTRL_SLOTS                 144U
#define CTRLER_COUNT                 2U
#define COLOR_R                      0U
#define COLOR_G                     12U
#define COLOR_B                     24U


typedef union {
    uint8_t raw[3];
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
} rgb_color_t;

typedef struct {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} hsv_color_t;

typedef struct {
    uint8_t enable[CTRL_SLOTS/8];
    uint8_t blink[CTRL_SLOTS/8];
    uint8_t pwm[CTRL_SLOTS];
} led_regs_t;

// SLED17341 Function Register Address
typedef enum {
    SLED_CONFIG = 0,
    SLED_PICTURE_DISPLAY,
    SLED_AUTOPLAY_DISPLAY,
    SLED_AUTOPLAY_DELAY,
    SLED_LED_RESERVED_00,
    SLED_DISPLAY_OPTION,
    SLED_AUDIO_SYNC,
    SLED_RESERVED_01,
    SLED_BREATH_FADE,
    SLED_BREATH_CFG,
    SLED_SHUTDOWN,
    SLED_AGC,
    SLED_AUDIO_ADC_RATE,
    SLED_STAGGERED_DELAY,
    SLED_SLEW_RATE,
    SLED_CURRENT_CTRL,
    SLED_OPEN_SHORT_TEST_1,
    SLED_OPEN_SHORT_TEST_2,
    SLED_RESERVED_02,
    SLED_ADC_OUTPUT,
    SLED_VAF_1,
    SLED_VAF_2,
    SLED_THERMO_STATE,
    SLED_TYPE4_VAF_1,
    SLED_TYPE4_VAF_2,
    SLED_TYPE4_VAF_3,
    SLED_CHIP_ID,
    SLED_FN_COUNT
} sled_fn_t;

typedef enum {
    SLED_STATUS_CONF_NUM_LOCK,
    SLED_STATUS_CONF_CAPS_LOCK,
    SLED_STATUS_CONF_SCROLL_LOCK,
    SLED_STATUS_CONF_BLE1,
    SLED_STATUS_CONF_BLE2,
    SLED_STATUS_CONF_BLE3,
    SLED_STATUS_CONF_SIZE
} sled_status_t;

typedef struct {
    uint8_t     pos;
    bool        enable;
    bool        blink;
    rgb_color_t rgb;
} sled_status_conf_t;

typedef struct {
    ioline_t line;
    uint8_t  frame;
    uint8_t  offset;
    uint8_t  length;
    uint8_t* buffer;
} sled_spiio_t;

typedef struct {
    uint8_t ind;
    uint8_t ctrler;
    uint8_t byte;
    uint8_t bit;
} sled_pos_t;

extern const uint8_t keypos_led_map[MATRIX_ROWS][MATRIX_COLS];
extern const uint8_t ledpos_phy_map[PHY_ROWS][PHY_COLS];
extern sled_status_conf_t status_conf[SLED_STATUS_CONF_SIZE];

void sled_apply(void);
void sled_set_cache_color(uint8_t led_base_index, rgb_color_t rgb);
void sled_set_cache_blink(uint8_t led_base_index, bool enable);
void sled_cache_init(void);
void sled_early_init(void);
void sled_init(void);
void sled_on(void);
void sled_off(void);

rgb_color_t hsv_to_rgb(hsv_color_t hsv);


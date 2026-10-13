// Copyright 2026 Michael Loukeris (@milouk)
// SPDX-License-Identifier: GPL-2.0-or-later

// Kemove's stock layout and Fn shortcuts, as printed in the K61 manual.
// Fn+B (2.4G on newer boards, unused here) enters the bootloader for updates.

#include <stdint.h>
#include "snowfox.h"
#include QMK_KEYBOARD_H

enum layers {
    WIN_BASE,
    MAC_BASE,
    _FN,
};

#define SF_LTOG SNOWFOX_LED_TOGGLE
#define SF_LNXT SNOWFOX_LED_NEXT
#define SF_LBUP SNOWFOX_LED_BUP
#define SF_LBDN SNOWFOX_LED_BDN
#define SF_LPUP SNOWFOX_LED_PUP
#define SF_LPDN SNOWFOX_LED_PDN
#define SF_BT1 SNOWFOX_BLE_KB1
#define SF_BT2 SNOWFOX_BLE_KB2
#define SF_BT3 SNOWFOX_BLE_KB3

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [WIN_BASE] = LAYOUT_ansi_61(
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENT,
        KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,                   KC_RSFT,
        KC_LCTL, KC_LGUI, KC_LALT,                            KC_SPC,                             KC_RALT, KC_APP,  KC_RCTL, MO(_FN)
    ),
    [MAC_BASE] = LAYOUT_ansi_61(
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,
        KC_CAPS, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENT,
        KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,                   KC_RSFT,
        KC_LCTL, KC_LALT, KC_LGUI,                            KC_SPC,                             KC_RGUI, KC_RALT, KC_RCTL, MO(_FN)
    ),
    [_FN] = LAYOUT_ansi_61(
        KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  SF_LTOG,
        _______, _______, _______, _______, _______, _______, _______, KC_PSCR, KC_SCRL, KC_PAUS, SF_LBDN, SF_LBUP, _______, SF_LNXT,
        _______, KC_MPLY, KC_MPRV, KC_MNXT, KC_MUTE, KC_VOLD, KC_VOLU, KC_INS,  KC_HOME, KC_PGUP, SF_LPDN, SF_LPUP,          _______,
        _______, SF_BT1,  SF_BT2,  SF_BT3,  _______, QK_BOOT, _______, KC_DEL,  KC_END,  KC_PGDN, KC_UP,                     _______,
        _______, GU_TOGG, _______,                            _______,                            KC_LEFT, KC_DOWN, KC_RGHT, _______
    ),
};

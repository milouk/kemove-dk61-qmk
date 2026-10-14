/*
    Copyright (C) 2020 Yaotian Feng, Codetector<codetector@codetector.cn>

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "snowfox_ble.h"
#include "snowfox_led.h"
#include "usb_util.h"
#include "ch.h"
#include "hal.h"
#include "host.h"
#include "host_driver.h"

cmd_queue_api* ble_cmdq;

/* -------------------- Static Function Prototypes -------------------------- */
static cmd_queue_api* queue_init(void);

static void uart_start(uint32_t baud);
static void uart_stop(void);
static void ble_custom_on(void);
static void ble_command(const char* cmd);
static void ble_command_lock(const char* cmd);
static void ack_take(void);
static void ack_release(void);
static bool ack_pass(void);
static void ble_send_consumer_report(uint16_t usage);
static void ble_send_system_report(uint8_t usage);

/* -------------------- Static Local Variables ------------------------------ */
ble_handle_t ble_handle = {
    .state = OFF,
    .keyboard = BLE_KEYBOARD1,
    .led_page = 0,
    .ack_lock = false,
    .ack_lock_timestmp = 0,
};

static _cmd_queue_t _cqdata = {0};

static char uart_rx_buffer[BLE_UART_BUFFER_SIZE];
const static char SENTINEL[] = "\r\n";
/* -------------------- Static Function Implementation ---------------------- */
static bool _queue_empty(void) {
    return (_cqdata.counter == 0);
}

static bool _queue_full(void) {
    return (_cqdata.counter == BLE_MAX_COMMAND_QUEUE);
}

static ble_queue_cmd_t _queue_pop(void) {
    if (_cqdata.counter > 0) {
        _cqdata.counter--;
    } else {
        return 0;
    }

    ble_queue_cmd_t ret = (ble_queue_cmd_t)_cqdata.buffer[_cqdata.p_start++];
    if (_cqdata.p_start >= BLE_MAX_COMMAND_QUEUE) {
        _cqdata.p_start -= BLE_MAX_COMMAND_QUEUE;
    }
    return ret;
}

static void _queue_put(ble_queue_cmd_t cmd) {
    if (_cqdata.counter < BLE_MAX_COMMAND_QUEUE) {
        _cqdata.counter++;
    } else {
        dprint("BLE command queue full\n");
        return;
    }

    _cqdata.buffer[_cqdata.p_end++] = (uint8_t)cmd;
    if (_cqdata.p_end >= BLE_MAX_COMMAND_QUEUE) {
        _cqdata.p_end -= BLE_MAX_COMMAND_QUEUE;
    }
}

static cmd_queue_api* queue_init(void) {
    memset(&_cqdata, 0, sizeof(_cqdata));
    static cmd_queue_api handle = {
        .is_empty = _queue_empty,
        .is_full = _queue_full,
        .put = _queue_put,
        .pop = _queue_pop
    };
    return &handle;
}

static void uart_start(uint32_t baud) {
    SerialConfig cfg;
    if (baud > 0) {
        cfg.speed = baud;
    } else {
        cfg.speed = 9600;
    }
    if (SD1.state == SD_STOP) {
        sdStart(&SD1, &cfg);
    }
}

static void uart_stop(void) {
    if (SD1.state == SD_READY) {
        sdStop(&SD1);
    }
}

static void ble_custom_on(void) {
    palSetLine(LINE_BLE_RSTN);
}

static void ack_take(void) {
    while (!ack_pass()) {
        wait_ms(100);
    }
    ble_handle.ack_lock = true;
    ble_handle.ack_lock_timestmp = timer_read32();
}

static void ack_release(void) {
    ble_handle.ack_lock = false;
    ble_handle.ack_lock_timestmp = 0;
}

static bool ack_pass(void) {
    if (ble_handle.ack_lock == true) {
        uint32_t elapsed_time = timer_elapsed32(ble_handle.ack_lock_timestmp);
        if (elapsed_time >= BLE_LOCK_TIMEOUT_MS) {
            ack_release();
        }
    }
    return !ble_handle.ack_lock;
}

static void ble_command(const char* raw_cmd) {
    dprintf("BLE UART Tx: %s\n", raw_cmd);
    sdWrite(&SD1, (uint8_t*) raw_cmd, strlen(raw_cmd));
    sdWrite(&SD1, (uint8_t*) SENTINEL, strlen((char*) SENTINEL));
}

static void ble_command_lock(const char* raw_cmd) {
    ble_command(raw_cmd);
    ack_take();
}

// After the link drops, or a connection attempt gives up, reconnect to the same host:
// first after 3 seconds, then doubling up to once a minute, at most 10 times (about
// 7 minutes). Not after a requested disconnect, and not for the drop that pairing or
// switching host causes.
#define RECONNECT_FIRST_MS 3000
#define RECONNECT_MAX_MS 60000
#define RECONNECT_MAX_ATTEMPTS 10
// A report that releases a key is sent again this long after, so one lost
// packet cannot leave a key held on the host.
#define RELEASE_REPEAT_MS 20

static bool     user_disconnected;
static bool     expect_drop;
static uint8_t  reconnect_attempts;
static bool     reconnect_pending;
static uint32_t reconnect_from;
static uint32_t reconnect_delay = RECONNECT_FIRST_MS;

static bool     consumer_repeat_pending;
static uint16_t consumer_repeat_from;
static uint8_t  system_release_step;
static uint16_t system_release_from;

static report_keyboard_t last_report;
static bool              repeat_pending;
static uint16_t          repeat_from;

snowfox_ble_stats_t snowfox_ble_stats;

static void schedule_reconnect(void) {
    if (!user_disconnected && reconnect_attempts < RECONNECT_MAX_ATTEMPTS) {
        reconnect_pending = true;
        reconnect_from    = timer_read32();
    }
}

static void stop_reconnect(void) {
    reconnect_pending  = false;
    reconnect_attempts = 0;
    reconnect_delay    = RECONNECT_FIRST_MS;
}

static void ble_send_keyboard_report(const report_keyboard_t *report) {
    sdWrite(&SD1, (uint8_t*) "AT+HID=\1", 8);
    sdWrite(&SD1, (uint8_t*) &report->mods, sizeof(report->mods));
    sdWrite(&SD1, (uint8_t*) &report->keys, sizeof(report->keys));
    snowfox_ble_stats.reports++;
}

static void release_all(void) {
    static const uint8_t no_consumer[2] = {0, 0};
    memset(&last_report, 0, sizeof(last_report));
    ble_send_keyboard_report(&last_report);
    sdWrite(&SD1, (uint8_t*) "AT+HID=\2", 8);
    sdWrite(&SD1, no_consumer, sizeof(no_consumer));
    repeat_pending = false;
}

static void update_event(uint8_t flag) {
    ble_state_t previous = ble_handle.state;

    switch (flag){
        case BLE_EVENT_POST:
            ble_handle.state = STANDBY;
            // A connection attempt that gave up: try again later.
            if (previous == CONNECTING) {
                schedule_reconnect();
            }
            break;

        case BLE_EVENT_DISCOVER:
            ble_handle.state = DISCOVERING;
            break;

        case BLE_EVENT_CONNECTED:
            ble_handle.state = CONNECTED;
            expect_drop      = false;
            stop_reconnect();
            // The host may still hold keys from before a drop.
            release_all();
            snowfox_ble_stats.connects++;
            break;

        case BLE_EVENT_DROP:
            ble_handle.state = STANDBY;
            if (previous == CONNECTED && !expect_drop) {
                schedule_reconnect();
            }
            expect_drop = false;
            snowfox_ble_stats.drops++;
            break;

        case BLE_EVENT_CONNECTING:
            ble_handle.state = CONNECTING;
            break;

        default:
            break;
    }
    ble_update_kb(&ble_handle);
}

static void dispatch_command(cmd_queue_api* qu) {
    if (qu->is_empty())
        return;

    ble_queue_cmd_t cmd = qu->pop();
    switch (cmd) {
        case CONNECT:
            user_disconnected = false;
            ble_command_lock("AT+CONN");
            break;
        case DISCOVER:
            user_disconnected = false;
            expect_drop       = true;
            stop_reconnect();
            ble_command_lock("AT+DISCOVER");
            break;
        case DROP_CONN:
            user_disconnected = true;
            stop_reconnect();
            ble_command_lock("AT+DISCONN");
            break;
        case CHANGE_NAME:
            ble_command_lock("AT+NAME=SnowfoxQMK");
            break;
        case PICK_KEYBOARD1:
            expect_drop         = ble_handle.state == CONNECTED;
            stop_reconnect();
            ble_handle.keyboard = BLE_KEYBOARD1;
            ble_command_lock("AT+KEYBOARD=1");
            break;
        case PICK_KEYBOARD2:
            expect_drop         = ble_handle.state == CONNECTED;
            stop_reconnect();
            ble_handle.keyboard = BLE_KEYBOARD2;
            ble_command_lock("AT+KEYBOARD=2");
            break;
        case PICK_KEYBOARD3:
            expect_drop         = ble_handle.state == CONNECTED;
            stop_reconnect();
            ble_handle.keyboard = BLE_KEYBOARD3;
            ble_command_lock("AT+KEYBOARD=3");
            break;
        default:
            break;
    }
}

// Reads a decimal or hex number. Saves the C library's strtol, which needs 80 bytes
// of RAM for errno.
static uint32_t parse_number(const char *text, uint8_t base) {
    uint32_t value = 0;
    for (;; text++) {
        uint8_t digit;
        if (*text >= '0' && *text <= '9') {
            digit = *text - '0';
        } else if (base == 16 && (*text | 0x20) >= 'a' && (*text | 0x20) <= 'f') {
            digit = (*text | 0x20) - 'a' + 10;
        } else {
            return value;
        }
        value = value * base + digit;
    }
}

// Replies collected while snowfox_ble_query() runs.
static char   *query_out;
static uint8_t query_len;
static uint8_t query_size;

static void process_response(char* buffer) {
    dprintf("BLE UART Rx: %s\n", buffer);
    if (query_out != NULL) {
        for (const char *c = buffer; *c && query_len + 1 < query_size; c++) {
            query_out[query_len++] = *c;
        }
        if (query_len + 1 < query_size) {
            query_out[query_len++] = '|';
        }
        query_out[query_len] = '\0';
    }

    if (strncmp("OK", buffer, 2) == 0) {
        ack_release();
    }

    else if (strncmp("+LEDPAGE:", buffer, 9) == 0) {
        ble_handle.led_page = parse_number(buffer + 9, 16);
    }

    else if (strncmp("+KEYBOARD:", buffer, 10) == 0) {
        ble_handle.keyboard = parse_number(buffer + 10, 10) - 1;
    }

    else if (strncmp("+EVENT:", buffer, 7) == 0) {
        update_event((uint8_t) parse_number(buffer + 7, 16));
    }
}

/* -------------------- Public Function Implementation ---------------------- */
void ble_custom_init(void) {
    uart_start(9600);
    ble_custom_on();
    wait_ms(10);

    ble_command("AT+UART=115200");
    // Let the command leave the wire at 9600 baud before the rate changes.
    wait_ms(30);

    uart_stop();
    uart_start(115200);
    ble_cmdq = queue_init();
    ble_cmdq->put(CHANGE_NAME);

    // Reconnect to the last host used, as the stock firmware does.
    snowfox_config_t config = snowfox_config_read();
    if (config.ble_slot > 0) {
        ble_cmdq->put(PICK_KEYBOARD1 + config.ble_slot - 1);
        ble_cmdq->put(CONNECT);
    }
}

void ble_custom_task(void) {
    static char* p_write = uart_rx_buffer;

    if (repeat_pending && timer_elapsed(repeat_from) >= RELEASE_REPEAT_MS) {
        repeat_pending = false;
        ble_send_keyboard_report(&last_report);
        snowfox_ble_stats.repeats++;
    }
    if (consumer_repeat_pending && timer_elapsed(consumer_repeat_from) >= RELEASE_REPEAT_MS) {
        consumer_repeat_pending = false;
        ble_send_consumer_report(0);
        snowfox_ble_stats.repeats++;
    }
    if (system_release_step != 0 && timer_elapsed(system_release_from) >= 30) {
        ble_send_system_report(0);
        system_release_from = timer_read();
        system_release_step = system_release_step == 1 ? 2 : 0;
    }

    if (reconnect_pending && timer_elapsed32(reconnect_from) >= reconnect_delay) {
        reconnect_pending = false;
        reconnect_attempts++;
        reconnect_delay = MIN(reconnect_delay * 2, RECONNECT_MAX_MS);
        ble_cmdq->put(CONNECT);
        snowfox_ble_stats.reconnects++;
    }

    while(!ble_cmdq->is_empty()) {
        if (ack_pass()) {
            dispatch_command(ble_cmdq);
        } else {
            break;
        }
    }

    while(1) {
        msg_t raw = sdGetTimeout(&SD1, TIME_IMMEDIATE);

        if (raw == MSG_TIMEOUT || raw == MSG_RESET) {
            break;
        }

        if (raw == '\r' || raw == '\n') {
            if (p_write > uart_rx_buffer) {
                process_response(uart_rx_buffer);

                memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
                p_write = uart_rx_buffer;
            }
        } else if (p_write < uart_rx_buffer + sizeof(uart_rx_buffer) - 1) {
            *p_write++ = (char) raw;
        }
    }
}

bool ble_custom_is_connected(void) {
    return (ble_handle.state == CONNECTED);
}

void ble_custom_send_keyboard(report_keyboard_t *report) {
    bool released = (last_report.mods & ~report->mods) != 0;
    for (uint8_t i = 0; i < KEYBOARD_REPORT_KEYS && !released; i++) {
        if (last_report.keys[i] != 0 && memchr(report->keys, last_report.keys[i], KEYBOARD_REPORT_KEYS) == NULL) {
            released = true;
        }
    }

    ble_send_keyboard_report(report);
    memcpy(&last_report, report, sizeof(last_report));
    repeat_pending = released;
    repeat_from    = timer_read();
}

void ble_custom_send_mouse(report_mouse_t *report) {}

static void ble_send_consumer_report(uint16_t usage) {
    uint8_t report[2] = {usage & 0xFF, usage >> 8};
    sdWrite(&SD1, (uint8_t*) "AT+HID=\2", 8);
    sdWrite(&SD1, report, sizeof(report));
}

// Press and release go out together, and the release once more a little later.
void ble_custom_send_consumer(uint16_t usage) {
    ble_send_consumer_report(usage);
    ble_send_consumer_report(0);
    consumer_repeat_pending = true;
    consumer_repeat_from    = timer_read();
}


static void ble_send_system_report(uint8_t usage) {
    sdWrite(&SD1, (uint8_t*) "AT+HID=\3", 8);
    sdWrite(&SD1, &usage, 1);
}

// The module treats both Power (0x81) and Sleep (0x82) as Sleep. The release
// follows 30 ms later from the task, and is sent twice.
void ble_custom_send_system(uint16_t usage) {
    ble_send_system_report((usage == 0x81 || usage == 0x82) ? 0x81 : 0);
    system_release_step = 1;
    system_release_from = timer_read();
}

/* -------------------- QMK Bluetooth driver hooks -------------------------- */
void bluetooth_init(void) {
    ble_custom_init();
}

void bluetooth_task(void) {
    ble_custom_task();
}

bool bluetooth_is_connected(void) {
    return ble_custom_is_connected();
}

uint8_t bluetooth_keyboard_leds(void) {
    return ble_handle.led_page;
}

void bluetooth_send_keyboard(report_keyboard_t *report) {
    ble_custom_send_keyboard(report);
}

void bluetooth_send_consumer(uint16_t usage) {
    if (usage != 0) {
        ble_custom_send_consumer(usage);
    }
}

void bluetooth_send_system(uint16_t usage) {
    if (usage != 0) {
        ble_custom_send_system(usage);
    }
}

// Sends one AT command and collects every reply line for timeout_ms, separated by '|'.
void snowfox_ble_query(const char *command, char *out, uint8_t size, uint16_t timeout_ms) {
    query_out  = out;
    query_size = size;
    query_len  = 0;
    out[0]     = '\0';

    ble_command(command);
    uint16_t start = timer_read();
    while (timer_elapsed(start) < timeout_ms) {
        ble_custom_task();
    }
    query_out = NULL;
}

const ble_handle_t *snowfox_ble_handle(void) {
    return &ble_handle;
}

// After automatic reconnecting has given up, typing on battery tries again, at most
// every 10 seconds, as Keychron's wireless firmware does. Not after a requested disconnect.
#define KEYPRESS_RECONNECT_MS 10000

void snowfox_ble_key_pressed(void) {
    static uint32_t last_try;
    snowfox_config_t config = snowfox_config_read();

    if (ble_handle.state != STANDBY || user_disconnected || reconnect_pending || config.ble_slot == 0 || usb_connected_state()) {
        return;
    }
    if (last_try != 0 && timer_elapsed32(last_try) < KEYPRESS_RECONNECT_MS) {
        return;
    }
    last_try = timer_read32();
    stop_reconnect();
    ble_cmdq->put(CONNECT);
    snowfox_ble_stats.reconnects++;
}

/**
 * @file main.c
 * @brief 主板：1 主机 + 2 从板，每从板 8 路
 *
 * CAN DATA 前部位段（控制帧 DLC=8）：
 *   data[0] = bits 0–7   → N1（键 1–8）
 *   data[1] = bits 8–15  → N2（键 9–16）
 *   data[2] = 0          → 预留 N3
 *   data[3] = 0          → 预留 N4
 *   data[4..]            → 类型标记 / 序号（不参与开关）
 *
 * 两从板均接收同一帧，各自只取 1 个字节作为 8 路命令。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/twai.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#define CAN_TX_GPIO              GPIO_NUM_9
#define CAN_RX_GPIO              GPIO_NUM_10

#define CAN_ID_ALL_OFF           0x000
#define CAN_ID_HOST_BROADCAST    0x100
#define CAN_ID_REPORT_N1         0x201
#define CAN_ID_REPORT_N2         0x202

#define MSG_DISCOVER             0x01
#define MSG_SYNC                 0x02
#define MSG_KEY_STATE            0x03
#define MSG_SYSTEM_CTRL          0x04
#define MSG_ALL_OFF              0x05
#define MSG_DEVICE_REPORT        0x81

#define SYS_CMD_STOP             0
#define SYS_CMD_START            1
#define SYS_CMD_PAUSE            2

#define MAGNET_COUNT             16   /* N1:1-8  N2:9-16 */
#define CHANNELS_PER_NODE        8
#define ACTIVE_NODES             2

#if defined(CONFIG_ESP_CONSOLE_UART_NUM)
#define CMD_UART_NUM             CONFIG_ESP_CONSOLE_UART_NUM
#else
#define CMD_UART_NUM             UART_NUM_0
#endif

static const char *TAG = "CAN_HOST";

/** bit0=键1 … bit7=键8(N1)；bit8=键9 … bit15=键16(N2) */
static uint16_t s_key_state = 0;
static uint8_t s_msg_seq = 0;
static uint8_t s_discover_seq = 0;
static uint8_t s_play_seq = 0;
static bool s_playing = false;

static uint8_t next_seq(void)
{
    return ++s_msg_seq;
}

static esp_err_t cmd_uart_init(void)
{
    esp_err_t err = uart_driver_install(CMD_UART_NUM, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "UART install fail: %s", esp_err_to_name(err));
        return err;
    }
    esp_vfs_dev_uart_use_driver(CMD_UART_NUM);
    return ESP_OK;
}

static int cmd_uart_read_line(char *buf, size_t buf_len)
{
    size_t idx = 0;
    while (idx < buf_len - 1) {
        uint8_t ch = 0;
        int n = uart_read_bytes(CMD_UART_NUM, &ch, 1, pdMS_TO_TICKS(500));
        if (n != 1) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (idx == 0) {
                continue;
            }
            buf[idx] = '\0';
            printf("\r\n");
            fflush(stdout);
            return (int)idx;
        }
        if (ch == 0x08 || ch == 0x7F) {
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (ch < 0x20) {
            continue;
        }
        buf[idx++] = (char)ch;
        printf("%c", ch);
        fflush(stdout);
    }
    buf[buf_len - 1] = '\0';
    printf("\r\n");
    return (int)(buf_len - 1);
}

static void print_frame(const char *dir, const twai_message_t *m)
{
    printf("%s ID=0x%03" PRIX32 " DLC=%u DATA=",
           dir, m->identifier, m->data_length_code);
    for (int i = 0; i < m->data_length_code; i++) {
        printf("%02X ", m->data[i]);
    }
    printf("\n");
}

static esp_err_t can_init(void)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    g.tx_queue_len = 16;
    g.rx_queue_len = 16;
    g.alerts_enabled = TWAI_ALERT_NONE;

    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) {
        return err;
    }
    err = twai_start();
    if (err != ESP_OK) {
        twai_driver_uninstall();
        return err;
    }
    return ESP_OK;
}

static esp_err_t can_send(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    twai_message_t msg = {0};
    msg.identifier = id;
    msg.data_length_code = dlc;
    memcpy(msg.data, data, dlc);

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TX fail: %s", esp_err_to_name(err));
        return err;
    }
    print_frame("[CAN TX]", &msg);
    return ESP_OK;
}

/**
 * 广播 16 键状态：每从板 1 字节（8 路）。
 * data[0]=N1，data[1]=N2；两板同收一帧，各取 1 字节。
 */
static esp_err_t broadcast_key_state(void)
{
    uint8_t n1 = (uint8_t)(s_key_state & 0xFFu);
    uint8_t n2 = (uint8_t)((s_key_state >> 8) & 0xFFu);

    uint8_t data[8] = {
        n1,
        n2,
        0x00,            /* N3 预留 */
        0x00,            /* N4 预留 */
        MSG_KEY_STATE,
        next_seq(),
        0x00,
        0x00
    };

    ESP_LOGI(TAG, "KEY=0x%04X N1=0x%02X N2=0x%02X", s_key_state, n1, n2);
    return can_send(CAN_ID_HOST_BROADCAST, data, 8);
}

static esp_err_t broadcast_all_off(void)
{
    s_key_state = 0;
    uint8_t data[2] = {MSG_ALL_OFF, next_seq()};
    return can_send(CAN_ID_ALL_OFF, data, 2);
}

static esp_err_t broadcast_discover(void)
{
    s_discover_seq++;
    uint8_t data[2] = {MSG_DISCOVER, s_discover_seq};
    return can_send(CAN_ID_HOST_BROADCAST, data, 2);
}

static esp_err_t broadcast_sync(void)
{
    uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint8_t data[6] = {
        MSG_SYNC,
        next_seq(),
        (uint8_t)(ms & 0xFF),
        (uint8_t)((ms >> 8) & 0xFF),
        (uint8_t)((ms >> 16) & 0xFF),
        (uint8_t)((ms >> 24) & 0xFF)
    };
    return can_send(CAN_ID_HOST_BROADCAST, data, 6);
}

static esp_err_t broadcast_system_ctrl(uint8_t cmd)
{
    if (cmd == SYS_CMD_START) {
        s_playing = true;
        s_play_seq++;
    } else if (cmd == SYS_CMD_STOP) {
        s_playing = false;
    }
    uint8_t data[3] = {MSG_SYSTEM_CTRL, cmd, s_play_seq};
    return can_send(CAN_ID_HOST_BROADCAST, data, 3);
}

static void collect_reports_ms(uint32_t wait_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(wait_ms);
    while (xTaskGetTickCount() < deadline) {
        twai_message_t rx = {0};
        TickType_t left = deadline - xTaskGetTickCount();
        if (left == 0) {
            break;
        }
        if (twai_receive(&rx, left) != ESP_OK) {
            break;
        }
        if (rx.extd || rx.rtr) {
            continue;
        }
        if (rx.identifier >= CAN_ID_REPORT_N1 &&
            rx.identifier <= CAN_ID_REPORT_N2) {
            print_frame("[CAN RX]", &rx);
            if (rx.data_length_code >= 6 && rx.data[0] == MSG_DEVICE_REPORT) {
                printf("DEVICE_REPORT NODE=%u CH=%u VER=%u.%u\r\n",
                       rx.data[1], rx.data[2], rx.data[3], rx.data[4]);
            }
        }
    }
}

static void trim_line(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' ||
                     line[n - 1] == ' ' || line[n - 1] == '\t')) {
        line[--n] = '\0';
    }
    char *s = line;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (s != line) {
        memmove(line, s, strlen(s) + 1);
    }
    for (char *p = line; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
}

static void print_help(void)
{
    printf(
        "\r\n=== 主机：每从板8路（键1-8=N1, 9-16=N2）===\r\n"
        "DATA: data[0]=N1, data[1]=N2（同帧广播）\r\n"
        "  ON/OFF/SET <n>...   n=1..16\r\n"
        "  ALL_OFF  DISCOVER  SYNC  START/STOP/PAUSE\r\n"
        "  STATUS  HELP\r\n\r\n"
    );
}

static void print_status(void)
{
    printf("KEY=0x%04X N1=0x%02X N2=0x%02X\r\n",
           s_key_state,
           (unsigned)(s_key_state & 0xFF),
           (unsigned)((s_key_state >> 8) & 0xFF));
    printf("ON:");
    int any = 0;
    for (int i = 0; i < MAGNET_COUNT; i++) {
        if (s_key_state & (1u << i)) {
            printf(" %d", i + 1);
            any = 1;
        }
    }
    if (!any) {
        printf(" (none)");
    }
    printf("\r\n");
}

static int parse_key_list(char *args, int *ids, int max_ids)
{
    int count = 0;
    char *token = strtok(args, " ,;\t");
    while (token) {
        char *end = NULL;
        long v = strtol(token, &end, 10);
        if (end == token || *end || v < 1 || v > MAGNET_COUNT) {
            printf("ERR id 1..%d got '%s'\r\n", MAGNET_COUNT, token);
            return -1;
        }
        if (count >= max_ids) {
            return -1;
        }
        ids[count++] = (int)v;
        token = strtok(NULL, " ,;\t");
    }
    return count > 0 ? count : -1;
}

static void handle_line(char *line)
{
    trim_line(line);
    if (!line[0]) {
        return;
    }
    if (!strcmp(line, "HELP") || !strcmp(line, "?")) {
        print_help();
        return;
    }
    if (!strcmp(line, "STATUS")) {
        print_status();
        return;
    }
    if (!strcmp(line, "ALL_OFF")) {
        broadcast_all_off();
        print_status();
        return;
    }
    if (!strcmp(line, "DISCOVER")) {
        broadcast_discover();
        collect_reports_ms(500);
        return;
    }
    if (!strcmp(line, "SYNC")) {
        broadcast_sync();
        return;
    }
    if (!strcmp(line, "START")) {
        broadcast_system_ctrl(SYS_CMD_START);
        return;
    }
    if (!strcmp(line, "STOP")) {
        broadcast_system_ctrl(SYS_CMD_STOP);
        return;
    }
    if (!strcmp(line, "PAUSE")) {
        broadcast_system_ctrl(SYS_CMD_PAUSE);
        return;
    }

    int ids[MAGNET_COUNT];
    int count;
    if (!strncmp(line, "ON ", 3)) {
        count = parse_key_list(line + 3, ids, MAGNET_COUNT);
        if (count < 0) {
            return;
        }
        for (int i = 0; i < count; i++) {
            s_key_state |= (uint16_t)(1u << (ids[i] - 1));
        }
        broadcast_key_state();
        print_status();
        return;
    }
    if (!strncmp(line, "OFF ", 4)) {
        count = parse_key_list(line + 4, ids, MAGNET_COUNT);
        if (count < 0) {
            return;
        }
        for (int i = 0; i < count; i++) {
            s_key_state &= (uint16_t)~(1u << (ids[i] - 1));
        }
        broadcast_key_state();
        print_status();
        return;
    }
    if (!strncmp(line, "SET ", 4)) {
        count = parse_key_list(line + 4, ids, MAGNET_COUNT);
        if (count < 0) {
            return;
        }
        s_key_state = 0;
        for (int i = 0; i < count; i++) {
            s_key_state |= (uint16_t)(1u << (ids[i] - 1));
        }
        broadcast_key_state();
        print_status();
        return;
    }
    printf("ERR unknown, type HELP\r\n");
}

void app_main(void)
{
    if (can_init() != ESP_OK || cmd_uart_init() != ESP_OK) {
        return;
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    ESP_LOGI(TAG, "Host: 2 slaves x %d ch, data[0]=N1 data[1]=N2",
             CHANNELS_PER_NODE);
    print_help();

    broadcast_discover();
    collect_reports_ms(400);
    broadcast_sync();

    char line[128];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (cmd_uart_read_line(line, sizeof(line)) <= 0) {
            continue;
        }
        handle_line(line);
    }
}

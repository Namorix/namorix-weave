/*
 * Frame TCP transport: BR listens on port (CONFIG_BR_FRAME_TCP_PORT), accepts 1 client,
 * RX loop parses frames (block-copy, no per-byte memmove) then dispatches to the command
 * table, sends frames via single-buffer (inline CRC, no malloc), state watchdog
 * (no CMD_STATE from backend within N intervals -> esp_restart).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sdkconfig.h"

#include "frame/frame.h"
#include "frame_tcp.h"
#include "command.h"

#define TAG "frame_tcp"

#define LISTEN_BACKLOG        1
#define RX_READ_TIMEOUT_MS    50
#define RECONNECT_DELAY_MS    500
#define ACCEPT_RETRY_MS       200

#define SEND_MUTEX_TIMEOUT_MS 100
#define SEND_EAGAIN_RETRY_MS  5
#define SEND_EAGAIN_MAX_MS    50

#define STATE_WATCHDOG_INTERVAL_MS (15 * 1000)
#define STATE_WATCHDOG_MAX_MISS    5

#define TASK_NAME_TCP_ACCEPT "tcp_accept"
#define TASK_NAME_TCP_RX     "tcp_rx"
#define TASK_NAME_STATE_WD   "state_wd"
#define TASK_STACK_TCP_ACCEPT 4096
#define TASK_STACK_TCP_RX     4096
#define TASK_STACK_STATE_WD   3072
#define TASK_PRIO_ACCEPT      4
#define TASK_PRIO_RX          5
#define TASK_PRIO_STATE_WD    3

#define RX_BUF_SIZE FRAME_MAX_BUFFER

static SemaphoreHandle_t s_fd_mutex;   /* protects s_client_fd */
static SemaphoreHandle_t s_tx_mutex;   /* protects s_tx_buf */
static int s_client_fd = -1;
static int s_listen_fd = -1;
static volatile bool s_inited = false;
static volatile bool s_state_received = false;

static uint8_t s_tx_buf[FRAME_MAX_BUFFER];
static uint8_t s_rx_buf[RX_BUF_SIZE];
static size_t s_rx_len = 0;

/* ---- socket access (single owner, mutex-guarded) ---- */

static void close_client(void)
{
    if (s_fd_mutex == NULL || xSemaphoreTake(s_fd_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (s_client_fd >= 0) {
        shutdown(s_client_fd, SHUT_RDWR);
        close(s_client_fd);
        s_client_fd = -1;
    }
    xSemaphoreGive(s_fd_mutex);
}

static int current_client_fd(void)
{
    int fd = -1;
    if (s_fd_mutex != NULL && xSemaphoreTake(s_fd_mutex, portMAX_DELAY) == pdTRUE) {
        fd = s_client_fd;
        xSemaphoreGive(s_fd_mutex);
    }
    return fd;
}

static void set_client(int fd)
{
    if (s_fd_mutex == NULL || xSemaphoreTake(s_fd_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (s_client_fd >= 0) {
        shutdown(s_client_fd, SHUT_RDWR);
        close(s_client_fd);
    }
    s_client_fd = fd;
    xSemaphoreGive(s_fd_mutex);
}

/* ---- RX parse (block-copy, resync 1 byte on SOF/CRC/EOF error) ---- */

static void rx_parse_and_dispatch(void)
{
    for (;;) {
        if (s_rx_len == 0) {
            return;
        }
        if (s_rx_buf[0] != FRAME_SOF) {
            memmove(s_rx_buf, s_rx_buf + 1, s_rx_len - 1);
            s_rx_len--;
            continue;
        }
        if (s_rx_len < FRAME_FIXED_OVERHEAD) {
            return; /* not enough for minimum header + CRC + EOF */
        }
        uint16_t frame_len = ((uint16_t)s_rx_buf[3] << 8) | s_rx_buf[4];
        if (frame_len > FRAME_MAX_DATA_LEN) {
            ESP_LOGW(TAG, "invalid LEN=%u, drop and resync", (unsigned)frame_len);
            memmove(s_rx_buf, s_rx_buf + 1, s_rx_len - 1);
            s_rx_len--;
            continue;
        }
        size_t total = FRAME_FIXED_OVERHEAD + frame_len;
        if (s_rx_len < total) {
            return; /* not enough for the full frame */
        }
        size_t data_offset = FRAME_HEADER_LEN + 1; /* 5: DATA start */
        uint8_t crc = frame_crc8(&s_rx_buf[1], FRAME_HEADER_LEN + frame_len);
        if (crc != s_rx_buf[data_offset + frame_len] || s_rx_buf[data_offset + frame_len + 1] != FRAME_EOF) {
            ESP_LOGW(TAG, "bad crc/eof len=%u, drop and resync", (unsigned)frame_len);
            memmove(s_rx_buf, s_rx_buf + 1, s_rx_len - 1);
            s_rx_len--;
            continue;
        }
        frame_t f = {
            .frame_id = s_rx_buf[1],
            .cmd = s_rx_buf[2],
            .len = frame_len,
            .data = &s_rx_buf[data_offset],
        };
        ESP_LOGD(TAG, "rx frame_id=%u cmd=%s len=%u",
                 (unsigned)f.frame_id, frame_cmd_name(f.cmd), (unsigned)f.len);
        (void)command_dispatch(&f);
        size_t remain = s_rx_len - total;
        memmove(s_rx_buf, s_rx_buf + total, remain);
        s_rx_len = remain;
    }
}

static void rx_append(const uint8_t *data, size_t len)
{
    while (len > 0) {
        if (s_rx_len >= RX_BUF_SIZE) {
            memmove(s_rx_buf, s_rx_buf + 1, s_rx_len - 1);
            s_rx_len--;
            continue;
        }
        size_t n = (len < RX_BUF_SIZE - s_rx_len) ? len : (RX_BUF_SIZE - s_rx_len);
        memcpy(&s_rx_buf[s_rx_len], data, n);
        s_rx_len += n;
        data += n;
        len -= n;
        rx_parse_and_dispatch();
    }
}

/* ---- RX task ---- */

static void tcp_rx_task(void *pv)
{
    (void)pv;
    uint8_t tmp[256];
    for (;;) {
        int fd = current_client_fd();
        if (fd < 0) {
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            continue;
        }
        int n = recv(fd, tmp, sizeof(tmp), 0);
        if (n > 0) {
            rx_append(tmp, (size_t)n);
            continue;
        }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            ESP_LOGW(TAG, "client disconnect");
            close_client();
        } else {
            vTaskDelay(pdMS_TO_TICKS(RX_READ_TIMEOUT_MS));
        }
    }
}

/* ---- accept task ---- */

static void accept_task(void *pv)
{
    (void)pv;
    const int port = CONFIG_BR_FRAME_TCP_PORT;
    struct sockaddr_in listen_addr = { 0 };
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons((uint16_t)port);

    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        ESP_LOGE(TAG, "socket failed %d", errno);
        s_inited = true;
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    if (setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ESP_LOGW(TAG, "setsockopt SO_REUSEADDR %d", errno);
    }
    if (bind(s_listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        ESP_LOGE(TAG, "bind port %d failed %d", port, errno);
        close(s_listen_fd);
        s_listen_fd = -1;
        s_inited = true;
        vTaskDelete(NULL);
        return;
    }
    if (listen(s_listen_fd, LISTEN_BACKLOG) < 0) {
        ESP_LOGE(TAG, "listen failed %d", errno);
        close(s_listen_fd);
        s_listen_fd = -1;
        s_inited = true;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "listen on port %d", port);
    s_inited = true;

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = accept(s_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(ACCEPT_RETRY_MS));
            continue;
        }
        int flags = fcntl(client, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(client, F_SETFL, flags | O_NONBLOCK);
        }
        set_client(client); /* close the old client if any */
        ESP_LOGI(TAG, "client connected");
    }
}

/* ---- frame_send: single-buffer, inline CRC, no malloc ---- */

esp_err_t frame_send(uint8_t frame_id, uint8_t cmd, const uint8_t *data, size_t len)
{
    if (len > FRAME_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_tx_mutex == NULL || xSemaphoreTake(s_tx_mutex, pdMS_TO_TICKS(SEND_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT; /* socket/buffer busy */
    }
    int fd = current_client_fd();
    if (fd < 0) {
        xSemaphoreGive(s_tx_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    s_tx_buf[0] = FRAME_SOF;
    s_tx_buf[1] = frame_id;
    s_tx_buf[2] = cmd;
    s_tx_buf[3] = (uint8_t)((len >> 8) & 0xFF);
    s_tx_buf[4] = (uint8_t)(len & 0xFF);
    if (len > 0 && data != NULL) {
        memcpy(&s_tx_buf[FRAME_HEADER_LEN + 1], data, len);
    }
    s_tx_buf[FRAME_HEADER_LEN + 1 + len] = frame_crc8(&s_tx_buf[1], FRAME_HEADER_LEN + len);
    s_tx_buf[FRAME_HEADER_LEN + 2 + len] = FRAME_EOF;
    size_t total = FRAME_FIXED_OVERHEAD + len;

    size_t off = 0;
    int retry_ms = 0;
    while (off < total) {
        ssize_t n = send(fd, &s_tx_buf[off], total - off, 0);
        if (n > 0) {
            off += (size_t)n;
            retry_ms = 0;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (retry_ms >= SEND_EAGAIN_MAX_MS) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(SEND_EAGAIN_RETRY_MS));
            retry_ms += SEND_EAGAIN_RETRY_MS;
        } else {
            break;
        }
    }
    xSemaphoreGive(s_tx_mutex);

    if (off != total) {
        ESP_LOGW(TAG, "tcp tx short %u/%u cmd=%s",
                 (unsigned)off, (unsigned)total, frame_cmd_name(cmd));
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "tx frame_id=%u cmd=%s len=%u",
             (unsigned)frame_id, frame_cmd_name(cmd), (unsigned)len);
    return ESP_OK;
}

/* ---- state watchdog ---- */

void frame_tcp_state_mark_received(void)
{
    s_state_received = true;
}

static void state_watchdog_task(void *pv)
{
    (void)pv;
    uint32_t miss = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STATE_WATCHDOG_INTERVAL_MS));
        if (s_state_received) {
            s_state_received = false;
            miss = 0;
        } else if (++miss >= STATE_WATCHDOG_MAX_MISS) {
            ESP_LOGW(TAG, "no state from backend in %u intervals, restarting", (unsigned)miss);
            esp_restart();
        }
    }
}

/* ---- init ---- */

esp_err_t frame_tcp_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    s_fd_mutex = xSemaphoreCreateMutex();
    s_tx_mutex = xSemaphoreCreateMutex();
    if (s_fd_mutex == NULL || s_tx_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_client_fd = -1;
    s_listen_fd = -1;
    s_rx_len = 0;
    s_state_received = false;

    if (xTaskCreate(accept_task, TASK_NAME_TCP_ACCEPT, TASK_STACK_TCP_ACCEPT, NULL,
                    TASK_PRIO_ACCEPT, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    while (!s_inited) {
        vTaskDelay(pdMS_TO_TICKS(20)); /* wait for bind/listen to finish */
    }
    if (s_listen_fd < 0) {
        return ESP_FAIL;
    }
    if (xTaskCreate(tcp_rx_task, TASK_NAME_TCP_RX, TASK_STACK_TCP_RX, NULL,
                    TASK_PRIO_RX, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(state_watchdog_task, TASK_NAME_STATE_WD, TASK_STACK_STATE_WD, NULL,
                    TASK_PRIO_STATE_WD, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "frame TCP transport init OK (port %d)", CONFIG_BR_FRAME_TCP_PORT);
    return ESP_OK;
}

/* ---- frame_cmd_name ---- */

const char *frame_cmd_name(uint8_t cmd)
{
    static char buf[8];
    switch (cmd) {
    case CMD_DATA: return "DATA";
    case CMD_ACK: return "ACK";
    case CMD_NACK: return "NACK";
    case CMD_RESET: return "RESET";
    case CMD_FACTORY: return "FACTORY";
    case CMD_STATE: return "STATE";
    case CMD_IP_ADDR: return "IP_ADDR";
    case CMD_DATASET_ACTIVE: return "DATASET_ACTIVE";
    case CMD_DATASET_COMMIT_ACTIVE: return "DATASET_COMMIT_ACTIVE";
    case CMD_MAC_ADDRESS: return "MAC_ADDRESS";
    case CMD_BR_HEALTH: return "BR_HEALTH";
    case CMD_SET_PANID: return "SET_PANID";
    case CMD_SET_CHANNEL: return "SET_CHANNEL";
    case CMD_SET_NETWORK_NAME: return "SET_NETWORK_NAME";
    case CMD_SET_EXTENDED_PANID: return "SET_EXTENDED_PANID";
    case CMD_SET_NETWORK_KEY: return "SET_NETWORK_KEY";
    case CMD_ROUTER_TABLE: return "ROUTER_TABLE";
    case CMD_CHILD_TABLE: return "CHILD_TABLE";
    case CMD_JOINER_TABLE: return "JOINER_TABLE";
    case CMD_THREAD_START: return "THREAD_START";
    case CMD_THREAD_STOP: return "THREAD_STOP";
    case CMD_THREAD_VERSION: return "THREAD_VERSION";
    case CMD_COMMISSIONER_JOINER: return "COMMISSIONER_JOINER";
    case CMD_SRP_REGISTER: return "SRP_REGISTER";
    case CMD_NOTIFY: return "NOTIFY";
    default:
        snprintf(buf, sizeof(buf), "0x%02x", cmd);
        return buf;
    }
}

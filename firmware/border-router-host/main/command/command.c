/*
 * Command dispatch table. Plan 05: every command (except STATE) returns NACK not-ready
 * because OpenThread/boot is not set up yet (Plan 07). STATE is the backend keepalive:
 * ACK + mark for the state watchdog in the transport.
 */

#include <stddef.h>

#include "esp_log.h"

#include "frame/frame.h"
#include "command.h"
#include "frame_tcp.h"

#define TAG "command"

static int command_nack_not_ready(frame_t *frame)
{
    uint8_t nack = FRAME_NACK_NOT_READY;
    return frame_send(frame->frame_id, CMD_NACK, &nack, 1);
}

static int command_handle_state(frame_t *frame)
{
    frame_tcp_state_mark_received();
    return frame_send(frame->frame_id, CMD_ACK, NULL, 0);
}

#define COMMAND_STUB(name) static int command_handle_##name(frame_t *frame) \
    { \
        return command_nack_not_ready(frame); \
    }

COMMAND_STUB(reset)
COMMAND_STUB(factory)
COMMAND_STUB(ipaddr)
COMMAND_STUB(dataset_active)
COMMAND_STUB(dataset_commit_active)
COMMAND_STUB(mac_address)
COMMAND_STUB(br_health)
COMMAND_STUB(router_table)
COMMAND_STUB(child_table)
COMMAND_STUB(joiner_table)
COMMAND_STUB(set_panid)
COMMAND_STUB(set_channel)
COMMAND_STUB(set_network_name)
COMMAND_STUB(set_extended_panid)
COMMAND_STUB(set_network_key)
COMMAND_STUB(thread_start)
COMMAND_STUB(thread_stop)
COMMAND_STUB(thread_version)
COMMAND_STUB(commissioner_joiner)
COMMAND_STUB(srp_register)

static const command_handler_entry_t s_handlers[] = {
    { CMD_STATE, command_handle_state },
    { CMD_RESET, command_handle_reset },
    { CMD_FACTORY, command_handle_factory },
    { CMD_IP_ADDR, command_handle_ipaddr },
    { CMD_DATASET_ACTIVE, command_handle_dataset_active },
    { CMD_DATASET_COMMIT_ACTIVE, command_handle_dataset_commit_active },
    { CMD_MAC_ADDRESS, command_handle_mac_address },
    { CMD_BR_HEALTH, command_handle_br_health },
    { CMD_ROUTER_TABLE, command_handle_router_table },
    { CMD_CHILD_TABLE, command_handle_child_table },
    { CMD_JOINER_TABLE, command_handle_joiner_table },
    { CMD_SET_PANID, command_handle_set_panid },
    { CMD_SET_CHANNEL, command_handle_set_channel },
    { CMD_SET_NETWORK_NAME, command_handle_set_network_name },
    { CMD_SET_EXTENDED_PANID, command_handle_set_extended_panid },
    { CMD_SET_NETWORK_KEY, command_handle_set_network_key },
    { CMD_THREAD_START, command_handle_thread_start },
    { CMD_THREAD_STOP, command_handle_thread_stop },
    { CMD_THREAD_VERSION, command_handle_thread_version },
    { CMD_COMMISSIONER_JOINER, command_handle_commissioner_joiner },
    { CMD_SRP_REGISTER, command_handle_srp_register },
};

esp_err_t command_dispatch(const frame_t *frame)
{
    for (size_t i = 0; i < sizeof(s_handlers) / sizeof(s_handlers[0]); i++) {
        if (s_handlers[i].cmd == frame->cmd) {
            int rc = s_handlers[i].handler(frame);
            if (rc != 0) {
                ESP_LOGW(TAG, "%s handle failed rc=%d", frame_cmd_name(frame->cmd), rc);
            }
            return (rc == 0) ? ESP_OK : ESP_FAIL;
        }
    }
    ESP_LOGW(TAG, "unknown cmd 0x%02x frame_id=%u", frame->cmd, (unsigned)frame->frame_id);
    uint8_t nack = FRAME_NACK_INVALID_CMD;
    (void)frame_send(frame->frame_id, CMD_NACK, &nack, 1);
    return ESP_OK;
}

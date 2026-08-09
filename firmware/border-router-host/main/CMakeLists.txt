idf_component_register(
  SRCS "br_rcp_ctrl.c" "br_main.c" "br_launch.c" "br_console.c"
  "backhaul/eth_w5500.c"
  "hardware/led_status.c"
  "hardware/boot_btn.c"
  "communicate/communicate.c"
  "communicate/communicate_command.c"
  "communicate/communicate_queue.c"
  "communicate/communicate_task.c"
  "communicate/transport_tcp.c"
  "openthread/dataset_init.c"
  "openthread/ot_table_snapshot.c"
  "openthread/ot_change_detector.c"
  INCLUDE_DIRS "../include"
  REQUIRES mdns esp_timer
  PRIV_REQUIRES console cmd_system openthread nvs_flash driver esp_eth
)

idf_component_register(
  SRCS "main.c" "command/command.c" "transport/frame_tcp.c" "backhaul/eth_w5500.c"
       "console/console.c" "rcp/rcp_ctrl.c"
       "openthread/ot_launch.c" "openthread/dataset_init.c"
       "openthread/ot_table_snapshot.c" "openthread/ot_change_detector.c"
       "hardware/led_status.c" "hardware/boot_btn.c"
  INCLUDE_DIRS "../include" "command" "transport" "backhaul"
  REQUIRES esp_netif nvs_flash lwip esp_eth driver mdns esp_timer
           console cmd_system openthread esp_hw_support
)

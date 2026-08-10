idf_component_register(
  SRCS "main.c" "command/command.c" "transport/frame_tcp.c"
  INCLUDE_DIRS "../include" "command" "transport"
  REQUIRES esp_netif nvs_flash lwip
)

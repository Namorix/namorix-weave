/*
 * Console: esp_console REPL (UART) + system commands (debug CLI).
 */

#include "esp_console.h"
#include "cmd_system.h"

#include "console/console.h"

static esp_console_repl_t *s_repl = NULL;

void console_start(void)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CONFIG_IDF_TARGET ">";
    repl_config.max_cmdline_length = 256;
    repl_config.max_history_len = 10;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &s_repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &s_repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &s_repl));
#else
#error Unsupported console type
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
}

void console_stop(void)
{
    ESP_ERROR_CHECK(esp_console_stop_repl(s_repl));
}

void console_register_system(void)
{
    register_system();
}

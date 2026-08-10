/*
 * Console: esp_console REPL (UART/USB) + system commands. Debug CLI.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the REPL (esp_console) on the configured console device. */
void console_start(void);

/* Stop the REPL. */
void console_stop(void);

/* Register the system commands (version/restart/free/heap/log_level/sleep). */
void console_register_system(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_H */

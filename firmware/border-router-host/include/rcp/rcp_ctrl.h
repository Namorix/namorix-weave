/*
 * RCP control: drive the RESET/BOOT pins of the ESP32-H2 RCP from the ESP32-S3 host.
 */

#ifndef RCP_CTRL_H
#define RCP_CTRL_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configure GPIO for the RCP RESET/BOOT pins. */
esp_err_t rcp_ctrl_init(void);

/* Reset the RCP: RESET LOW -> 10ms -> RESET HIGH -> 30ms. */
void rcp_ctrl_reset(void);

/* Enter RCP download mode (for flashing): BOOT LOW, RESET pulse; keep BOOT LOW while flashing. */
void rcp_ctrl_enter_download_mode(void);

/* Exit RCP download mode (normal boot): BOOT HIGH, RESET pulse. */
void rcp_ctrl_exit_download_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* RCP_CTRL_H */

/*
 * Dataset init: on boot check the active dataset; if none, create and commit one.
 */

#ifndef OPENTHREAD_DATASET_INIT_H
#define OPENTHREAD_DATASET_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Call after esp_openthread_start() is done. Checks the active dataset
 * (otDatasetGetActiveTlvs); if missing or invalid, creates a new dataset
 * (otDatasetCreateNewNetwork), sets it active and commits to NVS.
 */
void dataset_init_on_boot(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_DATASET_INIT_H */

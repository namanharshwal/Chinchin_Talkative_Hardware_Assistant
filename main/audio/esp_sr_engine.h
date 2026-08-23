#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_sr_engine_init(void);
void esp_sr_engine_start_wwe(void);
void esp_sr_engine_stop_wwe(void);
void esp_sr_register_wakeup_callback(void (*cb)(void));

#ifdef __cplusplus
}
#endif

#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t power_monitor_init(void);
int power_monitor_get_battery_level(void);
void power_monitor_enter_sleep(void);

#ifdef __cplusplus
}
#endif

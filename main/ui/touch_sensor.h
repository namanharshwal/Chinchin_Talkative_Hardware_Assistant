#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the TTP224 touch sensor task
esp_err_t touch_sensor_init(void);

#ifdef __cplusplus
}
#endif

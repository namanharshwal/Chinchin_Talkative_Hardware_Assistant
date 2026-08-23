#pragma once
#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t device_control_init(void);
esp_err_t device_control_set_gpio(int pin, int level);
esp_err_t device_control_set_pwm(int channel, uint32_t duty);

#ifdef __cplusplus
}
#endif

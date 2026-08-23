#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mqtt_udp_transport_init(void);
esp_err_t mqtt_publish(const char *topic, const char *data);

#ifdef __cplusplus
}
#endif

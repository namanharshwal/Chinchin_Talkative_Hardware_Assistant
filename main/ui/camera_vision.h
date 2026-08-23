#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t camera_vision_init(void);
esp_err_t camera_vision_capture_frame(uint8_t **out_buf, size_t *out_len);

#ifdef __cplusplus
}
#endif

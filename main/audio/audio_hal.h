#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_hal_init(void);
esp_err_t audio_hal_start_mic(void);
esp_err_t audio_hal_stop_mic(void);
esp_err_t audio_hal_write_speaker(const uint8_t *data, size_t size);
esp_err_t audio_hal_flush_speaker(void);
esp_err_t audio_hal_read_mic(uint8_t *data, size_t size, size_t *bytes_read);

void audio_hal_set_volume(float vol);
float audio_hal_get_volume(void);

#ifdef __cplusplus
}
#endif

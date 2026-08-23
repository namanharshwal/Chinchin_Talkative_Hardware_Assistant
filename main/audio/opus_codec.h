#pragma once
#include <stdint.h>
#include <stddef.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t opus_codec_init(void);
esp_err_t opus_encode_audio(const int16_t *pcm_data, int frame_size, uint8_t *opus_data, int *out_len);
esp_err_t opus_decode_audio(const uint8_t *opus_data, int len, int16_t *pcm_data, int *out_frame_size);

#ifdef __cplusplus
}
#endif

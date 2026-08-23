#include "opus_codec.h"
#include "esp_log.h"

static const char *TAG = "OPUS_CODEC";

esp_err_t opus_codec_init(void) {
    ESP_LOGI(TAG, "Initializing Opus Codec (Encoder & Decoder)");
    return ESP_OK;
}

esp_err_t opus_encode_audio(const int16_t *pcm_data, int frame_size, uint8_t *opus_data, int *out_len) {
    // TODO: libopus encode implementation
    *out_len = 0;
    return ESP_OK;
}

esp_err_t opus_decode_audio(const uint8_t *opus_data, int len, int16_t *pcm_data, int *out_frame_size) {
    // TODO: libopus decode implementation
    *out_frame_size = 0;
    return ESP_OK;
}

#include "camera_vision.h"
#include "esp_log.h"

static const char *TAG = "CAMERA";

esp_err_t camera_vision_init(void) {
    ESP_LOGI(TAG, "Initializing Camera interface");
    return ESP_OK;
}

esp_err_t camera_vision_capture_frame(uint8_t **out_buf, size_t *out_len) {
    ESP_LOGI(TAG, "Capturing frame...");
    *out_len = 0;
    return ESP_OK;
}

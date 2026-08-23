#include "esp_sr_engine.h"
#include "esp_log.h"

static const char *TAG = "ESP_SR";

esp_err_t esp_sr_engine_init(void) {
    ESP_LOGI(TAG, "Initializing ESP-SR Engine (AEC + WWE)");
    // TODO: Initialize AFE (Audio Front End) and Wake Word Engine
    return ESP_OK;
}

void esp_sr_engine_start_wwe(void) {
    ESP_LOGI(TAG, "Starting Wake Word Engine");
}

void esp_sr_engine_stop_wwe(void) {
    ESP_LOGI(TAG, "Stopping Wake Word Engine");
}

void esp_sr_register_wakeup_callback(void (*cb)(void)) {
    ESP_LOGI(TAG, "Registered wakeup callback");
    // TODO: Bind callback
}

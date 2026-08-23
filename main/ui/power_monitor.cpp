#include "power_monitor.h"
#include "esp_log.h"

static const char *TAG = "POWER_MON";

esp_err_t power_monitor_init(void) {
    ESP_LOGI(TAG, "Initializing Battery Power Monitor");
    return ESP_OK;
}

int power_monitor_get_battery_level(void) {
    return 100; // Fake battery level 100%
}

void power_monitor_enter_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep...");
}

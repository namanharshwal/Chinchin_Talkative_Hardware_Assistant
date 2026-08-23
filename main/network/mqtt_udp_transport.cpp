#include "mqtt_udp_transport.h"
#include "esp_log.h"

static const char *TAG = "MQTT_UDP";

esp_err_t mqtt_udp_transport_init(void) {
    ESP_LOGI(TAG, "Initializing MQTT and UDP Transport with PSA Crypto");
    return ESP_OK;
}

esp_err_t mqtt_publish(const char *topic, const char *data) {
    ESP_LOGI(TAG, "Publishing to topic %s", topic);
    return ESP_OK;
}

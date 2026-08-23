#include "net_manager.h"
#include "esp_log.h"

static const char *TAG = "NET_MANAGER";

esp_err_t net_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Network Manager (Wi-Fi, 4G, Ethernet)");
    // TODO: Setup network interface state machine for failover
    return ESP_OK;
}

net_type_t net_manager_get_current_type(void) {
    return NET_TYPE_DISCONNECTED;
}

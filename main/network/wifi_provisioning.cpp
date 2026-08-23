#include "wifi_provisioning.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include <string.h>
#include "ui/ui_manager.h"

static const char *TAG = "WIFI_PROV";

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected. Retrying connection...");
        ui_manager_set_wifi(false);
        ui_manager_set_status("WiFi Failed...");
        ui_manager_set_state(UI_STATE_ERROR);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi connected.");
        ui_manager_set_wifi(true);
        ui_manager_set_status("Connecting AI...");
        ui_manager_set_state(UI_STATE_IDLE);
    }
}

esp_err_t wifi_provisioning_init_blufi(void) {
    ESP_LOGI(TAG, "Initializing BluFi Provisioning with PSA Crypto");
    return ESP_OK; // Stub for BluFi
}

esp_err_t wifi_provisioning_init_hotspot(void) {
    ESP_LOGI(TAG, "Initializing AP Hotspot Provisioning");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Connect to user's WiFi network
    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, "nammy");
    strcpy((char*)wifi_config.sta.password, "ramramji");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();

    return ESP_OK;
}

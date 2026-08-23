#include "mcp_router.h"
#include "device_control.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MCP_ROUTER";

esp_err_t mcp_router_init(void) {
    ESP_LOGI(TAG, "Initializing MCP Router");
    return ESP_OK;
}

esp_err_t mcp_router_process_msg(const char *json_msg) {
    ESP_LOGI(TAG, "Processing MCP Message: %s", json_msg);
    
    cJSON *root = cJSON_Parse(json_msg);
    if (root == NULL) {
        ESP_LOGE(TAG, "Error parsing JSON");
        return ESP_FAIL;
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (cJSON_IsString(method) && (method->valuestring != NULL)) {
        if (strcmp(method->valuestring, "set_led") == 0) {
            cJSON *params = cJSON_GetObjectItem(root, "params");
            cJSON *level = cJSON_GetObjectItem(params, "level");
            if (cJSON_IsNumber(level)) {
                device_control_set_gpio(2, level->valueint); // Generic GPIO 2
            }
        } else if (strcmp(method->valuestring, "set_servo") == 0) {
            cJSON *params = cJSON_GetObjectItem(root, "params");
            cJSON *duty = cJSON_GetObjectItem(params, "duty");
            if (cJSON_IsNumber(duty)) {
                device_control_set_pwm(0, duty->valueint); // Generic PWM Channel 0
            }
        } else {
            ESP_LOGW(TAG, "Unknown MCP Method: %s, forwarding to cloud...", method->valuestring);
            // Forward context to cloud via WebSocket
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

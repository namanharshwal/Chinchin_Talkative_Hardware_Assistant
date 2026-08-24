#include "websocket_client.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "ui/ui_manager.h"
#include "audio/audio_hal.h"
#include "cJSON.h"

static const char *TAG = "WEBSOCKET";
static esp_websocket_client_handle_t client = NULL;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    (void)data; // Suppress unused warning
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            ui_manager_set_status("AI Connected!");
            ui_manager_set_state(UI_STATE_IDLE);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            ui_manager_set_status("AI Offline");
            ui_manager_set_state(UI_STATE_ERROR);
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 2) {
                ui_manager_set_status("Speaking...");
                ui_manager_set_state(UI_STATE_SPEAKING);
                audio_hal_write_speaker((const uint8_t*)data->data_ptr, data->data_len);
            } else if (data->op_code == 1) { // Text data
                ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA: Text data received");
                // Null-terminate the string safely for parsing
                char *text_data = (char *)malloc(data->data_len + 1);
                if (text_data) {
                    memcpy(text_data, data->data_ptr, data->data_len);
                    text_data[data->data_len] = '\0';
                    
                    cJSON *json = cJSON_Parse(text_data);
                    if (json) {
                        cJSON *state = cJSON_GetObjectItem(json, "state");
                        if (state && cJSON_IsString(state)) {
                            const char *s = state->valuestring;
                            if (strcmp(s, "listening") == 0) {
                                ui_manager_set_state(UI_STATE_LISTENING);
                                ui_manager_set_status("Listening...");
                            } else if (strcmp(s, "thinking") == 0) {
                                ui_manager_set_state(UI_STATE_THINKING);
                                ui_manager_set_status("Thinking...");
                            } else if (strcmp(s, "idle") == 0) {
                                ui_manager_set_state(UI_STATE_IDLE);
                                ui_manager_set_status("AI Connected!");
                            } else if (strcmp(s, "happy") == 0) {
                                ui_manager_set_state(UI_STATE_HAPPY);
                                ui_manager_set_status("AI Connected!");
                            } else if (strcmp(s, "sad") == 0) {
                                ui_manager_set_state(UI_STATE_SAD);
                                ui_manager_set_status("AI Connected!");
                            } else if (strcmp(s, "angry") == 0) {
                                ui_manager_set_state(UI_STATE_ANGRY);
                                ui_manager_set_status("AI Connected!");
                            } else if (strcmp(s, "sleepy") == 0) {
                                ui_manager_set_state(UI_STATE_SLEEPY);
                                ui_manager_set_status("AI Connected!");
                            }
                        }
                        cJSON_Delete(json);
                    }
                    
                    free(text_data);
                }
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
            ui_manager_set_status("Server Error");
            ui_manager_set_state(UI_STATE_ERROR);
            break;
    }
}

esp_err_t websocket_client_start(const char *uri) {
    ESP_LOGI(TAG, "Starting WebSocket client connecting to %s", uri);
    
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = uri;
    websocket_cfg.buffer_size = 4096;

    client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    return esp_websocket_client_start(client);
}

esp_err_t websocket_client_send_audio(const uint8_t *data, size_t size) {
    if (esp_websocket_client_is_connected(client)) {
        esp_websocket_client_send_bin(client, (const char *)data, size, portMAX_DELAY);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t websocket_client_send_mcp(const char *json_msg) {
    if (esp_websocket_client_is_connected(client)) {
        ESP_LOGI(TAG, "Sending MCP msg via WS: %s", json_msg);
        esp_websocket_client_send_text(client, json_msg, strlen(json_msg), portMAX_DELAY);
        return ESP_OK;
    }
    return ESP_FAIL;
}

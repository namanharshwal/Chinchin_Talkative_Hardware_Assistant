#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio/audio_hal.h"
#include "audio/esp_sr_engine.h"
#include "network/net_manager.h"
#include "network/wifi_provisioning.h"
#include "network/websocket_client.h"
#include "mcp/mcp_router.h"
#include "mcp/device_control.h"
#include "ui/ui_manager.h"
#include "ui/touch_sensor.h"
#include <math.h>

static const char *TAG = "MAIN";

static void mic_recording_task(void *arg) {
    uint8_t *mic_buf = (uint8_t *)malloc(1024);
    if (!mic_buf) {
        ESP_LOGE(TAG, "Failed to allocate mic buffer");
        vTaskDelete(NULL);
    }
    while (1) {
        ui_state_t current_state = ui_manager_get_state();
        // Stream constantly unless we are speaking, thinking, or in menu/hacking modes
        if (current_state != UI_STATE_SPEAKING && 
            current_state != UI_STATE_THINKING &&
            current_state != UI_STATE_MENU && 
            current_state != UI_STATE_HACKING) {
            
            size_t bytes_read = 0;
            if (audio_hal_read_mic(mic_buf, 1024, &bytes_read) == ESP_OK && bytes_read > 0) {
                websocket_client_send_audio(mic_buf, bytes_read);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 AI Chatbot & MCP Hub");

    // 1. Initialize UI & Power
    ui_manager_init();
    touch_sensor_init();
    
    // 2. Initialize Hardware Control
    device_control_init();

    // 3. Initialize Networking (Wi-Fi/4G)
    wifi_provisioning_init_hotspot(); // Fallback generic init for demo
    net_manager_init();

    // 4. Initialize Audio Subsystem
    audio_hal_init();
    esp_sr_engine_init();

    // 5. Initialize MCP Router
    mcp_router_init();

    // 6. Connect WebSocket to Backend
    websocket_client_start("ws://10.42.0.1:8765");

    // 7. Start Mic Recording Task
    xTaskCreate(mic_recording_task, "mic_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System initialization complete. Main task halting.");
    
    // In FreeRTOS, app_main can simply return or block indefinitely.
    // Individual subsystems are running their own tasks.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Heartbeat - System Running");
    }
}

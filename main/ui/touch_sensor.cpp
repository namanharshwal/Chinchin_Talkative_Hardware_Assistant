#include "touch_sensor.h"
#include "ui_manager.h"
#include "audio/audio_hal.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network/websocket_client.h"

static const char *TAG = "TOUCH";

#define TOUCH_PIN_1 GPIO_NUM_13
#define TOUCH_PIN_2 GPIO_NUM_14
#define TOUCH_PIN_3 GPIO_NUM_18
#define TOUCH_PIN_4 GPIO_NUM_19

static void touch_task(void *arg) {
    uint32_t press_time_1 = 0;
    uint32_t press_time_2 = 0;
    uint32_t press_time_3 = 0;
    
    bool last_state_1 = false;
    bool last_state_2 = false;
    bool last_state_3 = false;
    bool last_state_4 = false;

    while (1) {
        bool state_1 = gpio_get_level(TOUCH_PIN_1);
        bool state_2 = gpio_get_level(TOUCH_PIN_2);
        bool state_3 = gpio_get_level(TOUCH_PIN_3);
        bool state_4 = gpio_get_level(TOUCH_PIN_4);
        
        uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());

        // --- MENU NAVIGATION OVERRIDE ---
        if (ui_manager_get_state() == UI_STATE_MENU) {
            // Button 1: Scroll UP
            if (state_1 && !last_state_1) {
                ui_manager_menu_scroll(-1);
            }
            // Button 2: Scroll DOWN
            if (state_2 && !last_state_2) {
                ui_manager_menu_scroll(1);
            }
            // Button 3: Back / Exit Menu
            if (state_3 && !last_state_3) {
                ui_manager_menu_back();
            }
            // Button 4: Select
            if (state_4 && !last_state_4) {
                ui_manager_menu_select();
            }
            goto skip_normal_logic;
        }

        // Button 1: Volume Down
        if (state_1) {
            if (!last_state_1) press_time_1 = now;
            else if (now - press_time_1 > 3000) {
                float vol = audio_hal_get_volume();
                audio_hal_set_volume(vol - 0.05f);
                ui_manager_set_status("Vol Down");
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else if (last_state_1) { // Released
            if (now - press_time_1 <= 3000) {
                ESP_LOGI(TAG, "Touch Button 1 single press! (Happy)");
                ui_manager_set_state(UI_STATE_HAPPY);
                ui_manager_set_status("Happy");
            }
        }

        // Button 2: Volume Up
        if (state_2) {
            if (!last_state_2) press_time_2 = now;
            else if (now - press_time_2 > 3000) {
                float vol = audio_hal_get_volume();
                audio_hal_set_volume(vol + 0.05f);
                ui_manager_set_status("Vol Up");
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else if (last_state_2) {
            if (now - press_time_2 <= 3000) {
                ESP_LOGI(TAG, "Touch Button 2 single press! (Sleepy)");
                ui_manager_set_state(UI_STATE_SLEEPY);
                ui_manager_set_status("Sleepy");
            }
        }

        // Button 3: Interrupt / Menu Enter
        if (state_3) {
            if (!last_state_3) {
                press_time_3 = now;
            } else if (now - press_time_3 > 2000) {
                // Long press: Enter Menu
                ESP_LOGI(TAG, "Entering Menu OS");
                audio_hal_flush_speaker();
                // We need to set previous_state_before_menu inside ui_manager. 
                // But setting state to menu will work for now, it's just idle if we exit.
                ui_manager_set_state(UI_STATE_MENU);
            }
        } else if (last_state_3) {
            // Short press: Stop Speaking / Angry
            if (now - press_time_3 <= 2000) {
                ESP_LOGI(TAG, "Touch Button 3 pressed! (Stop Speaking)");
                audio_hal_flush_speaker();
                ui_manager_set_state(UI_STATE_ANGRY);
                ui_manager_set_status("Interrupted");
            }
        }

        // Button 4: Listen (Toggle)
        if (state_4 && !last_state_4) {
            static bool is_listening = false;
            is_listening = !is_listening;
            if (is_listening) {
                ESP_LOGI(TAG, "Touch Button 4 pressed! (Start Listening)");
                if (ui_manager_get_state() == UI_STATE_SPEAKING) {
                    audio_hal_flush_speaker();
                }
                ui_manager_set_state(UI_STATE_LISTENING);
                ui_manager_set_status("Listening...");
            } else {
                ESP_LOGI(TAG, "Touch Button 4 pressed! (Stop Listening)");
                ui_manager_set_state(UI_STATE_THINKING);
                ui_manager_set_status("Thinking...");
                websocket_client_send_mcp("{\"action\":\"process_audio\"}");
            }
        }

skip_normal_logic:
        last_state_1 = state_1;
        last_state_2 = state_2;
        last_state_3 = state_3;
        last_state_4 = state_4;

        vTaskDelay(pdMS_TO_TICKS(30)); // Slightly faster polling for touch response
    }
}

esp_err_t touch_sensor_init(void) {
    ESP_LOGI(TAG, "Initializing TTP224 Touch Sensor on GPIOs 13, 14, 18, 19");

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = (1ULL << TOUCH_PIN_1) | (1ULL << TOUCH_PIN_2) | (1ULL << TOUCH_PIN_3) | (1ULL << TOUCH_PIN_4);
    
    gpio_config(&io_conf);

    xTaskCreate(touch_task, "touch_task", 2048, NULL, 5, NULL);
    
    return ESP_OK;
}

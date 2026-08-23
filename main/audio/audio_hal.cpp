#include "audio_hal.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "AUDIO_HAL";

static i2s_chan_handle_t rx_chan; // Microphone
static i2s_chan_handle_t tx_chan; // Speaker
static float current_volume = 1.0f;

// Generic I2S pins (Safe for ESP-WROOM-32)
#define I2S_BCLK GPIO_NUM_26
#define I2S_WS   GPIO_NUM_25
#define I2S_DOUT GPIO_NUM_27 // Speaker DIN
#define I2S_DIN  GPIO_NUM_33 // Mic SD

esp_err_t audio_hal_init(void) {
    ESP_LOGI(TAG, "Initializing I2S for Mic and Speaker");
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    return ESP_OK;
}

esp_err_t audio_hal_start_mic(void) {
    ESP_LOGI(TAG, "Starting Microphone stream");
    // Already enabled in init for demo
    return ESP_OK;
}

esp_err_t audio_hal_stop_mic(void) {
    ESP_LOGI(TAG, "Stopping Microphone stream");
    return ESP_OK;
}

esp_err_t audio_hal_read_mic(uint8_t *data, size_t size, size_t *bytes_read) {
    return i2s_channel_read(rx_chan, data, size, bytes_read, portMAX_DELAY);
}

esp_err_t audio_hal_flush_speaker(void) {
    ESP_LOGI(TAG, "Flushing speaker buffer...");
    i2s_channel_disable(tx_chan);
    i2s_channel_enable(tx_chan);
    return ESP_OK;
}

esp_err_t audio_hal_write_speaker(const uint8_t *data, size_t size) {
    if (current_volume != 1.0f) {
        int16_t *scaled_data = (int16_t *)malloc(size);
        if (scaled_data) {
            int16_t *orig_data = (int16_t *)data;
            size_t num_samples = size / 2;
            for (size_t i = 0; i < num_samples; i++) {
                int32_t val = (int32_t)(orig_data[i] * current_volume);
                if (val > 32767) val = 32767;
                else if (val < -32768) val = -32768;
                scaled_data[i] = (int16_t)val;
            }
            size_t bytes_written = 0;
            i2s_channel_write(tx_chan, (const void *)scaled_data, size, &bytes_written, portMAX_DELAY);
            free(scaled_data);
            return ESP_OK;
        }
    }
    size_t bytes_written = 0;
    i2s_channel_write(tx_chan, (const void *)data, size, &bytes_written, portMAX_DELAY);
    return ESP_OK;
}

void audio_hal_set_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 2.0f) vol = 2.0f;
    current_volume = vol;
    ESP_LOGI(TAG, "Volume set to %.2f", current_volume);
}

float audio_hal_get_volume(void) {
    return current_volume;
}

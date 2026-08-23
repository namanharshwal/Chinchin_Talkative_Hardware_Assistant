#include "device_control.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "DEVICE_CTRL";

// Generic macros for placeholder
#define DEFAULT_LED_GPIO GPIO_NUM_2
#define DEFAULT_PWM_GPIO GPIO_NUM_4

esp_err_t device_control_init(void) {
    ESP_LOGI(TAG, "Initializing Device Control HW (GPIO, PWM, I2C)");

    // Initialize generic GPIO
    gpio_reset_pin(DEFAULT_LED_GPIO);
    gpio_set_direction(DEFAULT_LED_GPIO, GPIO_MODE_OUTPUT);

    // Initialize generic PWM (LEDC)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num       = DEFAULT_PWM_GPIO,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    return ESP_OK;
}

esp_err_t device_control_set_gpio(int pin, int level) {
    ESP_LOGI(TAG, "Set GPIO %d to %d", pin, level);
    return gpio_set_level((gpio_num_t)pin, level);
}

esp_err_t device_control_set_pwm(int channel, uint32_t duty) {
    ESP_LOGI(TAG, "Set PWM Channel %d to duty %lu", channel, duty);
    return ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
}

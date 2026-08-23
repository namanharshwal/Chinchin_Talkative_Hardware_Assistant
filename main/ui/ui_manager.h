#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Configuration for SH1106
#define I2C_MASTER_SCL_IO           22      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21      /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0 /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000  /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0       /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

// We will dynamically detect the address (0x3C or 0x3D) in ui_manager.cpp
// #define OLED_I2C_ADDRESS            0x3C    

typedef enum {
    UI_STATE_BOOTING,
    UI_STATE_IDLE,
    UI_STATE_LISTENING,
    UI_STATE_THINKING,
    UI_STATE_SPEAKING,
    UI_STATE_ERROR,
    UI_STATE_HAPPY,
    UI_STATE_SAD,
    UI_STATE_ANGRY,
    UI_STATE_SLEEPY,
    UI_STATE_MENU,
    UI_STATE_HACKING
} ui_state_t;

esp_err_t ui_manager_init(void);
void ui_manager_set_emoji(int emoji_id);
void ui_manager_set_language(const char *lang_code);

// UI Status setters
void ui_manager_set_status(const char* status_text);
void ui_manager_set_wifi(bool connected);
void ui_manager_set_state(ui_state_t state);
ui_state_t ui_manager_get_state(void);

// Menu OS Controls
void ui_manager_menu_scroll(int dir);
void ui_manager_menu_select(void);
void ui_manager_menu_back(void);

// Custom SH1106 specific functions
esp_err_t i2c_master_init(void);
void sh1106_init(void);
void sh1106_clear(void);
void sh1106_update(void);
void ui_render(void);

#ifdef __cplusplus
}
#endif


#include "ui_manager.h"
#include "font5x7.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>
#include "rabbit_logo.h"

static const char *TAG = "UI_MANAGER";
static uint8_t oled_i2c_address = 0x3C;

static uint8_t display_buffer[1024];

static bool is_wifi_connected = false;
static ui_state_t current_state = UI_STATE_IDLE;
static char current_status[32] = "Hello! Booting...";
static SemaphoreHandle_t ui_mutex = NULL;
static uint32_t frame_counter = 0;

// Custom Bitmaps (8x8)
static const uint8_t bmp_wifi_on[8] = { 0x00, 0x3E, 0x41, 0x80, 0x22, 0x41, 0x14, 0x08 }; // Custom wifi icon
static const uint8_t bmp_mic[8] = { 0x1C, 0x22, 0x22, 0x1C, 0x08, 0x08, 0x3E, 0x00 };

// --- I2C Communication ---
static void sh1106_send_cmd(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (oled_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x00, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}

static void sh1106_send_data(const uint8_t *data, size_t len) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (oled_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x40, true);
    i2c_master_write(handle, data, len, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
}

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void sh1106_init(void) {
    sh1106_send_cmd(0xAE); sh1106_send_cmd(0xD5); sh1106_send_cmd(0x80);
    sh1106_send_cmd(0xA8); sh1106_send_cmd(0x3F); sh1106_send_cmd(0xD3);
    sh1106_send_cmd(0x00); sh1106_send_cmd(0x40); sh1106_send_cmd(0x8D);
    sh1106_send_cmd(0x14); sh1106_send_cmd(0x20); sh1106_send_cmd(0x00);
    sh1106_send_cmd(0xA1); sh1106_send_cmd(0xC8); sh1106_send_cmd(0xDA);
    sh1106_send_cmd(0x12); sh1106_send_cmd(0x81); sh1106_send_cmd(0xCF);
    sh1106_send_cmd(0xD9); sh1106_send_cmd(0xF1); sh1106_send_cmd(0xDB);
    sh1106_send_cmd(0x40); sh1106_send_cmd(0xA4); sh1106_send_cmd(0xA6);
    sh1106_send_cmd(0xAF);
}

void sh1106_update(void) {
    for (int page = 0; page < 8; page++) {
        sh1106_send_cmd(0xB0 + page);
        sh1106_send_cmd(0x02);
        sh1106_send_cmd(0x10);
        sh1106_send_data(&display_buffer[page * 128], 128);
    }
}

// --- Graphics Engine ---

void draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
    int index = x + (y / 8) * 128;
    if (color) display_buffer[index] |= (1 << (y % 8));
    else display_buffer[index] &= ~(1 << (y % 8));
}

void draw_rect(int x, int y, int w, int h, bool color) {
    for(int i=x; i<x+w; i++) {
        for(int j=y; j<y+h; j++) {
            draw_pixel(i, j, color);
        }
    }
}

void draw_bitmap8x8(int x, int y, const uint8_t *bitmap) {
    for (int i = 0; i < 8; i++) {
        uint8_t line = bitmap[i];
        for (int j = 0; j < 8; j++) {
            if (line & 0x01) draw_pixel(x + i, y + j, true);
            line >>= 1;
        }
    }
}

void draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) return;
    int font_idx = (c - 32) * 5;
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[font_idx + i];
        for (int j = 0; j < 8; j++) {
            if (line & 0x01) draw_pixel(x + i, y + j, true);
            line >>= 1;
        }
    }
}

void draw_string(int x, int y, const char* str) {
    int start_x = x;
    while (*str) {
        draw_char(x, y, *str);
        x += 6;
        if (x + 5 >= 128) { x = start_x; y += 8; }
        str++;
    }
}

// --- Animation & Rendering ---

void draw_line(int x0, int y0, int x1, int y1, bool color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_filled_circle(int x0, int y0, int r, bool color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                draw_pixel(x0+x, y0+y, color);
            }
        }
    }
}

// Draws the 128x64 bitmap with an X/Y offset and true topological eye squashing
void draw_bitmap_animated(const uint8_t *bitmap, int dx, int dy, float blink_scale) {
    // Eye bounding boxes in original logo coordinates
    int le_x_start = 42, le_x_end = 52;
    int le_y_start = 24, le_y_end = 46;
    int re_x_start = 68, re_x_end = 78;
    int re_y_start = 24, re_y_end = 46;
    int eye_y_center = 35;

    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            int src_x = x - dx;
            int src_y = y - dy;
            
            if (src_x >= 0 && src_x < 128 && src_y >= 0 && src_y < 64) {
                
                // If we are inside the eye bounding box, we apply the blink scaling
                if ((src_x >= le_x_start && src_x <= le_x_end && src_y >= le_y_start && src_y <= le_y_end) ||
                    (src_x >= re_x_start && src_x <= re_x_end && src_y >= re_y_start && src_y <= re_y_end)) {
                    
                    if (blink_scale < 0.99f) {
                        float scale = blink_scale < 0.05f ? 0.05f : blink_scale;
                        int dist_y = src_y - eye_y_center;
                        int orig_src_y = eye_y_center + (int)(dist_y / scale);
                        
                        // If the inverse scaled Y is outside the original eye box, it becomes the eyelid (face color = lit/true)
                        if (orig_src_y < le_y_start || orig_src_y > le_y_end) {
                            draw_pixel(x, y, true);
                            continue;
                        } else {
                            src_y = orig_src_y;
                        }
                    }
                }
                
                int src_page = src_y / 8;
                int src_bit = src_y % 8;
                if (bitmap[src_page * 128 + src_x] & (1 << src_bit)) {
                    draw_pixel(x, y, true);
                }
            }
        }
    }
}

void ui_render(void) {
    xSemaphoreTake(ui_mutex, portMAX_DELAY);
    bool wifi = is_wifi_connected;
    ui_state_t state = current_state;
    char status_copy[32];
    strncpy(status_copy, current_status, sizeof(status_copy));
    xSemaphoreGive(ui_mutex);

    memset(display_buffer, 0, sizeof(display_buffer)); // Clear

    bool is_speaking = (state == UI_STATE_SPEAKING);
    bool is_thinking = (state == UI_STATE_THINKING);
    bool is_listening = (state == UI_STATE_LISTENING);
    bool is_happy = (state == UI_STATE_HAPPY);
    bool is_sad = (state == UI_STATE_SAD);
    bool is_angry = (state == UI_STATE_ANGRY);
    bool is_sleepy = (state == UI_STATE_SLEEPY);
    bool is_error = (state == UI_STATE_ERROR);

    // --- High-FPS Sway & Breathing Math ---
    float time_sec = frame_counter / 60.0f;
    
    // Smooth breathing up and down
    int sway_y = (int)(sin(time_sec * 2.5f) * 2.0f); 
    
    // Look around left and right
    int sway_x = 0;
    if (state == UI_STATE_IDLE || is_listening) {
        sway_x = (int)(sin(time_sec * 1.5f) * 3.0f);
    }
    
    if (is_thinking) {
        sway_x = (frame_counter % 20 < 10) ? 2 : -2; // Jitter head when thinking
    }

    // Blinking (Fluid high-FPS eyelid animation)
    int blink_frame = frame_counter % 180;
    bool is_blinking = (state == UI_STATE_IDLE && blink_frame < 12);
    
    float blink_scale = 1.0f;
    if (is_blinking || is_sleepy) {
        if (is_sleepy) {
            blink_scale = 0.3f; // Half closed
        } else {
            // Smoothly animate eyelid scale down and up over 12 frames
            if (blink_frame < 4) {
                blink_scale = 1.0f - (blink_frame * 0.25f); // Closing: 1.0 -> 0.0
            } else if (blink_frame < 8) {
                blink_scale = 0.0f; // Fully closed
            } else {
                blink_scale = (blink_frame - 7) * 0.25f; // Opening: 0.0 -> 1.0
            }
        }
    }

    // 1. Draw the exact user logo with the swaying offset and topological eye squash
    draw_bitmap_animated(rabbit_logo_bmp, sway_x, sway_y, blink_scale);

    // 2. High-FPS Emotion Overlays (Matching Logo Coordinates + Sway)
    int base_left_eye_x = 42 + sway_x;
    int base_right_eye_x = 70 + sway_x;
    int base_eye_y = 33 + sway_y;
    
    if (is_angry) {
        for(int t=0; t<6; t++) {
            draw_line(base_left_eye_x - 2, base_eye_y - 3 + t, base_left_eye_x + 18, base_eye_y + 7 + t, true);
            draw_line(base_right_eye_x + 18, base_eye_y - 3 + t, base_right_eye_x - 2, base_eye_y + 7 + t, true);
        }
    }
    
    if (is_sad) {
        for(int t=0; t<6; t++) {
            draw_line(base_left_eye_x + 18, base_eye_y - 3 + t, base_left_eye_x - 2, base_eye_y + 7 + t, true);
            draw_line(base_right_eye_x - 2, base_eye_y - 3 + t, base_right_eye_x + 18, base_eye_y + 7 + t, true);
        }
    }
    
    // Speaking Animation (Mouth roughly at x=60 to 68, y=56 to 62)
    if (is_speaking) {
        int mouth_x = 60 + sway_x;
        int mouth_y = 56 + sway_y;
        
        // High-FPS mouth opening and closing using sine wave
        int mouth_h = 2 + (int)(abs(sin(time_sec * 15.0f)) * 8.0f);
        int mouth_w = 8 + (int)(abs(cos(time_sec * 10.0f)) * 4.0f);
        
        draw_rect(mouth_x - (mouth_w - 8)/2, mouth_y, mouth_w, mouth_h, false); // Draw black hole for mouth
    }
    
    if (is_listening) {
        draw_bitmap8x8(118, 2, bmp_mic);
    }

    // --- Status Overlay ---
    if (!wifi) {
        for(int i=0; i<42; i++) {
            for(int j=56; j<64; j++) draw_pixel(i, j, false);
        }
        draw_string(0, 56, "NO WIFI");
    }
    if (is_error) {
        for(int i=0; i<32; i++) {
            for(int j=0; j<8; j++) draw_pixel(i, j, false);
        }
        draw_string(0, 0, "ERROR");
    }

    sh1106_update();
}

static void ui_task(void *arg) {
    while (1) {
        ui_render();
        frame_counter++;
        vTaskDelay(pdMS_TO_TICKS(16)); // ~60 FPS
    }
}

// --- Public API ---

void ui_manager_set_status(const char* status_text) {
    if (ui_mutex) {
        xSemaphoreTake(ui_mutex, portMAX_DELAY);
        strncpy(current_status, status_text, sizeof(current_status) - 1);
        current_status[sizeof(current_status) - 1] = '\0';
        xSemaphoreGive(ui_mutex);
    }
}

void ui_manager_set_wifi(bool connected) {
    if (ui_mutex) {
        xSemaphoreTake(ui_mutex, portMAX_DELAY);
        is_wifi_connected = connected;
        xSemaphoreGive(ui_mutex);
    }
}

void ui_manager_set_state(ui_state_t state) {
    if (ui_mutex) {
        xSemaphoreTake(ui_mutex, portMAX_DELAY);
        current_state = state;
        xSemaphoreGive(ui_mutex);
    }
}

ui_state_t ui_manager_get_state(void) {
    ui_state_t state = UI_STATE_IDLE;
    if (ui_mutex) {
        xSemaphoreTake(ui_mutex, portMAX_DELAY);
        state = current_state;
        xSemaphoreGive(ui_mutex);
    }
    return state;
}

esp_err_t ui_manager_init(void) {
    ESP_LOGI(TAG, "Initializing Premium UI Engine for SH1106");
    ui_mutex = xSemaphoreCreateMutex();
    
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    // I2C Scanner
    bool found = false;
    for (uint8_t addr = 0x3C; addr <= 0x3D; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        if (i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50)) == ESP_OK) {
            oled_i2c_address = addr;
            found = true;
            i2c_cmd_link_delete(cmd);
            break;
        }
        i2c_cmd_link_delete(cmd);
    }
    
    if (found) ESP_LOGI(TAG, "Found OLED display at 0x%02X", oled_i2c_address);
    else ESP_LOGE(TAG, "Failed to find I2C display at 0x3C or 0x3D! Check wiring.");
    
    sh1106_init();
    
    // Start rendering task (20 FPS)
    xTaskCreate(ui_task, "ui_task", 4096, NULL, 5, NULL);
    
    return ESP_OK;
}

void ui_manager_set_emoji(int emoji_id) { ESP_LOGI(TAG, "Legacy Set Emoji: %d", emoji_id); }
void ui_manager_set_language(const char *lang_code) { ESP_LOGI(TAG, "Legacy Set Lang: %s", lang_code); }

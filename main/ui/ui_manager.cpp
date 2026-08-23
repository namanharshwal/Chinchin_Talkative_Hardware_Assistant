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
#include "audio/audio_hal.h"

static const char *TAG = "UI_MANAGER";
static uint8_t oled_i2c_address = 0x3C;

static uint8_t display_buffer[1024];

static bool is_wifi_connected = false;
static ui_state_t current_state = UI_STATE_IDLE;
static char current_status[32] = "Hello! Booting...";
static SemaphoreHandle_t ui_mutex = NULL;
static uint32_t frame_counter = 0;

// --- MENU OS STATE ---
#define MENU_ITEM_COUNT 5
static int menu_cursor_index = 0;
static const char* menu_items[MENU_ITEM_COUNT] = {
    "1. Set Happy",
    "2. Wi-Fi Sniffer",
    "3. Set Sleepy",
    "4. Stop Audio",
    "5. Exit Menu"
};
static ui_state_t previous_state_before_menu = UI_STATE_IDLE;

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

void draw_char(int x, int y, char c, bool color = true) {
    if (c < 32 || c > 127) return;
    int font_idx = (c - 32) * 5;
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[font_idx + i];
        for (int j = 0; j < 8; j++) {
            if (line & 0x01) draw_pixel(x + i, y + j, color);
            line >>= 1;
        }
    }
}

void draw_string(int x, int y, const char* str, bool color = true) {
    int start_x = x;
    while (*str) {
        draw_char(x, y, *str, color);
        x += 6;
        if (x + 5 >= 128) { x = start_x; y += 8; }
        str++;
    }
}

void draw_filled_rect(int x, int y, int w, int h, bool color) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            draw_pixel(x + i, y + j, color);
        }
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

// --- PURE ANIME VECTOR ENGINE ---
// Every element is drawn with math. No bitmap needed. 60FPS fluid animation.

// Draw a filled ellipse (oval) centered at (cx, cy) with radii (rx, ry)
void draw_filled_ellipse(int cx, int cy, int rx, int ry, bool color) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        for (int x = -rx; x <= rx; x++) {
            // Ellipse equation: (x/rx)^2 + (y/ry)^2 <= 1
            if ((x * x * ry * ry + y * y * rx * rx) <= (rx * rx * ry * ry)) {
                draw_pixel(cx + x, cy + y, color);
            }
        }
    }
}

// Draw a thick curved line (using two overlapping ellipses to create an arc/crescent)
void draw_arc(int cx, int cy, int rx, int ry, int thickness, bool color, bool is_upper) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        // Upper arc only draws top half, lower arc only draws bottom half
        if (is_upper && y > 0) continue;
        if (!is_upper && y < 0) continue;
        
        for (int x = -rx; x <= rx; x++) {
            float val = (float)(x * x) / (rx * rx) + (float)(y * y) / (ry * ry);
            if (val <= 1.0f) {
                // Inner cutout
                float val_inner = (float)(x * x) / ((rx - thickness) * (rx - thickness)) + (float)(y * y) / ((ry - thickness) * (ry - thickness));
                if (val_inner > 1.0f || (rx - thickness <= 0) || (ry - thickness <= 0)) {
                    draw_pixel(cx + x, cy + y, color);
                }
            }
        }
    }
}

// --- MENU CONTROL FUNCTIONS ---
void ui_manager_menu_scroll(int dir) {
    if (current_state != UI_STATE_MENU) return;
    if (ui_mutex) {
        xSemaphoreTake(ui_mutex, portMAX_DELAY);
        menu_cursor_index += dir;
        if (menu_cursor_index < 0) menu_cursor_index = MENU_ITEM_COUNT - 1;
        if (menu_cursor_index >= MENU_ITEM_COUNT) menu_cursor_index = 0;
        xSemaphoreGive(ui_mutex);
    }
}

void ui_manager_menu_back(void) {
    if (current_state != UI_STATE_MENU) return;
    ui_manager_set_state(previous_state_before_menu);
}

void ui_manager_menu_select(void) {
    if (current_state != UI_STATE_MENU) return;
    
    // Execute action based on cursor
    switch (menu_cursor_index) {
        case 0: ui_manager_set_state(UI_STATE_HAPPY); break;
        case 1: ui_manager_set_state(UI_STATE_HACKING); break;
        case 2: ui_manager_set_state(UI_STATE_SLEEPY); break;
        case 3: audio_hal_flush_speaker(); ui_manager_set_state(UI_STATE_IDLE); break;
        case 4: ui_manager_set_state(previous_state_before_menu); break; // Exit
    }
}

void ui_render(void) {
    xSemaphoreTake(ui_mutex, portMAX_DELAY);
    bool wifi = is_wifi_connected;
    ui_state_t state = current_state;
    char status_copy[32];
    strncpy(status_copy, current_status, sizeof(status_copy));
    xSemaphoreGive(ui_mutex);

    memset(display_buffer, 0, sizeof(display_buffer)); // Clear to black

    if (state == UI_STATE_MENU) {
        draw_string(20, 2, "--- SETTINGS ---", true);
        
        for (int i = 0; i < MENU_ITEM_COUNT; i++) {
            int y = 14 + (i * 10);
            if (i == menu_cursor_index) {
                // Highlight box for selected item
                draw_filled_rect(4, y - 1, 120, 9, true);
                draw_string(6, y, menu_items[i], false); // Black text on white
            } else {
                draw_string(6, y, menu_items[i], true); // White text on black
            }
        }
        
        sh1106_update();
        frame_counter++;
        return; // Skip face rendering
    }

    if (state == UI_STATE_HACKING) {
        draw_string(0, 2, "> INITIATING HACK...", true);
        
        // Pseudo-random scrolling matrix effect (to avoid dropping the WebSocket connection with a real scan)
        for (int i = 0; i < 5; i++) {
            int y = 14 + (i * 10);
            char hex_buf[24];
            snprintf(hex_buf, sizeof(hex_buf), "0x%04X %04X %04X", 
                     (uint16_t)(frame_counter * (i+1) * 31),
                     (uint16_t)((frame_counter + i) * 17),
                     (uint16_t)(frame_counter * 99 % 0xFFFF));
            draw_string(4, y, hex_buf, true);
        }
        
        sh1106_update();
        frame_counter++;
        return;
    }

    bool is_speaking = (state == UI_STATE_SPEAKING);
    bool is_thinking = (state == UI_STATE_THINKING);
    bool is_listening = (state == UI_STATE_LISTENING);
    bool is_happy = (state == UI_STATE_HAPPY);
    bool is_sad = (state == UI_STATE_SAD);
    bool is_angry = (state == UI_STATE_ANGRY);
    bool is_sleepy = (state == UI_STATE_SLEEPY);
    bool is_error = (state == UI_STATE_ERROR);

    // ====== HIGH-FPS ANIMATION MATH ======
    float time_sec = frame_counter / 60.0f;

    // --- 3D Parallax: Breathing ---
    float breath = sin(time_sec * 2.5f) * 1.5f;
    int sway_y = (int)breath;

    // --- 3D Parallax: Looking around ---
    float look_x = 0.0f;
    float look_y = 0.0f;
    if (state == UI_STATE_IDLE || is_listening) {
        look_x = sin(time_sec * 0.8f) * 4.0f; // Slow smooth look
        look_y = cos(time_sec * 0.5f) * 2.0f;
    }
    if (is_thinking) {
        look_x = (frame_counter % 20 < 10) ? 2.0f : -2.0f; // Nervous jitter
        look_y = -3.0f; // Look up when thinking
    }
    int sway_x = (int)look_x;

    // --- Blinking ---
    int blink_cycle = frame_counter % 150; // Blink every 2.5 seconds
    float blink_t = 1.0f; // 1.0 = fully open, 0.0 = fully closed
    if (blink_cycle < 12) {
        // Blink animation over 12 frames
        if (blink_cycle < 4)       blink_t = 1.0f - blink_cycle * 0.25f;
        else if (blink_cycle < 8)  blink_t = 0.0f;
        else                       blink_t = (blink_cycle - 7) * 0.25f;
    }
    if (is_sleepy) blink_t = 0.3f; // Drowsy half-closed
    if (is_happy) blink_t = 0.0f;  // Happy is always closed crescent
    if (is_sad) blink_t = 0.7f;    // Sad is drooping

    // --- Speaking mouth ---
    float mouth_open = 0.0f; // 0 = closed, 1 = wide open
    if (is_speaking) {
        mouth_open = 0.5f + 0.5f * sin(time_sec * 15.0f); // Fast open-close interpolation
    }

    // ====== FACE CENTER (with sway) ======
    int cx = 64 + sway_x;
    int cy = 34 + sway_y;

    int left_eye_cx = cx - 18;
    int right_eye_cx = cx + 18;
    int eye_y = cy - 4;

    // ====== 1. ANIME EYES ======
    if (is_happy) {
        // Happy closed eyes (^ ^)
        draw_arc(left_eye_cx, eye_y + 4, 10, 8, 2, true, true);
        draw_arc(right_eye_cx, eye_y + 4, 10, 8, 2, true, true);
    } 
    else if (is_sleepy) {
        // Sleepy closed eyes (- -)
        draw_line(left_eye_cx - 10, eye_y + 4, left_eye_cx + 10, eye_y + 4, true);
        draw_line(right_eye_cx - 10, eye_y + 4, right_eye_cx + 10, eye_y + 4, true);
    }
    else {
        // Open Anime Eyes
        int eye_rx = 12;
        int eye_ry = (int)(14.0f * blink_t);
        if (eye_ry < 1) eye_ry = 1;

        if (eye_ry > 2) {
            // --- AGGRESSIVE CYBERPUNK EYES ---
            
            // Left Eyebrow (Aggressive downward slant)
            draw_line(left_eye_cx - 16, eye_y - 10, left_eye_cx + 12, eye_y - 2, true);
            draw_line(left_eye_cx - 16, eye_y - 11, left_eye_cx + 12, eye_y - 3, true); // thickness
            draw_line(left_eye_cx - 16, eye_y - 12, left_eye_cx + 12, eye_y - 4, true); // thickness
            
            // Left Eye
            // Top eyelid (Aggressive slant down towards center)
            draw_line(left_eye_cx - 14, eye_y - 6, left_eye_cx + 10, eye_y + 3, true);
            draw_line(left_eye_cx - 14, eye_y - 5, left_eye_cx + 10, eye_y + 4, true); // thickness
            draw_line(left_eye_cx - 14, eye_y - 4, left_eye_cx + 10, eye_y + 5, true); // thickness
            
            // Bottom eyelid (Flat/slight curve)
            draw_line(left_eye_cx - 10, eye_y + 8, left_eye_cx + 6, eye_y + 8, true);
            draw_line(left_eye_cx - 10, eye_y + 7, left_eye_cx + 6, eye_y + 7, true);
            
            // Connect left side
            draw_line(left_eye_cx - 14, eye_y - 6, left_eye_cx - 10, eye_y + 8, true);
            draw_line(left_eye_cx - 13, eye_y - 6, left_eye_cx - 9, eye_y + 8, true);
            
            // Left Pupil (Tech U-Shape)
            int pupil_x = left_eye_cx - 4 + (int)(look_x * 0.7f);
            int pupil_y = eye_y + 1 + (int)(look_y * 0.7f);
            draw_filled_rect(pupil_x, pupil_y, 7, 7, true);
            draw_filled_rect(pupil_x + 2, pupil_y + 2, 3, 5, false); // Hollow out to make U-shape

            // Right Eyebrow (Aggressive downward slant)
            draw_line(right_eye_cx + 16, eye_y - 10, right_eye_cx - 12, eye_y - 2, true);
            draw_line(right_eye_cx + 16, eye_y - 11, right_eye_cx - 12, eye_y - 3, true);
            draw_line(right_eye_cx + 16, eye_y - 12, right_eye_cx - 12, eye_y - 4, true);

            // Right Eye
            // Top eyelid (Aggressive slant down towards center)
            draw_line(right_eye_cx + 14, eye_y - 6, right_eye_cx - 10, eye_y + 3, true);
            draw_line(right_eye_cx + 14, eye_y - 5, right_eye_cx - 10, eye_y + 4, true);
            draw_line(right_eye_cx + 14, eye_y - 4, right_eye_cx - 10, eye_y + 5, true);
            
            // Bottom eyelid
            draw_line(right_eye_cx + 10, eye_y + 8, right_eye_cx - 6, eye_y + 8, true);
            draw_line(right_eye_cx + 10, eye_y + 7, right_eye_cx - 6, eye_y + 7, true);
            
            // Connect right side
            draw_line(right_eye_cx + 14, eye_y - 6, right_eye_cx + 10, eye_y + 8, true);
            draw_line(right_eye_cx + 13, eye_y - 6, right_eye_cx + 9, eye_y + 8, true);
            
            // Right Pupil
            pupil_x = right_eye_cx - 3 + (int)(look_x * 0.7f);
            draw_filled_rect(pupil_x, pupil_y, 7, 7, true);
            draw_filled_rect(pupil_x + 2, pupil_y + 2, 3, 5, false);
            
        } else {
            // Blinking (horizontal line)
            draw_line(left_eye_cx - 12, eye_y + 4, left_eye_cx + 10, eye_y + 4, true);
            draw_line(left_eye_cx - 12, eye_y + 5, left_eye_cx + 10, eye_y + 5, true);
            draw_line(right_eye_cx - 10, eye_y + 4, right_eye_cx + 12, eye_y + 4, true);
            draw_line(right_eye_cx - 10, eye_y + 5, right_eye_cx + 12, eye_y + 5, true);
        }
    }

    // ====== 3. BLUSH (cute cheeks) ======
    // Removed for cyberpunk hacker aesthetic

    // ====== 4. MOUTH ======
    int mouth_y = cy + 14;
    
    if (is_speaking) {
        // Dynamic speaking mouth (Digital wide static bar)
        int mw = 4 + (int)(mouth_open * 4.0f);
        int mh = 2 + (int)(mouth_open * 6.0f);
        draw_filled_rect(cx - mw, mouth_y, mw * 2, mh, true);
        // Inner void
        if (mh > 2) draw_filled_rect(cx - mw + 1, mouth_y + 1, mw * 2 - 2, mh - 2, false);
    } 
    else if (is_happy) {
        // Hacker smirk
        draw_line(cx - 6, mouth_y, cx, mouth_y + 2, true);
        draw_line(cx - 6, mouth_y - 1, cx, mouth_y + 1, true);
        draw_line(cx, mouth_y + 2, cx + 6, mouth_y - 2, true);
        draw_line(cx, mouth_y + 1, cx + 6, mouth_y - 3, true);
    }
    else {
        // Sharp 'v' mouth (Idle/Default)
        draw_line(cx - 4, mouth_y - 2, cx, mouth_y + 2, true);
        draw_line(cx - 3, mouth_y - 2, cx, mouth_y + 1, true);
        draw_line(cx + 4, mouth_y - 2, cx, mouth_y + 2, true);
        draw_line(cx + 3, mouth_y - 2, cx, mouth_y + 1, true);
    }

    // ====== 5. STATUS ICONS ======
    if (is_listening) {
        draw_bitmap8x8(118, 2, bmp_mic);
    }
    if (is_thinking) {
        // Thinking dots animation
        int dot_phase = (frame_counter / 10) % 4;
        for (int d = 0; d < dot_phase; d++) {
            draw_filled_circle(cx + 24 + d * 4, cy - 16, 1, true);
        }
    }

    // ====== 6. STATUS OVERLAYS ======
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

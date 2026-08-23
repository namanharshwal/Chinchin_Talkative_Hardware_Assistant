# Hardware Wiring & Command Execution Guide

This guide covers the specific setup for your hardware: **ESP32 DEVKIT V1 (ESP-WROOM-32)**, **DELLZO INMP441 Microphone**, **MAX98357A Audio Amplifier**, and **1.30" I2C OLED (SH1106)**.

---

## 1. Complete Wiring Setup

Because you are using an ESP32-WROOM-32, it's critical to avoid certain "strapping pins" that can cause the board to fail to boot. The pins chosen below are 100% safe for your specific DevKit.

### 🎙️ DELLZO INMP441 (I2S Microphone)
*   **VDD**  ->  ESP32 `3.3V`
*   **GND**  ->  ESP32 `GND`
*   **L/R**  ->  ESP32 `GND` *(Sets mic to Left channel)*
*   **SCK**  ->  ESP32 `GPIO 26` *(Serial Clock)*
*   **WS**   ->  ESP32 `GPIO 25` *(Word Select)*
*   **SD**   ->  ESP32 `GPIO 33` *(Serial Data IN)*

### 🔊 MAX98357A (I2S Speaker Amplifier)
*   **VIN**  ->  ESP32 `5V` (or `3.3V`)
*   **GND**  ->  ESP32 `GND`
*   **LRC**  ->  ESP32 `GPIO 25` *(Word Select, shared with Mic)*
*   **BCLK** ->  ESP32 `GPIO 26` *(Serial Clock, shared with Mic)*
*   **DIN**  ->  ESP32 `GPIO 27` *(Serial Data OUT)*
*   **SD**   ->  **Leave Unconnected** *(Defaults to Mixed Mono output)*


### 📺 1.30" I2C OLED Display (SH1106)
*   **VCC**  ->  ESP32 `3.3V`
*   **GND**  ->  ESP32 `GND`
*   **SCL**  ->  ESP32 `GPIO 22` *(I2C Clock)*
*   **SDA**  ->  ESP32 `GPIO 21` *(I2C Data)*

### 👆 TTP224 4-Channel Touch Sensor
*   **VCC**  ->  ESP32 `3.3V` (or 5V)
*   **GND**  ->  ESP32 `GND`
*   **OUT1** ->  ESP32 `GPIO 13` *(Volume Down)*
*   **OUT2** ->  ESP32 `GPIO 14` *(Volume Up)*
*   **OUT3** ->  ESP32 `GPIO 18` *(Toggle Mute)*
*   **OUT4** ->  ESP32 `GPIO 19` *(Manual Wake/Interrupt)*

### 💡 MCP Test LED
*   **Positive (Long Leg)** -> ESP32 `GPIO 2` *(Through a 220Ω resistor)*
*   **Negative (Short Leg)** -> ESP32 `GND`

### 🎤 INMP441 I2S Microphone
*   **VDD**  ->  ESP32 `3.3V`
*   **GND**  ->  ESP32 `GND`
*   **L/R**  ->  ESP32 `GND` *(Important: Ties mic to the Left channel for Mono)*
*   **WS**   ->  ESP32 `GPIO 25` *(I2S Word Select / LRCLK)*
*   **SCK**  ->  ESP32 `GPIO 26` *(I2S Bit Clock / BCLK)*
*   **SD**   ->  ESP32 `GPIO 33` *(I2S Data Out)*

> **Note:** The audio pins (25, 26, 27, 33) match exactly what is currently configured in the `audio_hal.cpp` file!

---

## 2. Execution Commands

To get the entire system running, you need to start the Cloud AI Backend in one terminal, and flash the ESP32 Firmware in another.

### Step 1: Start the Cloud AI Backend
Open a new terminal on your computer and run:
```bash
# Navigate to the backend folder
cd /home/nammy/esp32_ai_chatbot/backend

# Export your NVIDIA API Key
export NVIDIA_API_KEY="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"

# Start the WebSocket AI server
python3 server.py
```
*(You should see a message saying the server is listening on port 8765)*

### Step 2: Flash the ESP32 Firmware
Open a **second** terminal, plug your ESP32 into your computer via USB, and run:

```bash
# Load the ESP-IDF toolchain (Assumes default install path)
. $HOME/esp/esp-idf/export.sh

# Navigate to the main project folder
cd /home/nammy/esp32_ai_chatbot

# Set the target board for ESP-WROOM-32
idf.py set-target esp32

# Build, flash, and open the serial monitor (Assumes standard USB port)
idf.py build flash monitor
```

Once flashed, the ESP32 will connect to Wi-Fi, open a WebSocket to your backend, and you can start talking to the NVIDIA AI!

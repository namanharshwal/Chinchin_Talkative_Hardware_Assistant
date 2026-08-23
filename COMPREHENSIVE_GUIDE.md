# ESP32 Voice-Controlled AI Chatbot & MCP Hub: Comprehensive Guide

## 1. Project Overview & Objectives
This project aims to build a robust, highly versatile, voice-controlled AI chatbot on ESP32 hardware. Beyond simple chat capabilities, the device is designed to act as a **multi-terminal control entry point** utilizing a custom Model Context Protocol (MCP). 

### Core Features
*   **Voice Interface:** Offline wake-up (custom wake words), full-duplex real-time interaction, and Acoustic Echo Cancellation (AEC) using ESP-SR.
*   **Cloud AI Integration:** Streams Opus-encoded audio to cloud backends for Automatic Speech Recognition (ASR), Large Language Models (LLMs like Qwen/DeepSeek), and Text-to-Speech (TTS).
*   **Dual-Role MCP Protocol:** Uses MCP for both local device control (speakers, LEDs, servos, GPIOs) and extending cloud-side capabilities (smart home control, PC operations).
*   **Dynamic Networking:** Supports Wi-Fi (Hotspot/BluFi provisioning), Ethernet, USB RNDIS, and 4G Cat.1 (e.g., ML307), with seamless network failover.
*   **Cross-Platform ESP-IDF:** Targets modern ESP-IDF (v6.0.2) with built-in configurations for ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3, and ESP32-P4.

---

## 2. System Architecture

The firmware is modularized into several core subsystems within the `main/` directory:

### 2.1 Audio Subsystem (`main/audio/`)
*   **`audio_hal.cpp`**: Hardware Abstraction Layer for I2S communication with microphones and speakers/DACs.
*   **`esp_sr_engine.cpp`**: Integrates Espressif's ESP-SR for wake-word detection (WWE) and Acoustic Echo Cancellation (AEC), allowing for localized offline voice triggering.
*   **`opus_codec.cpp`**: Handles Opus encoding (for upstream mic data) and decoding (for downstream TTS data) to minimize bandwidth.

### 2.2 Network & Transport Subsystem (`main/network/`)
*   **`net_manager.cpp`**: A state machine that handles failover and switching between available network interfaces (Wi-Fi, 4G, Ethernet).
*   **`wifi_provisioning.cpp`**: Manages secure onboarding via BluFi (using PSA Crypto) or standard AP Hotspot.
*   **`websocket_client.cpp`**: Maintains a persistent connection to the cloud backend for real-time audio streaming and LLM integration.
*   **`mqtt_udp_transport.cpp`**: Uses MQTT and UDP (secured via PSA Crypto) for local discovery, telemetry, and secondary control streams.

### 2.3 MCP Protocol Router (`main/mcp/`)
*   **`mcp_router.cpp`**: The central brain for protocol handling. It parses incoming JSON-based MCP commands and routes them appropriately.
*   **`device_control.cpp`**: Executes hardware-level commands parsed by the router (e.g., toggling GPIOs, driving PWM servos, or interacting with I2C peripherals).

### 2.4 User Interface & Hardware (`main/ui/`)
*   **`ui_manager.cpp`**: Manages the OLED/LCD display (via LVGL), supporting dynamic emojis, chat backgrounds, and 38 localized interface languages.
*   **`power_monitor.cpp`**: Handles battery level reading and aggressive sleep state management for portable use cases.
*   **`camera_vision.cpp`**: Optional module for boards with camera hardware (e.g., ESP32-S3-EYE) to capture and stream visual context.

---

## 3. Directory Structure

```text
esp32_ai_chatbot/
├── CMakeLists.txt
├── partitions.csv                  # Custom partition table (NVS, OTA, UI Assets)
├── sdkconfig.defaults.esp32        # Legacy ESP32 config
├── sdkconfig.defaults.esp32s3      # ESP32-S3 config
├── sdkconfig.defaults.esp32c6      # ESP32-C6 config
└── main/
    ├── CMakeLists.txt
    ├── main.cpp                    # Application entry point & FreeRTOS task init
    ├── audio/                      # I2S, ESP-SR, Opus
    ├── network/                    # Wi-Fi, 4G, WebSockets, MQTT
    ├── mcp/                        # MCP Router & GPIO control
    └── ui/                         # LVGL Display & Power Management
```

---

## 4. Build and Flash Instructions

This project requires **ESP-IDF v6.0.2** (with fallback support for v5.5).

1.  **Set the Target:** Choose the appropriate hardware target.
    ```bash
    idf.py set-target esp32s3
    ```
2.  **Configuration (Optional):**
    ```bash
    idf.py menuconfig
    ```
3.  **Build, Flash, and Monitor:**
    ```bash
    idf.py build flash monitor
    ```

---

## 5. Development History & Changelog

### Phase 1: Project Initialization (July 2026)
*   Initialized CMake and ESP-IDF workspace.
*   Created default `sdkconfig` files for cross-architecture support (`esp32`, `esp32s3`, `esp32c6`).
*   Designed the `partitions.csv` for OTA and data storage.
*   Stubbed out all major C++ class architectures for Audio, Networking, MCP, and UI subsystems to define the compilation boundaries.
*   Workspace moved to `/home/nammy/esp32_ai_chatbot`.
*   Python Backend configured to use free NVIDIA Build API (Llama-3.3-70B-Instruct).

### Phase 2: Hardware Bring-Up & Cloud Bridging (August 2026)
*   **Dependencies Fixed:** Added `espressif/esp_websocket_client` to the `idf_component.yml` to resolve ESP-IDF v5/v6 compatibility issues.
*   **Compile Fixes:** Patched `portMAX_DELAY` scope errors in `audio_hal.cpp` and `websocket_client.cpp` by injecting `FreeRTOS.h`.
*   **Wi-Fi Auto-Recovery:** Implemented a `wifi_event_handler` in `wifi_provisioning.cpp` to aggressively auto-reconnect to Wi-Fi if the router drops the connection (e.g., during a Channel Switch Announcement).
*   **Hardcoded Environment:** Injected the user's Wi-Fi credentials (`nammy`) and the local Python Backend IP (`10.46.164.186`).
*   **Successful Test:** Compiled and flashed cleanly to the ESP32 DevKit V1. Verified 2+ minutes of stable, continuous bidirectional WebSocket telemetry between the ESP32 and the backend.

---

## 6. Roadmap & Next Steps

To transition this architectural skeleton into a functional product, the following requirements must be addressed:

1.  **Backend Execution:** Keep `python3 server.py` actively running on the host machine (`10.46.164.186`) to prevent the ESP32 from encountering a `Connection reset by peer` WebSocket error.
2.  **MCP JSON Schema:** Define the exact Model Context Protocol schema used to differentiate between local device execution commands and cloud-forwarded context.
3.  **UI & Display Integration:** Activate the ST7789/SSD1306 LVGL display drivers to show visual feedback when the AI is talking.

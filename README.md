# 🐰 Chinchin Talkative Hardware Assistant

Welcome to the **Chinchin Talkative Hardware Assistant**! This is a cutting-edge, highly responsive AI hardware companion powered by an ESP32 microcontroller and a Python-based WebSocket backend.

Designed as a physical desktop assistant, "Chin Chin" features a fully fluid, 60FPS responsive UI rendering engine that brings a custom rabbit character to life. It features a conversational AI pipeline capable of real-time voice interaction, dynamic interruptions, context-aware memory, and autonomous multilingual capabilities.

## ✨ Key Features

### 🖥️ High-Performance Hardware UI
- **60FPS Custom Parallax Engine:** Drives a monochrome OLED display (SH1106) using a mathematically optimized offset rendering engine.
- **Topological Eye Squashing:** The rabbit logo features "true blinking" using real-time topological inverse-scaling on the original image pixels, ensuring sharp edges and lifelike fluidity.
- **Dynamic Organic Movement:** Uses sine-wave jitters and sway to simulate breathing, looking around, and expressive talking movements synced with AI speech.

### 🧠 Intelligent AI Pipeline
- **Real-Time Voice Interaction:** Streams I2S audio from the ESP32 to a WebSocket backend for ultra-low latency interaction.
- **Google Web Speech ASR:** Accurate, multilingual speech-to-text integration.
- **Minimax / OpenAI LLM:** Advanced conversational intelligence capable of adapting response length and context dynamically based on the interaction.
- **Edge-TTS Voice:** High-quality, fast text-to-speech synthesis (using `en-US-AriaNeural`).

### ⚡ Advanced Hardware Interaction
- **Zero-Latency Interruptions:** You can physically interrupt the AI while it is speaking by pressing a button. The ESP32 triggers an instant `audio_hal_flush_speaker()` command, immediately stopping playback and listening to your new input.
- **State Machine Architecture:** Flawless transitions between `IDLE`, `LISTENING`, `THINKING`, `SPEAKING`, and `HAPPY` states.

## 🛠️ Hardware Requirements

*   **Microcontroller:** ESP32 (WROOM or WROVER)
*   **Display:** 1.3" I2C OLED Display (SH1106, 128x64)
*   **Microphone:** INMP441 (I2S)
*   **Speaker/Amp:** MAX98357A (I2S)
*   **Input:** 4 Push Buttons

### Pin Configuration (ESP32)

| Component | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **I2C OLED** | SDA: 21, SCL: 22 | SH1106 Driver |
| **I2S Mic (INMP441)** | WS: 15, SCK: 2, SD: 13 | L/R to GND |
| **I2S Amp (MAX98357A)**| LRC: 25, BCLK: 26, DIN: 27 | 3W Speaker |
| **Buttons** | Pins 19, 18, 5, 17 | Pulled UP (Active Low) |

## 🚀 Installation & Setup

### 1. Backend Server Setup
The backend is a Python WebSocket server that handles ASR, LLM, and TTS processing.

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

You will need an `.env` file in the `backend` directory with your API keys:
```env
MINIMAX_API_KEY=your_minimax_key_here
MINIMAX_GROUP_ID=your_minimax_group_here
```

Start the server:
```bash
python3 websocket_server.py
```

### 2. ESP32 Firmware Setup
This project uses the official **ESP-IDF v5.2**.

```bash
# Source your ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure your Wi-Fi and WebSocket IP in menuconfig (if applicable) or directly in code
idf.py menuconfig

# Build and Flash
idf.py build
idf.py flash monitor
```

## 📂 Project Structure

*   `main/`: ESP-IDF C++ source code.
    *   `main.cpp`: Entry point and task scheduling.
    *   `audio_hal.cpp`: I2S configuration for the INMP441 and MAX98357A.
    *   `websocket_client.cpp`: Real-time streaming logic to the Python backend.
    *   `ui_manager.cpp`: The 60FPS fluid topological rendering engine.
*   `backend/`: Python server.
    *   `websocket_server.py`: Asyncio socket server.
    *   `ai_pipeline.py`: Orchestrates ASR, LLM context/memory, and TTS generation.
    *   `convert_logo.py`: Utility to perfectly extract transparent logos into the ESP32 `const uint8_t` array format.

## 🤝 Contributing
Contributions, issues, and feature requests are welcome! Feel free to check the issues page.

## 📝 License
This project is open-source.

---
*Created with ❤️ for a highly interactive, personalized desktop AI experience.*

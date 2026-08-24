import asyncio
import websockets
import json
import logging
from ai_pipeline import AIPipeline
import time
import random
import audioop
import re

# 100+ Emotion Parametric Mapping
# 100+ Emotion Dictionary mapping to UI States
# Available states: angry, happy, sad, idle, sleepy, thinking
EMOTION_MAP = {
    # Core Emotions
    "angry": "angry", "happy": "happy", "sad": "sad", 
    "cynical": "idle", "manic": "happy", "suspicious": "thinking", 
    "amused": "happy", "calculating": "thinking", "depressed": "sad", 
    "confused": "thinking", "neutral": "idle",
    
    # Hacker / Cyberpunk Extended Emotions
    "smug": "happy", "bored": "sleepy", "intense": "angry",
    "psychotic": "angry", "arrogant": "happy", "focused": "idle",
    "triumphant": "happy", "defeated": "sad", "mocking": "happy",
    "malicious": "angry", "annoyed": "angry", "curious": "thinking",
    "shocked": "thinking", "tired": "sleepy", "puzzled": "thinking",
    "elated": "happy", "furious": "angry", "sarcastic": "idle",
    "cold": "idle", "ruthless": "angry", "mischievous": "happy",
    "apathetic": "sleepy", "hostile": "angry", "glitchy": "thinking",
    "overloaded": "sleepy", "zen": "idle", "judgmental": "angry",
    "sinister": "angry", "alert": "idle", "panicked": "thinking",
    "impatient": "angry", "intrigued": "thinking", "disgusted": "angry"
}

def get_face_state(emotion: str):
    emotion = emotion.lower().strip()
    return EMOTION_MAP.get(emotion, "idle")

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Server")

pipeline = AIPipeline(llm_model="meta/llama-3.1-8b-instruct")

async def handle_client(websocket, path=""):
    logger.info(f"ESP32 Connected from {websocket.remote_address}")
    
    # Send a spoken greeting immediately on connection
    logger.info("Generating and sending startup greeting...")
    greeting = await pipeline.text_to_speech("System online. Hello! My name is Kiwi. How can I help you today?")
    if greeting:
        await websocket.send(greeting)
        
    last_interaction_time = time.time()
    is_user_speaking = False
    # Autonomous loop disabled as per user request to only respond when spoken to.
        
    audio_buffer = bytearray()
    silence_frames = 0
    is_processing = False
    
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                if is_processing:
                    continue
                    
                # Energy-based VAD (Voice Activity Detection)
                # ESP32 sends 16-bit PCM. audioop.rms calculates the energy.
                if len(message) % 2 != 0:
                    message = message[:-1]
                
                if len(message) == 0:
                    continue
                    
                rms = audioop.rms(message, 2)
                
                if rms > 500: # Threshold for voice (lowered to 500 to be more sensitive)
                    if not is_user_speaking:
                        is_user_speaking = True
                        logger.info(f"User started speaking (RMS: {rms})")
                        await websocket.send(json.dumps({"state": "listening"}))
                    silence_frames = 0
                    audio_buffer.extend(message)
                elif is_user_speaking:
                    audio_buffer.extend(message)
                    silence_frames += 1
                    # If silence for ~1.5s (assuming ~50ms frames, ~30 frames)
                    if silence_frames > 30:
                        is_user_speaking = False
                        is_processing = True
                        logger.info("User stopped speaking. Processing...")
                        await websocket.send(json.dumps({"state": "thinking"}))
                        
                        loop = asyncio.get_running_loop()
                        # 1. Ask ASR
                        text = await loop.run_in_executor(None, pipeline.speech_to_text, bytes(audio_buffer))
                        audio_buffer.clear()
                        
                        if text:
                            # 2. Ask LLM
                            llm_reply = await loop.run_in_executor(None, pipeline.generate_response, text)
                            
                            # Parse EMOTION tag from reply
                            emotion = "neutral"
                            clean_reply = llm_reply
                            
                            match = re.search(r"\[EMOTION:\s*([a-zA-Z]+)\](.*)", llm_reply, re.IGNORECASE | re.DOTALL)
                            if match:
                                emotion = match.group(1).lower()
                                clean_reply = match.group(2).strip()
                                
                            logger.info(f"Detected Emotion: {emotion}")
                            
                            # Send state update to ESP32
                            face_state = get_face_state(emotion)
                            state_json = json.dumps({"state": face_state})
                            await websocket.send(state_json)
                            
                            # 3. TTS (without the emotion tag)
                            audio_reply = await pipeline.text_to_speech(clean_reply)
                            if audio_reply:
                                # Send in 1024 byte chunks to prevent ESP-IDF websocket buffer overflow (default is 1024)
                                chunk_size = 1024
                                for i in range(0, len(audio_reply), chunk_size):
                                    await websocket.send(audio_reply[i:i+chunk_size])
                                
                        await websocket.send(json.dumps({"state": "idle"}))
                        is_processing = False
                        
            elif isinstance(message, str):
                last_interaction_time = time.time()
                try:
                    data = json.loads(message)
                    if data.get("method") == "mcp_event":
                        pass
                except json.JSONDecodeError:
                    pass
    except websockets.exceptions.ConnectionClosed:
        logger.info("ESP32 Disconnected")

async def main():
    port = 8765
    logger.info(f"Starting WebSocket server on ws://0.0.0.0:{port}")
    async with websockets.serve(handle_client, "0.0.0.0", port):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())

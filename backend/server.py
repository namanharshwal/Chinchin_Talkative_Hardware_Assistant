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
# Values are (slant, height, mouth_curve)
# Slant: -1.0 (happy/up), 0.0 (flat), 1.0 (angry/down)
# Height: 0.1 (closed), 1.0 (open), 1.5 (wide)
# Mouth: -1.0 (frown), 0.0 (flat), 1.0 (smile)
EMOTION_MAP = {
    # Core Emotions
    "angry": (1.0, 0.8, -1.0), "happy": (-0.8, 0.5, 1.0), "sad": (-0.5, 0.4, -1.0), 
    "cynical": (0.8, 0.6, -0.5), "manic": (-0.5, 1.5, 1.0), "suspicious": (0.9, 0.3, 0.0), 
    "amused": (-0.4, 0.6, 0.8), "calculating": (0.7, 0.5, 0.0), "depressed": (0.0, 0.2, -0.8), 
    "confused": (0.2, 0.9, -0.2), "neutral": (0.0, 0.8, 0.0),
    
    # Hacker / Cyberpunk Extended Emotions
    "smug": (0.6, 0.5, 0.7), "bored": (0.0, 0.3, -0.2), "intense": (1.0, 1.2, -0.5),
    "psychotic": (1.0, 1.5, 1.0), "arrogant": (0.8, 0.4, 0.5), "focused": (0.7, 0.6, 0.0),
    "triumphant": (-0.8, 1.0, 1.0), "defeated": (-0.2, 0.2, -1.0), "mocking": (0.5, 0.5, 0.8),
    "malicious": (1.0, 0.8, 0.9), "annoyed": (0.6, 0.4, -0.5), "curious": (0.0, 1.2, 0.3),
    "shocked": (-0.5, 1.5, -0.5), "tired": (0.1, 0.2, -0.1), "puzzled": (0.4, 0.8, -0.3),
    "elated": (-1.0, 1.2, 1.0), "furious": (1.0, 0.9, -1.0), "sarcastic": (0.8, 0.5, 0.6),
    "cold": (0.5, 0.5, -0.2), "ruthless": (0.9, 0.6, -0.8), "mischievous": (-0.2, 0.6, 0.9),
    "apathetic": (0.0, 0.3, 0.0), "hostile": (0.9, 0.7, -0.9), "glitchy": (0.5, 0.5, -0.5),
    "overloaded": (0.0, 1.5, -1.0), "zen": (-0.5, 0.2, 0.2), "judgmental": (0.8, 0.4, -0.4),
    "sinister": (1.0, 0.7, 0.8), "alert": (0.5, 1.3, 0.0), "panicked": (-0.5, 1.4, -0.8),
    "impatient": (0.7, 0.5, -0.3), "intrigued": (0.2, 1.1, 0.4), "disgusted": (0.8, 0.3, -0.7)
    # The system will mathematically fallback to neutral if an unknown emotion is generated
}

def get_face_params(emotion: str):
    emotion = emotion.lower().strip()
    return EMOTION_MAP.get(emotion, EMOTION_MAP["neutral"])

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Server")

pipeline = AIPipeline(llm_model="minimaxai/minimax-m3")

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
                
                if rms > 800: # Threshold for voice (increased to 800 to ignore background noise)
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
                            
                            # Send parametric UI update to ESP32
                            s, h, m = get_face_params(emotion)
                            face_json = json.dumps({"face": {"s": s, "h": h, "m": m}})
                            await websocket.send(face_json)
                            
                            # 3. TTS (without the emotion tag)
                            audio_reply = await pipeline.text_to_speech(clean_reply)
                            if audio_reply:
                                await websocket.send(audio_reply)
                                
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

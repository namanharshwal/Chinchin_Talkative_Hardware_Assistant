import asyncio
import websockets
import json
import logging
from ai_pipeline import AIPipeline
import time
import random
import audioop

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
    
    async def autonomous_loop():
        nonlocal last_interaction_time
        nonlocal is_user_speaking
        while True:
            await asyncio.sleep(5)
            if not is_user_speaking and (time.time() - last_interaction_time > random.uniform(20, 30)):
                logger.info("Autonomous trigger activated!")
                last_interaction_time = time.time() # Reset immediately to prevent spam
                
                # Ask LLM a random spontaneous prompt
                spontaneous_prompts = [
                    "Report on local network status.",
                    "Say something chaotic and cyberpunk.",
                    "Are there any vulnerable devices nearby?",
                    "Comment on how quiet it is."
                ]
                loop = asyncio.get_running_loop()
                llm_reply = await loop.run_in_executor(None, pipeline.generate_response, f"SYSTEM TRIGGER: {random.choice(spontaneous_prompts)}")
                
                if llm_reply:
                    audio_reply = await pipeline.text_to_speech(llm_reply)
                    if audio_reply:
                        logger.info(f"Sending autonomous TTS audio to ESP32 ({len(audio_reply)} bytes)")
                        try:
                            await websocket.send(audio_reply)
                        except:
                            break

    auto_task = asyncio.create_task(autonomous_loop())
        
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
                
                if rms > 500: # Threshold for voice (lowered for better sensitivity)
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
                            
                            # 3. TTS
                            audio_reply = await pipeline.text_to_speech(llm_reply)
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
    finally:
        auto_task.cancel()

async def main():
    port = 8765
    logger.info(f"Starting WebSocket server on ws://0.0.0.0:{port}")
    async with websockets.serve(handle_client, "0.0.0.0", port):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())

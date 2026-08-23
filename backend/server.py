import asyncio
import websockets
import json
import logging
from ai_pipeline import AIPipeline
import time
import random

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
    
    async def autonomous_loop():
        nonlocal last_interaction_time
        while True:
            await asyncio.sleep(5)
            if time.time() - last_interaction_time > random.uniform(20, 30):
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
    
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                # Received audio chunk from ESP32
                audio_buffer.extend(message)
                
            elif isinstance(message, str):
                last_interaction_time = time.time()
                logger.info(f"Received JSON from ESP32: {message}")
                try:
                    data = json.loads(message)
                    
                    if data.get("action") == "process_audio":
                        loop = asyncio.get_running_loop()
                        
                        # 1. Ask ASR (using thread pool for sync I/O)
                        # NOTE: Real implementations require a valid WAV header around raw PCM before API submission.
                        text = await loop.run_in_executor(None, pipeline.speech_to_text, bytes(audio_buffer))
                        audio_buffer.clear()
                        
                        if text:
                            # 2. Ask LLM (using thread pool)
                            llm_reply = await loop.run_in_executor(None, pipeline.generate_response, text)
                            
                            # 3. Convert to Voice (async)
                            audio_reply = await pipeline.text_to_speech(llm_reply)
                            
                            # 4. Stream Voice back to ESP32
                            if audio_reply:
                                logger.info(f"Sending TTS audio to ESP32 ({len(audio_reply)} bytes)")
                                await websocket.send(audio_reply)
                                
                    elif data.get("method") == "mcp_event":
                        # Future: Handle smart home context from ESP32 here
                        pass
                        
                except json.JSONDecodeError:
                    logger.error("Failed to parse JSON")
                    
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

import asyncio
import websockets
import json
import logging
from ai_pipeline import AIPipeline

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("Server")

pipeline = AIPipeline(llm_model="minimaxai/minimax-m3")

async def handle_client(websocket, path=""):
    logger.info(f"ESP32 Connected from {websocket.remote_address}")
    
    # Send a spoken greeting immediately on connection
    logger.info("Generating and sending startup greeting...")
    greeting = await pipeline.text_to_speech("System online. Hello! My name is Chin Chin. How can I help you today?")
    if greeting:
        await websocket.send(greeting)
        
    audio_buffer = bytearray()
    
    try:
        async for message in websocket:
            if isinstance(message, bytes):
                # Received audio chunk from ESP32
                audio_buffer.extend(message)
                
            elif isinstance(message, str):
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

async def main():
    port = 8765
    logger.info(f"Starting WebSocket server on ws://0.0.0.0:{port}")
    async with websockets.serve(handle_client, "0.0.0.0", port):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())

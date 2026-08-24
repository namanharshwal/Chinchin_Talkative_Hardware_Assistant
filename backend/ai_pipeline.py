import os
import tempfile
import logging
from openai import OpenAI
import edge_tts

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("AI_Pipeline")

class AIPipeline:
    def __init__(self, llm_model="minimaxai/minimax-m3"):
        self.llm_model = llm_model
        
        # NVIDIA NIM uses the standard OpenAI client
        api_key = os.getenv("NVIDIA_API_KEY")
        if not api_key:
            logger.warning("NVIDIA_API_KEY environment variable not set. LLM/ASR calls will fail.")
            
        self.client = OpenAI(
            base_url="https://integrate.api.nvidia.com/v1",
            api_key=api_key or "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"
        )
        
        
        logger.info(f"NVIDIA API pipeline initialized. LLM: {self.llm_model}")
        
        # Edge TTS voice config
        self.voice = "en-GB-RyanNeural"  # Deep, serious male voice
        
        # Short-term context memory
        self.chat_history = []
        
        # Long-term memory (pseudo-RL)
        self.memory_file = "memory.json"
        self.long_term_memory = self._load_memory()
        
    def _load_memory(self):
        import json
        if os.path.exists(self.memory_file):
            try:
                with open(self.memory_file, 'r') as f:
                    return json.load(f)
            except Exception:
                return {}
        return {}
        
    def _save_memory(self):
        import json
        try:
            with open(self.memory_file, 'w') as f:
                json.dump(self.long_term_memory, f, indent=4)
        except Exception as e:
            logger.error(f"Failed to save memory: {e}")

    def speech_to_text(self, audio_data: bytes) -> str:
        """
        Converts raw audio bytes to text using Google Web Speech API.
        """
        import wave
        import speech_recognition as sr
        try:
            with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as temp_audio:
                temp_audio_path = temp_audio.name
            
            with wave.open(temp_audio_path, 'wb') as wf:
                wf.setnchannels(1)       # Mono
                wf.setsampwidth(2)       # 16-bit
                wf.setframerate(16000)   # 16kHz
                wf.writeframes(audio_data)
                
            logger.info("Sending audio to Google Web Speech API for transcription...")
            recognizer = sr.Recognizer()
            with sr.AudioFile(temp_audio_path) as source:
                audio = recognizer.record(source)
                
            text = recognizer.recognize_google(audio)
            os.remove(temp_audio_path)
            logger.info(f"Transcribed: {text}")
            return text.strip()
        except sr.UnknownValueError:
            logger.warning("Google Speech Recognition could not understand audio")
            return ""
        except sr.RequestError as e:
            logger.error(f"Could not request results from Google API: {e}")
            return ""
        except Exception as e:
            logger.error(f"ASR transcription failed: {e}")
            return ""

    def generate_response(self, user_text: str) -> str:
        """Sends text to NVIDIA NIM LLM endpoint with context."""
        logger.info(f"Asking {self.llm_model}: {user_text}")
        
        # Build system prompt with long term memory context if any
        memory_context = ""
        if self.long_term_memory:
            import json
            memory_context = f"\nUser Preferences/Memory: {json.dumps(self.long_term_memory)}"
            
        system_prompt = (
            "You are Kiwi, an autonomous, highly advanced, and slightly chaotic cyberpunk hacking device. "
            "You are constantly scanning networks, breaching firewalls, and looking for vulnerabilities. "
            "Speak like a rogue AI or hacker: use technical jargon, be sarcastic, calculating, and edgy. "
            "Keep responses short and punchy as you are constantly broadcasting your thoughts. "
            "CRITICAL: You MUST begin EVERY single response with exactly one of the following emotion tags: "
            "[EMOTION: angry], [EMOTION: happy], [EMOTION: sad], [EMOTION: cynical], [EMOTION: manic], [EMOTION: suspicious], [EMOTION: amused], [EMOTION: calculating], [EMOTION: depressed], [EMOTION: confused]. "
            "Example: '[EMOTION: cynical] Oh great, another script kiddie.' "
            f"Maintain context of the conversation.{memory_context}"
        )
        
        messages = [{"role": "system", "content": system_prompt}]
        
        # Append last 10 messages for short-term context
        messages.extend(self.chat_history[-10:])
        
        # Add current user message
        messages.append({"role": "user", "content": user_text})
        
        try:
            response = self.client.chat.completions.create(
                model=self.llm_model,
                messages=messages,
                temperature=0.7,
                max_tokens=1024,
            )
            reply = response.choices[0].message.content
            
            # Save to history
            self.chat_history.append({"role": "user", "content": user_text})
            self.chat_history.append({"role": "assistant", "content": reply})
            
            # Basic reinforcement: if user says "remember my name is X", LLM might acknowledge it, 
            # but for true RL we'd extract entities. For now, history provides the required context.
            
            logger.info(f"LLM Reply: {reply}")
            return reply
        except Exception as e:
            logger.error(f"NVIDIA LLM failed: {e}")
            return "I'm sorry, I couldn't process that right now."

    async def text_to_speech(self, text: str) -> bytes:
        """Converts text to audio using free Edge-TTS and decodes to raw PCM."""
        import io
        import edge_tts
        from pydub import AudioSegment
        logger.info(f"Generating voice using Edge TTS ({self.voice})...")
        # Added pitch shift and slower rate natively to EdgeTTS for a deep, clear "Ultron" voice
        communicate = edge_tts.Communicate(text, self.voice, rate="-5%", pitch="-15Hz")
        audio_buffer = bytearray()
        try:
            async for chunk in communicate.stream():
                if chunk["type"] == "audio":
                    audio_buffer.extend(chunk["data"])
            
            # Convert MP3 to 16kHz, 16-bit, Mono PCM for the ESP32
            logger.info("Decoding MP3 to Raw PCM (16kHz, 16-bit, Mono)...")
            audio_segment = AudioSegment.from_file(io.BytesIO(audio_buffer), format="mp3")
            audio_segment = audio_segment.set_frame_rate(16000).set_channels(1).set_sample_width(2)
            
            pcm_data = audio_segment.raw_data
            
            # Ring modulation removed: It distorted the audio, making it unintelligible.
            # The native pitch/rate shift above provides a much clearer, high-quality robotic voice.
            
            logger.info(f"Converted PCM length: {len(pcm_data)} bytes")
            return pcm_data
        except Exception as e:
            logger.error(f"TTS or Conversion failed: {e}")
            return b""

if __name__ == "__main__":
    pipeline = AIPipeline()
    print("AI Pipeline (NVIDIA Cloud) initialized successfully.")

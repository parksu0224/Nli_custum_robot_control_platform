import os
import queue
import subprocess
import tempfile
import threading

import soundfile as sf
from gtts import gTTS

from bridge_io import (
    send_speaking_state_to_cpp,
    send_narration_to_cpp,
    hide_narration_on_cpp,
)


ALSA_DEVICE = "plughw:CARD=UACDemoV10,DEV=0"

_tts_queue = queue.Queue()
_tts_thread = None
_tts_initialized = False


def initialize_tts():
    """
    Initializes the TTS function once.
    Called once from main.py when the program starts.
    """
    global _tts_thread
    global _tts_initialized

    if _tts_initialized:
        return

    _tts_initialized = True

    _tts_thread = threading.Thread(
        target=_tts_worker_loop,
        daemon=True,
        name="tts-worker",
    )
    _tts_thread.start()

    print(f"[TTS] initialized: {ALSA_DEVICE}", flush=True)
    

def speak(text):
    """
    Places the string to be spoken into the queue.

    Use from another file as follows:
        from tts import speak
        speak("Hello")
    """
    if text is None:
        return

    text = str(text).strip()

    if not text:
        return

    if not _tts_initialized:
        initialize_tts()

    _tts_queue.put(text)


def _tts_worker_loop():
    while True:
        text = _tts_queue.get()

        try:
            _play_text(text)

        except Exception as error:
            print(f"[TTS ERROR] {error}", flush=True)

        finally:
            _tts_queue.task_done()

def speak_narration(text):
    speak(text)


def speak_and_wait(text):
    """Queue one TTS utterance and wait until queued TTS playback is complete."""
    speak(text)
    _tts_queue.join()


def _play_text(text):
    mp3_path = None
    wav_path = None

    try:
        print(f"[TTS TEXT] {text}", flush=True)

        with tempfile.NamedTemporaryFile(
            suffix=".mp3",
            delete=False,
        ) as mp3_file:
            mp3_path = mp3_file.name

        with tempfile.NamedTemporaryFile(
            suffix=".wav",
            delete=False,
        ) as wav_file:
            wav_path = wav_file.name

        # Text → MP3
        gTTS(
            text=text,
            lang="en", ##language
            slow=False,
        ).save(mp3_path)

        # MP3 → WAV
        audio_data, sample_rate = sf.read(
            mp3_path,
            dtype="float32",
        )

        sf.write(
            wav_path,
            audio_data,
            sample_rate,
            subtype="PCM_16",
        )

        # Show exactly what the robot is about to say on Nextion g0.
        # C++ sends g0.en=1 first and then updates g0.txt.
        send_narration_to_cpp(text)

        # USB speaker output.
        # Mouth animation is enabled only while aplay is actually running.
        send_speaking_state_to_cpp(True)

        try:
            result = subprocess.run(
                [
                    "aplay",
                    "-q",
                    "-D",
                    ALSA_DEVICE,
                    wav_path,
                ],
                capture_output=True,
                text=True,
                timeout=120,
            )
        finally:
            # Always stop the mouth even if playback fails or times out.
            send_speaking_state_to_cpp(False)

            # The full utterance is finished, so hide the narration text.
            # This also applies to the one-time startup "Activated" cue.
            hide_narration_on_cpp()

        if result.returncode != 0:
            raise RuntimeError(
                result.stderr.strip()
                or f"aplay return code: {result.returncode}"
            )

        print("[TTS] playback complete", flush=True)

    finally:
        for file_path in (mp3_path, wav_path):
            if file_path and os.path.exists(file_path):
                try:
                    os.remove(file_path)
                except OSError:
                    pass
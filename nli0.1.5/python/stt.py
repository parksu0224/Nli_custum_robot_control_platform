import io
import math
import subprocess
import time
from datetime import datetime

import numpy as np
import soundfile as sf
import speech_recognition as sr

import config
from camera_emotion import get_user_emotion
from bridge_io import send_recording_state_to_cpp
from tts import speak_and_wait
import state
from module_handler import enqueue_llm_request, get_latest_report
from nli_protocol import make_voice_report_code


def get_flac_data_without_binary(self, convert_rate=None, convert_width=None):
    wav_data = self.get_wav_data(
        convert_rate=convert_rate,
        convert_width=convert_width,
    )

    audio_data, sample_rate = sf.read(
        io.BytesIO(wav_data),
        dtype="int16",
        always_2d=False,
    )

    flac_buffer = io.BytesIO()
    sf.write(
        flac_buffer,
        audio_data,
        sample_rate,
        format="FLAC",
        subtype="PCM_16",
    )
    return flac_buffer.getvalue()


def install_flac_patch():
    sr.AudioData.get_flac_data = get_flac_data_without_binary


def calc_rms_peak(raw_bytes):
    samples = np.frombuffer(raw_bytes, dtype="<i2").astype(np.float32)

    if len(samples) == 0:
        return 0.0, 0

    samples = samples - np.mean(samples)
    rms = math.sqrt(np.mean(samples * samples))
    peak = int(np.max(np.abs(samples)))
    return rms, peak


def record_raw_audio(record_count):
    print()
    print(f"========== RECORD {record_count} ==========")
    print(f"[REC] {config.RECORD_SECONDS}s recording")

    command = [
        "arecord", "-q",
        "-D", config.MIC_DEVICE,
        "-f", "S16_LE",
        "-r", str(config.SAMPLE_RATE),
        "-c", str(config.CHANNELS),
        "-d", str(config.RECORD_SECONDS),
        "-t", "raw",
        "-",
    ]

    # Turn on the Nextion REC indicator only while arecord is actually running.
    send_recording_state_to_cpp(True)

    try:
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                timeout=config.RECORD_SECONDS + 5,
                check=False,
            )

        except subprocess.TimeoutExpired:
            print("[REC ERROR] arecord timed out")
            return None, 0.0, 0

        except Exception as error:
            print("[REC ERROR] Failed to run arecord:", error)
            return None, 0.0, 0

        if result.returncode != 0:
            print("[REC ERROR] arecord failed")
            error_text = result.stderr.decode(errors="ignore").strip()
            if error_text:
                print(error_text)
            return None, 0.0, 0

        raw_audio = result.stdout
        rms, peak = calc_rms_peak(raw_audio)

        print(
            f"[AUDIO] RMS={rms:.1f}, "
            f"PEAK={peak}, bytes={len(raw_audio)}"
        )
        return raw_audio, rms, peak

    finally:
        # Always turn the recording indicator OFF so it does not remain on screen even when an error/return occurs.
        send_recording_state_to_cpp(False)


def save_wav(raw_audio):
    samples = np.frombuffer(raw_audio, dtype="<i2")
    filename = datetime.now().strftime("usb_record_%Y%m%d_%H%M%S.wav")

    sf.write(
        filename,
        samples,
        config.SAMPLE_RATE,
        subtype="PCM_16",
    )

    print("[SAVE]", filename)
    return filename


def speech_to_text(raw_audio):
    audio = sr.AudioData(
        raw_audio,
        config.SAMPLE_RATE,
        config.SAMPLE_WIDTH,
    )
    recognizer = sr.Recognizer()

    try:
        text = recognizer.recognize_google(audio, language="en-US")
        text = str(text).strip()

        print()
        print("========== USER VOICE ==========")
        print("[STT]", text)
        print("================================")
        return text

    except sr.UnknownValueError:
        print("[STT] Could not recognize speech")
        return ""

    except sr.RequestError as error:
        print("[STT ERROR] Google request failed:", error)
        return ""

    except Exception as error:
        print("[STT ERROR]", error)
        return ""


def continuous_recording_loop():
    record_count = 1
    print()
    print("[MIC] Continuous voice recognition started")

    # One-time boot/activation voice cue. Wait for it to finish before
    # the first arecord call so the microphone does not capture "Activated".
    print("[BOOT VOICE] Activated", flush=True)
    speak_and_wait("Activated")

    while state.is_running():
        raw_audio, rms, peak = record_raw_audio(record_count)
        record_count += 1

        if raw_audio is None:
            time.sleep(1.0)
            continue

        if config.SAVE_WAV:
            save_wav(raw_audio)

        if rms < config.VOICE_RMS_THRESHOLD:
            print(
                f"[MIC] Silence skipped: RMS {rms:.1f} "
                f"< {config.VOICE_RMS_THRESHOLD}"
            )
            time.sleep(config.RECORD_RETRY_DELAY)
            continue

        user_text = speech_to_text(raw_audio)

        if not user_text:
            time.sleep(config.RECORD_RETRY_DELAY)
            continue

        if "stop voice recognition" in user_text or "stop recording" in user_text:
            print("[MIC] Voice recognition loop stopped")
            break

        stored_report = get_latest_report()

        try:
            voice_report_code = make_voice_report_code(
                original_report_code=stored_report,
                user_text=user_text,
                user_emotion=get_user_emotion(),
            )

        except ValueError as error:
            print("[VOICE CODE ERROR]", error)
            time.sleep(config.RECORD_RETRY_DELAY)
            continue

        print()
        print("========== VOICE -> NLI ==========")
        print("[BASE R]", stored_report if stored_report else "NONE (NORMAL CHAT)")
        print("[USER TEXT]", user_text)
        print("[FINAL INPUT]", voice_report_code)
        print("==================================")

        enqueue_llm_request(
            nli_code=voice_report_code,
            request_source="USER_VOICE",
        )

        time.sleep(config.RECORD_RETRY_DELAY)

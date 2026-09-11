import threading
import time

from arduino.app_utils import App

import config
from code_mirror import start_code_mirror_worker
from camera_emotion import start_camera_worker
from module_handler import register_bridge_handlers as register_module_bridge
from hf_client import hf_worker_loop
from stt import continuous_recording_loop, install_flac_patch
from tcp_client import (
    chat_client,
    register_bridge_handlers as register_tcp_bridge,
)

from tts import initialize_tts, speak_narration


def main_loop():
    # The actual work is handled by background threads.
    time.sleep(0.1)


def start_threads():
    tcp_receive_thread = threading.Thread(
        target=chat_client.receive_loop,
        daemon=True,
        name="tcp-receive",
    )
    tcp_receive_thread.start()

    recording_thread = threading.Thread(
        target=continuous_recording_loop,
        daemon=True,
        name="stt-recording",
    )
    recording_thread.start()

    for worker_index in range(config.HF_WORKER_COUNT):
        worker_thread = threading.Thread(
            target=hf_worker_loop,
            args=(worker_index + 1,),
            daemon=True,
            name=f"hf-worker-{worker_index + 1}",
        )
        worker_thread.start()


def main():
    # Do not run automatically on import; initialize explicitly in main.
    install_flac_patch()

    initialize_tts()
    #speak_narration("This is a TTS connection test.") # for TTS speaker testing
    
    register_module_bridge()
    register_tcp_bridge()

    # Load the TCP destination from C++'s cached Nextion setting.tcp_ip.txt before
    # starting the receive thread. connect() will refresh it again on reconnect.
    chat_client.refresh_host_from_cpp()

    # Dedicated Nextion mirror thread for NLI codes. Runs completely separately from existing processing.
    start_code_mirror_worker()

    # Camera emotion recognition also runs in the background in the same Python process as NLI
    start_camera_worker()
    start_threads()

    print("========================================")
    print("Arduino UNO Q NLI0.1.3 Core Ready")
    print("I register + HF / R report + HF / normal chat + HF")
    print("Voice works with or without a saved R report")
    print("O response -> command to C++ immediately")
    print("========================================")
    print(f"ESP address (Nextion -> C++ -> Python): {chat_client.host}:{config.ESP_PORT}")
    print(f"Mic device: {config.MIC_DEVICE}")
    print(f"Voice RMS threshold: {config.VOICE_RMS_THRESHOLD}")
    print(f"HF model: {config.HF_MODEL}")
    print(f"RAG files: {config.RAG_SOURCE_FILES}")

    App.run(user_loop=main_loop)


if __name__ == "__main__":
    main()

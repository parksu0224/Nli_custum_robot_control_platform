import queue
import threading

# State and locks shared across the entire program
registered_modules = {}
registered_modules_lock = threading.Lock()

latest_report_code = None
latest_report_lock = threading.Lock()

llm_request_queue = queue.Queue(maxsize=100)


bridge_call_lock = threading.Lock()
tts_lock = threading.Lock()

running = True


def is_running():
    return running


def stop():
    global running
    running = False

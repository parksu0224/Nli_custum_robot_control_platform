import queue
import threading

from arduino.app_utils import Bridge

import state


_CODE_QUEUE_SIZE = 100
_code_queue = queue.Queue(maxsize=_CODE_QUEUE_SIZE)
_worker_started = False
_worker_start_lock = threading.Lock()


def mirror_code(code):
    """Copies code strings asynchronously to the C++ Nextion display queue.

    This never blocks the original NLI/communication processing. Even if the C++ Bridge is temporarily busy or
    the Nextion display fails, the existing logic continues unchanged.
    """
    text = str(code).strip()

    if not text:
        return False

    # Prevent excessively long strings from being placed in a single Nextion component
    if len(text) > 800:
        text = text[:800]

    try:
        _code_queue.put_nowait(text)
        return True
    except queue.Full:
        return False


def _code_mirror_worker():
    while state.is_running():
        try:
            code = _code_queue.get(timeout=0.2)
        except queue.Empty:
            continue

        try:
            # Use the same lock as the existing Python -> C++ RPC to prevent Bridge call collisions.
            with state.bridge_call_lock:
                Bridge.call("show_code", code)
        except Exception:
            # A display failure must not affect the original robot operation.
            pass
        finally:
            _code_queue.task_done()


def start_code_mirror_worker():
    global _worker_started

    with _worker_start_lock:
        if _worker_started:
            return

        worker = threading.Thread(
            target=_code_mirror_worker,
            daemon=True,
            name="nextion-code-mirror",
        )
        worker.start()
        _worker_started = True

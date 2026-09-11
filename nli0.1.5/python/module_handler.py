import queue
import time

from arduino.app_utils import Bridge

import config
import state
from code_mirror import mirror_code
from camera_emotion import get_user_emotion
from nli_protocol import make_core_chat_code, parse_single_nli_code


def save_latest_report(report_code):
    """Stores the most recent R status report for STT combination."""
    with state.latest_report_lock:
        state.latest_report_code = str(report_code).strip()

    print()
    print("========== LATEST R SAVED ==========")
    print(state.latest_report_code)
    print("====================================")


def get_latest_report():
    with state.latest_report_lock:
        if state.latest_report_code is None:
            return None
        return str(state.latest_report_code)


def enqueue_llm_request(nli_code, request_source):
    # Copy the NLI code that exists at this point to Nextion regardless of actual HF transmission success
    mirror_code(nli_code)

    item = {
        "code": str(nli_code).strip(),
        "source": str(request_source),
        "queued_at": time.time(),
    }

    try:
        state.llm_request_queue.put_nowait(item)

        print()
        print("========== HF REQUEST QUEUED ==========")
        print("[SOURCE]", request_source)
        print("[CODE]", nli_code)
        print("[QUEUE SIZE]", state.llm_request_queue.qsize())
        print("=======================================")
        return True

    except queue.Full:
        print("[QUEUE ERROR] Hugging Face request queue is full")
        return False


def _register_module(module_id, module_code):
    with state.registered_modules_lock:
        state.registered_modules[module_id] = {
            "initial_code": module_code,
            "registered_at": time.time(),
        }

    print()
    print("========== MODULE INITIAL ==========")
    print("[MODULE ID]", module_id)
    print("[INITIAL CODE]", module_code)
    print("[MODULE] I-type currently connected module registration complete")
    print("====================================")


def _queue_plain_chat(user_text):
    chat_code = make_core_chat_code(
        user_text=user_text,
        user_emotion=get_user_emotion(),
    )

    print()
    print("========== DIRECT CHAT -> NLI ==========")
    print("[USER TEXT]", user_text)
    print("[NORMALIZED]", chat_code)
    print("=========================================")

    queued = enqueue_llm_request(
        nli_code=chat_code,
        request_source="DIRECT_CHAT",
    )
    return "CHAT_QUEUED" if queued else "QUEUE_FULL"


def receive_module_info(core_input):
    """Processes input entering the Python core from C++.

    Processing rules:
    - I code: register currently connected module + call Hugging Face
    - R code: save latest status + call Hugging Face
    - Normal sentence: convert to module-free conversation R code + call Hugging Face

    In other words, the LLM call itself does not require an R code to exist.
    """
    core_input = str(core_input).strip()

    print()
    print("========== C++ -> PYTHON CORE ==========")
    print(core_input)
    print("========================================")

    if not core_input:
        return "EMPTY_INPUT"

    parsed = parse_single_nli_code(core_input)

    if parsed is None:
        # Treat input beginning with a bracket as intended to be an NLI code
        # and do not mistake a format error for normal conversation.
        if core_input.startswith("[") or core_input.endswith("]"):
            print("[CORE ERROR] Invalid 6-part partition format")
            return "INVALID_FORMAT"

        return _queue_plain_chat(core_input)

    code_type = parsed["code_type"].upper().strip()
    module_id = parsed["module_id"].strip()

    if code_type == "I":
        if not module_id or module_id.lower() == "none":
            print("[MODULE ERROR] I type has no module ID")
            return "INVALID_MODULE_ID"

        _register_module(module_id, core_input)

        queued = enqueue_llm_request(
            nli_code=core_input,
            request_source="MODULE_INITIAL",
        )

        return (
            "INITIAL_REGISTERED_AND_QUEUED"
            if queued
            else "INITIAL_REGISTERED_QUEUE_FULL"
        )

    if code_type == "R":
        save_latest_report(core_input)

        queued = enqueue_llm_request(
            nli_code=core_input,
            request_source="MODULE_REPORT",
        )

        return "REPORT_QUEUED" if queued else "QUEUE_FULL"

    print("[CORE ERROR] Input NLI code must be I or R")
    return "INVALID_CODE_TYPE"


def register_bridge_handlers():
    Bridge.provide("receive_module_info", receive_module_info)

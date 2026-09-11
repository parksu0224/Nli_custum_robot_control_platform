from arduino.app_utils import Bridge

import state


def send_command_to_cpp(command):
    """Sends only the ③ command of an external LLM O code to C++."""
    command = str(command).strip()

    if not command or command.lower() == "none":
        print("[BRIDGE] Empty command; not sending")
        return False

    try:
        print()
        print("========== PYTHON -> C++ ==========")
        print("[COMMAND]", command)
        print("==================================")

        with state.bridge_call_lock:
            result = Bridge.call("receive_command", command)

        print("[C++ RESPONSE]", result)
        return True

    except Exception as error:
        print("[BRIDGE ERROR]", error)
        return False


def send_esp_message_to_cpp(message):
    """Delivers I/R codes received from the ESP to C++ show_remote_message."""
    try:
        with state.bridge_call_lock:
            result = Bridge.call("show_remote_message", str(message))

        print("[C++ RESPONSE]", result)

    except Exception as error:
        print("[BRIDGE ERROR] Failed to deliver module code to C++:", error)

def send_recording_state_to_cpp(is_recording):
    """Delivers the current microphone recording state through C++ -> Nextion."""
    state_text = "1" if is_recording else "0"

    try:
        with state.bridge_call_lock:
            result = Bridge.call("set_recording_state", state_text)

        print("[REC DISPLAY]", result)
        return True

    except Exception as error:
        print("[REC DISPLAY ERROR]", error)
        return False

def send_face_expression_to_cpp(expression):
    """Sends the already-decided NLI partition ④ face expression to C++."""
    expression = str(expression).strip()

    if not expression or expression.lower() == "none":
        print("[FACE BRIDGE] Empty face expression; not sending")
        return False

    try:
        print()
        print("========== FACE PYTHON -> C++ ==========")
        print("[EXPRESSION]", expression)
        print("=========================================")

        with state.bridge_call_lock:
            result = Bridge.call("set_face_expression", expression)

        print("[FACE C++ RESPONSE]", result)
        return True

    except Exception as error:
        print("[FACE BRIDGE ERROR]", error)
        return False


def send_speaking_state_to_cpp(is_speaking):
    """Controls Nextion mouth animation only during actual TTS playback."""
    state_text = "1" if is_speaking else "0"

    try:
        with state.bridge_call_lock:
            result = Bridge.call("set_speaking_state", state_text)

        print("[MOUTH DISPLAY]", result, flush=True)
        return True

    except Exception as error:
        print("[MOUTH DISPLAY ERROR]", error, flush=True)
        return False



def send_narration_to_cpp(text):
    """Shows the robot's spoken narration on Nextion g0 through C++."""
    text = str(text).strip()

    if not text or text.lower() == "none":
        return False

    try:
        with state.bridge_call_lock:
            result = Bridge.call("show_narration", text)

        print("[NARRATION DISPLAY]", result, flush=True)
        return True

    except Exception as error:
        print("[NARRATION DISPLAY ERROR]", error, flush=True)
        return False

def hide_narration_on_cpp():
    """Disables Nextion g0 after one complete TTS utterance finishes."""
    try:
        with state.bridge_call_lock:
            result = Bridge.call("hide_narration", "0")

        print("[NARRATION DISPLAY]", result, flush=True)
        return True

    except Exception as error:
        print("[NARRATION DISPLAY ERROR]", error, flush=True)
        return False



def get_tcp_ip_from_cpp():
    """Read the IPv4 string cached by C++ from Nextion setting.tcp_ip.txt."""
    try:
        with state.bridge_call_lock:
            result = Bridge.call("get_tcp_ip", "0")

        ip_address = str(result).strip()

        if not ip_address:
            print("[TCP IP] C++ has no valid Nextion setting.tcp_ip.txt yet", flush=True)
            return None

        parts = ip_address.split(".")

        if len(parts) != 4:
            print(f"[TCP IP ERROR] Invalid C++ IP string: {ip_address}", flush=True)
            return None

        try:
            octets = [int(part) for part in parts]
        except ValueError:
            print(f"[TCP IP ERROR] Invalid C++ IP string: {ip_address}", flush=True)
            return None

        if any(octet < 0 or octet > 255 for octet in octets):
            print(f"[TCP IP ERROR] Out-of-range C++ IP string: {ip_address}", flush=True)
            return None

        print(f"[TCP IP] Nextion -> C++ -> Python: {ip_address}", flush=True)
        return ip_address

    except Exception as error:
        print("[TCP IP BRIDGE ERROR]", error, flush=True)
        return None

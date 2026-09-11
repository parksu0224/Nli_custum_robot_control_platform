import socket
import threading
import time

from arduino.app_utils import Bridge

import config
import state
from bridge_io import get_tcp_ip_from_cpp, send_esp_message_to_cpp


class ESPChatClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port

        self.sock = None
        self.sock_lock = threading.Lock()

        self.running = True
        self.connected = False
        self.receive_buffer = b""

    def refresh_host_from_cpp(self):
        """Refresh the TCP host from C++'s cached Nextion setting.tcp_ip.txt value."""
        host = get_tcp_ip_from_cpp()

        if not host:
            return False

        if host != self.host:
            print(f"[TCP] Host updated from C++: {self.host} -> {host}")
            self.host = host

        return True

    def connect(self):
        self.close()

        # Always ask C++ for the current cached address before connecting.
        # C++ owns the Nextion UART read; Python never reads the HMI directly.
        if not self.refresh_host_from_cpp():
            print("[TCP ERROR] No valid IP available from Nextion/C++")
            return False

        try:
            print(f"[TCP] Attempting ESP connection: {self.host}:{self.port}")

            new_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            new_socket.settimeout(5.0)
            new_socket.connect((self.host, self.port))
            new_socket.settimeout(None)

            with self.sock_lock:
                self.sock = new_socket
                self.connected = True
                self.receive_buffer = b""

            print("[TCP] ESP connection successful")
            return True

        except OSError as error:
            print("[TCP ERROR] Connection failed:", error)
            self.close()
            return False

    def send(self, message):
        message = str(message).strip()

        if not message:
            return False

        with self.sock_lock:
            current_socket = self.sock

        if current_socket is None or not self.connected:
            print("[TCP ERROR] Not connected to ESP")
            return False

        try:
            current_socket.sendall((message + "\n").encode("utf-8"))

            print()
            print("========== AQ -> MODULE ==========")
            print(message)
            print("=================================")
            return True

        except OSError as error:
            print("[TCP ERROR] Send failed:", error)
            self.close()
            return False

    def receive_loop(self):
        while self.running and state.is_running():
            if not self.connected:
                if not self.connect():
                    time.sleep(config.RECONNECT_DELAY)
                    continue

            with self.sock_lock:
                current_socket = self.sock

            if current_socket is None:
                time.sleep(config.RECONNECT_DELAY)
                continue

            try:
                data = current_socket.recv(1024)

                if not data:
                    raise ConnectionError("ESP closed the TCP connection")

                self.receive_buffer += data

                while b"\n" in self.receive_buffer:
                    line, self.receive_buffer = self.receive_buffer.split(b"\n", 1)
                    message = line.decode("utf-8", errors="replace").strip()

                    if not message:
                        continue

                    print()
                    print("========== MODULE -> TCP PYTHON ==========")
                    print(message)
                    print("===========================================")

                    send_esp_message_to_cpp(message)

            except (OSError, ConnectionError) as error:
                print("[TCP ERROR] Connection lost:", error)
                self.close()
                time.sleep(config.RECONNECT_DELAY)

    def close(self):
        with self.sock_lock:
            current_socket = self.sock
            self.sock = None
            self.connected = False
            self.receive_buffer = b""

        if current_socket is not None:
            try:
                current_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass

            try:
                current_socket.close()
            except OSError:
                pass


chat_client = ESPChatClient(None, config.ESP_PORT)


def send_to_esp(message):
    try:
        return chat_client.send(str(message))
    except Exception as error:
        print("[SEND ERROR]", error)
        return False


def register_bridge_handlers():
    Bridge.provide("send_to_esp", send_to_esp)

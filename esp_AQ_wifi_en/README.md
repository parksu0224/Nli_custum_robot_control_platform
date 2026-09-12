# ESP8266 Wi-Fi Communication Module

[System overview](../README.md)

`esp_AQ_wifi_en.ino` implements a TCP server for ESP8266 / ESP-12F. After UNO Q connects as a client, the sketch forwards ESP serial input to UNO Q and prints incoming messages to Serial Monitor. It does not include complete motor or sensor control logic.

## Upload and configure

1. Prepare Arduino IDE with ESP8266 board support.
2. Open `esp_AQ_wifi_en.ino`. Preserve the matching directory and sketch names.
3. Replace the `WIFI_SSID` and `WIFI_PASSWORD` placeholders with your local settings. Do not commit actual credentials.
4. Select your ESP8266 board and port, then upload. Follow your board requirements for power and upload mode.
5. Open Serial Monitor at **115200 baud** with **Newline** selected.
6. Check for `[WiFi] Connection successful` and `[WiFi] ESP IP:`.
7. Enter that ESP IP in Nextion `setting.tcp_ip.txt`, then start the UNO Q application.

The sketch uses `ESP8266WiFi.h`. ESP32 support and builds have not been verified; do not assume changing the board selection alone is sufficient.

## Protocol and communication test

- The server uses `SERVER_PORT = 5000`, matching UNO Q `ESP_PORT`.
- The implementation handles one active UNO Q client.
- Messages are newline-delimited; carriage returns are ignored. Each line buffer accepts up to 500 characters.
- A successful connection prints `[CONNECT] UNO Q connected:`.
- Sending `Hello` from ESP Serial Monitor produces an `[ESP -> UNO Q]` log. Incoming UNO Q messages produce `[UNO Q]` logs.

ESP and UNO Q must be able to reach each other over TCP. For initial setup, use the same local network and check wireless client isolation settings.

## Extending this into a device module

Replace serial input with sensor reports or registration messages, and add handlers that interpret received commands and drive your hardware. See the [NLI guide](../nli0.1.5/README.md) for `I` registration, `R` reporting, and `O` command formats. Receiving TCP data does not automatically operate an actuator.

## Connection troubleshooting

Check Wi-Fi credentials, the ESP IP printed in Serial Monitor, the Nextion IP value, and port 5000. If the ESP IP changes, update Nextion and restart the UNO Q application.

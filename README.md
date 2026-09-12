# NLI Custom Robot Control Platform

Related components are maintained in separate folders:

- `nli0.1.5/`: NLI application and UNO Q code.
- `esp_AQ_wifi_en/`: ESP8266 (ESP-12F) TCP server for communication with UNO Q.

## ESP8266 setup

1. Open `esp_AQ_wifi_en/esp_AQ_wifi_en.ino` in Arduino IDE with ESP8266 board support installed.
2. Replace `YOUR_WIFI_SSID` and `YOUR_WIFI_PASSWORD` locally with your Wi-Fi settings. Do not commit actual credentials.
3. Select your ESP8266 board and port, then upload the sketch.
4. Open Serial Monitor at 115200 baud with Newline enabled. The sketch prints the ESP IP address.
5. Configure the UNO Q client to connect to that IP address on TCP port 5000 over the same reachable network.

The ESP forwards newline-terminated Serial Monitor input to the connected UNO Q client and prints messages received from it.

Credentials required by the NLI component are excluded from this repository and must be configured locally.

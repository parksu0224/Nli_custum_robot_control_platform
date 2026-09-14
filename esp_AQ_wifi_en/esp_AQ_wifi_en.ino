#include <WiFi.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const uint16_t SERVER_PORT = 5000;

WiFiServer server(SERVER_PORT);
WiFiClient unoQClient;

String serialInput = "";
String networkInput = "";

void connectWiFi();
void checkNewClient();
void readSerialMonitor();
void readFromUnoQ();

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 TCP CHAT SERVER");
  Serial.println("==============================");

  connectWiFi();
  server.begin();
  server.setNoDelay(true);

  Serial.print("[SERVER] TCP port: ");
  Serial.println(SERVER_PORT);
  Serial.println("[CHAT] Enter a sentence in Serial Monitor.");
  Serial.println("[CHAT] Line ending: Newline");
}

void loop() {
  checkNewClient();
  readSerialMonitor();
  readFromUnoQ();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected. Reconnecting...");
    if (unoQClient) unoQClient.stop();
    connectWiFi();
  }
  delay(1);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[WiFi] Connection successful");
  Serial.print("[WiFi] ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WiFi] Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void checkNewClient() {
  if (unoQClient && unoQClient.connected()) return;
  if (unoQClient) unoQClient.stop();

  WiFiClient newClient = server.available();
  if (!newClient) return;

  unoQClient = newClient;
  unoQClient.setNoDelay(true);

  Serial.print("[CONNECT] UNO Q connected: ");
  Serial.println(unoQClient.remoteIP());

  unoQClient.println("[ESP32] Connected to the chat server.");
}

void readSerialMonitor() {
  while (Serial.available()) {
    char c = (char)Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      serialInput.trim();

      if (serialInput.length() > 0) {
        Serial.print("[ESP32 -> UNO Q] ");
        Serial.println(serialInput);

        if (unoQClient && unoQClient.connected())
          unoQClient.println(serialInput);
        else
          Serial.println("[ERROR] UNO Q is not connected.");
      }

      serialInput = "";
      continue;
    }

    if (serialInput.length() < 500) serialInput += c;
  }
}

void readFromUnoQ() {
  if (!unoQClient || !unoQClient.connected()) return;

  while (unoQClient.available()) {
    char c = (char)unoQClient.read();

    if (c == '\r') continue;

    if (c == '\n') {
      networkInput.trim();

      if (networkInput.length() > 0) {
        Serial.print("[UNO Q -> ESP32] ");
        Serial.println(networkInput);
        
        // ======================================
        // ★ Received command is networkInput
        // ======================================
        //
        // Example:
        //
        // if (networkInput == "CW01") {
        //     ...
        // }
        //
        // if (networkInput == "CCW03") {
        //     ...
        // }
        //
        // ======================================
      }

      networkInput = "";
      continue;
    }

    if (networkInput.length() < 500) networkInput += c;
  }
}

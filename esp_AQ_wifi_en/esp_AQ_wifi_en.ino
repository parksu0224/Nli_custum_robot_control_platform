#include <ESP8266WiFi.h>
//#include <WiFi.h>

// Modify with your own Wi-Fi information ★★★★★★★★★★★
const char* WIFI_SSID = "YOUR_WIFI_SSID"; // Network name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // Leave blank if there is no network password

const uint16_t SERVER_PORT = 5000;

WiFiServer server(SERVER_PORT);
WiFiClient unoQClient;

String serialInput = ""; // Data send to Core
String networkInput = "";// Recieveed Data from Core

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
  Serial.println("ESP-12F TCP CHAT SERVER");
  Serial.println("==============================");

  connectWiFi();

  server.begin();
  server.setNoDelay(true);

  Serial.println();
  Serial.print("[SERVER] TCP port: ");
  Serial.println(SERVER_PORT);

  Serial.println("[CHAT] Enter a sentence in the Serial Monitor.");
  Serial.println("[CHAT] Line ending setting: Newline");
  Serial.println();
}

void loop() {
  // Check UNO Q connection
  checkNewClient();
  // Send ESP serial input to UNO Q
  readSerialMonitor();
  // Output data sent by UNO Q
  readFromUnoQ();

  // Reconnect if Wi-Fi is disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnecting.");

    if (unoQClient) {
      unoQClient.stop();
    }

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

  Serial.print("[WiFi] ESP IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("[WiFi] Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void checkNewClient() {
  // Normally connected state
  if (unoQClient && unoQClient.connected()) {
    return;
  }
  // Clean up object
  if (unoQClient) {
    unoQClient.stop();
  }
  WiFiClient newClient = server.accept();
  if (!newClient) {
    return;
  }

  unoQClient = newClient;
  unoQClient.setNoDelay(true);

  Serial.println();
  Serial.print("[CONNECT] UNO Q connected: ");
  Serial.println(unoQClient.remoteIP());

  unoQClient.println("[ESP] Connected to the chat server.");
}

void readSerialMonitor() { // Transmission code, for now sends Serial Monitor input
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      serialInput.trim();

      if (serialInput.length() > 0) {
        Serial.print("[ESP -> UNO Q] ");
        Serial.println(serialInput);

        if (unoQClient && unoQClient.connected()) {
          unoQClient.println(serialInput); // Transmission command
        } else {
          Serial.println("[ERROR] UNO Q is not connected.");
        }
      }

      serialInput = ""; // Put the data to be transmitted here ★★★★★★ (String type)
      continue;
    }

    if (serialInput.length() < 500) {
      serialInput += c;
    }
  }
}

void readFromUnoQ() {
  if (!unoQClient || !unoQClient.connected()) {
    return;
  }

  while (unoQClient.available()) {
    char c = static_cast<char>(unoQClient.read());

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      networkInput.trim();

      if (networkInput.length() > 0) {
        Serial.print("[UNO Q] ");
        Serial.println(networkInput);
      }

      networkInput = ""; // Received data ★★★★★★★★ (Do not modify this part; just use it as is) (String type)
      continue;
    }

    if (networkInput.length() < 500) {
      networkInput += c;
    }
  }
}

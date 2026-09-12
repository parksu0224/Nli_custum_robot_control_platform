# NLI Robot Control Platform

This project combines an Arduino UNO Q AI core, a Nextion face display, and an ESP8266 communication module. Deploy each component to its own device.

## Deployment map

| Directory | Target | Deployment unit | Guide |
| --- | --- | --- | --- |
| `nli0.1.5/` | Arduino UNO Q | Complete App Lab application directory | [NLI guide](nli0.1.5/README.md) |
| `nextion/` | Nextion display | Device firmware built from the HMI project | [Nextion guide](nextion/README.md) |
| `esp_AQ_wifi_en/` | ESP8266 / ESP-12F | Arduino sketch | [ESP guide](esp_AQ_wifi_en/README.md) |

The project files are on the `master` branch. Select `master` if they are not visible on the repository landing page.

## System architecture

```text
ESP8266 (TCP server, port 5000)
          ↕ Wi-Fi / TCP
Arduino UNO Q (TCP client + Python AI + C++ control)
          ↕ Serial1 UART, 9600 baud
Nextion (face display + ESP IP configuration)

UNO Q Python ↔ Hugging Face API
             + local knowledge files
```

The ESP sketch currently relays messages between its serial interface and TCP. Device-specific motor and sensor control must be implemented separately.

## Requirements and startup sequence

Prepare an Arduino UNO Q with an App Lab runtime, an ESP8266 development environment, and a Nextion display with Nextion Editor. AI and online speech features require internet access. Audio and camera features also require the corresponding peripherals.

1. Download the repository and preserve the three component directories.
2. Follow the [ESP guide](esp_AQ_wifi_en/README.md), configure Wi-Fi, and upload the sketch. Record the ESP IP address printed in Serial Monitor.
3. Follow the [Nextion guide](nextion/README.md), build and upload the display project, and connect its UART to UNO Q.
4. Set Nextion `setting.tcp_ip.txt` to the ESP IP address, not the UNO Q address.
5. Follow the [NLI guide](nli0.1.5/README.md) to deploy the complete application and configure the HF token and peripherals.
6. Start the UNO Q application after ESP and Nextion are ready.
7. Check for `[CONNECT] UNO Q connected:` on the ESP. Send a newline-terminated sentence from ESP Serial Monitor to check communication, then verify AI responses, display behavior, and audio separately.

If the ESP IP changes, update Nextion and restart the UNO Q application. UNO Q caches the address, so editing the display value alone does not guarantee an immediate update.

## Credentials

`nli0.1.5/secrets/hf_token.txt` is an empty deployment template. Enter the actual token only in the copy deployed on UNO Q, or set `HF_TOKEN` in the application environment. This file is tracked by Git: committing a populated copy would expose the token. Do not commit actual ESP Wi-Fi credentials either.

## Troubleshooting

| Symptom | Check first |
| --- | --- |
| ESP connection fails | ESP Wi-Fi, Nextion ESP IP, TCP port 5000 reachability, UNO Q application restart |
| HF request fails | Token, internet access, configured model/provider availability |
| Display does not respond | UART wiring and electrical compatibility, 9600 baud, HMI object names |
| Audio or camera fails | Connected devices, audio configuration, Python dependencies, application logs |

## Verification scope

These guides describe the repository code and existing setup notes. They do not establish successful integration on physical hardware. Verify the actual Nextion model and resolution, board pin assignments, power and signal levels, and development tool versions for your hardware.

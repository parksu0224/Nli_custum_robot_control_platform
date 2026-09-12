# NLI 0.1.5 — Arduino UNO Q AI Core

[System overview](../README.md)

This App Lab application runs Python AI processing and C++ hardware control on UNO Q. It communicates with ESP over TCP and sends expressions and status updates to Nextion.

## What to deploy

Deploy the entire `nli0.1.5` directory as one application. Uploading only `sketch.ino` does not deploy the Python AI and communication components.

```text
nli0.1.5/
├── app.yaml
├── python/       # main.py, config.py, requirements.txt, model files
├── sketch/       # C++ sketch and build profile
├── knowledge/    # local module knowledge
└── secrets/      # empty hf_token.txt and setup instructions
```

## UNO Q setup

1. Prepare UNO Q for use with App Lab.
2. Place the complete directory in the App Lab application workspace with `app.yaml` at the application root. Use the import/open procedure supported by your installed App Lab version. Preserve the `python`, `sketch`, `knowledge`, and `secrets` directories.
3. Ensure the runtime has the dependencies listed in `python/requirements.txt`. The application also uses `arduino.app_utils` and `Arduino_RouterBridge`; running ordinary desktop Python alone is not a replacement for this environment.
4. Put one HF token line in the deployed `secrets/hf_token.txt`, or set `HF_TOKEN` in the application environment. The environment variable takes precedence. The repository template is empty; actual AI requests require a token.
5. Adjust `HF_MODEL`, `MIC_DEVICE`, and other settings in `python/config.py` for your setup. Audio uses `arecord`/`aplay` and requires suitable input/output devices. Speech recognition is currently configured as `en-US` in the code.
6. Start the ESP server, enter its IP in Nextion `setting.tcp_ip.txt`, and run the application through App Lab.

Do not upload the populated token file back to GitHub. Git tracks the empty template, so later edits containing credentials would be included if committed.

## Communication and data settings

- TCP destination: Nextion `setting.tcp_ip.txt` → UNO Q C++ cache → Bridge `get_tcp_ip` → Python TCP client.
- TCP port: `ESP_PORT = 5000` in `python/config.py`; it must match the ESP server.
- Nextion UART: C++ `Serial1`, `NEXTION_BAUD = 9600`.
- HF configuration: `python/config.py`. Do not hard-code the actual token.
- Default RAG source: `knowledge/module_knowledge.csv`. Check `RAG_SOURCE_FILES` before adding other inputs.

## Initial checks

Check the application logs for the ESP destination and errors, then verify the connection message on ESP. Send a normal sentence from ESP and inspect NLI processing logs. Check face display and audio output independently. Some legacy messages and notes still use the `0.1.3` version label.

## Detailed setup references

- [Nextion IP input](NEXTION_TCP_IP_SETUP.txt)
- [Nextion face control](NEXTION_FACE_CONTROL_SETUP.txt)
- [Nextion recording indicator](NEXTION_RECORDING_SETUP.txt)
- [Nextion narration](NEXTION_NARRATION_SETUP.txt)
- [HF migration notes](MIGRATION_HF.md)

## Existing protocol and implementation notes

The original detailed documentation is preserved below, including historical version notes. Use the steps above for initial deployment. In particular, the USB host command below is a historical workaround for a specific environment, not a required setup step for every device.

New fix: the issue was that host mode was disabled. Run `sudo sh -c 'echo host > /sys/kernel/debug/usb/4e00000.usb/mode'`.

This is an NLI robot central core based on Arduino UNO Q App Lab.

## Main Flow

```text
Module/ESP → TCP Python → C++ → Python NLI Core
Python NLI Core → Hugging Face API + Local RAG
Hugging Face Response → Python → C++ → Module/ESP
```

n8n is not used. External LLM inference is handled by the Hugging Face Inference Providers API.

## Behavior by Input Type

### I Type

```text
[I|ARM01|type=robot_arm; functions=move_to,grip,stop|neutral||]
```

Processing:

```text
Register or update ARM01 in the currently connected module list
→ Deliver the I code to Hugging Face as well
→ Generate a connection-confirmation response or required initial command
```

The I registration information is included as `[Currently Connected Modules]` in the system context of all subsequent HF requests.

### R Type

```text
[R|ARM01|position=home, gripper=open|neutral|Pick up the cup for me|]
```

Processing:

```text
Save as the latest module status
→ Deliver to Hugging Face
→ Generate a control command based on the current status and user request
```

R is not used for module registration.

### Normal Conversation

A normal sentence can be entered through the serial monitor or STT.

```text
Hello, how are you feeling today?
```

Inside Python, it is converted into a conversation input like the following.

```text
[R|none|none|neutral|Hello, how are you feeling today?|]
```

Therefore, normal conversation with Hugging Face is possible even if no I or R has been received yet.

## Serial Debug Input

Set the serial monitor line-ending option to `Newline`.

### 6-Part NLI Code Input

```text
[R|ARM01|position=home, gripper=open|neutral|Pick up the cup for me|]
```

The original text is delivered simultaneously through the following two paths.

```text
Serial Input → C++ → Python NLI Core
Serial Input → C++ → TCP Python → ESP Module
```

### Normal Sentence Input

```text
Hello, introduce yourself.
```

A normal sentence is not sent to the module and is delivered only to the Python core.

```text
Serial Normal Sentence → C++ → Python NLI Core → Hugging Face
```

## Serial Relay Log

The serial monitor shows the actual data moving in the following directions.

```text
MODULE → C++
C++ → PYTHON CORE
PYTHON CORE → C++
C++ → MODULE
```

## STT Behavior

- If a recent R exists, the user's voice is combined with that module status.
- If no recent R exists, it is automatically converted into a normal conversation code with `module_id=none`.
- Therefore, voice conversation is possible immediately after program startup.

## Hugging Face Token

Create the following file in the project root.

```text
secrets/hf_token.txt
```

Put exactly one Hugging Face token with Inference Providers permission in the file.

```text
hf_xxxxxxxxxxxxxxxxx
```

Modify the model and system message in `python/config.py`.

## RAG Data

Default data file:

```text
knowledge/module_knowledge.xlsx
```

When an XLSX file is opened in the App Lab text editor, seeing binary content beginning with `PK` is normal. Edit it using Excel, LibreOffice Calc, or Google Sheets.

Supported formats:

```text
.xlsx
.csv
.txt
```

Configure the reference file list in `RAG_SOURCE_FILES` in `python/config.py`.

## USB Audio Note

If USB host mode is disabled:

```bash
sudo sh -c 'echo host > /sys/kernel/debug/usb/4e00000.usb/mode'
```

## Volume Control

```bash
alsamixer -c 1
```

## Nextion-sourced TCP IP

The TCP module IP is no longer hard-coded in Python. `sketch.ino` reads the global Nextion Text component `setting.tcp_ip.txt`, validates/caches the dotted IPv4 text, and exposes the cached address to Python through the `get_tcp_ip` Bridge RPC. See `NEXTION_TCP_IP_SETUP.txt` for the 32-bit packing format and setup example.

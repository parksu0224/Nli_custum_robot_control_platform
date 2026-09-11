# NLI0.1.3

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

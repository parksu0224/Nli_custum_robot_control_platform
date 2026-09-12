# NLI 0.1.5 — Arduino UNO Q AI 코어

[전체 시스템 안내](../README.md)

UNO Q에서 Python AI 처리와 C++ 하드웨어 제어를 함께 실행하는 App Lab 앱입니다. ESP와 TCP 통신을 하고 Nextion 화면에 표정·상태를 전달합니다.

## 배포할 파일

`nli0.1.5` 폴더 전체를 앱 단위로 사용하세요. `sketch.ino`만 업로드하면 Python AI와 통신 기능까지 실행되지 않습니다.

```text
nli0.1.5/
├── app.yaml
├── python/       # main.py, config.py, requirements.txt, 모델 파일
├── sketch/       # C++ 스케치와 빌드 프로필
├── knowledge/    # 로컬 모듈 지식 자료
└── secrets/      # 빈 hf_token.txt와 설정 안내
```

## UNO Q 설정

1. UNO Q를 App Lab에서 사용할 수 있는 상태로 준비합니다.
2. App Lab의 앱 작업 공간에 이 폴더 전체를 배치하여 `app.yaml`이 앱 루트에 있도록 합니다. 설치된 App Lab 버전의 앱 가져오기/열기 절차를 사용하세요. 폴더 내부의 `python`, `sketch`, `knowledge`, `secrets` 구조를 유지합니다.
3. 실행 환경에 `python/requirements.txt`의 의존성이 준비되어 있는지 확인합니다. 코드에는 `arduino.app_utils`와 `Arduino_RouterBridge`가 필요하므로 일반 PC의 Python 실행만으로 대체할 수 없습니다.
4. UNO Q에 배치한 `secrets/hf_token.txt`에 HF 키 한 줄을 입력하거나 앱 실행 환경에 `HF_TOKEN`을 설정합니다. 환경변수가 우선합니다. 빈 파일은 배포용이며 AI 호출에는 실제 키가 필요합니다.
5. `python/config.py`의 `HF_MODEL`, `MIC_DEVICE` 등 설정을 자신의 환경에 맞춥니다. 음성 처리에는 `arecord`/`aplay`, 마이크와 출력 장치가 필요합니다. 현재 음성 인식 언어는 코드상 `en-US`입니다.
6. ESP 서버를 실행하고 Nextion의 `setting.tcp_ip.txt`에 ESP IP를 입력한 뒤 App Lab에서 앱을 실행합니다.

실제 키를 입력한 `hf_token.txt`는 다시 GitHub에 올리지 마세요. 현재 파일은 빈 템플릿으로 Git이 추적합니다.

## 통신과 데이터

- TCP 목적지: Nextion `setting.tcp_ip.txt` → UNO Q C++ 캐시 → Bridge `get_tcp_ip` → Python TCP 클라이언트.
- TCP 포트: `python/config.py`의 `ESP_PORT = 5000`. ESP 서버 설정과 같아야 합니다.
- Nextion UART: C++의 `Serial1`, `NEXTION_BAUD = 9600`.
- HF 설정: `python/config.py`. 실제 키를 코드에 직접 넣지 않습니다.
- RAG 기본 입력: `knowledge/module_knowledge.csv`. 다른 파일을 읽으려면 `RAG_SOURCE_FILES` 설정을 확인합니다.

## 확인 순서

앱 로그에서 ESP 목적지와 오류를 확인하고, ESP 측 연결 로그를 확인합니다. ESP에서 일반 문장을 전송한 뒤 NLI 처리 로그를 확인하세요. 표정 표시와 음성 출력은 각각 별도로 확인합니다. 일부 기존 로그·설명에는 `0.1.3` 표기가 남아 있습니다.

## 상세 설정 문서

- [Nextion IP 입력](NEXTION_TCP_IP_SETUP.txt)
- [Nextion 표정](NEXTION_FACE_CONTROL_SETUP.txt)
- [Nextion 녹음 표시](NEXTION_RECORDING_SETUP.txt)
- [Nextion 내레이션](NEXTION_NARRATION_SETUP.txt)
- [HF 전환 설명](MIGRATION_HF.md)

## 기존 프로토콜 및 구현 설명

아래는 기존 상세 설명입니다. 버전별 과거 메모를 포함하며, 최초 설치는 위 절차를 기준으로 진행하세요. 특히 아래 USB host 관련 명령은 환경별 과거 문제 해결 기록이며 모든 장치에 필요한 설치 단계가 아닙니다.

### 기존 NLI 프로토콜 설명

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

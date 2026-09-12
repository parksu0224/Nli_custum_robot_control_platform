# NLI 로봇 제어 플랫폼

Arduino UNO Q의 AI 코어, Nextion 얼굴 화면, ESP8266 통신 모듈을 함께 사용하는 프로젝트입니다. 세 폴더는 서로 다른 장치에 배포합니다.

## 무엇을 어디에 올리나요?

| 폴더 | 대상 장치 | 배포 단위 | 상세 안내 |
| --- | --- | --- | --- |
| `nli0.1.5/` | Arduino UNO Q | 폴더 전체를 App Lab 앱으로 사용 | [NLI README](nli0.1.5/README.md) |
| `nextion/` | Nextion 디스플레이 | HMI 프로젝트에서 빌드한 장치용 파일 | [Nextion README](nextion/README.md) |
| `esp_AQ_wifi_en/` | ESP8266 / ESP-12F | Arduino 스케치 | [ESP README](esp_AQ_wifi_en/README.md) |

현재 구성 파일은 `master` 브랜치에 있습니다. 저장소 첫 화면에서 파일이 보이지 않으면 브랜치를 `master`로 선택하세요.

## 전체 연결 구조

```text
ESP8266 (TCP 서버, 포트 5000)
          ↕ Wi-Fi / TCP
Arduino UNO Q (TCP 클라이언트 + Python AI + C++ 제어)
          ↕ Serial1 UART, 9600 baud
Nextion (얼굴 화면 + ESP IP 입력)

UNO Q Python ↔ Hugging Face API
             + 로컬 knowledge 자료
```

ESP 코드는 현재 시리얼 입력과 TCP 메시지를 중계합니다. 모터·센서별 제어 기능은 사용자가 모듈에 추가해야 합니다.

## 준비물과 실행 순서

Arduino UNO Q와 App Lab 실행 환경, ESP8266 개발 환경, Nextion Editor 및 디스플레이를 준비합니다. AI와 온라인 음성 기능에는 인터넷 연결이 필요합니다. 음성·카메라 기능에는 해당 주변기기도 필요합니다.

1. 저장소를 내려받고 세 폴더를 유지합니다.
2. [ESP 안내](esp_AQ_wifi_en/README.md)에 따라 Wi-Fi 정보를 설정하고 스케치를 업로드합니다. 시리얼 모니터에 출력되는 ESP IP를 기록합니다.
3. [Nextion 안내](nextion/README.md)에 따라 HMI를 장치에 맞게 빌드·업로드하고 UNO Q의 UART에 연결합니다.
4. Nextion의 `setting.tcp_ip.txt`에 ESP IP를 설정합니다. 이 주소는 UNO Q 자신의 IP가 아닙니다.
5. [NLI 안내](nli0.1.5/README.md)에 따라 전체 앱을 UNO Q에 배치하고 HF 키와 주변기기 설정을 입력합니다.
6. ESP와 Nextion이 준비된 상태에서 UNO Q 앱을 실행합니다.
7. ESP 로그의 `[CONNECT] UNO Q connected:`를 확인하고, ESP 시리얼 모니터에서 줄바꿈을 포함한 문장을 보내 통신을 확인합니다. 이후 AI 응답·화면·음성을 각각 확인합니다.

ESP IP가 바뀌면 Nextion 값을 갱신하고 UNO Q 앱을 재시작하세요. UNO Q는 읽은 IP를 캐시하므로 화면 변경만으로 즉시 반영된다고 가정하지 마세요.

## 인증 정보

`nli0.1.5/secrets/hf_token.txt`는 폴더 구조를 유지하기 위한 빈 파일입니다. UNO Q에 배포한 파일에만 실제 키를 입력하거나 실행 환경의 `HF_TOKEN`을 사용하세요. 이 파일은 Git이 추적하므로 키를 입력한 내용을 다시 커밋하면 노출됩니다. ESP의 실제 Wi-Fi 비밀번호도 커밋하지 마세요.

## 문제 해결

| 증상 | 먼저 확인할 사항 |
| --- | --- |
| ESP에 연결되지 않음 | ESP Wi-Fi 연결, Nextion의 ESP IP, TCP 5000 접근 가능 여부, UNO Q 앱 재시작 |
| HF 응답 오류 | 키 설정, 인터넷, 설정 모델·제공자 사용 가능 여부 |
| 화면이 반응하지 않음 | UART 배선·전기 규격, 9600 baud, HMI 객체 이름 |
| 음성·카메라 오류 | 연결 장치, 오디오 설정, Python 의존성 및 로그 |

## 확인 범위

이 문서는 저장소 코드와 기존 설정 문서를 기준으로 작성했습니다. 실물 보드에서 통합 동작을 검증한 문서는 아닙니다. 정확한 Nextion 모델·해상도, 보드별 핀 번호·전원·신호 전압, 개발 도구 버전은 실제 구성에 맞춰 확인해야 합니다.

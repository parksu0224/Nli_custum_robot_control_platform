# Nextion 얼굴 화면 — AQ Face2

[전체 시스템 안내](../README.md)

`AQ_Face2.HMI`는 Nextion Editor에서 편집하는 화면 프로젝트입니다. UNO Q의 C++ 코드가 UART로 얼굴 표정·상태를 전달하며, UNO Q는 화면의 ESP IP 설정을 읽습니다.

## 편집 및 장치 배포

1. Nextion Editor에서 `AQ_Face2.HMI`를 엽니다.
2. 프로젝트의 대상 모델과 해상도를 실제 디스플레이와 비교합니다. 저장소 문서만으로 정확한 장치 모델과 Editor 버전은 확정하지 않았습니다.
3. 아래 UART 속도와 필수 객체를 확인한 뒤 프로젝트를 빌드합니다.
4. Editor에서 생성한 장치용 `.tft` 파일을 해당 디스플레이가 지원하는 방식으로 업로드합니다. 저장소에는 현재 `.HMI` 원본만 포함되어 있습니다.
5. UNO Q의 `Serial1`과 Nextion UART를 연결합니다. TX/RX 교차 연결, 공통 GND, 전원·신호 전압과 실제 핀 번호를 양쪽 장치 규격으로 확인하세요. 이 문서는 특정 핀 배치를 검증하지 않았습니다.
6. 통신 속도를 UNO Q 코드의 **9600 baud**와 맞춥니다.

## ESP IP 설정

UNO Q는 다음 이름을 그대로 사용합니다.

| 항목 | 값 |
| --- | --- |
| 페이지 | `setting` |
| Text 객체 | `tcp_ip` |
| 읽는 속성 | `setting.tcp_ip.txt` |
| 객체 범위 | `vscope = global` |
| 텍스트 최대 길이 | `txt_maxl >= 15` |

ESP 시리얼 모니터에 출력된 IPv4 주소를 입력하세요. UNO Q는 `get setting.tcp_ip.txt`를 전송하고 문자열 응답을 읽어 TCP 목적지로 사용합니다. 주소를 바꾼 뒤에는 UNO Q 앱을 재시작하여 캐시를 갱신하세요.

## 표정과 상태 연동

코드에서 요구하는 변수에는 `face_exp`, `under_ani`, `forehead_ani`, `l_eye_x_ani`, `r_eye_x_ani`, `l_eye_y_ani`, `r_eye_y_ani`, `l_eye_r_ani`, `r_eye_r_ani`, `mouth_ani`, `mouth_h`, `mouth_dir`가 있습니다. 다른 페이지에서도 접근해야 하는 변수의 범위를 확인하세요.

표정 프리셋은 0 NORMAL, 1 SMILE, 2 ANGRY, 3 THINK, 4 VERY ANGRY, 5 DIZZY, 99 MANUAL입니다. 타이머·입 애니메이션 등 세부 이벤트는 다음 문서를 확인하세요.

- [표정 변수 및 타이머](../nli0.1.5/NEXTION_FACE_CONTROL_SETUP.txt)
- [TCP IP 설정](../nli0.1.5/NEXTION_TCP_IP_SETUP.txt)
- [녹음 표시](../nli0.1.5/NEXTION_RECORDING_SETUP.txt)
- [내레이션 표시](../nli0.1.5/NEXTION_NARRATION_SETUP.txt)

위 내용은 UNO Q 코드와 연동 문서가 요구하는 조건입니다. HMI를 Editor에서 열어 모든 객체·이벤트가 일치하는지 검증한 상태는 아닙니다.

## 확인 순서

먼저 화면 업로드와 표시를 확인하고, 이어서 UNO Q가 ESP IP를 읽는지 앱 로그를 확인합니다. 마지막으로 표정과 녹음·발화 표시를 확인하세요. 화면이 응답하지 않으면 UART 연결과 속도, 페이지·객체 이름부터 점검합니다.

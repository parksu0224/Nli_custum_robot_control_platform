# Nextion Face Display — AQ Face2

[System overview](../README.md)

`AQ_Face2.HMI` is a screen project for Nextion Editor. UNO Q C++ sends face and status commands over UART and reads the ESP IP configuration from the display.

## Edit and deploy

1. Open `AQ_Face2.HMI` in Nextion Editor.
2. Compare the project target model and resolution with your physical display. The exact display model and Editor version have not been established from the repository documentation.
3. Check the UART speed and required objects below, then build the project.
4. Upload the generated `.tft` device file using a method supported by your display. The repository currently includes the `.HMI` source only.
5. Connect UNO Q `Serial1` to the Nextion UART. Check crossed TX/RX, shared GND, power and signal levels, and actual pin assignments against both devices' specifications. This guide does not verify a specific pin layout.
6. Match the UNO Q communication speed of **9600 baud**.

## ESP IP configuration

UNO Q uses the following names exactly.

| Item | Required value |
| --- | --- |
| Page | `setting` |
| Text object | `tcp_ip` |
| Property queried | `setting.tcp_ip.txt` |
| Object scope | `vscope = global` |
| Maximum text length | `txt_maxl >= 15` |

Enter the IPv4 address printed by ESP Serial Monitor. UNO Q sends `get setting.tcp_ip.txt`, reads the string response, and uses it as the TCP destination. Restart the UNO Q application after changing the address to refresh its cache.

## Face and status integration

Required variables include `face_exp`, `under_ani`, `forehead_ani`, `l_eye_x_ani`, `r_eye_x_ani`, `l_eye_y_ani`, `r_eye_y_ani`, `l_eye_r_ani`, `r_eye_r_ani`, `mouth_ani`, `mouth_h`, and `mouth_dir`. Check the scope of variables that must remain accessible from other pages.

Expression presets are 0 NORMAL, 1 SMILE, 2 ANGRY, 3 THINK, 4 VERY ANGRY, 5 DIZZY, and 99 MANUAL. Refer to the detailed event and animation instructions:

- [Face variables and timers](../nli0.1.5/NEXTION_FACE_CONTROL_SETUP.txt)
- [TCP IP setup](../nli0.1.5/NEXTION_TCP_IP_SETUP.txt)
- [Recording indicator](../nli0.1.5/NEXTION_RECORDING_SETUP.txt)
- [Narration display](../nli0.1.5/NEXTION_NARRATION_SETUP.txt)

These are requirements from the UNO Q code and integration notes. The HMI has not been opened in Editor to verify that every object and event matches them.

## Initial checks

Verify display upload and rendering first, then check the UNO Q logs to confirm that the ESP IP is read. Finally test expressions and recording/playback indicators. If the display does not respond, start with UART wiring, speed, and page/object names.

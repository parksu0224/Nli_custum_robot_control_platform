#include <Arduino_RouterBridge.h>

// =========================================================
// Settings
// =========================================================

const int MESSAGE_QUEUE_SIZE = 20;
const size_t SERIAL_INPUT_MAX_LENGTH = 1000;

// Nextion code monitor
const int NEXTION_MAX_LOG_LINES = 8;
const unsigned long NEXTION_BAUD = 9600;

String nextionLogs[NEXTION_MAX_LOG_LINES];
int nextionLogCount = 0;


// =========================================================
// Nextion face control
//
// Python decides the ④ face-expression field.
// C++ only converts that already-decided face command to
// Nextion UART .val commands.
//
// Standard presets:
//   NORMAL      -> face_exp=0
//   SMILE       -> face_exp=1
//   ANGRY       -> face_exp=2
//   THINK       -> face_exp=3
//   VERY_ANGRY  -> face_exp=4
//   DIZZY       -> face_exp=5
//
// Manual example:
//   MANUAL:LX0,RX0,LY0,RY0,LR1,RR1,U0,F0
// C++ always sends face_exp=99 first, then the detailed values.
//
// Mouth animation is handled inside Nextion:
//   mouth_ani=1 while TTS is actually playing
//   mouth_ani=0 when TTS playback ends
//
// Every face/mouth change wakes tm0 with tm0.en=1.
// Nextion turns tm0 off itself when ani_busy=0.
// Any non-NORMAL facial expression returns to NORMAL after 3 seconds.
// =========================================================

enum FacePreset {
  FACE_PRESET_NORMAL,
  FACE_PRESET_SMILE,
  FACE_PRESET_ANGRY,
  FACE_PRESET_THINK,
  FACE_PRESET_VERY_ANGRY,
  FACE_PRESET_DIZZY,
  FACE_PRESET_UNKNOWN
};

enum ManualFaceField {
  MANUAL_FIELD_LX,
  MANUAL_FIELD_RX,
  MANUAL_FIELD_LY,
  MANUAL_FIELD_RY,
  MANUAL_FIELD_LR,
  MANUAL_FIELD_RR,
  MANUAL_FIELD_U,
  MANUAL_FIELD_F,
  MANUAL_FIELD_UNKNOWN
};


// =========================================================
// Face expression timeout
// Any non-NORMAL face returns to NORMAL after 3 seconds.
// A new face command restarts the 3-second timer.
// =========================================================

unsigned long faceExpressionStartTime = 0;
bool faceExpressionActive = false;
const unsigned long FACE_EXPRESSION_DURATION = 3000;


// =========================================================
// Narration auto-hide delay
// g0 is hidden 5 seconds after the complete TTS utterance ends.
// A new narration cancels any pending hide request.
// =========================================================

bool narrationHidePending = false;
unsigned long narrationHideStartTime = 0;
const unsigned long NARRATION_HIDE_DELAY = 6000;


// =========================================================
// TCP IP sourced from Nextion setting.tcp_ip.txt
//
// Nextion component:
//   page    = setting
//   objname = tcp_ip
//   type    = Text
//   vscope  = global
//
// Example text:
//   172.30.1.6
//
// C++ reads the text over UART, validates/caches it, and exposes
// the cached dotted-decimal address to Python through get_tcp_ip.
// Python never reads a hard-coded config.ESP_IP.
// =========================================================

String nextionTcpIpAddress = "";
bool nextionTcpIpValid = false;
const unsigned long NEXTION_QUERY_TIMEOUT = 1000;


// =========================================================
// Serial debug input
// =========================================================

String serialDebugInput = "";


// =========================================================
// Module/debug → C++ → NLI Python input queue
// =========================================================

String moduleMessageQueue[MESSAGE_QUEUE_SIZE];

int moduleQueueHead = 0;
int moduleQueueTail = 0;
int moduleQueueCount = 0;


// =========================================================
// NLI Python/debug → C++ → ESP module output queue
// =========================================================

String commandQueue[MESSAGE_QUEUE_SIZE];

int commandQueueHead = 0;
int commandQueueTail = 0;
int commandQueueCount = 0;


// =========================================================
// Function declarations
// =========================================================

String showRemoteMessage(String message);
String receiveCommand(String command);
String showCode(String code);
String setRecordingState(String state);
String setSpeakingState(String state);
String setFaceExpression(String expression);
String showNarration(String text);
String hideNarration(String unused);
String getTcpIp(String unused);

void sendNextionText(String text);
void addNextionCode(String code);
void setNextionRecordingIndicator(bool active);

void sendNextionCommand(String command);
void setNextionVal(const String& variableName, int value);
bool readNextionText(const String& expression, String& value, unsigned long timeoutMs);
bool isValidIpv4String(const String& address);
bool refreshTcpIpFromNextion();
void enableFaceTimer();
void startFaceExpressionTimer();
void updateFaceExpressionTimer();
void updateNarrationHide();

FacePreset parseFacePreset(String expression);
void applyFacePreset(FacePreset preset);
bool parseManualValue(const String& token, int valueStartIndex, int& value);
ManualFaceField parseManualFaceField(const String& token, int& value);
bool applyManualFaceToken(String token);
void applyManualFaceExpression(String expression);

void processSerialMonitor();
void routeSerialDebugInput(String input);
bool isSixPartitionNliCode(const String& code);

void sendModuleInfoToPython(String moduleCode);
void sendCommandToESP(String command);


// =========================================================
// Nextion code monitor
// =========================================================

void sendNextionText(String text) {
  // Simplify characters that could break the Nextion command string for display
  text.replace("\"", "'");
  text.replace("\n", " ");
  text.replace("\r", " ");

  Serial1.print("t0.txt=\"");
  Serial1.print(text);
  Serial1.print("\"");
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
}


void addNextionCode(String code) {
  code.trim();

  if (code.length() == 0) {
    return;
  }

  // Keep only the most recent N codes
  if (nextionLogCount >= NEXTION_MAX_LOG_LINES) {
    for (int i = 0; i < NEXTION_MAX_LOG_LINES - 1; i++) {
      nextionLogs[i] = nextionLogs[i + 1];
    }
    nextionLogCount = NEXTION_MAX_LOG_LINES - 1;
  }

  nextionLogs[nextionLogCount] = code;
  nextionLogCount++;

  String displayText = "";

  for (int i = 0; i < nextionLogCount; i++) {
    displayText += nextionLogs[i];

    if (i < nextionLogCount - 1) {
      displayText += "\\r";
    }
  }

  sendNextionText(displayText);
}


// Codes found/generated anywhere in Python enter through this RPC.
String showCode(String code) {
  addNextionCode(code);
  return "NEXTION_CODE_SHOWN";
}


// Use Nextion t1 exclusively as the recording-status indicator.
// The t1 component must be created in the Nextion Editor.
void setNextionRecordingIndicator(bool active) {
  Serial1.print("t1.txt=\"REC\"");
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);

  if (active) {
    Serial1.print("vis t1,1");
  }
  else {
    Serial1.print("vis t1,0");
  }

  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
}


// Python -> C++ RPC: "1" means recording; any other value means recording stopped.
String setRecordingState(String state) {
  state.trim();

  bool active = (state == "1");
  setNextionRecordingIndicator(active);

  Monitor.print("[NEXTION REC] " );
  Monitor.println(active ? "ON" : "OFF");

  return active ? "RECORDING_ON" : "RECORDING_OFF";
}


// =========================================================
// Nextion face / mouth control
// =========================================================

void sendNextionCommand(String command) {
  Serial1.print(command);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
  Serial1.write(0xFF);
}


void setNextionVal(const String& variableName, int value) {
  sendNextionCommand(
    variableName + String(".val=") + String(value)
  );
}


// Query one Nextion Text value using the standard string return packet:
//   0x70 + ASCII bytes + 0xFF 0xFF 0xFF
// The expression is supplied without "get", e.g. "setting.tcp_ip.txt".
bool readNextionText(
  const String& expression,
  String& value,
  unsigned long timeoutMs
) {
  // Remove stale response bytes before starting a synchronous query.
  while (Serial1.available() > 0) {
    Serial1.read();
  }

  Monitor.print("[NEXTION TCP IP] REQUEST: get ");
  Monitor.println(expression);

  sendNextionCommand(String("get ") + expression);

  unsigned long started = millis();

  // Search for the string-data header byte 0x70.
  while (millis() - started < timeoutMs) {
    if (Serial1.available() <= 0) {
      delay(1);
      continue;
    }

    int header = Serial1.read();

    if (header != 0x70) {
      // Keep scanning. 0x1A, for example, means invalid variable name.
      Monitor.print("[NEXTION TCP IP] Unexpected header: 0x");
      Monitor.println(header, HEX);
      continue;
    }

    value = "";
    int ffCount = 0;

    // Read text until the Nextion FF FF FF terminator.
    while (millis() - started < timeoutMs) {
      if (Serial1.available() <= 0) {
        delay(1);
        continue;
      }

      uint8_t b = (uint8_t)Serial1.read();

      if (b == 0xFF) {
        ffCount++;

        if (ffCount >= 3) {
          return true;
        }

        continue;
      }

      // If fewer than three FF bytes occurred inside the payload,
      // preserve them rather than silently dropping data.
      while (ffCount > 0) {
        value += (char)0xFF;
        ffCount--;
      }

      value += (char)b;
    }

    return false;
  }

  return false;
}


// Validate A.B.C.D before allowing Python to use the value as a TCP host.
bool isValidIpv4String(const String& address) {
  if (address.length() < 7 || address.length() > 15) {
    return false;
  }

  int partCount = 0;
  int partValue = 0;
  int digitsInPart = 0;

  for (int i = 0; i <= address.length(); i++) {
    char c = (i < address.length()) ? address.charAt(i) : '.';

    if (c >= '0' && c <= '9') {
      partValue = partValue * 10 + (c - '0');
      digitsInPart++;

      if (digitsInPart > 3 || partValue > 255) {
        return false;
      }

      continue;
    }

    if (c == '.') {
      if (digitsInPart == 0) {
        return false;
      }

      partCount++;
      partValue = 0;
      digitsInPart = 0;
      continue;
    }

    return false;
  }

  return partCount == 4;
}


// Read setting.tcp_ip.txt from Nextion and cache the dotted IPv4 string in C++.
bool refreshTcpIpFromNextion() {
  String value = "";

  if (!readNextionText(
        "setting.tcp_ip.txt",
        value,
        NEXTION_QUERY_TIMEOUT
      )) {
    nextionTcpIpValid = false;
    Monitor.println(
      "[NEXTION TCP IP ERROR] No text response from setting.tcp_ip.txt"
    );
    return false;
  }

  value.trim();

  if (!isValidIpv4String(value) || value == "0.0.0.0") {
    nextionTcpIpAddress = "";
    nextionTcpIpValid = false;

    Monitor.print("[NEXTION TCP IP ERROR] Invalid IPv4 text: ");
    Monitor.println(value);

    return false;
  }

  nextionTcpIpAddress = value;
  nextionTcpIpValid = true;

  Monitor.print("[NEXTION TCP IP] cached address=");
  Monitor.println(nextionTcpIpAddress);

  return true;
}


// Python -> C++ RPC.
// Python receives the IP cached by C++.
// If the initial Nextion read failed, retry once when Python asks.
String getTcpIp(String unused) {
  if (!nextionTcpIpValid) {
    refreshTcpIpFromNextion();
  }

  if (!nextionTcpIpValid) {
    return "";
  }

  return nextionTcpIpAddress;
}


// Python -> C++ RPC: show the robot's spoken narration on Nextion g0.
// Per the current HMI design, enable g0 first and then update g0.txt.
String showNarration(String text) {
  text.trim();

  if (text.length() == 0 || text.equalsIgnoreCase("none")) {
    return "NARRATION_EMPTY";
  }

  // Prevent the text from breaking the quoted Nextion command.
  text.replace("\\", "/");
  text.replace("\"", "'");
  text.replace("\n", " ");
  text.replace("\r", " ");

  // A new narration must cancel any previously scheduled hide.
  narrationHidePending = false;
  sendNextionCommand("g0.bco=65535");
  sendNextionCommand("g0.en=1");
  sendNextionCommand(String("g0.txt=\"") + text + String("\""));

  Monitor.print("[NEXTION NARRATION] ");
  Monitor.println(text);

  return "NARRATION_SHOWN";
}


// Python -> C++ RPC: called after the complete TTS utterance ends.
// Do not hide g0 immediately. Schedule it to turn off 5 seconds later.
String hideNarration(String unused) {
  narrationHideStartTime = millis();
  narrationHidePending = true;

  Monitor.println("[NEXTION NARRATION] OFF scheduled in 5 seconds");

  return "NARRATION_HIDE_SCHEDULED";
}


// Non-blocking narration hide timer.
// Keep this in loop(); never use delay(5000), because that would stop
// Bridge.update(), serial handling, face timeout, and robot communication.
void updateNarrationHide() {
  if (!narrationHidePending) {
    return;
  }

  unsigned long now = millis();

  if (now - narrationHideStartTime < NARRATION_HIDE_DELAY) {
    return;
  }

  sendNextionCommand("g0.en=0");
  sendNextionCommand("g0.bco=0");
  narrationHidePending = false;

  Monitor.println("[NEXTION NARRATION] OFF");
}


// Wake the Nextion face animation timer.
// Nextion itself turns tm0 off again when ani_busy becomes 0.
void enableFaceTimer() {
  sendNextionCommand("tm0.en=1");
}


void startFaceExpressionTimer() {
  faceExpressionStartTime = millis();
  faceExpressionActive = true;
}


void updateFaceExpressionTimer() {
  if (!faceExpressionActive) {
    return;
  }

  unsigned long now = millis();

  if (now - faceExpressionStartTime < FACE_EXPRESSION_DURATION) {
    return;
  }

  // Return to NORMAL after 3 seconds.
  // tm0 must be woken so the Nextion can animate all features back.
  setNextionVal("face_exp", 0);
  enableFaceTimer();

  faceExpressionActive = false;

  Monitor.println("[FACE TIMER] 3s elapsed -> NORMAL");
}


FacePreset parseFacePreset(String expression) {
  expression.trim();
  expression.toUpperCase();

  if (expression == "NORMAL") {
    return FACE_PRESET_NORMAL;
  }

  if (expression == "SMILE") {
    return FACE_PRESET_SMILE;
  }

  if (expression == "ANGRY") {
    return FACE_PRESET_ANGRY;
  }

  if (expression == "THINK") {
    return FACE_PRESET_THINK;
  }

  if (expression == "VERY_ANGRY") {
    return FACE_PRESET_VERY_ANGRY;
  }

  if (expression == "DIZZY") {
    return FACE_PRESET_DIZZY;
  }

  return FACE_PRESET_UNKNOWN;
}


void applyFacePreset(FacePreset preset) {
  switch (preset) {
    case FACE_PRESET_NORMAL:
      setNextionVal("face_exp", 0);
      break;

    case FACE_PRESET_SMILE:
      setNextionVal("face_exp", 1);
      break;

    case FACE_PRESET_ANGRY:
      setNextionVal("face_exp", 2);
      break;

    case FACE_PRESET_THINK:
      setNextionVal("face_exp", 3);
      break;

    case FACE_PRESET_VERY_ANGRY:
      setNextionVal("face_exp", 4);
      break;

    case FACE_PRESET_DIZZY:
      setNextionVal("face_exp", 5);
      break;

    case FACE_PRESET_UNKNOWN:
    default:
      return;
  }

  // Every face value change must wake tm0.
  enableFaceTimer();

  if (preset == FACE_PRESET_NORMAL) {
    faceExpressionActive = false;
  }
  else {
    // Any new non-normal expression gets a fresh 3-second lifetime.
    startFaceExpressionTimer();
  }
}


bool parseManualValue(const String& token, int valueStartIndex, int& value) {
  if (valueStartIndex < 0 || valueStartIndex >= token.length()) {
    return false;
  }

  for (int i = valueStartIndex; i < token.length(); i++) {
    char c = token.charAt(i);

    if (c < '0' || c > '9') {
      return false;
    }
  }

  value = token.substring(valueStartIndex).toInt();
  return true;
}


ManualFaceField parseManualFaceField(const String& rawToken, int& value) {
  String token = rawToken;
  token.trim();
  token.toUpperCase();

  if (token.startsWith("LX") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_LX;
  }

  if (token.startsWith("RX") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_RX;
  }

  if (token.startsWith("LY") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_LY;
  }

  if (token.startsWith("RY") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_RY;
  }

  if (token.startsWith("LR") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_LR;
  }

  if (token.startsWith("RR") && parseManualValue(token, 2, value)) {
    return MANUAL_FIELD_RR;
  }

  if (token.startsWith("U") && parseManualValue(token, 1, value)) {
    return MANUAL_FIELD_U;
  }

  if (token.startsWith("F") && parseManualValue(token, 1, value)) {
    return MANUAL_FIELD_F;
  }

  return MANUAL_FIELD_UNKNOWN;
}


bool applyManualFaceToken(String token) {
  int value = 0;
  ManualFaceField field = parseManualFaceField(token, value);

  switch (field) {
    case MANUAL_FIELD_LX:
      if (value > 2) return false;
      setNextionVal("l_eye_x_ani", value);
      return true;

    case MANUAL_FIELD_RX:
      if (value > 2) return false;
      setNextionVal("r_eye_x_ani", value);
      return true;

    case MANUAL_FIELD_LY:
      if (value > 2) return false;
      setNextionVal("l_eye_y_ani", value);
      return true;

    case MANUAL_FIELD_RY:
      if (value > 2) return false;
      setNextionVal("r_eye_y_ani", value);
      return true;

    case MANUAL_FIELD_LR:
      if (value > 2) return false;
      setNextionVal("l_eye_r_ani", value);
      return true;

    case MANUAL_FIELD_RR:
      if (value > 2) return false;
      setNextionVal("r_eye_r_ani", value);
      return true;

    case MANUAL_FIELD_U:
      if (value > 1) return false;
      setNextionVal("under_ani", value);
      return true;

    case MANUAL_FIELD_F:
      if (value > 1) return false;
      setNextionVal("forehead_ani", value);
      return true;

    case MANUAL_FIELD_UNKNOWN:
    default:
      return false;
  }
}


void applyManualFaceExpression(String expression) {
  // The required order is important:
  // 1) disable automatic preset writes
  // 2) apply detailed manual animation values
  setNextionVal("face_exp", 99);

  int colonIndex = expression.indexOf(':');

  if (colonIndex < 0 || colonIndex >= expression.length() - 1) {
    Monitor.println("[FACE MANUAL ERROR] Missing detail tokens");
    enableFaceTimer();
    startFaceExpressionTimer();
    return;
  }

  String details = expression.substring(colonIndex + 1);
  int startIndex = 0;

  while (startIndex < details.length()) {
    int commaIndex = details.indexOf(',', startIndex);
    String token;

    if (commaIndex < 0) {
      token = details.substring(startIndex);
      startIndex = details.length();
    }
    else {
      token = details.substring(startIndex, commaIndex);
      startIndex = commaIndex + 1;
    }

    token.trim();

    if (token.length() == 0) {
      continue;
    }

    bool applied = applyManualFaceToken(token);

    Monitor.print("[FACE MANUAL] ");
    Monitor.print(token);
    Monitor.print(" -> ");
    Monitor.println(applied ? "APPLIED" : "INVALID");
  }

  // All manual values are sent first. Wake tm0 only after the full manual
  // command has been applied, then start the 3-second return timer.
  enableFaceTimer();
  startFaceExpressionTimer();
}


// Python -> C++ RPC.
// Python has already decided the ④ face-expression field.
// C++ does not decide whether the NLI code is O or none here.
String setFaceExpression(String expression) {
  expression.trim();

  if (expression.length() == 0 || expression.equalsIgnoreCase("none")) {
    return "FACE_EMPTY";
  }

  Monitor.println();
  Monitor.println("========== FACE: PYTHON -> C++ -> NEXTION ==========");
  Monitor.println(expression);
  Monitor.println("=====================================================");

  String normalized = expression;
  normalized.trim();
  normalized.toUpperCase();

  if (normalized == "MANUAL" || normalized.startsWith("MANUAL:")) {
    applyManualFaceExpression(expression);
    return "FACE_MANUAL_APPLIED";
  }

  FacePreset preset = parseFacePreset(expression);

  if (preset == FACE_PRESET_UNKNOWN) {
    Monitor.println("[FACE ERROR] Unknown face expression");
    return "FACE_UNKNOWN";
  }

  applyFacePreset(preset);
  return "FACE_PRESET_APPLIED";
}


// Python -> C++ RPC
// "1" = actual TTS playback started
// "0" = actual TTS playback ended
//
// Nextion itself performs the repeated mouth_h 10 <-> 20 animation.
String setSpeakingState(String state) {
  state.trim();
  bool active = (state == "1");

  if (active) {
    // Start from the default mouth and let Nextion perform 10 <-> 20.
    setNextionVal("mouth_h", 10);
    setNextionVal("mouth_dir", 1);
    setNextionVal("mouth_ani", 1);

    // The face timer may currently be OFF because the face is static.
    enableFaceTimer();

    Monitor.println("[NEXTION MOUTH] SPEAKING ON");
    return "SPEAKING_ON";
  }

  // Stop repeated mouth motion.
  // Do NOT force mouth_h=10 here; the Nextion timer code smoothly returns
  // mouth_h to 10, then ani_busy becomes 0 and tm0 turns itself off.
  setNextionVal("mouth_ani", 0);
  setNextionVal("mouth_dir", 1);
  enableFaceTimer();

  Monitor.println("[NEXTION MOUTH] SPEAKING OFF");
  return "SPEAKING_OFF";
}


// =========================================================
// Input queue
// =========================================================

bool enqueueModuleMessage(const String& message) {
  if (moduleQueueCount >= MESSAGE_QUEUE_SIZE) {
    return false;
  }

  moduleMessageQueue[moduleQueueTail] = message;

  moduleQueueTail =
    (moduleQueueTail + 1) % MESSAGE_QUEUE_SIZE;

  moduleQueueCount++;

  return true;
}


bool dequeueModuleMessage(String& message) {
  if (moduleQueueCount <= 0) {
    return false;
  }

  message = moduleMessageQueue[moduleQueueHead];
  moduleMessageQueue[moduleQueueHead] = "";

  moduleQueueHead =
    (moduleQueueHead + 1) % MESSAGE_QUEUE_SIZE;

  moduleQueueCount--;

  return true;
}


// =========================================================
// Output queue
// =========================================================

bool enqueueCommand(const String& command) {
  if (commandQueueCount >= MESSAGE_QUEUE_SIZE) {
    return false;
  }

  commandQueue[commandQueueTail] = command;

  commandQueueTail =
    (commandQueueTail + 1) % MESSAGE_QUEUE_SIZE;

  commandQueueCount++;

  return true;
}


bool dequeueCommand(String& command) {
  if (commandQueueCount <= 0) {
    return false;
  }

  command = commandQueue[commandQueueHead];
  commandQueue[commandQueueHead] = "";

  commandQueueHead =
    (commandQueueHead + 1) % MESSAGE_QUEUE_SIZE;

  commandQueueCount--;

  return true;
}


// =========================================================
// NLI code format check
// [①|②|③|④|⑤|⑥] = 5 pipe characters
// =========================================================

bool isSixPartitionNliCode(const String& code) {
  if (code.length() < 7) {
    return false;
  }

  if (code.charAt(0) != '[') {
    return false;
  }

  if (code.charAt(code.length() - 1) != ']') {
    return false;
  }

  int pipeCount = 0;

  for (size_t index = 0; index < code.length(); index++) {
    if (code.charAt(index) == '|') {
      pipeCount++;
    }
  }

  return pipeCount == 5;
}


// =========================================================
// Serial debug input
// Queue the entered NLI code simultaneously to the module and Python core
// =========================================================

void processSerialMonitor() {
  while (Monitor.available() > 0) {
    int rawValue = Monitor.read();

    if (rawValue < 0) {
      break;
    }

    char receivedChar = (char)rawValue;

    if (receivedChar == '\r') {
      continue;
    }

    if (receivedChar == '\n') {
      String code = serialDebugInput;
      serialDebugInput = "";

      code.trim();

      if (code.length() > 0) {
        // Display debug input regardless of actual routing success
        addNextionCode(code);
        routeSerialDebugInput(code);
      }

      continue;
    }

    if (serialDebugInput.length() >= SERIAL_INPUT_MAX_LENGTH) {
      serialDebugInput = "";

      Monitor.println();
      Monitor.println("[DEBUG INPUT ERROR] Input length exceeded");
      Monitor.println("[DEBUG INPUT] Buffer cleared.");
      continue;
    }

    serialDebugInput += receivedChar;
  }
}


void routeSerialDebugInput(String input) {
  input.trim();

  Monitor.println();
  Monitor.println("========== DEBUG SERIAL INPUT ==========");
  Monitor.println(input);
  Monitor.println("========================================");

  bool isNliCode = isSixPartitionNliCode(input);

  // Treat input containing brackets as intended to be an NLI code.
  // If the format is invalid, do not send it as normal conversation.
  if (!isNliCode &&
      (input.startsWith("[") || input.endsWith("]"))) {
    Monitor.println(
      "[DEBUG INPUT ERROR] Invalid [①|②|③|④|⑤|⑥] format."
    );
    return;
  }

  Monitor.println();
  Monitor.println("========== DEBUG ROUTE RESULT ==========");

  if (isNliCode) {
    // As previously required, I/R debug codes are sent to the Python core and the actual module
    // with the same original text simultaneously.
    bool coreQueued = enqueueModuleMessage(input);
    bool moduleQueued = enqueueCommand(input);

    Monitor.print("[NLI CODE -> PYTHON CORE] ");
    Monitor.println(coreQueued ? "QUEUED" : "QUEUE_FULL");

    Monitor.print("[NLI CODE -> MODULE] ");
    Monitor.println(moduleQueued ? "QUEUED" : "QUEUE_FULL");
  }
  else {
    // Normal sentences are conversations with the robot core, so
    // send them only to the Python core. Python converts them to an internal NLI conversation code.
    bool coreQueued = enqueueModuleMessage(input);

    Monitor.print("[NORMAL CHAT -> PYTHON CORE] ");
    Monitor.println(coreQueued ? "QUEUED" : "QUEUE_FULL");
    Monitor.println("[NORMAL CHAT -> MODULE] NOT_SENT");
  }

  Monitor.println("========================================");
}

// =========================================================
// ESP module → TCP Python → C++
// =========================================================

String showRemoteMessage(String message) {
  message.trim();

  if (message.length() == 0) {
    return "EMPTY_MESSAGE";
  }

  // Display original text received from the module regardless of later queue success
  addNextionCode(message);

  Monitor.println();
  Monitor.println("========== RELAY: MODULE -> C++ ==========");
  Monitor.println(message);
  Monitor.println("==========================================");

  /*
   * Because a Python → C++ RPC is currently executing,
   * do not call Python again immediately here.
   * Store it in the queue and process it in loop().
   */

  if (!enqueueModuleMessage(message)) {
    Monitor.println(
      "[RELAY ERROR] Python input queue is full"
    );

    return "QUEUE_FULL";
  }

  return "QUEUED_FOR_PYTHON";
}


// =========================================================
// NLI Python → C++
// The current system delivers the ③ command field of an O code.
// =========================================================

String receiveCommand(String command) {
  command.trim();

  if (command.length() == 0) {
    return "EMPTY_COMMAND";
  }

  // Display the actual command string extracted from the O code regardless of module-send success
  addNextionCode(String("[CMD] ") + command);

  Monitor.println();
  Monitor.println("========== RELAY: PYTHON CORE -> C++ ==========");
  Monitor.println(command);
  Monitor.println("===============================================");

  /*
   * Because a Python → C++ RPC is currently executing,
   * do not call Python send_to_esp immediately here.
   */

  if (!enqueueCommand(command)) {
    Monitor.println(
      "[RELAY ERROR] Module output queue is full"
    );

    return "QUEUE_FULL";
  }

  return "QUEUED_FOR_MODULE";
}


// =========================================================
// C++ → NLI Python
// =========================================================

void sendModuleInfoToPython(String moduleCode) {
  moduleCode.trim();

  if (moduleCode.length() == 0) {
    return;
  }

  Monitor.println();
  Monitor.println("========== RELAY: C++ -> PYTHON CORE ==========");
  Monitor.println(moduleCode);
  Monitor.println("===============================================");

  String pythonResult;

  RpcCall rpc = Bridge.call(
    "receive_module_info",
    moduleCode
  );

  bool ok = rpc.result(
    pythonResult
  );

  if (ok) {
    Monitor.print("[PYTHON CORE RESULT] ");
    Monitor.println(pythonResult);
  }
  else {
    Monitor.println(
      "[RELAY ERROR] Python function call failed"
    );

    Monitor.print("[ERROR CODE] ");
    Monitor.println(
      rpc.getErrorCode()
    );

    Monitor.print("[ERROR MESSAGE] ");
    Monitor.println(
      rpc.getErrorMessage()
    );
  }
}


// =========================================================
// C++ → TCP Python → ESP module
// =========================================================

void sendCommandToESP(String command) {
  command.trim();

  if (command.length() == 0) {
    return;
  }

  Monitor.println();
  Monitor.println("========== RELAY: C++ -> MODULE ==========");
  Monitor.println(command);
  Monitor.println("==========================================");

  bool result = false;

  RpcCall rpc = Bridge.call(
    "send_to_esp",
    command
  );

  bool ok = rpc.result(
    result
  );

  if (!ok) {
    Monitor.println(
      "[RELAY ERROR] send_to_esp call failed"
    );

    Monitor.print("[ERROR CODE] ");
    Monitor.println(
      rpc.getErrorCode()
    );

    Monitor.print("[ERROR MESSAGE] ");
    Monitor.println(
      rpc.getErrorMessage()
    );

    return;
  }

  if (result) {
    Monitor.println(
      "[MODULE SEND RESULT] SUCCESS"
    );
  }
  else {
    Monitor.println(
      "[MODULE SEND RESULT] FAILED_OR_DISCONNECTED"
    );
  }
}


// =========================================================
// setup
// =========================================================

void setup() {
  Serial.begin(115200);
  Monitor.begin(115200);
  Serial1.begin(NEXTION_BAUD);

  // Give Nextion enough time to boot, then read setting.tcp_ip.txt once and cache it.
  delay(700);

  // Retry a few times because the HMI page may still be initializing.
  for (int attempt = 0; attempt < 3 && !nextionTcpIpValid; attempt++) {
    refreshTcpIpFromNextion();

    if (!nextionTcpIpValid) {
      delay(250);
    }
  }

  setNextionRecordingIndicator(false);

  // Initial face state.
  setNextionVal("face_exp", 0);
  setNextionVal("mouth_ani", 0);
  setNextionVal("mouth_h", 10);
  setNextionVal("mouth_dir", 1);

  // Draw/settle once, then Nextion ani_busy logic will turn tm0 off.
  enableFaceTimer();

  sendNextionText("NEXTION READY");

  if (!Bridge.begin()) {
    Serial.println(
      "[ERROR] Bridge start failed"
    );
  }
  else {
    Serial.println(
      "[OK] Bridge start successful"
    );
  }

  if (!Bridge.provide(
        "show_remote_message",
        showRemoteMessage
      )) {
    Serial.println(
      "[ERROR] show_remote_message registration failed"
    );
  }

  if (!Bridge.provide(
        "receive_command",
        receiveCommand
      )) {
    Serial.println(
      "[ERROR] receive_command registration failed"
    );
  }

  if (!Bridge.provide(
        "show_code",
        showCode
      )) {
    Serial.println(
      "[ERROR] show_code registration failed"
    );
  }

  if (!Bridge.provide(
        "set_recording_state",
        setRecordingState
      )) {
    Serial.println(
      "[ERROR] set_recording_state registration failed"
    );
  }


  if (!Bridge.provide(
        "get_tcp_ip",
        getTcpIp
      )) {
    Serial.println(
      "[ERROR] get_tcp_ip registration failed"
    );
  }

  if (!Bridge.provide(
        "show_narration",
        showNarration
      )) {
    Serial.println(
      "[ERROR] show_narration registration failed"
    );
  }

  if (!Bridge.provide(
        "hide_narration",
        hideNarration
      )) {
    Serial.println(
      "[ERROR] hide_narration registration failed"
    );
  }

  if (!Bridge.provide(
        "set_speaking_state",
        setSpeakingState
      )) {
    Serial.println(
      "[ERROR] set_speaking_state registration failed"
    );
  }

  if (!Bridge.provide(
        "set_face_expression",
        setFaceExpression
      )) {
    Serial.println(
      "[ERROR] set_face_expression registration failed"
    );
  }

  Monitor.println();
  Monitor.println("========================================");
  Monitor.println("Arduino UNO Q NLI0.1.3 Gateway Ready");
  Monitor.println("Module -> C++ -> Python -> Hugging Face");
  Monitor.println("Hugging Face -> Python -> C++ -> Module");
  Monitor.println("NLI partition ④ -> Python -> C++ case -> Nextion face_exp/manual ani");
  Monitor.print("TCP IP source: Nextion setting.tcp_ip.txt -> C++ cache -> Python: ");
  Monitor.println(nextionTcpIpValid ? nextionTcpIpAddress : String("NOT_AVAILABLE"));
  Monitor.println("----------------------------------------");
  Monitor.println("DEBUG 1: 6-part I/R codes are sent simultaneously to the module and Python");
  Monitor.println("Example: [R|ARM01|position=home|neutral|Pick up the cup for me|]");
  Monitor.println("DEBUG 2: Normal sentences communicate directly with the Python core");
  Monitor.println("Example: Hello, how are you feeling today?");
  Monitor.println("Serial line-ending setting: Newline");
  Monitor.println("========================================");
}


// =========================================================
// loop
// =========================================================

void loop() {
  Bridge.update();

  // Return any non-normal expression to NORMAL after 3 seconds.
  updateFaceExpressionTimer();

  // Hide g0 5 seconds after the completed TTS utterance.
  updateNarrationHide();

  // Check serial debug input
  processSerialMonitor();

  // Deliver module or serial debug input to the Python core
  String moduleMessage;

  if (dequeueModuleMessage(moduleMessage)) {
    sendModuleInfoToPython(
      moduleMessage
    );
  }

  // Deliver Python commands or serial debug input to the module
  String command;

  if (dequeueCommand(command)) {
    sendCommandToESP(
      command
    );
  }

  delay(5);
}
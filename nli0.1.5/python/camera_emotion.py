import os
import threading
import time

import cv2
import numpy as np
import onnxruntime as ort

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

RAW_EMOTION_CLASSES = ["Joy", "Flustered", "Anger", "Anxiety", "Hurt", "Sad"]
EMOTION_GROUP_MAP = {
    "Joy": "joy",
    "Flustered": "anxiety",
    "Anger": "anxiety",
    "Anxiety": "anxiety",
    "Hurt": "sadness",
    "Sad": "sadness",
}
EMOTION_GROUP_NAMES = list(dict.fromkeys(EMOTION_GROUP_MAP.values()))

FACE_INPUT_SIZE = 640
EMOTION_INPUT_SIZE = 224
IMAGENET_MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
IMAGENET_STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)

CONFIRM_COUNT = 3
MIN_FACE_AREA_RATIO = 12

_latest_emotion = "neutral"
_latest_confidence = 0.0
_state_lock = threading.Lock()
_started = False


def _set_latest_emotion(emotion, confidence=0.0):
    global _latest_emotion, _latest_confidence
    with _state_lock:
        _latest_emotion = str(emotion)
        _latest_confidence = float(confidence)


def get_user_emotion():
    """Returns the most recently confirmed user emotion for NLI."""
    with _state_lock:
        return _latest_emotion


def get_user_emotion_state():
    """For debugging: returns (emotion, confidence)."""
    with _state_lock:
        return _latest_emotion, _latest_confidence


def _letterbox(frame, new_size=FACE_INPUT_SIZE, color=(114, 114, 114)):
    h, w = frame.shape[:2]
    scale = min(new_size / h, new_size / w)
    new_w, new_h = int(round(w * scale)), int(round(h * scale))
    resized = cv2.resize(frame, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

    pad_w, pad_h = new_size - new_w, new_size - new_h
    top, bottom = pad_h // 2, pad_h - pad_h // 2
    left, right = pad_w // 2, pad_w - pad_w // 2

    padded = cv2.copyMakeBorder(
        resized, top, bottom, left, right,
        cv2.BORDER_CONSTANT, value=color,
    )
    return padded, scale, left, top


def _softmax(logits):
    e = np.exp(logits - np.max(logits))
    return e / e.sum()


def _open_first_available_camera(max_index=5):
    for idx in range(max_index):
        candidate = cv2.VideoCapture(idx)
        if candidate.isOpened():
            ret, _ = candidate.read()
            if ret:
                print(f"[Camera] index {idx} webcam is being used.")
                candidate.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                return candidate
            candidate.release()
    return None


def _camera_loop():
    face_session = None
    emotion_session = None
    cap = None

    try:
        face_session = ort.InferenceSession(
            os.path.join(BASE_DIR, "yolov8n-face.onnx"),
            providers=["CPUExecutionProvider"],
        )
        face_input_name = face_session.get_inputs()[0].name

        emotion_session = ort.InferenceSession(
            os.path.join(BASE_DIR, "emotion_resnet18.onnx"),
            providers=["CPUExecutionProvider"],
        )
        emotion_input_name = emotion_session.get_inputs()[0].name

        cap = _open_first_available_camera()
        if cap is None:
            print("[Camera ERROR] No available webcam was found.")
            _set_latest_emotion("neutral", 0.0)
            return

        candidate_emotion = None
        candidate_count = 0
        last_confirmed_emotion = None

        print("[Camera] User emotion recognition started")

        while True:
            try:
                for _ in range(2):
                    cap.grab()

                ret, frame = cap.read()
                if not ret:
                    print("[Camera] Failed to read frame; retrying...")
                    time.sleep(0.5)
                    continue

                h_frame, w_frame, _ = frame.shape
                min_face_area = (w_frame * h_frame) / MIN_FACE_AREA_RATIO

                padded, scale, pad_left, pad_top = _letterbox(frame)
                rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)
                blob = rgb.astype(np.float32) / 255.0
                blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

                outputs = face_session.run(None, {face_input_name: blob})[0][0]
                valid_face_found = False

                for x1, y1, x2, y2, conf, _cls in outputs:
                    if conf <= 0:
                        continue

                    ox1 = int((x1 - pad_left) / scale)
                    oy1 = int((y1 - pad_top) / scale)
                    ox2 = int((x2 - pad_left) / scale)
                    oy2 = int((y2 - pad_top) / scale)

                    ox1, oy1 = max(0, ox1), max(0, oy1)
                    ox2, oy2 = min(w_frame, ox2), min(h_frame, oy2)

                    face_area = (ox2 - ox1) * (oy2 - oy1)
                    if face_area < min_face_area:
                        continue

                    face = frame[oy1:oy2, ox1:ox2]
                    if face.size == 0:
                        continue

                    valid_face_found = True

                    resized = cv2.resize(
                        face,
                        (EMOTION_INPUT_SIZE, EMOTION_INPUT_SIZE),
                        interpolation=cv2.INTER_LINEAR,
                    )
                    arr = resized.astype(np.float32) / 255.0
                    arr = (arr - IMAGENET_MEAN) / IMAGENET_STD
                    arr = arr.transpose(2, 0, 1)[np.newaxis, ...].astype(np.float32)

                    logits = emotion_session.run(None, {emotion_input_name: arr})[0][0]
                    probs = _softmax(logits)

                    group_probs = {name: 0.0 for name in EMOTION_GROUP_NAMES}
                    for raw_name, prob in zip(RAW_EMOTION_CLASSES, probs.tolist()):
                        group_probs[EMOTION_GROUP_MAP[raw_name]] += prob

                    current_emotion = max(group_probs, key=group_probs.get)
                    group_prob = group_probs[current_emotion]

                    if current_emotion == candidate_emotion:
                        candidate_count += 1
                    else:
                        candidate_emotion = current_emotion
                        candidate_count = 1

                    if candidate_count >= CONFIRM_COUNT:
                        _set_latest_emotion(current_emotion, group_prob)

                        if current_emotion != last_confirmed_emotion:
                            last_confirmed_emotion = current_emotion
                            print(
                                f"[Emotion] confirmed: {current_emotion} "
                                f"({group_prob * 100:.1f}%)"
                            )

                    # As in the existing code, process detected valid faces in order.

                if not valid_face_found:
                    candidate_emotion = None
                    candidate_count = 0
                    _set_latest_emotion("neutral", 0.0)

            except Exception as error:
                print(f"[Camera Loop ERROR] {error}")
                time.sleep(0.5)

    except Exception as error:
        print(f"[Camera Init ERROR] {error}")
        _set_latest_emotion("neutral", 0.0)

    finally:
        if cap is not None:
            cap.release()


def start_camera_worker():
    """Starts the camera emotion-recognition thread only once."""
    global _started

    if _started:
        return

    _started = True
    threading.Thread(
        target=_camera_loop,
        daemon=True,
        name="camera-emotion",
    ).start()

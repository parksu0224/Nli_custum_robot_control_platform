# Face control changes

- NLI output partition ④ is now sent from Python to C++ for both `O` and `none` outputs.
- Python remains responsible for deciding `O` vs `none`; C++ does not reinterpret the routing decision.
- C++ maps preset face names to `face_exp.val` using an enum + `switch`.
- `MANUAL:...` sends `face_exp.val=99` first, then detailed `_ani.val` commands.
- Existing external-module command path remains unchanged: only partition ③ of an `O` output is passed to `receive_command` and queued for ESP.
- TTS drives `mouth_ani.val=1` only while `aplay` is running, then returns it to `0` and resets `mouth_h` to `10`.

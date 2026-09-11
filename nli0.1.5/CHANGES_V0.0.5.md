# NLI v0.0.5 Change Specification

## Hugging Face Call Conditions

Hugging Face calls are not dependent on the R type.

- `I`: Send an HF request after registering or updating the currently connected module
- `R`: Send an HF request after saving the latest status
- Normal sentence: Convert to the conversation code `[R|none|none|emotion|utterance|]` and then send an HF request
- STT: An HF request can be sent regardless of whether a recent R exists

## Module Registration

- Only the `I` type is used to register currently connected modules
- The `R` type is used only for status reporting and control decisions
- Registered I codes are included in the `[Currently Connected Modules]` context of every HF request

## Serial Debug Routing

### 6-Part NLI Code

The same original text is delivered simultaneously to the Python NLI core and the ESP module.

### Normal Sentence

It is delivered only to the Python NLI core and is not sent directly to the module.

## Serial Relay Tracking

- MODULE → C++
- C++ → PYTHON CORE
- PYTHON CORE → C++
- C++ → MODULE

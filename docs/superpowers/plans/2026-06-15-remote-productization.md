# Remote Productization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the non-generated firmware foundation for a mature S3 remote: persistent joystick calibration, robust protocol metadata, safer receiver state, diagnostics, power-management hooks, sensor usability, firmware-upgrade documentation, regression matrix, and a final SquareLine UI component list.

**Architecture:** Keep generated SquareLine files untouched unless the user exports new UI. Add protocol and state helpers in headers so native tests cover packet versioning, CRC, diagnostics, and sensor formatting. Keep hardware-specific persistence and runtime behavior in `src/transmitter_s3/main.cpp` and `src/receiver/main.cpp`, then expose UI-ready fields through `S3UiState`.

**Tech Stack:** PlatformIO, Arduino ESP32-C3/S3, ESP-NOW, LVGL 8.3, Unity native tests, Preferences/NVS on S3.

---

### Task 1: Shared Protocol and Diagnostics

**Files:**
- Create: `include/protocol.h`
- Modify: `src/transmitter/main.cpp`
- Modify: `src/transmitter_s3/main.cpp`
- Modify: `src/receiver/main.cpp`
- Modify: `test/test_control_logic/test_main.cpp`

Steps:
- [ ] Add `CONTROL_PROTOCOL_VERSION`, `STATUS_PROTOCOL_VERSION`, packet sequence fields, status flags, and CRC8 helper.
- [ ] Native-test CRC8, sequence freshness, and version rejection.
- [ ] Update all packet structs and checksum calls to use CRC8 over the full packet except the trailing CRC byte.
- [ ] Receiver rejects wrong versions and stale sequence numbers.
- [ ] Transmitters show link diagnostics from status packet metadata.

### Task 2: Persistent Joystick Calibration

**Files:**
- Modify: `include/control_logic.h`
- Modify: `src/transmitter_s3/main.cpp`
- Modify: `test/test_control_logic/test_main.cpp`

Steps:
- [ ] Add tested helpers for calibration validity and throttle mapping from persisted min/center/max/deadzone.
- [ ] Store S3 joystick center and trim in NVS using `Preferences`.
- [ ] Save calibration when the user presses `CAL` or adjusts `-10/+10`.
- [ ] Fall back to boot sampling if no valid NVS record exists.

### Task 3: Safer Receiver State

**Files:**
- Modify: `src/receiver/main.cpp`
- Modify: `include/control_logic.h`
- Modify: `test/test_control_logic/test_main.cpp`

Steps:
- [ ] Keep output neutral until a stable, fresh, correctly versioned transmitter stream is seen.
- [ ] Add status bits for failsafe, armed/locked source state, VESC telemetry validity, and protocol fault.
- [ ] Make VESC telemetry failure visible in `StatusPacket`.

### Task 4: Diagnostics, Sensors, and Power Hooks

**Files:**
- Modify: `src/transmitter_s3/main.cpp`
- Modify: `src/transmitter_s3/ui/ui.h`
- Modify: `src/transmitter_s3/ui/ui.cpp`
- Modify: `include/s3_ui_bindings.h`
- Modify: `test/test_control_logic/test_main.cpp`

Steps:
- [ ] Add UI state fields for packet rate, packet loss estimate, RSSI, receiver status flags, firmware version, brightness, QMC calibration status, BMP altitude reference, and MCU temperature warning.
- [ ] Add tested formatting helpers for diagnostic labels.
- [ ] Dim backlight after inactivity and restore on touch/button/input activity.
- [ ] Keep labels optional: code compiles before SquareLine components exist; final document gives component names to add.

### Task 5: Documentation and Regression Matrix

**Files:**
- Create: `docs/productization-roadmap.md`
- Create: `docs/regression-matrix.md`
- Create: `docs/s3-ui-final-components.md`
- Modify: `docs/protocol.md`
- Modify: `docs/user-guide.md`
- Modify: `docs/progress.md`

Steps:
- [ ] Document firmware upgrade workflow through USB PlatformIO and version display.
- [ ] Document final required SquareLine pages and object names.
- [ ] Record regression commands and hardware checks, including receiver upload on COM4.

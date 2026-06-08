# S3 Transmitter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the formal ESP32-S3 advanced transmitter project and port the base transmitter behavior onto the new display/touch hardware.

**Architecture:** Keep `s3_lvgl_probe` as a hardware probe. Add a new `s3_transmitter` PlatformIO environment and `src/transmitter_s3/` formal application that reuses the existing ESP-NOW packet protocol, control logic helpers, and verified S3 display/I2C configuration.

**Tech Stack:** PlatformIO, Arduino framework, LovyanGFX, ESP-NOW, `Wire1`, existing `control_logic.h` and `beep_profiles.h`.

---

### Task 1: Project Structure

**Files:**
- Modify: `platformio.ini`
- Create: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Add `s3_transmitter` env**

Use ESP32-S3, Arduino, COM7, LovyanGFX, and source filter `+<transmitter_s3/>`.

- [x] **Step 2: Keep probe isolated**

Do not remove `s3_lvgl_probe`; exclude `transmitter_s3` from the probe build.

### Task 2: S3 Formal Transmitter

**Files:**
- Create: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Port base transmitter protocol**

Use the same `ControlPacket`, `StatusPacket`, checksum, 100Hz control send, and 500ms status timeout.

- [x] **Step 2: Add placeholder physical pins**

Use temporary GPIO definitions for joystick, switches, buttons, and buzzer. Mark them as placeholders in code and docs.

- [x] **Step 3: Reuse S3 display and touch**

Use the verified GC9A01/CST816 LovyanGFX configuration and corrected touch X mapping.

- [x] **Step 4: Reuse S3 local sensors**

Read CW2015, BMP280, and QMC5883L on GPIO18/GPIO19. Keep LSM6DSLTR as not found until hardware responds.

### Task 3: Receiver Compatibility

**Files:**
- Modify: `src/receiver/main.cpp`

- [x] **Step 1: Add S3 transmitter MAC**

Allow `48:CA:43:9A:A9:B0` in addition to the existing C3 transmitter MAC.

- [x] **Step 2: Keep C3 compatibility**

Do not remove the existing C3 MAC binding.

### Task 4: Documentation

**Files:**
- Modify: `docs/hardware.md`
- Modify: `docs/progress.md`

- [x] **Step 1: Record placeholder pins**

Document that S3 joystick/button/switch/buzzer pins are temporary software placeholders.

- [x] **Step 2: Record formal project start**

Document the new environment and current hardware limitations.

### Task 5: Verification

**Files:**
- Modify: `docs/test-runs/2026-06-09-s3-transmitter.md`

- [x] **Step 1: Build**

Run `pio run -e s3_transmitter`, `pio run -e receiver`, and `pio test -e native`.

- [ ] **Step 2: Upload S3**

Upload `s3_transmitter` to COM7.

Status: blocked by COM7 `PermissionError(13, '拒绝访问。')`; firmware builds successfully but has not been uploaded.

- [ ] **Step 3: Upload C3 if available**

Attempt C3 uploads only if COM3/COM10 are connected.

Status: skipped because `pio device list` shows COM1 and COM7 only.

- [x] **Step 4: Commit and push**

Commit the formal project files and push to GitHub.

# S3 Sensor Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Read CW2015, BMP280, LSM6DSLTR, and QMC5883L on ESP32-S3 GPIO18/GPIO19 and show live data on the round display.

**Architecture:** Keep the existing `s3_lvgl_probe` as a hardware probe and add a second I2C bus using `Wire1`. Use minimal register-level drivers inside the probe file so the first bring-up can distinguish wiring/address problems from library integration problems.

**Tech Stack:** PlatformIO, Arduino framework, LovyanGFX, ESP32-S3 `TwoWire`.

---

### Task 1: Add Second I2C Bus And Scan

**Files:**
- Modify: `src/transmitter_s3_lvgl_probe/main.cpp`

- [x] **Step 1: Include Wire and define bus pins**

Add `#include <Wire.h>` and constants for GPIO18/GPIO19.

- [x] **Step 2: Start `Wire1`**

Call `Wire1.begin(18, 19, 400000)` during setup.

- [x] **Step 3: Scan addresses**

Probe `0x08..0x77`, print detected addresses to COM7, and use the results for device presence flags.

### Task 2: Add Minimal Sensor Drivers

**Files:**
- Modify: `src/transmitter_s3_lvgl_probe/main.cpp`

- [x] **Step 1: CW2015**

Read voltage from register `0x02` and SOC from register `0x04`.

- [x] **Step 2: BMP280**

Detect chip ID `0x58`, read calibration registers, configure normal mode, compensate temperature and pressure.

- [x] **Step 3: LSM6DSLTR**

Detect WHO_AM_I `0x6A`, configure accel/gyro, read accel XYZ and gyro XYZ.

- [x] **Step 4: QMC5883L**

Configure continuous mode at address `0x0D`, read magnetic XYZ, and compute rough heading from `atan2(y, x)`.

### Task 3: Draw Dashboard

**Files:**
- Modify: `src/transmitter_s3_lvgl_probe/main.cpp`

- [x] **Step 1: Replace static probe screen**

Draw a compact status dashboard using fixed text rows.

- [x] **Step 2: Refresh periodically**

Refresh sensor values every 500ms and leave touch coordinate readout at the bottom.

### Task 4: Verify And Document

**Files:**
- Modify: `docs/progress.md`
- Modify: `docs/test-runs/2026-06-08-s3-lvgl-probe.md`

- [x] **Step 1: Build and upload**

Run `pio run -e s3_lvgl_probe` and upload to COM7.

- [x] **Step 2: Regression**

Run `pio test -e native`, `pio run -e transmitter`, and `pio run -e receiver`.

- [x] **Step 3: Firmware upload**

Upload transmitter to COM3 and receiver to COM10.

- [x] **Step 4: Commit and push**

Commit the code and docs, then push `main` to GitHub.

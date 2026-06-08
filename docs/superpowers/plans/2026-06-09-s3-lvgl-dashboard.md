# S3 LVGL Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add LVGL 8.3.11 to the formal `s3_transmitter` project and move the S3 dashboard UI from direct LovyanGFX drawing to LVGL, while keeping the existing control, ESP-NOW, sensor, and beeper behavior unchanged.

**Architecture:** Keep LovyanGFX as the verified display/touch hardware layer. Register LVGL display flush and touch read callbacks against the existing `S3RoundDisplay`, then update LVGL widgets from the current transmitter state.

**Tech Stack:** PlatformIO, Arduino framework, LovyanGFX, LVGL 8.3.11, ESP-NOW.

---

### Task 1: PlatformIO and LVGL Config

**Files:**
- Modify: `platformio.ini`
- Create: `include/lv_conf.h`

- [x] **Step 1: Add LVGL dependency**

Add `lvgl/lvgl@8.3.11` only to `[env:s3_transmitter]`.

- [x] **Step 2: Add compile flag and config**

Enable `LV_CONF_INCLUDE_SIMPLE` and provide a minimal `lv_conf.h` for 16-bit color and the widgets used by the dashboard.

### Task 2: LVGL Driver Bridge

**Files:**
- Modify: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Initialize LVGL**

Create a draw buffer, register display flush callback, register CST816 touch read callback, and build the dashboard widgets.

- [x] **Step 2: Preserve existing hardware behavior**

Do not change ESP-NOW packet timing, sensor reads, joystick calibration, placeholder GPIOs, or beeper logic.

### Task 3: Dashboard Port

**Files:**
- Modify: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Replace direct drawing**

Replace `drawDashboard()` calls with LVGL label/bar updates.

- [x] **Step 2: Run LVGL timers**

Call `lv_tick_inc()` and `lv_timer_handler()` from the Arduino loop.

### Task 4: Documentation and Verification

**Files:**
- Modify: `docs/progress.md`
- Modify: `docs/test-runs/2026-06-09-s3-transmitter.md`

- [x] **Step 1: Record LVGL integration**

Document that the formal S3 transmitter now uses LVGL 8.3.11, while `s3_lvgl_probe` remains a LovyanGFX hardware probe.

- [x] **Step 2: Build, upload if possible, commit, push**

Run builds/tests, try COM7 upload, then commit only the LVGL-related changes and push to GitHub.

Status: builds and native tests passed. Initial COM7 upload failed because Windows denied access to the port. After the port was released, `s3_transmitter` uploaded successfully and startup logs were captured. LVGL changes were committed and pushed to GitHub.

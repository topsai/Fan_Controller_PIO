# S3 UI Split And Touch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the formal S3 transmitter LVGL page into a dedicated UI module and add a minimal touch visualizer for confirming pointer input.

**Architecture:** Keep `main.cpp` responsible for hardware, ESP-NOW, sensors, and LVGL driver callbacks. Move LVGL object creation and dashboard updates into `src/transmitter_s3/ui/` with a SquareLine-friendly `ui_init()` style boundary. Touch data flows from the existing LVGL input callback into the UI module through a small state function.

**Tech Stack:** PlatformIO, Arduino framework, LovyanGFX, LVGL 8.3.11.

---

### Task 1: UI Module Boundary

**Files:**
- Create: `src/transmitter_s3/ui/ui.h`
- Create: `src/transmitter_s3/ui/ui.cpp`
- Modify: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Define UI state and public API**

Create `S3UiState` in `ui.h` with the dashboard fields currently displayed by `main.cpp`, plus `ui_init()`, `ui_update()`, and `ui_set_touch()`.

- [x] **Step 2: Move LVGL object ownership**

Move label/bar globals, `createDashboardLabel()`, dashboard creation, and dashboard update logic from `main.cpp` into `ui.cpp`.

### Task 2: Touch Visualizer

**Files:**
- Modify: `src/transmitter_s3/ui/ui.cpp`
- Modify: `src/transmitter_s3/main.cpp`

- [x] **Step 1: Add touch dot and label**

Create a small dot object and `TOUCH x,y` label in `ui.cpp`. Hide the dot and show `TOUCH --` when released.

- [x] **Step 2: Feed touch state from callback**

Call `ui_set_touch(true, x, y)` from the LVGL touch read callback and `ui_set_touch(false, 0, 0)` when released.

### Task 3: Integration And Verification

**Files:**
- Modify: `docs/progress.md`
- Modify: `docs/test-runs/2026-06-09-s3-transmitter.md`

- [x] **Step 1: Build and upload**

Run `pio run -e s3_transmitter`, upload to COM7, and read startup logs.

- [x] **Step 2: Regression checks**

Run `pio run -e receiver`, `pio run -e transmitter`, `pio run -e s3_lvgl_probe`, and `pio test -e native`.

- [x] **Step 3: Document and push**

Record the UI split, touch visualizer, verification results, commit, and push to GitHub. Keep the existing `src/transmitter_s3_lvgl_probe/main.cpp` I2C frequency change out of this commit.

Status: implementation, build, upload, regression checks, documentation, commit, and push are complete.

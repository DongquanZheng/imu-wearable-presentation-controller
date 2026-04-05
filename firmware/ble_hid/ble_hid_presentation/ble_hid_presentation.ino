#include <M5Unified.h>
#include <BleKeyboard.h>

static constexpr uint16_t COLOR_BG = 0x0000;
static constexpr uint16_t COLOR_TEXT = 0xFD20;
static constexpr uint16_t COLOR_BADGE_SLIDES = 0x07E0;
static constexpr uint16_t COLOR_BADGE_POINTER = 0x5D1F;
static constexpr uint16_t COLOR_PANEL_SOFT = 0x18C3;
static constexpr uint16_t COLOR_OK = 0x07E0;
static constexpr uint16_t COLOR_WARN = 0xFBE0;
static constexpr uint16_t COLOR_SUBTEXT = 0x9CF3;
static constexpr uint16_t COLOR_HINT_VALUE = 0x07FF;
static constexpr uint8_t DISPLAY_ROTATION = 3;

static constexpr uint32_t EMIT_COOLDOWN_MS = 120;
static constexpr uint32_t A_LONG_PRESS_MS = 550;
static constexpr uint32_t B_LONG_PRESS_MS = 320;
static constexpr uint32_t A_DOUBLE_MIN_GAP_MS = 80;
static constexpr uint32_t A_DOUBLE_MAX_GAP_MS = 420;
static constexpr uint32_t B_DOUBLE_MIN_GAP_MS = 80;
static constexpr uint32_t B_DOUBLE_MAX_GAP_MS = 420;
static constexpr uint32_t DOUBLE_COOLDOWN_MS = 900;
static constexpr uint32_t DYNAMIC_REFRESH_MS = 250;
static constexpr float SWING_TRIGGER_T = 62.0f;
static constexpr float SWING_MARGIN_M = 10.0f;
static constexpr float SWING_REARM_T = 18.0f;
static constexpr uint32_t SWING_COOLDOWN_MS = 700;
static constexpr uint32_t SWING_IGNORE_AFTER_TRIGGER_MS = 220;
static constexpr int SWING_CALIB_SAMPLES = 12;

enum Mode {
  MODE_IDLE = 0,
  MODE_SLIDES = 1,
  MODE_POINTER = 2
};

struct AppState {
  Mode mode = MODE_IDLE;
  uint32_t lastEmitMs = 0;
  uint32_t lastToggleMs = 0;

  uint32_t aPressMs = 0;
  bool aPressed = false;
  bool aLongFired = false;
  bool aWaitSecond = false;
  uint32_t aLastReleaseMs = 0;
  uint32_t aSingleDeadlineMs = 0;

  uint32_t bPressMs = 0;
  bool bPressed = false;
  bool bLongFired = false;
  bool bWaitSecond = false;
  uint32_t bLastReleaseMs = 0;
  uint32_t bSingleDeadlineMs = 0;

  uint32_t sessionStartMs = 0;
  bool bleConnected = false;

  bool swingBiasReady = false;
  int swingCalibRemain = 0;
  float swingSumGx = 0.0f;
  float swingSumGy = 0.0f;
  float swingBiasGx = 0.0f;
  float swingBiasGy = 0.0f;
  bool swingArmed = true;
  uint32_t lastSwingMs = 0;
  uint32_t lastSwingTriggerMs = 0;

  bool dirty = true;
} state;

BleKeyboard bleKeyboard("PB-Watch", "PB-Control", 100);

uint32_t nowMs() {
  return millis();
}

bool canEmit() {
  return (nowMs() - state.lastEmitMs) >= EMIT_COOLDOWN_MS;
}

bool ensureBle() {
  if (bleKeyboard.isConnected()) return true;
  return false;
}

bool pressCombo(uint8_t mod1, uint8_t key) {
  if (!ensureBle()) return false;
  bleKeyboard.press(mod1);
  bleKeyboard.press(key);
  delay(12);
  bleKeyboard.release(key);
  bleKeyboard.release(mod1);
  state.lastEmitMs = nowMs();
  return true;
}

bool pressCombo2(uint8_t mod1, uint8_t mod2, uint8_t key) {
  if (!ensureBle()) return false;
  bleKeyboard.press(mod1);
  bleKeyboard.press(mod2);
  bleKeyboard.press(key);
  delay(12);
  bleKeyboard.release(key);
  bleKeyboard.release(mod2);
  bleKeyboard.release(mod1);
  state.lastEmitMs = nowMs();
  return true;
}

bool pressKey(uint8_t key) {
  if (!ensureBle()) return false;
  bleKeyboard.press(key);
  delay(12);
  bleKeyboard.release(key);
  state.lastEmitMs = nowMs();
  return true;
}

const char* modeText() {
  if (state.mode == MODE_SLIDES) return "SLIDES";
  if (state.mode == MODE_POINTER) return "POINTER";
  return "IDLE";
}

uint16_t modeColor() {
  if (state.mode == MODE_SLIDES) return COLOR_BADGE_SLIDES;
  if (state.mode == MODE_POINTER) return COLOR_BADGE_POINTER;
  return COLOR_TEXT;
}

void formatElapsed(char* out, size_t outSize) {
  uint32_t total = (nowMs() - state.sessionStartMs) / 1000;
  uint32_t mm = total / 60;
  uint32_t ss = total % 60;
  snprintf(out, outSize, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

void resetSwingCalibration() {
  state.swingBiasReady = false;
  state.swingCalibRemain = SWING_CALIB_SAMPLES;
  state.swingSumGx = 0.0f;
  state.swingSumGy = 0.0f;
  state.swingBiasGx = 0.0f;
  state.swingBiasGy = 0.0f;
  state.swingArmed = true;
  state.lastSwingMs = 0;
  state.lastSwingTriggerMs = 0;
}

void drawControlLine(int x, int y, const char* label1, const char* value1, const char* label2 = nullptr, const char* value2 = nullptr) {
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setCursor(x, y);
  M5.Display.print(label1);

  int xNext = x + M5.Display.textWidth(label1);
  M5.Display.setTextColor(COLOR_HINT_VALUE, COLOR_BG);
  M5.Display.setCursor(xNext, y);
  M5.Display.print(value1);

  if (!label2 || !value2) return;

  xNext += M5.Display.textWidth(value1) + 8;
  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setCursor(xNext, y);
  M5.Display.print(label2);

  xNext += M5.Display.textWidth(label2);
  M5.Display.setTextColor(COLOR_HINT_VALUE, COLOR_BG);
  M5.Display.setCursor(xNext, y);
  M5.Display.print(value2);
}

const char* footerHint() {
  if (state.mode == MODE_POINTER) return "Hold A/B to zoom";
  if (state.mode == MODE_SLIDES) return "Swing wrist to turn";
  return "Click A/B to flip";
}

void renderStatus() {
  if (!state.dirty) return;
  state.dirty = false;

  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const int pad = 8;
  M5.Display.fillScreen(COLOR_BG);
  M5.Display.drawFastHLine(0, 18, w, COLOR_PANEL_SOFT);
  M5.Display.drawFastHLine(0, 76, w, COLOR_PANEL_SOFT);

  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(pad, 26);
  M5.Display.print("MODE");

  M5.Display.setTextColor(modeColor(), COLOR_BG);
  M5.Display.setTextSize(2);
  int modeTw = M5.Display.textWidth(modeText());
  M5.Display.setCursor((w - modeTw) / 2, 42);
  M5.Display.print(modeText());

  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setTextSize(1);
  if (state.mode == MODE_POINTER) {
    drawControlLine(pad, 86, "A long: ", "Zoom +", "B long: ", "Zoom -");
    drawControlLine(pad, 100, "B double: ", "Exit to Idle");
  } else if (state.mode == MODE_SLIDES) {
    drawControlLine(pad, 86, "A single: ", "Prev", "B single: ", "Next");
    drawControlLine(pad, 100, "Swing: ", "Prev/Next", "A/B long: ", "WinTab");
  } else {
    drawControlLine(pad, 86, "A single: ", "Prev", "B single: ", "Next");
    drawControlLine(pad, 100, "A double: ", "Pointer", "B double: ", "Slides");
  }

  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(pad, h - 12);
  M5.Display.print(footerHint());
}

void renderDynamic() {
  const int w = M5.Display.width();
  const int pad = 8;

  M5.Display.setTextColor(COLOR_SUBTEXT, COLOR_BG);
  M5.Display.setTextSize(1);

  M5.Display.fillRect(pad, 4, 70, 12, COLOR_BG);
  M5.Display.setCursor(pad, 6);
  if (bleKeyboard.isConnected()) {
    M5.Display.setTextColor(COLOR_OK, COLOR_BG);
    M5.Display.print("BLE ON");
  } else {
    M5.Display.setTextColor(COLOR_WARN, COLOR_BG);
    M5.Display.print("BLE OFF");
  }

  char elapsed[16];
  formatElapsed(elapsed, sizeof(elapsed));
  M5.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Display.fillRect((w / 2) - 28, 4, 56, 12, COLOR_BG);
  M5.Display.setCursor((w / 2) - 18, 6);
  M5.Display.print(elapsed);

  M5.Display.fillRect(w - 56, 4, 56, 12, COLOR_BG);
  M5.Display.setCursor(w - 50, 6);
  M5.Display.printf("%d%%", M5.Power.getBatteryLevel());
}

void sendSlidesPrev() {
  if (!canEmit()) return;
  pressKey(KEY_LEFT_ARROW);
}

void sendSlidesNext() {
  if (!canEmit()) return;
  pressKey(KEY_RIGHT_ARROW);
}

void sendWindowNext() {
  if (!canEmit()) return;
  pressCombo(KEY_LEFT_ALT, KEY_TAB);
}

void sendWindowPrev() {
  if (!canEmit()) return;
  pressCombo2(KEY_LEFT_ALT, KEY_LEFT_SHIFT, KEY_TAB);
}

void sendZoomOnStep() {
  if (!canEmit()) return;
  pressCombo(KEY_LEFT_GUI, KEY_NUM_PLUS);
}

void sendZoomOffStep() {
  if (!canEmit()) return;
  pressCombo(KEY_LEFT_GUI, KEY_NUM_MINUS);
}

void closeMagnifier() {
  pressCombo(KEY_LEFT_GUI, KEY_ESC);
}

void setModeIdle() {
  if (state.mode == MODE_POINTER) {
    closeMagnifier();
  }
  state.mode = MODE_IDLE;
  resetSwingCalibration();
  state.dirty = true;
}

void setModeSlides() {
  if (state.mode == MODE_POINTER) {
    closeMagnifier();
  }
  state.mode = MODE_SLIDES;
  resetSwingCalibration();
  state.dirty = true;
}

void setModePointer() {
  state.mode = MODE_POINTER;
  resetSwingCalibration();
  state.dirty = true;
}

void handleSlideSwings() {
  if (state.mode != MODE_SLIDES) return;
  if (!M5.Imu.isEnabled()) return;
  if (!M5.Imu.update()) return;

  const uint32_t t = nowMs();
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  M5.Imu.getGyro(&gx, &gy, &gz);

  if (!state.swingBiasReady) {
    state.swingSumGx += gx;
    state.swingSumGy += gy;
    state.swingCalibRemain -= 1;

    if (state.swingCalibRemain <= 0) {
      state.swingBiasGx = state.swingSumGx / SWING_CALIB_SAMPLES;
      state.swingBiasGy = state.swingSumGy / SWING_CALIB_SAMPLES;
      state.swingBiasReady = true;
      state.swingCalibRemain = 0;
    }
    return;
  }

  if (state.lastSwingTriggerMs > 0 && (t - state.lastSwingTriggerMs) < SWING_IGNORE_AFTER_TRIGGER_MS) return;
  if (state.lastSwingMs > 0 && (t - state.lastSwingMs) < SWING_COOLDOWN_MS) return;

  gx -= state.swingBiasGx;
  gy -= state.swingBiasGy;

  const float sx = -gy;
  const float sy = -gx;
  const float nextScore = (sx > 0 ? sx : 0) * 0.7f + (sy > 0 ? sy : 0) * 0.5f;
  const float prevScore = (sx < 0 ? -sx : 0) * 0.7f + (sy < 0 ? -sy : 0) * 0.5f;
  const float best = nextScore > prevScore ? nextScore : prevScore;
  const float diff = nextScore > prevScore ? (nextScore - prevScore) : (prevScore - nextScore);

  if (!state.swingArmed) {
    if (best < SWING_REARM_T) state.swingArmed = true;
    return;
  }
  if (best < SWING_TRIGGER_T) return;
  if (diff < SWING_MARGIN_M) return;
  if (!canEmit()) return;

  bool sent = false;
  if (nextScore > prevScore) {
    sent = pressKey(KEY_RIGHT_ARROW);
  } else {
    sent = pressKey(KEY_LEFT_ARROW);
  }
  if (!sent) return;

  state.swingArmed = false;
  state.lastSwingMs = t;
  state.lastSwingTriggerMs = t;
}

void onAShortSingle() {
  if (state.mode == MODE_IDLE || state.mode == MODE_SLIDES) {
    sendSlidesPrev();
  }
}

void onBShortSingle() {
  if (state.mode == MODE_IDLE || state.mode == MODE_SLIDES) {
    sendSlidesNext();
  }
}

void onADouble() {
  if ((nowMs() - state.lastToggleMs) < DOUBLE_COOLDOWN_MS) return;
  setModePointer();
  state.lastToggleMs = nowMs();
}

void onBDouble() {
  if ((nowMs() - state.lastToggleMs) < DOUBLE_COOLDOWN_MS) return;
  if (state.mode == MODE_IDLE) setModeSlides();
  else setModeIdle();
  state.lastToggleMs = nowMs();
}

void onALong() {
  if (state.mode == MODE_SLIDES) {
    sendWindowNext();
    return;
  }
  if (state.mode == MODE_POINTER) {
    sendZoomOnStep();
  }
}

void onBLong() {
  if (state.mode == MODE_SLIDES) {
    sendWindowPrev();
    return;
  }
  if (state.mode == MODE_POINTER) {
    sendZoomOffStep();
  }
}

void handleA() {
  const uint32_t t = nowMs();
  const bool down = M5.BtnA.isPressed();

  if (down && !state.aPressed) {
    state.aPressed = true;
    state.aPressMs = t;
    state.aLongFired = false;
  }

  if (down && !state.aLongFired && (t - state.aPressMs >= A_LONG_PRESS_MS)) {
    onALong();
    state.aLongFired = true;
    state.aWaitSecond = false;
    state.aSingleDeadlineMs = 0;
  }

  if (!down && state.aPressed) {
    state.aPressed = false;
    if (state.aLongFired) {
      state.aLongFired = false;
      return;
    }

    if (state.aWaitSecond) {
      const uint32_t gap = t - state.aLastReleaseMs;
      if (gap >= A_DOUBLE_MIN_GAP_MS && gap <= A_DOUBLE_MAX_GAP_MS) {
        onADouble();
        state.aWaitSecond = false;
        state.aSingleDeadlineMs = 0;
      } else {
        state.aLastReleaseMs = t;
        state.aSingleDeadlineMs = t + A_DOUBLE_MAX_GAP_MS;
      }
    } else {
      state.aWaitSecond = true;
      state.aLastReleaseMs = t;
      state.aSingleDeadlineMs = t + A_DOUBLE_MAX_GAP_MS;
    }
  }

  if (state.aWaitSecond && state.aSingleDeadlineMs > 0 && t >= state.aSingleDeadlineMs) {
    onAShortSingle();
    state.aWaitSecond = false;
    state.aSingleDeadlineMs = 0;
  }
}

void handleB() {
  const uint32_t t = nowMs();
  const bool down = M5.BtnB.isPressed();

  if (down && !state.bPressed) {
    state.bPressed = true;
    state.bPressMs = t;
    state.bLongFired = false;
  }

  if (down && !state.bLongFired && (t - state.bPressMs >= B_LONG_PRESS_MS)) {
    onBLong();
    state.bLongFired = true;
    state.bWaitSecond = false;
    state.bSingleDeadlineMs = 0;
  }

  if (!down && state.bPressed) {
    state.bPressed = false;
    if (state.bLongFired) {
      state.bLongFired = false;
      return;
    }

    if (state.bWaitSecond) {
      const uint32_t gap = t - state.bLastReleaseMs;
      if (gap >= B_DOUBLE_MIN_GAP_MS && gap <= B_DOUBLE_MAX_GAP_MS) {
        onBDouble();
        state.bWaitSecond = false;
        state.bSingleDeadlineMs = 0;
      } else {
        state.bLastReleaseMs = t;
        state.bSingleDeadlineMs = t + B_DOUBLE_MAX_GAP_MS;
      }
    } else {
      state.bWaitSecond = true;
      state.bLastReleaseMs = t;
      state.bSingleDeadlineMs = t + B_DOUBLE_MAX_GAP_MS;
    }
  }

  if (state.bWaitSecond && state.bSingleDeadlineMs > 0 && t >= state.bSingleDeadlineMs) {
    onBShortSingle();
    state.bWaitSecond = false;
    state.bSingleDeadlineMs = 0;
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  M5.begin(cfg);

  M5.Display.setRotation(DISPLAY_ROTATION);
  M5.Display.setTextFont(2);
  M5.Display.fillScreen(COLOR_BG);

  bleKeyboard.begin();
  state.sessionStartMs = nowMs();
  state.bleConnected = bleKeyboard.isConnected();
  resetSwingCalibration();
  state.dirty = true;
  renderStatus();
  renderDynamic();
}

void loop() {
  M5.update();
  const bool bleNow = bleKeyboard.isConnected();
  if (bleNow != state.bleConnected) {
    state.bleConnected = bleNow;
    state.dirty = true;
  }

  handleA();
  handleB();
  handleSlideSwings();

  static uint32_t lastUi = 0;
  const uint32_t t = nowMs();
  if (state.dirty) {
    renderStatus();
    renderDynamic();
  }
  if (t - lastUi > DYNAMIC_REFRESH_MS) {
    renderDynamic();
    lastUi = t;
  }

  delay(5);
}

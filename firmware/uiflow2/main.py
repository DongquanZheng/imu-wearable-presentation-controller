import json
import time
import M5
from M5 import *


DISPLAY_ROTATION = 3
DISPLAY_FONT = Widgets.FONTS.EFontCN24
DISPLAY_COLOR = 0xFFA500
DISPLAY_BG = 0x000000
DISPLAY_PADDING = 8
DISPLAY_MIN_SIZE = 1
DISPLAY_MAX_SIZE = 2

TYPE_CMD = "cmd"
TYPE_IMU = "imu"
CHANNEL_SLIDES = "slides"
CHANNEL_MOTION = "motion"

MODE_IDLE = 0
MODE_SLIDES = 1
MODE_POINTER = 2

CMD_SLIDES_NEXT = "slides_next"
CMD_SLIDES_PREV = "slides_prev"
CMD_WINDOW_NEXT = "window_next"
CMD_WINDOW_PREV = "window_prev"
CMD_ZOOM_ON_STEP = "zoom_on_step"
CMD_ZOOM_OFF_STEP = "zoom_off_step"
CMD_ZOOM_CLOSE = "zoom_close"
CMD_MODE_IDLE = "mode_idle"
CMD_MODE_SLIDES = "mode_slides"
CMD_MODE_POINTER = "mode_pointer"

EMIT_COOLDOWN_MS = 120
A_LONG_PRESS_MS = 550
B_LONG_PRESS_MS = 320
A_DOUBLE_MIN_GAP_MS = 80
A_DOUBLE_MAX_GAP_MS = 420
B_DOUBLE_MIN_GAP_MS = 80
B_DOUBLE_MAX_GAP_MS = 420
DOUBLE_COOLDOWN_MS = 900

SAMPLE_HZ = 50
SAMPLE_INTERVAL_MS = 1000 // SAMPLE_HZ
ENABLE_IMU_STREAM = False
IMU_PRINT_DIV = 5

SWING_WX = 0.7
SWING_WY = 0.5
SWING_TRIGGER_T = 62.0
SWING_MARGIN_M = 10.0
SWING_REARM_T = 18.0
SWING_COOLDOWN_MS = 700
SWING_IGNORE_AFTER_TRIGGER_MS = 220
SWING_CALIB_SAMPLES = 12

MODE_TEXT = {
    MODE_IDLE: "IDLE",
    MODE_SLIDES: "SLIDES",
    MODE_POINTER: "POINTER"
}

state = {
    "mode": MODE_IDLE,
    "last_emit_ms": 0,
    "last_toggle_ms": 0,
    "dirty": True,
    "last_sample_ms": 0,
    "sample_count": 0,
    "a_press_ms": 0,
    "a_long_fired": False,
    "a_wait_second": False,
    "a_last_release_ms": 0,
    "a_single_deadline_ms": -1,
    "b_press_ms": 0,
    "b_long_fired": False,
    "b_wait_second": False,
    "b_last_release_ms": 0,
    "b_single_deadline_ms": -1,
    "swing_bias_ready": False,
    "swing_calib_remain": 0,
    "swing_sum_gx": 0.0,
    "swing_sum_gy": 0.0,
    "swing_bias_gx": 0.0,
    "swing_bias_gy": 0.0,
    "swing_armed": True,
    "last_swing_ms": 0,
    "last_swing_trigger_ms": 0
}


def now_ms():
    return time.ticks_ms()


def wrap_lines(text, max_width):
    lines = []
    for part in text.split("\n"):
        if part == "":
            lines.append("")
            continue
        line = ""
        for ch in part:
            test = line + ch
            if M5.Lcd.textWidth(test) <= max_width:
                line = test
            else:
                lines.append(line)
                line = ch
        lines.append(line)
    return lines


def measure_layout(text, size):
    M5.Lcd.setTextSize(size)
    sw = M5.Lcd.width()
    sh = M5.Lcd.height()
    max_w = sw - DISPLAY_PADDING * 2
    max_h = sh - DISPLAY_PADDING * 2
    lines = wrap_lines(text, max_w)
    line_h = M5.Lcd.fontHeight()
    total_h = line_h * len(lines)

    widest = 0
    for line in lines:
        w = M5.Lcd.textWidth(line)
        if w > widest:
            widest = w

    fits = widest <= max_w and total_h <= max_h
    return fits, lines, line_h, total_h


def pick_text_size(text):
    for size in range(DISPLAY_MAX_SIZE, DISPLAY_MIN_SIZE - 1, -1):
        fits, lines, line_h, total_h = measure_layout(text, size)
        if fits:
            return size, lines, line_h, total_h
    _, lines, line_h, total_h = measure_layout(text, DISPLAY_MIN_SIZE)
    return DISPLAY_MIN_SIZE, lines, line_h, total_h


def render_center(text):
    M5.Lcd.setFont(DISPLAY_FONT)
    size, lines, line_h, total_h = pick_text_size(text)
    M5.Lcd.setTextSize(size)

    sw = M5.Lcd.width()
    sh = M5.Lcd.height()
    start_y = (sh - total_h) // 2
    if start_y < DISPLAY_PADDING:
        start_y = DISPLAY_PADDING

    Widgets.fillScreen(DISPLAY_BG)
    for i, line in enumerate(lines):
        x = (sw - M5.Lcd.textWidth(line)) // 2
        y = start_y + i * line_h
        M5.Lcd.setCursor(x, y)
        M5.Lcd.print(line, DISPLAY_COLOR)


def mode_hint():
    if state["mode"] == MODE_POINTER:
        return "A/B hold: zoom"
    if state["mode"] == MODE_SLIDES:
        return "swing: turn page"
    return "A/B click: flip"


def render_status():
    if not state["dirty"]:
        return
    state["dirty"] = False
    title = MODE_TEXT[state["mode"]]
    render_center(title + "\n" + mode_hint())


def can_emit():
    return time.ticks_diff(now_ms(), state["last_emit_ms"]) >= EMIT_COOLDOWN_MS


def emit_cmd(cmd):
    packet = {
        "type": TYPE_CMD,
        "channel": CHANNEL_SLIDES,
        "mode": MODE_TEXT[state["mode"]],
        "cmd": cmd,
        "ts": now_ms()
    }
    print(json.dumps(packet, separators=(",", ":")))
    state["last_emit_ms"] = packet["ts"]


def reset_swing_calibration():
    state["swing_bias_ready"] = False
    state["swing_calib_remain"] = SWING_CALIB_SAMPLES
    state["swing_sum_gx"] = 0.0
    state["swing_sum_gy"] = 0.0
    state["swing_bias_gx"] = 0.0
    state["swing_bias_gy"] = 0.0
    state["swing_armed"] = True
    state["last_swing_ms"] = 0
    state["last_swing_trigger_ms"] = 0


def set_mode(mode):
    if state["mode"] == mode:
        return
    if mode == MODE_IDLE and state["mode"] == MODE_POINTER and can_emit():
        emit_cmd(CMD_ZOOM_CLOSE)
    state["mode"] = mode
    reset_swing_calibration()
    state["dirty"] = True


def set_mode_idle():
    set_mode(MODE_IDLE)
    if can_emit():
        emit_cmd(CMD_MODE_IDLE)


def set_mode_slides():
    set_mode(MODE_SLIDES)
    if can_emit():
        emit_cmd(CMD_MODE_SLIDES)


def set_mode_pointer():
    set_mode(MODE_POINTER)
    if can_emit():
        emit_cmd(CMD_MODE_POINTER)


def on_a_single():
    if state["mode"] == MODE_IDLE or state["mode"] == MODE_SLIDES:
        if can_emit():
            emit_cmd(CMD_SLIDES_PREV)


def on_b_single():
    if state["mode"] == MODE_IDLE or state["mode"] == MODE_SLIDES:
        if can_emit():
            emit_cmd(CMD_SLIDES_NEXT)


def on_a_double():
    if time.ticks_diff(now_ms(), state["last_toggle_ms"]) < DOUBLE_COOLDOWN_MS:
        return
    set_mode_pointer()
    state["last_toggle_ms"] = now_ms()


def on_b_double():
    if time.ticks_diff(now_ms(), state["last_toggle_ms"]) < DOUBLE_COOLDOWN_MS:
        return
    if state["mode"] == MODE_IDLE:
        set_mode_slides()
    else:
        set_mode_idle()
    state["last_toggle_ms"] = now_ms()


def on_a_long():
    if state["mode"] == MODE_SLIDES and can_emit():
        emit_cmd(CMD_WINDOW_NEXT)
    elif state["mode"] == MODE_POINTER and can_emit():
        emit_cmd(CMD_ZOOM_ON_STEP)


def on_b_long():
    if state["mode"] == MODE_SLIDES and can_emit():
        emit_cmd(CMD_WINDOW_PREV)
    elif state["mode"] == MODE_POINTER and can_emit():
        emit_cmd(CMD_ZOOM_OFF_STEP)


def handle_a():
    t = now_ms()

    if M5.BtnA.wasPressed():
        state["a_press_ms"] = t
        state["a_long_fired"] = False

    if M5.BtnA.isPressed() and (not state["a_long_fired"]):
        if time.ticks_diff(t, state["a_press_ms"]) >= A_LONG_PRESS_MS:
            on_a_long()
            state["a_long_fired"] = True
            state["a_wait_second"] = False
            state["a_single_deadline_ms"] = -1

    if M5.BtnA.wasReleased():
        if state["a_long_fired"]:
            state["a_long_fired"] = False
            return

        if state["a_wait_second"]:
            gap = time.ticks_diff(t, state["a_last_release_ms"])
            if A_DOUBLE_MIN_GAP_MS <= gap <= A_DOUBLE_MAX_GAP_MS:
                on_a_double()
                state["a_wait_second"] = False
                state["a_single_deadline_ms"] = -1
            else:
                state["a_last_release_ms"] = t
                state["a_single_deadline_ms"] = time.ticks_add(t, A_DOUBLE_MAX_GAP_MS)
        else:
            state["a_wait_second"] = True
            state["a_last_release_ms"] = t
            state["a_single_deadline_ms"] = time.ticks_add(t, A_DOUBLE_MAX_GAP_MS)

    if state["a_wait_second"] and state["a_single_deadline_ms"] >= 0:
        if time.ticks_diff(t, state["a_single_deadline_ms"]) >= 0:
            on_a_single()
            state["a_wait_second"] = False
            state["a_single_deadline_ms"] = -1


def handle_b():
    t = now_ms()

    if M5.BtnB.wasPressed():
        state["b_press_ms"] = t
        state["b_long_fired"] = False

    if M5.BtnB.isPressed() and (not state["b_long_fired"]):
        if time.ticks_diff(t, state["b_press_ms"]) >= B_LONG_PRESS_MS:
            on_b_long()
            state["b_long_fired"] = True
            state["b_wait_second"] = False
            state["b_single_deadline_ms"] = -1

    if M5.BtnB.wasReleased():
        if state["b_long_fired"]:
            state["b_long_fired"] = False
            return

        if state["b_wait_second"]:
            gap = time.ticks_diff(t, state["b_last_release_ms"])
            if B_DOUBLE_MIN_GAP_MS <= gap <= B_DOUBLE_MAX_GAP_MS:
                on_b_double()
                state["b_wait_second"] = False
                state["b_single_deadline_ms"] = -1
            else:
                state["b_last_release_ms"] = t
                state["b_single_deadline_ms"] = time.ticks_add(t, B_DOUBLE_MAX_GAP_MS)
        else:
            state["b_wait_second"] = True
            state["b_last_release_ms"] = t
            state["b_single_deadline_ms"] = time.ticks_add(t, B_DOUBLE_MAX_GAP_MS)

    if state["b_wait_second"] and state["b_single_deadline_ms"] >= 0:
        if time.ticks_diff(t, state["b_single_deadline_ms"]) >= 0:
            on_b_single()
            state["b_wait_second"] = False
            state["b_single_deadline_ms"] = -1


def handle_slide_swings():
    if state["mode"] != MODE_SLIDES:
        return

    t = now_ms()
    if state["last_swing_trigger_ms"] > 0:
        if time.ticks_diff(t, state["last_swing_trigger_ms"]) < SWING_IGNORE_AFTER_TRIGGER_MS:
            return
    if state["last_swing_ms"] > 0:
        if time.ticks_diff(t, state["last_swing_ms"]) < SWING_COOLDOWN_MS:
            return

    gx, gy, gz = Imu.getGyro()

    if not state["swing_bias_ready"]:
        state["swing_sum_gx"] += gx
        state["swing_sum_gy"] += gy
        state["swing_calib_remain"] -= 1
        if state["swing_calib_remain"] <= 0:
            state["swing_bias_gx"] = state["swing_sum_gx"] / SWING_CALIB_SAMPLES
            state["swing_bias_gy"] = state["swing_sum_gy"] / SWING_CALIB_SAMPLES
            state["swing_bias_ready"] = True
            state["swing_calib_remain"] = 0
        return

    gx -= state["swing_bias_gx"]
    gy -= state["swing_bias_gy"]
    sx = -gy
    sy = -gx

    next_score = SWING_WX * max(0.0, sx) + SWING_WY * max(0.0, sy)
    prev_score = SWING_WX * max(0.0, -sx) + SWING_WY * max(0.0, -sy)
    best = next_score if next_score > prev_score else prev_score
    diff = abs(next_score - prev_score)

    if not state["swing_armed"]:
        if best < SWING_REARM_T:
            state["swing_armed"] = True
        return

    if best < SWING_TRIGGER_T:
        return
    if diff < SWING_MARGIN_M:
        return
    if not can_emit():
        return

    if next_score > prev_score:
        emit_cmd(CMD_SLIDES_NEXT)
    else:
        emit_cmd(CMD_SLIDES_PREV)

    state["swing_armed"] = False
    state["last_swing_ms"] = t
    state["last_swing_trigger_ms"] = t


def emit_imu():
    ax, ay, az = Imu.getAccel()
    gx, gy, gz = Imu.getGyro()
    packet = {
        "type": TYPE_IMU,
        "channel": CHANNEL_MOTION,
        "a": [ax, ay, az],
        "g": [gx, gy, gz],
        "btn": M5.BtnA.isPressed() or M5.BtnB.isPressed(),
        "mode": MODE_TEXT[state["mode"]],
        "ts": now_ms()
    }
    print(json.dumps(packet, separators=(",", ":")))


def handle_imu_stream():
    if not ENABLE_IMU_STREAM:
        return
    t = now_ms()
    if time.ticks_diff(t, state["last_sample_ms"]) < SAMPLE_INTERVAL_MS:
        return
    state["last_sample_ms"] = t
    state["sample_count"] += 1
    if state["sample_count"] % IMU_PRINT_DIV == 0:
        emit_imu()


def setup():
    M5.begin()
    Widgets.setRotation(DISPLAY_ROTATION)
    M5.Lcd.setFont(DISPLAY_FONT)
    Widgets.fillScreen(DISPLAY_BG)
    state["last_sample_ms"] = now_ms()
    reset_swing_calibration()
    state["dirty"] = True
    render_status()


def loop():
    M5.update()
    handle_a()
    handle_b()
    handle_slide_swings()
    handle_imu_stream()
    render_status()


if __name__ == "__main__":
    setup()
    while True:
        loop()

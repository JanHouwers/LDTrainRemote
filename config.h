#pragma once

// ── XIAO ESP32S3 Pin Mapping ───────────────────────────
#define POT_PIN         2       // D1 / GPIO2 - Potentiometer (speed)
#define BTN_WATER_PIN   3       // D2 / GPIO3 - Water/action cycle
#define BTN_HORN_PIN    4       // D3 / GPIO4 - Horn sound
#define BTN_STOP_PIN    5       // D4 / GPIO5 - Stop (brake)
#define BTN_LED_PIN     6       // D5 / GPIO6 - LED color cycle

// ── Potentiometer ──────────────────────────────────────
#define POT_DEADBAND    307     // +/- around midpoint (2048) ~15% of 4096
#define POT_MID         2048
#define POT_MIN         0
#define POT_MAX         4095

// ── Button Timing (ms) ────────────────────────────────
#define DEBOUNCE_MS     50

// ── Train Speed ────────────────────────────────────────
#define SPEED_MAX       100

// ── Loop ───────────────────────────────────────────────
#define LOOP_DELAY_MS   50

#pragma once

// ── XIAO ESP32S3 Pin Mapping ───────────────────────────
#define POT_PIN         2       // D1 / GPIO2 - Potentiometer (speed)
#define BTN_WATER_PIN   3       // D2 / GPIO3 - Water/action cycle
#define BTN_HORN_PIN    4       // D3 / GPIO4 - Horn sound
#define BTN_STOP_PIN    5       // D4 / GPIO5 - Stop (brake)
#define BTN_LED_PIN     6       // D5 / GPIO6 - LED color cycle

// ── Potentiometer ──────────────────────────────────────
#define POT_DEADBAND    150     // +/- around midpoint (2048) ~7% of 4096
#define POT_MID         2048
#define POT_MIN         350     // actual pot minimum (tune to hardware)
#define POT_MAX         3750    // actual pot maximum (tune to hardware)

// ── Button Timing (ms) ────────────────────────────────
#define DEBOUNCE_MS     50

// ── Train Speed ────────────────────────────────────────
#define SPEED_MAX       100

// ── Auto Power-Off (deep sleep) ───────────────────────
#define SLEEP_TIMEOUT_MS  300000  // 5 minutes of inactivity
#define POT_ACTIVITY_THRESHOLD 50 // ADC change to count as activity

// ── Loop ───────────────────────────────────────────────
#define LOOP_DELAY_MS   50

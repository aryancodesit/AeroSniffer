#pragma once
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

// ── Calibration Telemetry Macros ──────────────────────────────────
//
//  CALIB(tag, value)           —  integer telemetry: "C,<tag>,<int>\n"
//  CALIB_STR(tag, str)         —  string telemetry:  "C,<tag>,<str>\n"
//  CALIB_RATE(tag,val,ms)      —  rate-limited CALIB  (max 1/ms Hz)
//  CALIB_RATE_STR(tag,str,ms)  —  rate-limited CALIB_STR (max 1/ms Hz)
//
//  Arguments must be side-effect free.  CALIB_RATE / CALIB_RATE_STR
//  use __LINE__ for their rate-limiter static; do not invoke two on
//  the same source line.
//
//  Actual loop-timing impact of Serial.printf must be measured during
//  Phase 0 — overhead depends on UART baud rate, buffer state, and
//  host serial terminal.
//
//  Zero flash/RAM cost when CALIBRATION is undefined.

#ifdef CALIBRATION

// ── Telemetry format strings ──────────────────────────────────────

#define CALIB_INT_FMT  "C,%s,%d\n"
#define CALIB_STR_FMT  "C,%s,%s\n"

// ── Two-stage token glue (ensures __LINE__ expands before ##) ─────

#define CALIB_GLUE2_(a, b) a ## b
#define CALIB_GLUE(a, b)   CALIB_GLUE2_(a, b)

// ── Integer telemetry ─────────────────────────────────────────────

#define CALIB(tag, value) \
    do { \
        Serial.printf(CALIB_INT_FMT, tag, (int)(value)); \
    } while (0)

// ── String telemetry (e.g. boot identity) ─────────────────────────

#define CALIB_STR(tag, str) \
    do { \
        Serial.printf(CALIB_STR_FMT, tag, str); \
    } while (0)

// ── Rate-limited integer telemetry ────────────────────────────────

#define CALIB_RATE(tag, value, interval_ms) \
    do { \
        static uint32_t CALIB_GLUE(_calib_last_, __LINE__) = 0; \
        uint32_t _calib_now = millis(); \
        if (_calib_now - CALIB_GLUE(_calib_last_, __LINE__) >= (interval_ms)) { \
            CALIB_GLUE(_calib_last_, __LINE__) = _calib_now; \
            Serial.printf(CALIB_INT_FMT, tag, (int)(value)); \
        } \
    } while (0)

// ── Rate-limited string telemetry ─────────────────────────────────

#define CALIB_RATE_STR(tag, str, interval_ms) \
    do { \
        static uint32_t CALIB_GLUE(_calib_last_, __LINE__) = 0; \
        uint32_t _calib_now = millis(); \
        if (_calib_now - CALIB_GLUE(_calib_last_, __LINE__) >= (interval_ms)) { \
            CALIB_GLUE(_calib_last_, __LINE__) = _calib_now; \
            Serial.printf(CALIB_STR_FMT, tag, str); \
        } \
    } while (0)

#else

#define CALIB(tag, value)                   do {} while (0)
#define CALIB_STR(tag, str)                 do {} while (0)
#define CALIB_RATE(tag, value, interval_ms) do {} while (0)
#define CALIB_RATE_STR(tag, str, interval_ms) do {} while (0)

#endif

#endif

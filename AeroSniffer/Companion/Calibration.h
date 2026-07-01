#pragma once
#ifndef CALIBRATION_H
#define CALIBRATION_H

// ── Calibration campaign active ──────────────────────────────────
// Defined here (not in .ino) so it propagates to all .cpp
// translation units.  Uncomment for future calibration campaigns.
// #define CALIBRATION

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

// ── Transport guard threshold ─────────────────────────────────────
// kCalibLineMax covers worst-case stamp line length:
//   CALIB_INT:  C,<tag:16>,<int:-2147483648:11>\n  = 31 B
//   CALIB_STR:  C,<tag:16>,<str:64>\n                = 84 B
// 96 B chosen to allow headroom and future tag/value expansion.
static constexpr size_t kCalibLineMax = 96;

// ── Drop counter (shared across all macros) ───────────────────────
// Returns reference to a single static counter so every call site
// increments the same variable.  Reports are piggybacked onto the
// next successful stamp emission, requiring zero caller changes.

inline uint32_t& _calib_drop_count() {
    static uint32_t _c = 0;
    return _c;
}

inline void _calib_report_drops() {
    uint32_t n = _calib_drop_count();
    if (n > 0) {
        _calib_drop_count() = 0;
        Serial.printf(CALIB_INT_FMT, "calib_drop", (int)n);
    }
}

// ── Integer telemetry ─────────────────────────────────────────────
// Drops stamp when TX buffer cannot accommodate the longest possible
// line.  Dropped stamps are counted and auto-reported on next success.

#define CALIB(tag, value) \
    do { \
        if (Serial.availableForWrite() >= (int)kCalibLineMax) { \
            _calib_report_drops(); \
            Serial.printf(CALIB_INT_FMT, tag, (int)(value)); \
        } else { \
            _calib_drop_count()++; \
        } \
    } while (0)

// ── String telemetry (e.g. boot identity) ─────────────────────────

#define CALIB_STR(tag, str) \
    do { \
        if (Serial.availableForWrite() >= (int)kCalibLineMax) { \
            _calib_report_drops(); \
            Serial.printf(CALIB_STR_FMT, tag, str); \
        } else { \
            _calib_drop_count()++; \
        } \
    } while (0)

// ── Rate-limited integer telemetry ────────────────────────────────
// Timer is updated only on successful print (not on drop), so the
// first available slot after a stall immediately emits a sample
// rather than waiting another full interval.

#define CALIB_RATE(tag, value, interval_ms) \
    do { \
        static uint32_t CALIB_GLUE(_calib_last_, __LINE__) = 0; \
        uint32_t _calib_now = millis(); \
        if (_calib_now - CALIB_GLUE(_calib_last_, __LINE__) >= (interval_ms)) { \
            if (Serial.availableForWrite() >= (int)kCalibLineMax) { \
                CALIB_GLUE(_calib_last_, __LINE__) = _calib_now; \
                _calib_report_drops(); \
                Serial.printf(CALIB_INT_FMT, tag, (int)(value)); \
            } else { \
                _calib_drop_count()++; \
            } \
        } \
    } while (0)

// ── Rate-limited string telemetry ─────────────────────────────────

#define CALIB_RATE_STR(tag, str, interval_ms) \
    do { \
        static uint32_t CALIB_GLUE(_calib_last_, __LINE__) = 0; \
        uint32_t _calib_now = millis(); \
        if (_calib_now - CALIB_GLUE(_calib_last_, __LINE__) >= (interval_ms)) { \
            if (Serial.availableForWrite() >= (int)kCalibLineMax) { \
                CALIB_GLUE(_calib_last_, __LINE__) = _calib_now; \
                _calib_report_drops(); \
                Serial.printf(CALIB_STR_FMT, tag, str); \
            } else { \
                _calib_drop_count()++; \
            } \
        } \
    } while (0)

#else

#define CALIB(tag, value)                   do {} while (0)
#define CALIB_STR(tag, str)                 do {} while (0)
#define CALIB_RATE(tag, value, interval_ms) do {} while (0)
#define CALIB_RATE_STR(tag, str, interval_ms) do {} while (0)

#endif

#endif

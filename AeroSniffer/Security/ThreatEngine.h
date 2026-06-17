// ================================================================
//  Security/ThreatEngine.h  —  Threat Intelligence Ring Buffer
//  AeroSniffer Security Layer | Fixed-size, no dynamic allocation
//  Dual-core safe via FreeRTOS mutex.
// ================================================================
#pragma once

#include <Arduino.h>
#include <FreeRTOS.h>
#include <semphr.h>

// ── Severity levels ──────────────────────────────────────────────
enum ThreatSeverity {
    THREAT_INFO     = 0,
    THREAT_WARNING  = 1,
    THREAT_CRITICAL = 2
};

// ── Alert types ──────────────────────────────────────────────────
enum ThreatType {
    THREAT_UNKNOWN_DEVICE   = 0,
    THREAT_RANDOMIZED_MAC   = 1,
    THREAT_PROBE_LEAK       = 2,
    THREAT_EVIL_TWIN        = 3,
    THREAT_DEAUTH           = 4,
    THREAT_CHANNEL_FLOOD    = 5,
    THREAT_NEW_AP           = 6,
    THREAT_TRUSTED_RETURN   = 7,
    THREAT_TYPE_COUNT
};

// String labels for JSON serialization (must match SPA filter values)
static const char* const THREAT_TYPE_LABELS[THREAT_TYPE_COUNT] = {
    "intruder",      // UNKNOWN_DEVICE
    "intruder",      // RANDOMIZED_MAC
    "intruder",      // PROBE_LEAK
    "eviltwin",      // EVIL_TWIN
    "deauth",        // DEAUTH
    "deauth",        // CHANNEL_FLOOD
    "intruder",      // NEW_AP
    "intruder"       // TRUSTED_RETURN
};

#define THREAT_RING_SIZE    48
#define THREAT_DESC_LEN     48

#define DEDUP_SLOTS         32
#define DEDUP_WINDOW_MS     60000

struct ThreatAlert {
    uint32_t    timestamp;   // seconds since boot
    uint8_t     type;        // ThreatType
    uint8_t     severity;    // ThreatSeverity
    char        src_mac[18]; // "AA:BB:CC:DD:EE:FF"
    char        desc[THREAT_DESC_LEN];
};

// ── Ring buffer state ────────────────────────────────────────────
static ThreatAlert       _threat_ring[THREAT_RING_SIZE];
static int               _threat_head = 0;
static int               _threat_tail = 0;
static int               _threat_len  = 0;
static SemaphoreHandle_t _threat_mutex = NULL;

// ── Dedup table ──────────────────────────────────────────────────
struct DedupEntry {
    uint8_t  type;
    uint32_t last_ms;
    char     source[18];
    bool     used;
};
static DedupEntry _dedup_table[DEDUP_SLOTS];

static void _threat_mutex_init() {
    if (_threat_mutex == NULL) {
        _threat_mutex = xSemaphoreCreateMutex();
    }
}

// ── Internal: write to ring (caller must hold mutex) ────────────
static void _threat_write(uint8_t type, uint8_t severity,
                          const char* mac, const char* desc) {
    int idx = _threat_head;
    ThreatAlert& a = _threat_ring[idx];
    a.timestamp = millis() / 1000;
    a.type   = type;
    a.severity = severity;
    if (mac) {
        strncpy(a.src_mac, mac, sizeof(a.src_mac) - 1);
        a.src_mac[sizeof(a.src_mac) - 1] = '\0';
    } else {
        a.src_mac[0] = '\0';
    }
    if (desc) {
        strncpy(a.desc, desc, THREAT_DESC_LEN - 1);
        a.desc[THREAT_DESC_LEN - 1] = '\0';
    } else {
        a.desc[0] = '\0';
    }
    _threat_head = (_threat_head + 1) % THREAT_RING_SIZE;
    if (_threat_len < THREAT_RING_SIZE) {
        _threat_len++;
    } else {
        _threat_tail = (_threat_tail + 1) % THREAT_RING_SIZE;
    }
}

// ── Internal: compute dedup source key ──────────────────────────
//  When mac is non-empty, use it as the key.
//  When mac is empty, use the type number packed as a string.
static void _dedup_key(char* out, int out_len, uint8_t type, const char* mac) {
    if (mac && mac[0] != '\0') {
        strncpy(out, mac, out_len - 1);
        out[out_len - 1] = '\0';
    } else {
        snprintf(out, out_len, "%u", (unsigned int)type);
    }
}

// ── Check dedup: returns true if this (type,source) may fire ────
static bool _dedup_check(uint8_t type, const char* source) {
    uint32_t now = millis();
    int oldest_slot = -1;
    uint32_t oldest_ms = now;
    for (int i = 0; i < DEDUP_SLOTS; i++) {
        DedupEntry& e = _dedup_table[i];
        if (!e.used) { oldest_slot = i; break; }
        if (e.type == type && strcmp(e.source, source) == 0) {
            if (now - e.last_ms < DEDUP_WINDOW_MS) return false;
            e.last_ms = now;
            return true;
        }
        if (e.last_ms < oldest_ms) {
            oldest_ms = e.last_ms;
            oldest_slot = i;
        }
    }
    if (oldest_slot >= 0) {
        DedupEntry& e = _dedup_table[oldest_slot];
        e.used = true;
        e.type = type;
        e.last_ms = now;
        strncpy(e.source, source, sizeof(e.source) - 1);
        e.source[sizeof(e.source) - 1] = '\0';
        return true;
    }
    return false;
}

// ── API: raw insert (no dedup) ──────────────────────────────────
static void threat_add(uint8_t type, uint8_t severity,
                       const char* mac, const char* desc) {
    _threat_mutex_init();
    xSemaphoreTake(_threat_mutex, portMAX_DELAY);
    _threat_write(type, severity, mac, desc);
    xSemaphoreGive(_threat_mutex);
}

// ── API: rate-limited insert (60s window per type+source) ───────
//  Returns true if the alert was added, false if dedup suppressed it.
static bool threat_add_dedup(uint8_t type, uint8_t severity,
                             const char* mac, const char* desc) {
    _threat_mutex_init();
    xSemaphoreTake(_threat_mutex, portMAX_DELAY);

    char key[18];
    _dedup_key(key, sizeof(key), type, mac);

    bool allowed = _dedup_check(type, key);
    if (allowed) {
        _threat_write(type, severity, mac, desc);
    }

    xSemaphoreGive(_threat_mutex);
    return allowed;
}

static int threat_count() {
    _threat_mutex_init();
    xSemaphoreTake(_threat_mutex, portMAX_DELAY);
    int count = _threat_len;
    xSemaphoreGive(_threat_mutex);
    return count;
}

static const ThreatAlert* threat_get(int index) {
    _threat_mutex_init();
    xSemaphoreTake(_threat_mutex, portMAX_DELAY);

    const ThreatAlert* result = nullptr;
    if (index >= 0 && index < _threat_len) {
        int idx = (_threat_tail + index) % THREAT_RING_SIZE;
        result = &_threat_ring[idx];
    }

    xSemaphoreGive(_threat_mutex);
    return result;
}

static void threat_clear() {
    _threat_mutex_init();
    xSemaphoreTake(_threat_mutex, portMAX_DELAY);

    _threat_head = 0;
    _threat_tail = 0;
    _threat_len  = 0;
    for (int i = 0; i < DEDUP_SLOTS; i++) {
        _dedup_table[i].used = false;
    }

    xSemaphoreGive(_threat_mutex);
}

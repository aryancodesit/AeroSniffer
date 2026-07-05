// ================================================================
//  Security/Statistics.h  —  Packet & AP Tracking State
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

// ── Packet capture state ────────────────────────────────────────
// Volatile module-level state is used because the WiFi sniffer ISR and the
// main loop share these counters. A dedicated SnifferState class would be
// warranted if multi-instance or testability becomes a requirement.
namespace SnifferState {
inline volatile uint32_t pkt_total     = 0;
inline volatile uint32_t pkt_deauth    = 0;
inline volatile uint32_t pkt_beacon    = 0;
inline volatile uint32_t pkt_probe     = 0;
inline volatile uint32_t pkt_per_sec   = 0;
inline volatile uint8_t  current_ch    = 1;
inline volatile uint32_t device_count  = 0;
inline volatile bool     sec_scanning  = false;
inline volatile bool     ch_hopping    = true;

// Rolling PPS counter
inline uint32_t _pps_last_ms    = 0;
inline uint32_t _pps_count      = 0;
}

// ── AP tracking table ───────────────────────────────────────────
struct APRecord {
  char     ssid[33];
  uint8_t  bssid[6];
  int8_t   rssi;
  uint8_t  channel;
  uint8_t  encryption;   // 0=unknown 1=open 2=WPA 3=WPA2 4=WPA3
  uint32_t last_seen;     // seconds since boot
  bool     active;
};

namespace APTable {
inline APRecord ap_table[MAX_AP_TABLE];
inline int      ap_count = 0;
}

using namespace SnifferState;
using namespace APTable;

// ================================================================
//  Security/Statistics.h  —  Packet & AP Tracking State
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

// ── Packet capture state ────────────────────────────────────────
static volatile uint32_t pkt_total     = 0;
static volatile uint32_t pkt_deauth    = 0;
static volatile uint32_t pkt_beacon    = 0;
static volatile uint32_t pkt_probe     = 0;
static volatile uint32_t pkt_per_sec   = 0;
static volatile uint8_t  current_ch    = 1;
static volatile uint32_t device_count  = 0;
static volatile bool     sec_scanning  = false;
static volatile bool     ch_hopping    = true;   // Channel hopping toggle

// Rolling PPS counter
static uint32_t _pps_last_ms    = 0;
static uint32_t _pps_count      = 0;

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
static APRecord ap_table[MAX_AP_TABLE];
static int      ap_count = 0;

// ================================================================
//  Security/EvilTwin.h  —  Rogue AP / Evil Twin Detector
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"
#include "Statistics.h"
#include "HomeGuard.h"

// ── Evil Twin (Rogue AP) state ──────────────────────────────────
static char     evil_twin_target_ssid[33] = "";
static uint8_t  evil_twin_trusted_bssid[6] = {0};
static volatile bool evil_twin_detected = false;
static char     evil_twin_attacker_bssid[20] = "";

static void sec_track_ap(const uint8_t* payload, int16_t rssi, uint8_t ch) {
  // Extract SSID first for signal finder
  int temp_ssid_len = 0;
  char current_ssid[33] = {0};
  if (payload[36] == 0x00) { // SSID tag
    temp_ssid_len = std::min<int>((int)payload[37], 32);
    memcpy(current_ssid, &payload[38], temp_ssid_len);
  }
  current_ssid[temp_ssid_len] = '\0';

  if (find_mode_active && strcmp(current_ssid, find_ssid) == 0) {
    find_rssi = rssi;
    find_last_seen_ms = millis();
    if (ch_hopping) {
      ch_hopping = false;
      WiFiService.setChannel(ch);
      current_ch = ch;
    }
  }

  // Extract BSSID from beacon/probe response (addr3 = bytes 16-21)
  const uint8_t* bssid = &payload[16];

  // Extract encryption from capability info (bytes 34-35, privacy bit = bit 4)
  uint8_t enc = 1; // default: open
  if (payload[34] & 0x10) enc = 3; // WPA2 if privacy bit set

  uint32_t now_sec = millis() / 1000;

  // Check if already tracked
  for (int i = 0; i < ap_count; i++) {
    if (memcmp(ap_table[i].bssid, bssid, 6) == 0) {
      ap_table[i].rssi = rssi;
      ap_table[i].channel = ch;
      ap_table[i].encryption = enc;
      ap_table[i].last_seen = now_sec;
      return;
    }
  }

  // Add new AP if space
  if (ap_count >= MAX_AP_TABLE) return;
  APRecord& ap = ap_table[ap_count];
  memcpy(ap.bssid, bssid, 6);
  ap.rssi = rssi;
  ap.channel = ch;
  ap.encryption = enc;
  ap.last_seen = now_sec;
  ap.active = true;
  strcpy(ap.ssid, current_ssid);
  ap_count++;
  
  // Publish AP discovered event
  EventBus.publish(EVENT_NETWORK_DISCOVERED);

  // ── Evil Twin Detection ──────────────────────────────────────────
  if (strlen(evil_twin_target_ssid) > 0 && strcmp(ap.ssid, evil_twin_target_ssid) == 0) {
    if (memcmp(ap.bssid, evil_twin_trusted_bssid, 6) != 0) {
      evil_twin_detected = true;
      EventBus.publish(EVENT_ATTACK_EVILTWIN);
      snprintf(evil_twin_attacker_bssid, sizeof(evil_twin_attacker_bssid),
               "%02X:%02X:%02X:%02X:%02X:%02X",
               ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    }
  }
}

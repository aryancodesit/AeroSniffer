// ================================================================
//  Security/Sniffer.h  —  802.11 Sniffer & Transmitter
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "AeroSnifferOS.h"
#include "Statistics.h"
#include "HomeGuard.h"
#include "EvilTwin.h"

// ── Wi-Fi Probe Request Leakage state ───────────────────────────
#define MAX_PROBE_LEAKS 20
struct ProbeLeak {
  char client_mac[20];
  char ssid[33];
  uint32_t last_seen;
};
static ProbeLeak probe_leaks[MAX_PROBE_LEAKS];
static int probe_leak_count = 0;

// ── Active Beacon Spam / Rick Roll Attack State ──────────────────
enum AttackType {
  ATTACK_NONE,
  ATTACK_BEACON_SPAM,
  ATTACK_RICK_ROLL
};
static volatile AttackType active_attack_type = ATTACK_NONE;
static uint32_t last_attack_frame_ms = 0;
static int rick_roll_index = 0;

static const char* const RICK_ROLL_LYRICS[] = {
  "1. Never gonna give you up",
  "2. Never gonna let you down",
  "3. Never gonna run around",
  "4. and desert you",
  "5. Never gonna make you cry",
  "6. Never gonna say goodbye",
  "7. Never gonna tell a lie",
  "8. and hurt you"
};
#define RICK_ROLL_LYRICS_COUNT 8

static volatile bool _deauth_evt_pending = false;

static void IRAM_ATTR sec_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  pkt_total++;
  _pps_count++;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  if (pkt->rx_ctrl.sig_len < 24) return;

  uint8_t frame_type    = (pkt->payload[0] & 0x0C) >> 2;
  uint8_t frame_subtype = (pkt->payload[0] & 0xF0) >> 4;

  // ── Welcome Home Device Detector ──
  const uint8_t* src_mac = &pkt->payload[10];
  if (welcome_mac[0] != 0 && memcmp(src_mac, welcome_mac, 6) == 0) {
    uint32_t now = millis();
    if (now - welcome_last_seen_ms > 60000 || welcome_last_seen_ms == 0) {
      welcome_triggered = true;
      welcome_triggered_ms = now;
    }
    welcome_last_seen_ms = now;
  }

  // ── Automatic Device Enrollment & Tracking ──
  if (frame_type == 2) {
    const uint8_t* ta = &pkt->payload[10];  // Transmitter Address
    if (ta[0] != 0xFF && (ta[0] & 0x01) == 0 && ta[0] != 0x00) {
      enroll_device(ta);
    }
  }

  if (frame_type == 0) {  // Management frame
    switch (frame_subtype) {
      case 0x04: // Probe request
        {
          pkt_probe++;
          if (pkt->payload[24] == 0x00) {
            int len = std::min<int>((int)pkt->payload[25], 32);
            if (len > 0) {
              char leaked_ssid[33];
              memcpy(leaked_ssid, &pkt->payload[26], len);
              leaked_ssid[len] = '\0';

              char client_mac_str[20];
              snprintf(client_mac_str, sizeof(client_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                       pkt->payload[10], pkt->payload[11], pkt->payload[12],
                       pkt->payload[13], pkt->payload[14], pkt->payload[15]);

              // Store / update
              bool found = false;
              for (int i = 0; i < probe_leak_count; i++) {
                if (strcmp(probe_leaks[i].client_mac, client_mac_str) == 0 &&
                    strcmp(probe_leaks[i].ssid, leaked_ssid) == 0) {
                  probe_leaks[i].last_seen = millis();
                  found = true;
                  break;
                }
              }
              if (!found) {
                if (probe_leak_count < MAX_PROBE_LEAKS) {
                  ProbeLeak& pl = probe_leaks[probe_leak_count++];
                  strcpy(pl.client_mac, client_mac_str);
                  strcpy(pl.ssid, leaked_ssid);
                  pl.last_seen = millis();
                } else {
                  for (int i = 1; i < MAX_PROBE_LEAKS; i++) {
                    probe_leaks[i - 1] = probe_leaks[i];
                  }
                  ProbeLeak& pl = probe_leaks[MAX_PROBE_LEAKS - 1];
                  strcpy(pl.client_mac, client_mac_str);
                  strcpy(pl.ssid, leaked_ssid);
                  pl.last_seen = millis();
                }
              }
            }
          }
        }
        break;
      case 0x08:                        // Beacon
        pkt_beacon++;
        sec_track_ap(pkt->payload, pkt->rx_ctrl.rssi, current_ch);
        break;
      case 0x0C:                        // Deauthentication
        pkt_deauth++;
        if (pkt_deauth >= getDeauthThreshold() && !_deauth_evt_pending) {
          _deauth_evt_pending = true;
          EventBus.publish(EVENT_ATTACK_DEAUTH);
        }
        break;
    }
  }
}

static void sec_hop_channel() {
  if (!ch_hopping) return;
  current_ch = (current_ch % 13) + 1;
  WiFiService.setChannel(current_ch);
}

static void send_beacon_frame(const char* ssid, uint8_t channel) {
  uint8_t packet[128];
  int idx = 0;

  packet[idx++] = 0x80;
  packet[idx++] = 0x00;
  packet[idx++] = 0x00;
  packet[idx++] = 0x00;

  for (int i = 0; i < 6; i++) packet[idx++] = 0xFF;

  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(0, 256);
  mac[0] &= 0xFE;
  mac[0] |= 0x02;
  for (int i = 0; i < 6; i++) packet[idx++] = mac[i];
  for (int i = 0; i < 6; i++) packet[idx++] = mac[i];

  packet[idx++] = 0x00;
  packet[idx++] = 0x00;

  for (int i = 0; i < 8; i++) packet[idx++] = 0x00;

  packet[idx++] = 0x64;
  packet[idx++] = 0x00;
  packet[idx++] = 0x01;
  packet[idx++] = 0x04;

  packet[idx++] = 0x00;
  int ssid_len = strlen(ssid);
  if (ssid_len > 32) ssid_len = 32;
  packet[idx++] = (uint8_t)ssid_len;
  for (int i = 0; i < ssid_len; i++) packet[idx++] = ssid[i];

  packet[idx++] = 0x01;
  packet[idx++] = 0x08;
  packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8B; packet[idx++] = 0x96;
  packet[idx++] = 0x24; packet[idx++] = 0x30; packet[idx++] = 0x48; packet[idx++] = 0x6C;

  packet[idx++] = 0x03;
  packet[idx++] = 0x01;
  packet[idx++] = channel;

  // Enforce WiFiService ownership wrapper for raw frames
  WiFiService.transmitRaw80211(packet, idx);
}

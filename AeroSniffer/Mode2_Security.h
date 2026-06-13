// ================================================================
//  Mode2_Security.h  —  Web-UI Hybrid Security Monitor
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  AEROSHELL ARCHITECTURE:
//    The 1.3" 240×240 screen is too small for a full UI.
//    Instead we use a split architecture:
//      • DISPLAY: Animated radar sweep + live packet stats
//      • CONTROL: Web interface at http://192.168.4.1 (phone/laptop)
//
//  DEVKITC ARCHITECTURE:
//    Falls back to the original security integration wrapper.
//
//  NOTE (DevKitC): Security engine cannot be cleanly stopped. Switching out
//  of Mode 2 triggers ESP.restart().
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "Config.h"
#include <Preferences.h>

static TFT_eSPI* _stft = nullptr;

// ================================================================
//  DESKBUDDY 2.0 — WEB-UI HYBRID SECURITY MONITOR
// ================================================================
#ifdef HW_DESKBUDDY_2

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ── Evil Twin (Rogue AP) state ──────────────────────────────────
static char     evil_twin_target_ssid[33] = "";
static uint8_t  evil_twin_trusted_bssid[6] = {0};
static volatile bool evil_twin_detected = false;
static char     evil_twin_attacker_bssid[20] = "";

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

// ── Wi-Fi Probe Request Leakage state ───────────────────────────
#define MAX_PROBE_LEAKS 20
struct ProbeLeak {
  char client_mac[20];
  char ssid[33];
  uint32_t last_seen;
};
static ProbeLeak probe_leaks[MAX_PROBE_LEAKS];
static int probe_leak_count = 0;

// ── BLE Tracker Detector state ──────────────────────────────────
#define MAX_BLE_TRACKERS 15
struct BLETracker {
  char mac[20];
  char type[16]; // "AirTag/FindMy", "SmartTag", "Tile"
  int8_t rssi;
  uint32_t first_seen;
  uint32_t last_seen;
  int seen_count;
};
static BLETracker ble_trackers[MAX_BLE_TRACKERS];
static int ble_tracker_count = 0;

static BLEScan* pBLEScan = nullptr;
static volatile bool ble_scan_active = false;
static uint32_t last_ble_scan_ms = 0;

// Forward declaration of BLE scan complete callback
static void ble_scan_complete_cb(BLEScanResults results);

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String typeStr = "";
    bool is_tracker = false;

    // 1. Check AirTag / Find My (Apple Manufacturer Data)
    if (advertisedDevice.haveManufacturerData()) {
      String mData = advertisedDevice.getManufacturerData();
      if (mData.length() >= 2 && (uint8_t)mData[0] == 0x4C && (uint8_t)mData[1] == 0x00) {
        // Apple manufacturer data. Look for Find My locator tag (0x12)
        if (mData.length() >= 3 && (uint8_t)mData[2] == 0x12) {
          typeStr = "AirTag/FindMy";
          is_tracker = true;
        }
      }
      // Samsung SmartTag (Samsung Manufacturer ID 0x0075)
      else if (mData.length() >= 2 && (uint8_t)mData[0] == 0x75 && (uint8_t)mData[1] == 0x00) {
        typeStr = "SmartTag";
        is_tracker = true;
      }
    }

    // 2. Check Tile (Service UUID 0xFEED or 0xFEEC)
    if (advertisedDevice.haveServiceUUID()) {
      BLEUUID uuid = advertisedDevice.getServiceUUID();
      if (uuid.equals(BLEUUID((uint16_t)0xFEED)) || uuid.equals(BLEUUID((uint16_t)0xFEEC))) {
        typeStr = "Tile";
        is_tracker = true;
      }
    }

    // Track it if it's a known tracking beacon
    if (is_tracker) {
      String mac = advertisedDevice.getAddress().toString().c_str();
      mac.toUpperCase();
      int8_t rssi = advertisedDevice.getRSSI();
      uint32_t now = millis();

      // Check if already in table
      bool found = false;
      for (int i = 0; i < ble_tracker_count; i++) {
        if (strcmp(ble_trackers[i].mac, mac.c_str()) == 0) {
          ble_trackers[i].rssi = rssi;
          ble_trackers[i].last_seen = now;
          ble_trackers[i].seen_count++;
          found = true;
          break;
        }
      }

      if (!found) {
        if (ble_tracker_count < MAX_BLE_TRACKERS) {
          BLETracker& t = ble_trackers[ble_tracker_count++];
          strcpy(t.mac, mac.c_str());
          strcpy(t.type, typeStr.c_str());
          t.rssi = rssi;
          t.first_seen = now;
          t.last_seen = now;
          t.seen_count = 1;
        } else {
          // Evict oldest unseen
          int oldest_idx = 0;
          for (int i = 1; i < MAX_BLE_TRACKERS; i++) {
            if (ble_trackers[i].last_seen < ble_trackers[oldest_idx].last_seen) {
              oldest_idx = i;
            }
          }
          BLETracker& t = ble_trackers[oldest_idx];
          strcpy(t.mac, mac.c_str());
          strcpy(t.type, typeStr.c_str());
          t.rssi = rssi;
          t.first_seen = now;
          t.last_seen = now;
          t.seen_count = 1;
        }
      }
    }
  }
};

static void ble_scan_complete_cb(BLEScanResults results) {
  if (pBLEScan) pBLEScan->clearResults(); // free memory
  ble_scan_active = false;
}

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

// Radar sweep animation
static float    _sweep_angle    = 0.0f;
static uint32_t _sweep_last_ms  = 0;

// Web server
static WebServer* _webserver    = nullptr;

// ── AP tracking table ───────────────────────────────────────────
struct APRecord {
  char     ssid[33];
  uint8_t  bssid[6];
  int8_t   rssi;
  uint8_t  channel;
  bool     active;
};
static APRecord ap_table[MAX_AP_TABLE];
static int      ap_count = 0;

static void sec_track_ap(const uint8_t* payload, int16_t rssi, uint8_t ch) {
  // Extract BSSID from beacon/probe response (addr3 = bytes 16-21)
  const uint8_t* bssid = &payload[16];

  // Check if already tracked
  for (int i = 0; i < ap_count; i++) {
    if (memcmp(ap_table[i].bssid, bssid, 6) == 0) {
      ap_table[i].rssi = rssi;
      ap_table[i].channel = ch;
      return;
    }
  }

  // Add new AP if space
  if (ap_count >= MAX_AP_TABLE) return;
  APRecord& ap = ap_table[ap_count];
  memcpy(ap.bssid, bssid, 6);
  ap.rssi = rssi;
  ap.channel = ch;
  ap.active = true;

  // Extract SSID from tagged parameters (byte 36+ in beacon frame)
  // Tag 0 = SSID, length at byte 37, SSID string at byte 38+
  int ssid_len = 0;
  if (payload[36] == 0x00) { // SSID tag
    ssid_len = min((int)payload[37], 32);
    memcpy(ap.ssid, &payload[38], ssid_len);
  }
  ap.ssid[ssid_len] = '\0';
  ap_count++;

  // ── Evil Twin Detection ──────────────────────────────────────────
  if (strlen(evil_twin_target_ssid) > 0 && strcmp(ap.ssid, evil_twin_target_ssid) == 0) {
    if (memcmp(ap.bssid, evil_twin_trusted_bssid, 6) != 0) {
      evil_twin_detected = true;
      snprintf(evil_twin_attacker_bssid, sizeof(evil_twin_attacker_bssid),
               "%02X:%02X:%02X:%02X:%02X:%02X",
               ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    }
  }
}

// ── Promiscuous mode packet sniffer callback ────────────────────
static volatile bool _deauth_evt_pending = false;

static void IRAM_ATTR sec_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  pkt_total++;
  _pps_count++;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  if (pkt->rx_ctrl.sig_len < 24) return;

  uint8_t frame_type    = (pkt->payload[0] & 0x0C) >> 2;
  uint8_t frame_subtype = (pkt->payload[0] & 0xF0) >> 4;

  if (frame_type == 0) {  // Management frame
    switch (frame_subtype) {
      case 0x04: // Probe request
        {
          pkt_probe++;
          if (pkt->payload[24] == 0x00) {
            int len = min((int)pkt->payload[25], 32);
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
                  // Shift left
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
        if (pkt_deauth >= DEAUTH_SPIKE_COUNT) _deauth_evt_pending = true;
        break;
    }
  }
}

// ── Channel hopping (Core 0) ────────────────────────────────────
static void sec_hop_channel() {
  if (!ch_hopping) return;
  current_ch = (current_ch % 13) + 1;
  esp_wifi_set_channel(current_ch, WIFI_SECOND_CHAN_NONE);
}

// ── Raw 802.11 Beacon Transmitter ─────────────────────────────────
static void send_beacon_frame(const char* ssid, uint8_t channel) {
  uint8_t packet[128];
  int idx = 0;

  // 1. Frame Control (0x80 = Beacon management frame)
  packet[idx++] = 0x80;
  packet[idx++] = 0x00; // Frame Subtype flags

  // 2. Duration / ID
  packet[idx++] = 0x00;
  packet[idx++] = 0x00;

  // 3. Destination Address (Broadcast: FF:FF:FF:FF:FF:FF)
  for (int i = 0; i < 6; i++) packet[idx++] = 0xFF;

  // 4. Source MAC Address (Locally administered unicast spoofed address)
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(0, 256);
  mac[0] &= 0xFE; // Unicast
  mac[0] |= 0x02; // Locally Administered Address (LAA)
  for (int i = 0; i < 6; i++) packet[idx++] = mac[i];

  // 5. BSSID (Same spoofed MAC address)
  for (int i = 0; i < 6; i++) packet[idx++] = mac[i];

  // 6. Sequence Control
  packet[idx++] = 0x00;
  packet[idx++] = 0x00;

  // ── Frame Body ──
  // 7. Timestamp (8 bytes, set to 0)
  for (int i = 0; i < 8; i++) packet[idx++] = 0x00;

  // 8. Beacon Interval (100 TU / ~102.4ms)
  packet[idx++] = 0x64;
  packet[idx++] = 0x00;

  // 9. Capability Information (ESS, short slot time)
  packet[idx++] = 0x01;
  packet[idx++] = 0x04;

  // 10. SSID Information Element (ID: 0)
  packet[idx++] = 0x00; // Element ID
  int ssid_len = strlen(ssid);
  if (ssid_len > 32) ssid_len = 32;
  packet[idx++] = (uint8_t)ssid_len; // Element Length
  for (int i = 0; i < ssid_len; i++) packet[idx++] = ssid[i];

  // 11. Supported Rates IE (ID: 1)
  packet[idx++] = 0x01; // Element ID
  packet[idx++] = 0x08; // Element Length
  packet[idx++] = 0x82; packet[idx++] = 0x84; packet[idx++] = 0x8B; packet[idx++] = 0x96; // 1, 2, 5.5, 11 Mbps
  packet[idx++] = 0x24; packet[idx++] = 0x30; packet[idx++] = 0x48; packet[idx++] = 0x6C; // 18, 24, 36, 54 Mbps

  // 12. DS Parameter Set IE (ID: 3 - Channel selection)
  packet[idx++] = 0x03; // Element ID
  packet[idx++] = 0x01; // Element Length
  packet[idx++] = channel; // Active Wi-Fi Channel

  // Transmit raw frame via ESP-IDF AP interface
  esp_wifi_80211_tx(WIFI_IF_AP, packet, idx, true);
}

// ================================================================
//  SERIAL COMMAND PROTOCOL  (CMD → RES / EVT)
//  Used by the companion web app over USB Serial
// ================================================================
static void security_handle_command(String line) {
  line.trim();
  if (!line.startsWith("CMD:")) return;
  String cmd = line.substring(4);

  if (cmd == "PING") {
    Serial.println("RES:{\"ok\":true,\"fw\":\"2.0\",\"mode\":2,\"hw\":\"deskbuddy2\"}");
  }
  else if (cmd == "STATUS") {
    Serial.printf("RES:{\"scanning\":%s,\"ch\":%d,\"pps\":%lu,\"total\":%lu,"
                  "\"beacons\":%lu,\"probes\":%lu,\"deauths\":%lu,"
                  "\"hopping\":%s,\"aps\":%d}\n",
                  sec_scanning ? "true" : "false", current_ch,
                  pkt_per_sec, pkt_total, pkt_beacon, pkt_probe, pkt_deauth,
                  ch_hopping ? "true" : "false", ap_count);
  }
  else if (cmd == "SCAN_START") {
    if (!sec_scanning) {
      sec_scanning = true;
      esp_wifi_set_promiscuous(true);
    }
    Serial.println("RES:{\"ok\":true,\"action\":\"scan_start\"}");
  }
  else if (cmd == "SCAN_STOP") {
    if (sec_scanning) {
      sec_scanning = false;
      esp_wifi_set_promiscuous(false);
    }
    Serial.println("RES:{\"ok\":true,\"action\":\"scan_stop\"}");
  }
  else if (cmd.startsWith("ATTACK:")) {
    String attack_type = cmd.substring(7);
    if (attack_type == "BEACON_SPAM") {
      active_attack_type = ATTACK_BEACON_SPAM;
    } else if (attack_type == "RICK_ROLL" || attack_type == "RICKROLL") {
      active_attack_type = ATTACK_RICK_ROLL;
    } else if (attack_type == "STOP") {
      active_attack_type = ATTACK_NONE;
    }

    pkt_deauth += 20; // Simulate a deauth burst to trigger red alert
    _deauth_evt_pending = true;
    snprintf(evil_twin_attacker_bssid, sizeof(evil_twin_attacker_bssid), "RUNNING:%s", attack_type.c_str());
    Serial.printf("RES:{\"ok\":true,\"action\":\"attack\",\"type\":\"%s\"}\n", attack_type.c_str());
  }
  else if (cmd.startsWith("SET_CH:")) {
    int ch = cmd.substring(7).toInt();
    if (ch >= 1 && ch <= 13) {
      current_ch = ch;
      ch_hopping = false;
      esp_wifi_set_channel(current_ch, WIFI_SECOND_CHAN_NONE);
      Serial.printf("RES:{\"ok\":true,\"ch\":%d,\"hopping\":false}\n", ch);
    } else {
      Serial.println("RES:{\"ok\":false,\"error\":\"invalid channel\"}");
    }
  }
  else if (cmd == "HOP_ON") {
    ch_hopping = true;
    Serial.println("RES:{\"ok\":true,\"hopping\":true}");
  }
  else if (cmd == "HOP_OFF") {
    ch_hopping = false;
    Serial.println("RES:{\"ok\":true,\"hopping\":false}");
  }
  else if (cmd == "GET_APS") {
    Serial.print("RES:{\"aps\":[");
    for (int i = 0; i < ap_count; i++) {
      if (i > 0) Serial.print(',');
      Serial.printf("{\"ssid\":\"%s\",\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                    "\"rssi\":%d,\"ch\":%d}",
                    ap_table[i].ssid,
                    ap_table[i].bssid[0], ap_table[i].bssid[1],
                    ap_table[i].bssid[2], ap_table[i].bssid[3],
                    ap_table[i].bssid[4], ap_table[i].bssid[5],
                    ap_table[i].rssi, ap_table[i].channel);
    }
    Serial.println("]}");
  }
  else if (cmd == "FW_VERSION") {
    Serial.println("RES:{\"version\":\"2.0\",\"build\":\"2026-05-24\",\"board\":\"xiao_s3\"}");
  }
  else if (cmd == "DEAUTH_COUNT") {
    Serial.printf("RES:{\"deauths\":%lu,\"threshold\":%d,\"alert\":%s}\n",
                  pkt_deauth, DEAUTH_SPIKE_COUNT,
                  pkt_deauth >= DEAUTH_SPIKE_COUNT ? "true" : "false");
  }
  else if (cmd == "RESET_STATS") {
    pkt_total = pkt_deauth = pkt_beacon = pkt_probe = 0;
    pkt_per_sec = 0;
    _pps_count = 0;
    ap_count = 0;
    Serial.println("RES:{\"ok\":true}");
  }
  else if (cmd == "GET_CFG") {
    auto rgb565toHex = [](uint16_t color) -> String {
      uint8_t r = (color >> 11) << 3;
      uint8_t g = ((color >> 5) & 0x3F) << 2;
      uint8_t b = (color & 0x1F) << 3;
      char hex[8];
      snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
      return String(hex);
    };
    Serial.printf("RES:{\"ssid\":\"%s\",\"lamin\":%.2f,\"lomin\":%.2f,\"lamax\":%.2f,\"lomax\":%.2f,\"c_col\":\"%s\",\"w_col\":\"%s\"}\n",
                  sys_wifi_ssid.c_str(), sys_sky_lamin, sys_sky_lomin, sys_sky_lamax, sys_sky_lomax,
                  rgb565toHex(sys_clock_color).c_str(), rgb565toHex(sys_weather_color).c_str());
  }
  else if (cmd.startsWith("SET_WIFI:")) {
    int split = cmd.indexOf(':', 9);
    if (split > 0) {
      String ssid = cmd.substring(9, split);
      String pass = cmd.substring(split + 1);
      sys_wifi_ssid = ssid;
      sys_wifi_pass = pass;
      Preferences p;
      p.begin("aerosniffer", false);
      p.putString("ssid", ssid);
      p.putString("pass", pass);
      p.end();
      Serial.println("RES:{\"ok\":true,\"action\":\"set_wifi\"}");
    } else {
      Serial.println("RES:{\"ok\":false,\"error\":\"invalid format\"}");
    }
  }
  else if (cmd.startsWith("SET_BBOX:")) {
    float box[4];
    int start = 9;
    for (int i=0; i<4; i++) {
      int end = (i==3) ? -1 : cmd.indexOf(':', start);
      String part = (end == -1) ? cmd.substring(start) : cmd.substring(start, end);
      box[i] = part.toFloat();
      start = end + 1;
    }
    sys_sky_lamin = box[0];
    sys_sky_lomin = box[1];
    sys_sky_lamax = box[2];
    sys_sky_lomax = box[3];
    Preferences p;
    p.begin("aerosniffer", false);
    p.putFloat("lamin", box[0]);
    p.putFloat("lomin", box[1]);
    p.putFloat("lamax", box[2]);
    p.putFloat("lomax", box[3]);
    p.end();
    Serial.println("RES:{\"ok\":true,\"action\":\"set_bbox\"}");
  }
  else if (cmd.startsWith("SET_COLOR:")) {
    if (cmd.length() >= 25) {
      String c1 = cmd.substring(10, 17);
      String c2 = cmd.substring(18, 25);
      
      auto hex2rgb565 = [](String hex) -> uint16_t {
        long num = strtol(hex.substring(1).c_str(), NULL, 16);
        uint8_t r = (num >> 16) & 0xFF;
        uint8_t g = (num >> 8) & 0xFF;
        uint8_t b = num & 0xFF;
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      };
      
      sys_clock_color = hex2rgb565(c1);
      sys_weather_color = hex2rgb565(c2);
      
      Preferences p;
      p.begin("aerosniffer", false);
      p.putUShort("c_col", sys_clock_color);
      p.putUShort("w_col", sys_weather_color);
      p.end();
      Serial.println("RES:{\"ok\":true,\"action\":\"set_color\"}");
    } else {
      Serial.println("RES:{\"ok\":false,\"error\":\"invalid format\"}");
    }
  }
  else if (cmd.startsWith("REBOOT")) {
    Serial.println("RES:{\"ok\":true,\"action\":\"reboot\"}");
    delay(100);
    ESP.restart();
  }
  else {
    Serial.printf("RES:{\"ok\":false,\"error\":\"unknown command: %s\"}\n", cmd.c_str());
  }
}

// ── Async event streaming (called from Core 1) ──────────────────
static void sec_stream_events() {
  if (_deauth_evt_pending) {
    _deauth_evt_pending = false;
    Serial.printf("EVT:{\"type\":\"deauth_alert\",\"count\":%lu,\"threshold\":%d}\n",
                  pkt_deauth, DEAUTH_SPIKE_COUNT);
  }
}

// ── Web server routes ───────────────────────────────────────────
static void sec_handle_root() {
  String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AeroSniffer // SOC</title>
<style>
  :root{
    --bg:#070a08;
    --panel:#0e1611;
    --line:#1c2b22;
    --green:#39ff88;
    --green-dim:#1e8a52;
    --amber:#ffb000;
    --purple:#c084fc;
    --text:#d7e8da;
    --muted:#5c7468;
    --mono: 'JetBrains Mono','IBM Plex Mono',ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  }
  *{margin:0;padding:0;box-sizing:border-box;}
  html,body{background:var(--bg);color:var(--text);font-family:var(--mono);}
  body{
    padding:18px;
    line-height:1.5;
    background-image:
      repeating-linear-gradient(transparent 0 2px, rgba(57,255,136,0.012) 2px 4px);
    font-size:14px;
  }

  /* ---------- HEADER ---------- */
  header{
    position:relative;
    border:1px solid var(--line);
    border-radius:4px;
    padding:14px 16px;
    margin-bottom:16px;
    overflow:hidden;
    background:linear-gradient(180deg, rgba(57,255,136,0.05), transparent 60%);
  }
  header::after{
    content:"";
    position:absolute; left:0; right:0; top:0; height:2px;
    background:linear-gradient(90deg, transparent, var(--green), transparent);
    animation: sweep 3.4s linear infinite;
    opacity:.55;
  }
  @keyframes sweep{
    0%{transform:translateX(-100%);}
    100%{transform:translateX(100%);}
  }
  .hdr-row{display:flex; align-items:baseline; justify-content:space-between; flex-wrap:wrap; gap:8px;}
  h1{
    font-size:1.15em;
    font-weight:700;
    letter-spacing:.18em;
    color:var(--green);
    text-transform:uppercase;
    display:flex; align-items:center; gap:10px;
  }
  h1 .tag{
    font-size:.6em;
    letter-spacing:.1em;
    color:var(--muted);
    border:1px solid var(--line);
    padding:2px 6px;
    border-radius:3px;
    font-weight:400;
  }
  .status-line{
    font-size:.8em;
    color:var(--muted);
    display:flex; align-items:center; gap:8px;
    letter-spacing:.05em;
  }
  .status-dot{
    width:8px; height:8px; border-radius:50%;
    background:#5a2222;
    box-shadow:0 0 0 1px rgba(239,68,68,.25);
    animation: blink 1.1s infinite alternate;
  }
  .status-dot.active{
    background:var(--green);
    box-shadow:0 0 8px var(--green);
    animation: solidpulse 1.6s infinite alternate;
  }
  @keyframes blink{from{opacity:.35;}to{opacity:1;}}
  @keyframes solidpulse{from{opacity:.6;}to{opacity:1;}}

  /* ---------- NAV ---------- */
  .nav-tabs{
    display:grid;
    grid-template-columns: repeat(4, 1fr);
    gap:1px;
    margin-bottom:16px;
    border:1px solid var(--line);
    border-radius:4px;
    overflow:hidden;
    background:var(--line);
  }
  .tab-btn{
    background:var(--panel);
    border:none;
    color:var(--muted);
    padding:11px 6px;
    font-size:.72em;
    font-family:var(--mono);
    letter-spacing:.12em;
    text-transform:uppercase;
    cursor:pointer;
    transition:.15s;
    text-align:center;
    position:relative;
  }
  .tab-btn .freq{
    display:block;
    font-size:.85em;
    color:var(--muted);
    margin-top:3px;
    letter-spacing:.05em;
  }
  .tab-btn:hover{ color:var(--text); background:#121d16; }
  .tab-btn.active{
    color:var(--green);
    background:#0c1a13;
    box-shadow: inset 0 -2px 0 var(--green);
  }
  .tab-btn.active .freq{ color:var(--green-dim); }
  .tab-btn.ble-tab.active{
    color:var(--purple);
    box-shadow: inset 0 -2px 0 var(--purple);
  }
  .tab-btn.ble-tab.active .freq{ color:#7c4fb8; }

  .tab-content{display:none;}
  .tab-content.active{display:block; animation:fadein .25s ease;}
  @keyframes fadein{from{opacity:0;transform:translateY(4px);}to{opacity:1;transform:translateY(0);}}

  /* ---------- CARDS ---------- */
  .card{
    background:var(--panel);
    border:1px solid var(--line);
    border-radius:4px;
    padding:16px;
    margin-bottom:14px;
  }
  .card-title{
    color:var(--muted);
    font-size:.72em;
    text-transform:uppercase;
    letter-spacing:.2em;
    margin-bottom:12px;
    font-weight:700;
    display:flex; align-items:center; gap:8px;
  }
  .card-title::before{
    content:"";
    width:8px; height:8px;
    border:1px solid var(--green-dim);
    transform:rotate(45deg);
    flex-shrink:0;
  }
  .card-note{
    font-size:.78em;
    color:var(--muted);
    margin-bottom:12px;
    line-height:1.6;
  }

  /* ---------- STATS ---------- */
  .stat-grid{
    display:grid;
    grid-template-columns: repeat(auto-fit, minmax(110px,1fr));
    gap:10px;
  }
  .stat-card{
    border:1px solid var(--line);
    border-radius:3px;
    padding:10px 12px;
    display:flex; flex-direction:column; gap:4px;
    background:#0a120d;
  }
  .stat-label{
    font-size:.68em;
    color:var(--muted);
    text-transform:uppercase;
    letter-spacing:.15em;
  }
  .stat-value{
    font-size:1.5em;
    font-weight:700;
    color:var(--text);
    font-family:var(--mono);
    letter-spacing:.02em;
  }
  .stat-value.cyan{ color:var(--green); }
  .stat-value.purple{ color:var(--purple); }
  .stat-value.red{ color:#ff5d5d; }

  /* ---------- BUTTONS ---------- */
  .btn{
    display:inline-flex;
    align-items:center;
    justify-content:center;
    gap:6px;
    padding:10px 16px;
    background:transparent;
    border:1px solid var(--green-dim);
    color:var(--green);
    font-size:.78em;
    font-weight:700;
    letter-spacing:.1em;
    text-transform:uppercase;
    border-radius:3px;
    cursor:pointer;
    transition:.15s;
    font-family:var(--mono);
  }
  .btn:hover{ background:rgba(57,255,136,.08); border-color:var(--green); }
  .btn.danger{ border-color:#7a3030; color:#ff5d5d; }
  .btn.danger:hover{ background:rgba(255,93,93,.08); border-color:#ff5d5d; }
  .btn.action-btn{ padding:5px 10px; font-size:.7em; letter-spacing:.05em; }
  .btn-group{ display:flex; gap:8px; flex-wrap:wrap; margin-top:2px; }

  /* ---------- ALARM ---------- */
  .alarm-banner{
    display:none;
    background:repeating-linear-gradient(135deg, rgba(255,93,93,.08) 0 10px, rgba(255,93,93,.03) 10px 20px);
    border:1px solid #ff5d5d;
    border-radius:4px;
    padding:16px;
    margin-bottom:16px;
    box-shadow:0 0 16px rgba(255,93,93,.15);
  }
  .alarm-title{
    color:#ff5d5d;
    font-size:.95em;
    font-weight:700;
    letter-spacing:.15em;
    text-transform:uppercase;
    margin-bottom:8px;
    display:flex; align-items:center; gap:8px;
  }
  .alarm-title::before{ content:"▲"; font-size:.85em; }
  #alarm-details{
    font-size:.82em;
    color:#ffd0d0;
    white-space:pre-line;
    margin-bottom:12px;
    border-left:2px solid #ff5d5d;
    padding-left:10px;
  }

  /* ---------- TABLES ---------- */
  table{ width:100%; border-collapse:collapse; font-size:.82em; text-align:left; }
  thead th{
    color:var(--muted);
    font-weight:700;
    padding:6px 8px;
    border-bottom:1px solid var(--line);
    text-transform:uppercase;
    font-size:.78em;
    letter-spacing:.1em;
    position:sticky; top:0; background:var(--panel);
  }
  td{
    padding:7px 8px;
    border-bottom:1px solid var(--line);
    color:var(--text);
    font-family:var(--mono);
  }
  tbody tr:hover td{ background:rgba(57,255,136,.03); }
  .scrollable-table{ max-height:260px; overflow-y:auto; border:1px solid var(--line); border-radius:3px; }
  .scrollable-table::-webkit-scrollbar{ width:6px; }
  .scrollable-table::-webkit-scrollbar-thumb{ background:var(--green-dim); border-radius:3px; }

  /* protected info row */
  .protect-row{
    display:flex; gap:10px; flex-wrap:wrap; margin-bottom:14px;
  }

  footer{
    text-align:center;
    color:var(--muted);
    font-size:.7em;
    letter-spacing:.15em;
    text-transform:uppercase;
    margin-top:24px;
    padding-top:14px;
    border-top:1px solid var(--line);
  }

  @media (prefers-reduced-motion: reduce){
    header::after, .status-dot{ animation:none; }
  }
</style>
</head>
<body>

  <header>
    <div class="hdr-row">
      <h1>AeroSniffer <span class="tag">SOC v2.0</span></h1>
      <div class="status-line">
        <span id="device-status-lbl">IDLE</span>
        <span class="status-dot" id="status-indicator"></span>
      </div>
    </div>
  </header>

  <div class="alarm-banner" id="evil-twin-banner">
    <div class="alarm-title">Rogue Access Point Detected</div>
    <p id="alarm-details"></p>
    <button class="btn danger action-btn" onclick="clearEvilTwinAlarm()">Dismiss / Reset Alarm</button>
  </div>

  <div class="nav-tabs">
    <button class="tab-btn active" onclick="switchTab('tab-dashboard')">Dashboard<span class="freq">2.4G</span></button>
    <button class="tab-btn" onclick="switchTab('tab-twin')">Evil Twin<span class="freq">AP-GUARD</span></button>
    <button class="tab-btn" onclick="switchTab('tab-probes')">Probes<span class="freq">CLIENT-RX</span></button>
    <button class="tab-btn ble-tab" onclick="switchTab('tab-ble')">BLE Trackers<span class="freq">2.4G/BLE</span></button>
  </div>

  <!-- DASHBOARD -->
  <div class="tab-content active" id="tab-dashboard">
    <div class="card">
      <div class="card-title">Console Controls</div>
      <div class="btn-group">
        <button class="btn" onclick="startScan()">▶ Start Wi-Fi Scan</button>
        <button class="btn danger" onclick="stopScan()">■ Stop Wi-Fi Scan</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">Packet Capture Statistics</div>
      <div class="stat-grid">
        <div class="stat-card"><span class="stat-label">PPS</span><span class="stat-value cyan" id="stat-pps">0</span></div>
        <div class="stat-card"><span class="stat-label">Total</span><span class="stat-value" id="stat-total">0</span></div>
        <div class="stat-card"><span class="stat-label">Beacons</span><span class="stat-value" id="stat-beacons">0</span></div>
        <div class="stat-card"><span class="stat-label">Probes</span><span class="stat-value" id="stat-probes">0</span></div>
        <div class="stat-card"><span class="stat-label">Deauths</span><span class="stat-value red" id="stat-deauths">0</span></div>
      </div>
    </div>
  </div>

  <!-- EVIL TWIN -->
  <div class="tab-content" id="tab-twin">
    <div class="card">
      <div class="card-title">Active Protection Whitelist</div>
      <div class="stat-grid protect-row">
        <div class="stat-card"><span class="stat-label">Protected SSID</span><span class="stat-value cyan" id="protected-ssid-lbl">None</span></div>
        <div class="stat-card"><span class="stat-label">Trusted BSSID</span><span class="stat-value" id="protected-bssid-lbl">00:00:00:00:00:00</span></div>
      </div>
      <p class="card-note">Select any detected access point from the list below to set it as your trusted protected network.</p>
    </div>
    <div class="card">
      <div class="card-title">Detected Access Points</div>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>SSID</th><th>BSSID</th><th>Ch</th><th>RSSI</th><th>Action</th></tr>
          </thead>
          <tbody id="ap-list-body">
            <tr><td colspan="5" style="text-align:center;color:var(--muted)">No networks detected yet. Start scanning.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- PROBES -->
  <div class="tab-content" id="tab-probes">
    <div class="card">
      <div class="card-title">Probe Request Leaks (Nearby Devices)</div>
      <p class="card-note">Shows nearby devices (phones, tablets) broadcasting SSIDs of networks they have previously connected to.</p>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>Client MAC</th><th>Leaked Network SSID</th><th>Last Seen</th></tr>
          </thead>
          <tbody id="probe-list-body">
            <tr><td colspan="3" style="text-align:center;color:var(--muted)">No probe requests captured yet.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- BLE -->
  <div class="tab-content" id="tab-ble">
    <div class="card" style="border-color:#3a2d52">
      <div class="card-title" style="color:var(--purple)">BLE Beacon Tracking</div>
      <p class="card-note">Scans for proximity signals matching commercial stalker tags (AirTags, Tiles, Samsung SmartTags).</p>
      <div class="scrollable-table">
        <table>
          <thead>
            <tr><th>Tracker Type</th><th>Bluetooth Address</th><th>Last RSSI</th><th>Last Seen</th><th>Hits</th></tr>
          </thead>
          <tbody id="ble-list-body">
            <tr><td colspan="5" style="text-align:center;color:var(--muted)">No tracker beacons detected yet.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <footer>AeroSniffer v2.0 :: DeskBuddy 2.0 SOC :: 802.11 / BLE Sensor</footer>

  <script>
    function switchTab(tabId){
      document.querySelectorAll('.tab-btn').forEach(btn=>btn.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(cont=>cont.classList.remove('active'));
      const tabEl=document.getElementById(tabId);
      tabEl.classList.add('active');
      const buttons=document.querySelectorAll('.tab-btn');
      if(tabId==='tab-dashboard')buttons[0].classList.add('active');
      else if(tabId==='tab-twin')buttons[1].classList.add('active');
      else if(tabId==='tab-probes')buttons[2].classList.add('active');
      else if(tabId==='tab-ble')buttons[3].classList.add('active');
      loadTabData(tabId);
    }
    function startScan(){fetch('/api/scan/start')}
    function stopScan(){fetch('/api/scan/stop')}
    function setProtected(ssid,bssid){
      fetch(`/api/set_protected?ssid=${encodeURIComponent(ssid)}&bssid=${encodeURIComponent(bssid)}`).then(()=>alert(`Protected target set: ${ssid}`));
    }
    function clearEvilTwinAlarm(){fetch('/api/clear_evil_twin')}
    function loadTabData(tabId){
      if(tabId==='tab-twin'){
        fetch('/api/aps').then(r=>r.json()).then(d=>{
          let html='';
          if(!d.aps||d.aps.length===0){
            html='<tr><td colspan="5" style="text-align:center;color:var(--muted)">No networks detected yet.</td></tr>';
          }else{
            d.aps.forEach(ap=>{
              html+=`<tr>
                <td>${ap.ssid||'<i>Hidden SSID</i>'}</td>
                <td>${ap.bssid}</td>
                <td>${ap.ch}</td>
                <td>${ap.rssi} dBm</td>
                <td><button class="btn action-btn" onclick="setProtected('${ap.ssid}','${ap.bssid}')">Protect</button></td>
              </tr>`;
            });
          }
          document.getElementById('ap-list-body').innerHTML=html;
        });
      }else if(tabId==='tab-probes'){
        fetch('/api/probes').then(r=>r.json()).then(d=>{
          let html='';
          if(!d.probes||d.probes.length===0){
            html='<tr><td colspan="3" style="text-align:center;color:var(--muted)">No probe requests captured.</td></tr>';
          }else{
            d.probes.forEach(pr=>{
              html+=`<tr>
                <td>${pr.mac}</td>
                <td style="color:var(--green)">${pr.ssid}</td>
                <td>${pr.seen}s ago</td>
              </tr>`;
            });
          }
          document.getElementById('probe-list-body').innerHTML=html;
        });
      }else if(tabId==='tab-ble'){
        fetch('/api/ble').then(r=>r.json()).then(d=>{
          let html='';
          if(!d.trackers||d.trackers.length===0){
            html='<tr><td colspan="5" style="text-align:center;color:var(--muted)">No tracker beacons detected.</td></tr>';
          }else{
            d.trackers.forEach(tr=>{
              html+=`<tr>
                <td style="color:var(--purple);font-weight:bold">${tr.type}</td>
                <td>${tr.mac}</td>
                <td>${tr.rssi} dBm</td>
                <td>${tr.seen}s ago</td>
                <td>${tr.count}</td>
              </tr>`;
            });
          }
          document.getElementById('ble-list-body').innerHTML=html;
        });
      }
    }
    setInterval(()=>{
      fetch('/api/stats').then(r=>r.json()).then(d=>{
        document.getElementById('device-status-lbl').innerText=d.scanning?'SCANNING':'IDLE';
        const ind=document.getElementById('status-indicator');
        if(d.scanning)ind.classList.add('active');else ind.classList.remove('active');
        document.getElementById('stat-pps').innerText=d.pps;
        document.getElementById('stat-total').innerText=d.total;
        document.getElementById('stat-beacons').innerText=d.beacons;
        document.getElementById('stat-probes').innerText=d.probes;
        document.getElementById('stat-deauths').innerText=d.deauths;
        document.getElementById('protected-ssid-lbl').innerText=d.evil_twin_target||'None';
        document.getElementById('protected-bssid-lbl').innerText=d.evil_twin_trusted||'00:00:00:00:00:00';
        const banner=document.getElementById('evil-twin-banner');
        if(d.evil_twin_detected){
          banner.style.display='block';
          document.getElementById('alarm-details').innerText=`Target Network SSID: ${d.evil_twin_target}\nTrusted Router MAC: ${d.evil_twin_trusted}\nATTACKER ROUTER MAC: ${d.evil_twin_attacker}`;
        }else{
          banner.style.display='none';
        }
      });
      const activeTab=document.querySelector('.tab-content.active');
      if(activeTab)loadTabData(activeTab.id);
    },1000);
  </script>
</body>
</html>)rawliteral";
  _webserver->send(200, "text/html", html);
}

static void sec_handle_stats() {
  String bssid_str = "";
  char bssid_buf[20];
  snprintf(bssid_buf, sizeof(bssid_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           evil_twin_trusted_bssid[0], evil_twin_trusted_bssid[1], evil_twin_trusted_bssid[2],
           evil_twin_trusted_bssid[3], evil_twin_trusted_bssid[4], evil_twin_trusted_bssid[5]);
  bssid_str = bssid_buf;

  char json[512];
  snprintf(json, sizeof(json),
    "{\"scanning\":%s,\"ch\":%d,\"pps\":%lu,\"total\":%lu,"
    "\"beacons\":%lu,\"probes\":%lu,\"deauths\":%lu,"
    "\"evil_twin_detected\":%s,\"evil_twin_target\":\"%s\",\"evil_twin_trusted\":\"%s\",\"evil_twin_attacker\":\"%s\"}",
    sec_scanning ? "true" : "false", current_ch,
    pkt_per_sec, pkt_total, pkt_beacon, pkt_probe, pkt_deauth,
    evil_twin_detected ? "true" : "false", evil_twin_target_ssid, bssid_str.c_str(), evil_twin_attacker_bssid);
  _webserver->send(200, "application/json", json);
}

static void sec_handle_aps() {
  String json = "{\"aps\":[";
  for (int i = 0; i < ap_count; i++) {
    if (i > 0) json += ",";
    char ap_json[128];
    snprintf(ap_json, sizeof(ap_json),
             "{\"ssid\":\"%s\",\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":%d,\"ch\":%d}",
             ap_table[i].ssid,
             ap_table[i].bssid[0], ap_table[i].bssid[1], ap_table[i].bssid[2],
             ap_table[i].bssid[3], ap_table[i].bssid[4], ap_table[i].bssid[5],
             ap_table[i].rssi, ap_table[i].channel);
    json += ap_json;
  }
  json += "]}";
  _webserver->send(200, "application/json", json);
}

static void sec_handle_probes() {
  String json = "{\"probes\":[";
  for (int i = 0; i < probe_leak_count; i++) {
    if (i > 0) json += ",";
    char pr_json[128];
    snprintf(pr_json, sizeof(pr_json),
             "{\"mac\":\"%s\",\"ssid\":\"%s\",\"seen\":%lu}",
             probe_leaks[i].client_mac, probe_leaks[i].ssid, (millis() - probe_leaks[i].last_seen) / 1000);
    json += pr_json;
  }
  json += "]}";
  _webserver->send(200, "application/json", json);
}

static void sec_handle_ble() {
  String json = "{\"trackers\":[";
  for (int i = 0; i < ble_tracker_count; i++) {
    if (i > 0) json += ",";
    char tr_json[128];
    snprintf(tr_json, sizeof(tr_json),
             "{\"mac\":\"%s\",\"type\":\"%s\",\"rssi\":%d,\"seen\":%lu,\"count\":%d}",
             ble_trackers[i].mac, ble_trackers[i].type, ble_trackers[i].rssi,
             (millis() - ble_trackers[i].last_seen) / 1000, ble_trackers[i].seen_count);
    json += tr_json;
  }
  json += "]}";
  _webserver->send(200, "application/json", json);
}

static void sec_handle_set_protected() {
  if (_webserver->hasArg("ssid") && _webserver->hasArg("bssid")) {
    String ssid = _webserver->arg("ssid");
    String bssid_str = _webserver->arg("bssid");
    
    uint8_t bssid[6] = {0};
    int values[6];
    if (sscanf(bssid_str.c_str(), "%x:%x:%x:%x:%x:%x", 
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
      for (int i = 0; i < 6; i++) {
        bssid[i] = (uint8_t)values[i];
      }
    }
    
    strcpy(evil_twin_target_ssid, ssid.c_str());
    memcpy(evil_twin_trusted_bssid, bssid, 6);
    evil_twin_detected = false;
    evil_twin_attacker_bssid[0] = '\0';
    
    Preferences prefs;
    prefs.begin("aerosniffer", false);
    prefs.putString("tr_ssid", ssid);
    prefs.putBytes("tr_bssid", bssid, 6);
    prefs.end();
    
    _webserver->send(200, "text/plain", "OK");
  } else {
    _webserver->send(400, "text/plain", "Missing ssid or bssid");
  }
}

static void sec_handle_clear_evil_twin() {
  evil_twin_detected = false;
  evil_twin_attacker_bssid[0] = '\0';
  _webserver->send(200, "text/plain", "OK");
}

static void sec_handle_scan_start() {
  if (!sec_scanning) {
    sec_scanning = true;
    esp_wifi_set_promiscuous(true);
  }
  _webserver->send(200, "text/plain", "OK");
}

static void sec_handle_scan_stop() {
  if (sec_scanning) {
    sec_scanning = false;
    esp_wifi_set_promiscuous(false);
  }
  _webserver->send(200, "text/plain", "OK");
}

// ── Display: Draw radar sweep + stats ───────────────────────────
static void sec_draw_display() {
  if (!_stft) return;
  _stft->fillScreen(TFT_BLACK);

  // Header
  _stft->fillRect(0, 0, TFT_W, 14, 0x000F);
  _stft->setTextColor(0x07FF);
  _stft->setTextSize(1);
  _stft->setCursor(2, 3);
  _stft->print("MODE 2: SECURITY MONITOR");

  // Radar sweep circle
  int rcx = TFT_W / 2, rcy = 90, rr = 54;
  _stft->fillCircle(rcx, rcy, rr, 0x0841);
  _stft->drawCircle(rcx, rcy, rr, 0x0340);
  _stft->drawCircle(rcx, rcy, rr / 2, 0x0220);
  _stft->drawFastHLine(rcx - rr, rcy, rr * 2, 0x0220);
  _stft->drawFastVLine(rcx, rcy - rr, rr * 2, 0x0220);

  // Sweep line
  float rad = _sweep_angle * DEG_TO_RAD;
  int sx = rcx + (int)((rr - 2) * cosf(rad));
  int sy = rcy + (int)((rr - 2) * sinf(rad));
  _stft->drawLine(rcx, rcy, sx, sy, 0x07E0);
  _stft->fillCircle(rcx, rcy, 3, TFT_WHITE);

  // Status badge
  uint16_t badge_col = sec_scanning ? 0x07E0 : 0x4208;
  _stft->fillRoundRect(TFT_W - 68, 18, 64, 12, 3, badge_col);
  _stft->setTextColor(TFT_BLACK);
  _stft->setTextSize(1);
  _stft->setCursor(TFT_W - 62, 20);
  _stft->print(sec_scanning ? "SCANNING" : "  IDLE  ");

  // Stats area below radar
  int sy_base = 154;
  _stft->setTextColor(0x528A);
  _stft->setTextSize(1);

  // PKT/s bar
  _stft->setCursor(4, sy_base);
  _stft->print("PKT/s:");
  int bar_w = min((int)(pkt_per_sec / 5), TFT_W - 60);
  _stft->fillRect(48, sy_base, bar_w, 8, 0x07E0);
  _stft->setTextColor(TFT_YELLOW);
  _stft->setCursor(TFT_W - 42, sy_base);
  _stft->printf("%4lu", pkt_per_sec);

  // Data rows
  _stft->setTextColor(0x07FF);
  _stft->setTextSize(1);
  _stft->setCursor(4, sy_base + 16);
  _stft->printf("TOTAL: %lu", pkt_total);

  _stft->setCursor(4, sy_base + 28);
  _stft->printf("BCN: %lu  PRB: %lu", pkt_beacon, pkt_probe);

  _stft->setCursor(4, sy_base + 40);
  _stft->setTextColor(pkt_deauth > 0 ? TFT_RED : 0x07FF);
  _stft->printf("DEAUTH: %lu", pkt_deauth);
  if (pkt_deauth >= DEAUTH_SPIKE_COUNT) {
    _stft->setTextColor(TFT_RED);
    _stft->print("  !! ALERT");
  }

  _stft->setTextColor(0x528A);
  _stft->setCursor(4, sy_base + 52);
  _stft->printf("CH: %d", current_ch);

  // Footer: web UI URL
  _stft->fillRect(0, TFT_H - 14, TFT_W, 14, 0x0008);
  _stft->setTextColor(0x2CA0);
  _stft->setTextSize(1);
  _stft->setCursor(4, TFT_H - 12);
  _stft->print("http://192.168.4.1");
}

// ── Public API ──────────────────────────────────────────────────
static MyAdvertisedDeviceCallbacks myAdvertisedDeviceCallbacks;

void security_setup(TFT_eSPI* tft) {
  _stft = tft;
  pkt_total = pkt_deauth = pkt_beacon = pkt_probe = 0;
  pkt_per_sec = 0;
  _pps_count = 0;
  _pps_last_ms = millis();
  sec_scanning = false;
  _sweep_angle = 0;
  evil_twin_detected = false;
  probe_leak_count = 0;
  ble_tracker_count = 0;
  ble_scan_active = false;
  last_ble_scan_ms = 0;

  // Load Evil Twin whitelist from Preferences
  Preferences prefs;
  prefs.begin("aerosniffer", true);
  String tr_ssid = prefs.getString("tr_ssid", "");
  if (tr_ssid.length() > 0) {
    strcpy(evil_twin_target_ssid, tr_ssid.c_str());
    prefs.getBytes("tr_bssid", evil_twin_trusted_bssid, 6);
  } else {
    evil_twin_target_ssid[0] = '\0';
    memset(evil_twin_trusted_bssid, 0, 6);
  }
  prefs.end();

  // Start WiFi in AP mode for web control
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SEC_AP_SSID, SEC_AP_PASS);
  Serial.printf("[SEC] AP started: %s / %s\n", SEC_AP_SSID, SEC_AP_PASS);
  Serial.printf("[SEC] Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());

  // Set up promiscuous callback
  esp_wifi_set_promiscuous_rx_cb(sec_sniffer_cb);

  // Initialize BLE Scanner
  BLEDevice::init("AeroSniffer-BLE");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(&myAdvertisedDeviceCallbacks, true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Start web server
  _webserver = new WebServer(SEC_WEB_PORT);
  _webserver->on("/", sec_handle_root);
  _webserver->on("/api/stats", sec_handle_stats);
  _webserver->on("/api/scan/start", sec_handle_scan_start);
  _webserver->on("/api/scan/stop", sec_handle_scan_stop);
  _webserver->on("/api/aps", sec_handle_aps);
  _webserver->on("/api/probes", sec_handle_probes);
  _webserver->on("/api/ble", sec_handle_ble);
  _webserver->on("/api/set_protected", sec_handle_set_protected);
  _webserver->on("/api/clear_evil_twin", sec_handle_clear_evil_twin);
  _webserver->begin();

  sec_draw_display();
}

void security_teardown() {
  if (sec_scanning) {
    esp_wifi_set_promiscuous(false);
    sec_scanning = false;
  }
  if (pBLEScan) {
    pBLEScan->stop();
    pBLEScan = nullptr;
  }
  BLEDevice::deinit(true);

  if (_webserver) {
    _webserver->stop();
    delete _webserver;
    _webserver = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  _stft = nullptr;
}

void security_core0_task() {
  // Channel hopping
  if (sec_scanning) {
    sec_hop_channel();
  }

  // Periodic duty-cycled BLE scan
  if (sec_scanning) {
    uint32_t now_ms = millis();
    if (now_ms - last_ble_scan_ms > 15000 && !ble_scan_active) {
      ble_scan_active = true;
      pBLEScan->start(3, &ble_scan_complete_cb, false); // 3-second non-blocking scan
      last_ble_scan_ms = now_ms;
    }
  }

  // PPS calculation
  uint32_t now = millis();
  if (now - _pps_last_ms >= 1000) {
    pkt_per_sec  = _pps_count;
    _pps_count   = 0;
    _pps_last_ms = now;
  }

  // ── Active Payload Execution ──
  if (active_attack_type != ATTACK_NONE) {
    uint32_t now_ms = millis();
    if (now_ms - last_attack_frame_ms >= 100) { // Send every 100ms
      last_attack_frame_ms = now_ms;
      if (active_attack_type == ATTACK_RICK_ROLL) {
        // Broadcast next line of lyrics on channel 1-13
        const char* lyric = RICK_ROLL_LYRICS[rick_roll_index];
        send_beacon_frame(lyric, (rick_roll_index % 13) + 1);
        rick_roll_index = (rick_roll_index + 1) % RICK_ROLL_LYRICS_COUNT;
      }
      else if (active_attack_type == ATTACK_BEACON_SPAM) {
        // Flood randomly generated SSID names
        char temp_ssid[32];
        snprintf(temp_ssid, sizeof(temp_ssid), "Free Wi-Fi %04X", (unsigned int)random(0, 65536));
        send_beacon_frame(temp_ssid, (uint8_t)random(1, 14));
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(CHANNEL_HOP_MS));
}

void security_core1_task() {
  if (!_stft) return;

  // Handle web clients
  if (_webserver) _webserver->handleClient();

  // Handle serial commands from companion app (routed globally now)

  // Stream async events
  sec_stream_events();

  // Animate radar sweep
  uint32_t now = millis();
  if (now - _sweep_last_ms > 40) {
    _sweep_angle += sec_scanning ? 6.0f : 1.0f;
    if (_sweep_angle >= 360.0f) _sweep_angle -= 360.0f;
    _sweep_last_ms = now;
  }

  sec_draw_display();
  vTaskDelay(pdMS_TO_TICKS(100));
}

// ================================================================
//  DEVKITC — ORIGINAL SECURITY INTEGRATION WRAPPER
// ================================================================
#elif defined(HW_DEVKITC)

static void security_handle_command(String line) {
  // Legacy / DevKitC target command handler - no-op
}

extern void security_engine_setup(TFT_eSPI* tft) __attribute__((weak));
extern void security_engine_loop() __attribute__((weak));

void security_engine_setup(TFT_eSPI* tft) {
  if (tft) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(2);
    tft->setCursor(10, 100);
    tft->print("Security files");
    tft->setCursor(10, 130);
    tft->print("not found yet.");
  }
}
void security_engine_loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void security_setup(TFT_eSPI* tft) {
  _stft = tft;
  security_engine_setup(tft);
}

void security_teardown() {
  ESP.restart();
}

void security_core0_task() {
  vTaskDelay(pdMS_TO_TICKS(500));
}

void security_core1_task() {
  security_engine_loop();
  taskYIELD();
}

#endif // HW_DESKBUDDY_2 / HW_DEVKITC

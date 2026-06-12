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
      std::string mData = advertisedDevice.getManufacturerData();
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
  String html = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AeroSniffer // SOC</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#080b11;color:#f1f5f9;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;padding:20px;line-height:1.5}
header{text-align:center;margin-bottom:24px;position:relative}
h1{color:#00ffcc;font-size:1.8em;font-weight:700;letter-spacing:1px;display:flex;align-items:center;justify-content:center;gap:8px}
.status-dot{width:10px;height:10px;border-radius:50%;background:#ef4444;display:inline-block;animation:blink 1s infinite alternate}
.status-dot.active{background:#00ffcc}
@keyframes blink{from{opacity:0.3}to{opacity:1}}
.nav-tabs{display:flex;overflow-x:auto;gap:8px;margin-bottom:20px;border-bottom:1px solid rgba(255,255,255,0.1);padding-bottom:8px}
.tab-btn{background:transparent;border:none;color:#94a3b8;padding:8px 16px;font-size:0.95em;cursor:pointer;border-radius:6px;transition:all .2s;white-space:nowrap;font-family:inherit}
.tab-btn:hover{color:#f1f5f9;background:rgba(255,255,255,0.05)}
.tab-btn.active{color:#00ffcc;background:rgba(0,255,204,0.1);font-weight:bold}
.tab-btn.ble-tab.active{color:#a855f7;background:rgba(168,85,247,0.1)}
.tab-content{display:none}
.tab-content.active{display:block}
.card{background:rgba(20,27,45,0.75);border:1px solid rgba(30,58,95,0.5);border-radius:12px;padding:18px;margin-bottom:16px;backdrop-filter:blur(10px)}
.card-title{color:#94a3b8;font-size:0.9em;text-transform:uppercase;letter-spacing:1px;margin-bottom:12px;font-weight:bold}
.stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px}
.stat-card{background:rgba(255,255,255,0.02);border:1px solid rgba(255,255,255,0.05);border-radius:8px;padding:12px;display:flex;flex-direction:column}
.stat-label{font-size:0.8em;color:#64748b}
.stat-value{font-size:1.4em;font-weight:bold;color:#f1f5f9;margin-top:4px;font-family:monospace}
.stat-value.cyan{color:#00ffcc}
.stat-value.purple{color:#a855f7}
.stat-value.red{color:#ef4444}
.btn{display:inline-block;padding:10px 18px;background:transparent;border:1px solid #00ffcc;color:#00ffcc;font-size:0.9em;font-weight:bold;border-radius:6px;cursor:pointer;transition:all .2s;text-align:center;font-family:inherit}
.btn:hover{background:rgba(0,255,204,0.1)}
.btn.danger{border-color:#ef4444;color:#ef4444}
.btn.danger:hover{background:rgba(239,68,68,0.1)}
.btn.action-btn{padding:4px 8px;font-size:0.8em}
.btn-group{display:flex;gap:8px;margin-top:10px}
.alarm-banner{display:none;background:rgba(239,68,68,0.15);border:2px solid #ef4444;border-radius:12px;padding:16px;text-align:center;margin-bottom:16px;animation:pulse 1.5s infinite alternate}
@keyframes pulse{from{border-color:#ef4444;box-shadow:0 0 5px rgba(239,68,68,0.3)}to{border-color:#f87171;box-shadow:0 0 15px rgba(239,68,68,0.6)}}
.alarm-title{color:#ef4444;font-size:1.3em;font-weight:bold;margin-bottom:6px}
table{width:100%;border-collapse:collapse;margin-top:8px;font-size:0.9em;text-align:left}
th{color:#64748b;font-weight:bold;padding:8px;border-bottom:1px solid rgba(255,255,255,0.1)}
td{padding:8px;border-bottom:1px solid rgba(255,255,255,0.05);color:#e2e8f0;font-family:monospace}
tr:hover td{background:rgba(255,255,255,0.02)}
.scrollable-table{max-height:250px;overflow-y:auto}
footer{text-align:center;color:#475569;font-size:0.8em;margin-top:30px}
</style></head><body>
<header>
  <h1>🛡️ AeroSniffer // SOC</h1>
  <div style="font-size:0.85em;color:#64748b;margin-top:4px">
    Device Status: <span id="device-status-lbl">Idle</span>
    <span class="status-dot" id="status-indicator"></span>
  </div>
</header>
<div class="alarm-banner" id="evil-twin-banner">
  <div class="alarm-title">⚠️ ROGUE ACCESS POINT DETECTED!</div>
  <p id="alarm-details" style="font-size:0.9em;margin-bottom:12px;white-space:pre-line"></p>
  <button class="btn danger action-btn" onclick="clearEvilTwinAlarm()">Dismiss / Reset Alarm</button>
</div>
<div class="nav-tabs">
  <button class="tab-btn active" onclick="switchTab('tab-dashboard')">Dashboard</button>
  <button class="tab-btn" onclick="switchTab('tab-twin')">Evil Twin Guard</button>
  <button class="tab-btn" onclick="switchTab('tab-probes')">Probe Monitor</button>
  <button class="tab-btn ble-tab" onclick="switchTab('tab-ble')">BLE Tracker Guard</button>
</div>
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
<div class="tab-content" id="tab-twin">
  <div class="card">
    <div class="card-title">Active Protection Whitelist</div>
    <div class="stat-grid" style="margin-bottom:12px">
      <div class="stat-card"><span class="stat-label">Protected SSID</span><span class="stat-value cyan" id="protected-ssid-lbl">None</span></div>
      <div class="stat-card"><span class="stat-label">Trusted BSSID</span><span class="stat-value" id="protected-bssid-lbl">00:00:00:00:00:00</span></div>
    </div>
    <p style="font-size:0.8em;color:#64748b">Select any detected Access Point from the list below to set it as your trusted protected network.</p>
  </div>
  <div class="card">
    <div class="card-title">Detected Access Points</div>
    <div class="scrollable-table">
      <table>
        <thead>
          <tr>
            <th>SSID</th>
            <th>BSSID</th>
            <th>Ch</th>
            <th>RSSI</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody id="ap-list-body">
          <tr><td colspan="5" style="text-align:center;color:#64748b">No networks detected yet. Start scanning.</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>
<div class="tab-content" id="tab-probes">
  <div class="card">
    <div class="card-title">Probe Request Leaks (Nearby Devices)</div>
    <p style="font-size:0.8em;color:#64748b;margin-bottom:12px">Shows nearby devices (phones, tablets) broadcasting SSIDs of networks they have previously connected to.</p>
    <div class="scrollable-table">
      <table>
        <thead>
          <tr>
            <th>Client MAC</th>
            <th>Leaked Network SSID</th>
            <th>Last Seen</th>
          </tr>
        </thead>
        <tbody id="probe-list-body">
          <tr><td colspan="3" style="text-align:center;color:#64748b">No probe requests captured yet.</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>
<div class="tab-content" id="tab-ble">
  <div class="card" style="border-color:rgba(168,85,247,0.4)">
    <div class="card-title" style="color:#a855f7">BLE Beacon Tracking</div>
    <p style="font-size:0.8em;color:#64748b;margin-bottom:12px">Scans for proximity signals matching commercial stalker tags (AirTags, Tiles, Samsung SmartTags).</p>
    <div class="scrollable-table">
      <table>
        <thead>
          <tr>
            <th>Tracker Type</th>
            <th>Bluetooth Address</th>
            <th>Last RSSI</th>
            <th>Last Seen</th>
            <th>Hits</th>
          </tr>
        </thead>
        <tbody id="ble-list-body">
          <tr><td colspan="5" style="text-align:center;color:#64748b">No tracker beacons detected yet.</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>
<footer>AeroSniffer v2.0 | DeskBuddy 2.0 SOC</footer>
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
        html='<tr><td colspan="5" style="text-align:center;color:#64748b">No networks detected yet.</td></tr>';
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
        html='<tr><td colspan="3" style="text-align:center;color:#64748b">No probe requests captured.</td></tr>';
      }else{
        d.probes.forEach(pr=>{
          html+=`<tr>
            <td>${pr.mac}</td>
            <td style="color:#00ffcc">${pr.ssid}</td>
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
        html='<tr><td colspan="5" style="text-align:center;color:#64748b">No tracker beacons detected.</td></tr>';
      }else{
        d.trackers.forEach(tr=>{
          html+=`<tr>
            <td style="color:#a855f7;font-weight:bold">${tr.type}</td>
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
    document.getElementById('device-status-lbl').innerText=d.scanning?'Scanning':'Idle';
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
</script></body></html>)rawliteral";
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
  pBLEScan = BLEDevice::getBLEScan();
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

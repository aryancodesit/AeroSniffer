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
      case 0x04: pkt_probe++;   break;  // Probe request
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
static void sec_handle_serial() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
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
<title>AeroSniffer Security</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0e17;color:#e0e0e0;font-family:'Courier New',monospace;padding:16px}
h1{color:#00ffcc;font-size:1.5em;margin-bottom:12px;text-align:center}
.card{background:#141b2d;border:1px solid #1e3a5f;border-radius:8px;padding:14px;margin:10px 0}
.stat{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #1e2a3f}
.stat:last-child{border:none}
.label{color:#6b8aad}.value{color:#00ffcc;font-weight:bold}
.alert{color:#ff4444;font-weight:bold}
.btn{display:block;width:100%;padding:12px;margin:8px 0;border:1px solid #00ffcc;
background:transparent;color:#00ffcc;font-family:inherit;font-size:1em;
border-radius:6px;cursor:pointer;text-align:center}
.btn:hover{background:#00ffcc22}.btn.stop{border-color:#ff4444;color:#ff4444}
.btn.stop:hover{background:#ff444422}
footer{text-align:center;color:#3a4a6a;margin-top:20px;font-size:0.8em}
</style></head><body>
<h1>🛡 AeroSniffer // Security</h1>
<div class="card" id="stats"></div>
<div class="card">
<button class="btn" onclick="fetch('/api/scan/start')">▶ Start Scan</button>
<button class="btn stop" onclick="fetch('/api/scan/stop')">■ Stop Scan</button>
</div>
<footer>AeroSniffer v2.0 | DeskBuddy 2.0</footer>
<script>
setInterval(()=>{
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    document.getElementById('stats').innerHTML=
    `<div class="stat"><span class="label">Status</span><span class="value">${d.scanning?'SCANNING':'IDLE'}</span></div>
     <div class="stat"><span class="label">Channel</span><span class="value">${d.ch}</span></div>
     <div class="stat"><span class="label">PKT/s</span><span class="value">${d.pps}</span></div>
     <div class="stat"><span class="label">Total Packets</span><span class="value">${d.total}</span></div>
     <div class="stat"><span class="label">Beacons</span><span class="value">${d.beacons}</span></div>
     <div class="stat"><span class="label">Probes</span><span class="value">${d.probes}</span></div>
     <div class="stat"><span class="label">Deauths</span><span class="${d.deauths>0?'alert':'value'}">${d.deauths}</span></div>`;
  });
},1000);
</script></body></html>)rawliteral";
  _webserver->send(200, "text/html", html);
}

static void sec_handle_stats() {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"scanning\":%s,\"ch\":%d,\"pps\":%lu,\"total\":%lu,"
    "\"beacons\":%lu,\"probes\":%lu,\"deauths\":%lu}",
    sec_scanning ? "true" : "false", current_ch,
    pkt_per_sec, pkt_total, pkt_beacon, pkt_probe, pkt_deauth);
  _webserver->send(200, "application/json", json);
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
void security_setup(TFT_eSPI* tft) {
  _stft = tft;
  pkt_total = pkt_deauth = pkt_beacon = pkt_probe = 0;
  pkt_per_sec = 0;
  _pps_count = 0;
  _pps_last_ms = millis();
  sec_scanning = false;
  _sweep_angle = 0;

  // Start WiFi in AP mode for web control
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SEC_AP_SSID, SEC_AP_PASS);
  Serial.printf("[SEC] AP started: %s / %s\n", SEC_AP_SSID, SEC_AP_PASS);
  Serial.printf("[SEC] Web UI: http://%s\n", WiFi.softAPIP().toString().c_str());

  // Set up promiscuous callback
  esp_wifi_set_promiscuous_rx_cb(sec_sniffer_cb);

  // Start web server
  _webserver = new WebServer(SEC_WEB_PORT);
  _webserver->on("/", sec_handle_root);
  _webserver->on("/api/stats", sec_handle_stats);
  _webserver->on("/api/scan/start", sec_handle_scan_start);
  _webserver->on("/api/scan/stop", sec_handle_scan_stop);
  _webserver->begin();

  sec_draw_display();
}

void security_teardown() {
  if (sec_scanning) {
    esp_wifi_set_promiscuous(false);
    sec_scanning = false;
  }
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

  // Handle serial commands from companion app
  sec_handle_serial();

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

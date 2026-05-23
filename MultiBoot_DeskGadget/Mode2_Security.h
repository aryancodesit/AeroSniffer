// ================================================================
//  Mode2_Security.h  —  Tactical Wireless Security Monitor
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Features:
//    • L2 promiscuous 802.11 packet capture (Probe / Beacon / EAPOL / Deauth)
//    • Automatic 13-channel hopping with dwell timer
//    • Real-time packets-per-second rolling bar graph
//    • AP table with Evil Twin SSID collision detection
//    • Deauth spike alerting (>10 frames in 5 s window)
//    • Split-screen layout: header | left menu | right graph
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Config.h"

static TFT_eSPI* _stft = nullptr;

// ================================================================
//  PACKET STATISTICS  (written in ISR — use volatile)
// ================================================================
static volatile uint32_t s_probe    = 0;
static volatile uint32_t s_beacon   = 0;
static volatile uint32_t s_eapol    = 0;
static volatile uint32_t s_deauth   = 0;
static volatile uint32_t s_total    = 0;
static volatile uint32_t s_pps_acc  = 0;   // accumulator, reset every second
static volatile uint32_t s_pps      = 0;   // last measured PPS

// ================================================================
//  AP TABLE  (populated from beacon frames)
// ================================================================
struct APRecord {
  char    ssid[33];
  uint8_t bssid[6];
  int8_t  rssi;
  uint8_t channel;
  bool    valid;
};
static APRecord ap_table[MAX_AP_TABLE];
static int      ap_count = 0;

// ================================================================
//  THREAT FLAGS
// ================================================================
static bool     threat_evil_twin  = false;
static bool     threat_deauth     = false;
static uint32_t deauth_win_count  = 0;
static uint32_t deauth_win_start  = 0;

// ================================================================
//  CHANNEL HOP STATE
// ================================================================
static uint8_t  hop_channel = 1;

// ================================================================
//  PPS HISTORY GRAPH
// ================================================================
#define GRAPH_X  124
#define GRAPH_Y   20
#define GRAPH_W  112
#define GRAPH_H   90

static uint16_t pps_hist[GRAPH_W] = {0};
static int      pps_idx = 0;

// ================================================================
//  802.11 FRAME STRUCTURES
// ================================================================
typedef struct {
  uint8_t  fc[2];       // Frame Control
  uint16_t duration;
  uint8_t  addr1[6];    // Receiver / Destination
  uint8_t  addr2[6];    // Transmitter / Source
  uint8_t  addr3[6];    // BSSID / Filtering
  uint16_t seq_ctrl;
} __attribute__((packed)) dot11_hdr_t;

// ================================================================
//  PROMISCUOUS RX CALLBACK  (runs on Core 0, keep it lean)
// ================================================================
static void IRAM_ATTR sec_pkt_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  auto* ppkt = (wifi_promiscuous_pkt_t*)buf;
  auto* hdr  = (dot11_hdr_t*)ppkt->payload;

  uint8_t frame_type    = (hdr->fc[0] & 0x0C) >> 2;
  uint8_t frame_subtype = (hdr->fc[0] & 0xF0) >> 4;

  s_total++;
  s_pps_acc++;

  // ── Management frames ──────────────────────────────────────
  if (frame_type == 0x00) {

    if (frame_subtype == 0x04) {          // Probe Request
      s_probe++;

    } else if (frame_subtype == 0x08) {   // Beacon
      s_beacon++;

      // Parse SSID Information Element
      uint8_t* body = ppkt->payload + sizeof(dot11_hdr_t) + 12; // skip fixed params
      if (body[0] == 0x00) {                                     // SSID tag ID
        int ssid_len = min((int)body[1], 32);
        if (ssid_len > 0) {
          char ssid[33];
          memcpy(ssid, body + 2, ssid_len);
          ssid[ssid_len] = '\0';

          bool found = false;
          for (int i = 0; i < ap_count; i++) {
            if (strcmp(ap_table[i].ssid, ssid) == 0) {
              // Same SSID — check BSSID
              if (memcmp(ap_table[i].bssid, hdr->addr3, 6) != 0) {
                threat_evil_twin = true;   // Two BSSIDs broadcasting same SSID
              }
              ap_table[i].rssi = ppkt->rx_ctrl.rssi;
              found = true;
              break;
            }
          }
          if (!found && ap_count < MAX_AP_TABLE) {
            strcpy(ap_table[ap_count].ssid, ssid);
            memcpy(ap_table[ap_count].bssid, hdr->addr3, 6);
            ap_table[ap_count].rssi    = ppkt->rx_ctrl.rssi;
            ap_table[ap_count].channel = ppkt->rx_ctrl.channel;
            ap_table[ap_count].valid   = true;
            ap_count++;
          }
        }
      }

    } else if (frame_subtype == 0x0C) {   // Deauthentication
      s_deauth++;
      deauth_win_count++;
      uint32_t now_ms = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
      if (now_ms - deauth_win_start > 5000) {
        deauth_win_count = 1;
        deauth_win_start = now_ms;
      }
      if (deauth_win_count >= DEAUTH_SPIKE_COUNT) {
        threat_deauth = true;
      }
    }
  }

  // ── Data frames — EAPOL detection ──────────────────────────
  else if (frame_type == 0x02 && type == WIFI_PKT_DATA) {
    uint8_t* llc = ppkt->payload + sizeof(dot11_hdr_t);
    // LLC SNAP header: 0xAA 0xAA 0x03 0x00 0x00 0x00 then EtherType
    if (llc[6] == 0x88 && llc[7] == 0x8E) {
      s_eapol++;
    }
  }
}

// ================================================================
//  UI DRAWING
// ================================================================

// Top header bar (channel / total packets / deauth count)
static void sec_draw_header() {
  _stft->fillRect(0, 0, TFT_W, 18, 0x000F);
  _stft->setTextColor(TFT_YELLOW);
  _stft->setTextSize(1);
  _stft->setCursor(2, 5);
  _stft->printf("CH:%02d | PKT:%7lu | DA:%4lu", hop_channel, s_total, s_deauth);
}

// Left panel: menu + stats + alerts
static void sec_draw_left_panel() {
  const char* items[] = {
    "Probe Scan",
    "Beacon Scan",
    "EAPOL Watch",
    "Chan-Hop",
    "AP Table",
    "Threat Log"
  };
  const int ITEMS  = 6;
  const int ITEM_H = 16;
  int px = 0, py = 20;

  _stft->fillRect(px, py, 120, ITEMS * ITEM_H + 2, 0x1082);

  for (int i = 0; i < ITEMS; i++) {
    int iy = py + i * ITEM_H;
    // Highlight active-looking entry based on live data
    bool active = (i == 0 && s_probe > 0)  || (i == 1 && s_beacon > 0) ||
                  (i == 2 && s_eapol > 0)  || (i == 3)                  ||
                  (i == 4 && ap_count > 0) || (i == 5 && (threat_deauth || threat_evil_twin));
    if (active) {
      _stft->fillRect(px, iy, 120, ITEM_H, 0x0340);
      _stft->setTextColor(TFT_WHITE);
    } else {
      _stft->setTextColor(0x4208);
    }
    _stft->setTextSize(1);
    _stft->setCursor(px + 4, iy + 4);
    _stft->print(items[i]);
  }

  // Live counters below menu
  int sy = py + ITEMS * ITEM_H + 6;
  _stft->setTextColor(TFT_CYAN);
  _stft->setTextSize(1);
  _stft->setCursor(2, sy);
  _stft->printf("PRB %-6lu\nBCN %-6lu\nEAP %-6lu", s_probe, s_beacon, s_eapol);

  // Threat banners
  sy += 36;
  if (threat_deauth) {
    _stft->fillRect(0, sy, 120, 10, TFT_RED);
    _stft->setTextColor(TFT_WHITE);
    _stft->setCursor(2, sy + 1);
    _stft->print("!! DEAUTH SPIKE !!");
    sy += 12;
  }
  if (threat_evil_twin) {
    _stft->fillRect(0, sy, 120, 10, 0xFBE0);
    _stft->setTextColor(TFT_BLACK);
    _stft->setCursor(2, sy + 1);
    _stft->print("!! EVIL TWIN AP !!");
    sy += 12;
  }

  // Last seen AP snippet
  if (ap_count > 0) {
    int last = ap_count - 1;
    _stft->setTextColor(0x6B4D);
    _stft->setTextSize(1);
    _stft->setCursor(2, TFT_H - 24);
    _stft->printf("%.16s", ap_table[last].ssid);
    _stft->setCursor(2, TFT_H - 14);
    _stft->printf("RSSI:%4d  APs:%d", ap_table[last].rssi, ap_count);
  }
}

// Right panel: rolling PPS bar graph
static void sec_draw_graph() {
  _stft->fillRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_BLACK);
  _stft->drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, 0x07E0);

  // Title
  _stft->setTextColor(TFT_GREEN);
  _stft->setTextSize(1);
  _stft->setCursor(GRAPH_X + 2, GRAPH_Y + 2);
  _stft->printf("PPS:%5lu", s_pps);

  // Find max for normalisation
  uint16_t mx = 1;
  for (int i = 0; i < GRAPH_W; i++) if (pps_hist[i] > mx) mx = pps_hist[i];

  int inner_h = GRAPH_H - 14;
  for (int i = 0; i < GRAPH_W; i++) {
    int idx  = (pps_idx + i) % GRAPH_W;
    int barH = (int)((long)pps_hist[idx] * inner_h / mx);
    if (barH > 0) {
      uint16_t col = (pps_hist[idx] > mx * 0.7) ? TFT_RED
                   : (pps_hist[idx] > mx * 0.4) ? TFT_YELLOW
                   : TFT_GREEN;
      _stft->drawFastVLine(GRAPH_X + i, GRAPH_Y + GRAPH_H - barH - 1, barH, col);
    }
  }

  // Separator line between panels
  _stft->drawFastVLine(GRAPH_X - 2, GRAPH_Y, GRAPH_H, 0x2104);
}

// ================================================================
//  PUBLIC API
// ================================================================

void security_setup(TFT_eSPI* tft) {
  _stft = tft;

  // Reset all counters
  s_probe = s_beacon = s_eapol = s_deauth = s_total = s_pps_acc = s_pps = 0;
  ap_count = 0;
  threat_evil_twin = threat_deauth = false;
  deauth_win_count = 0;
  deauth_win_start = millis();
  memset(pps_hist, 0, sizeof(pps_hist));
  pps_idx = 0;
  hop_channel = 1;

  // Initialise Wi-Fi in NULL mode (no STA, no AP) then enable promiscuous
  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(sec_pkt_cb);
  esp_wifi_set_channel(hop_channel, WIFI_SECOND_CHAN_NONE);

  _stft->fillScreen(TFT_BLACK);
  sec_draw_header();
  sec_draw_left_panel();
  sec_draw_graph();
}

void security_teardown() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_stop();
  esp_wifi_deinit();
  _stft = nullptr;
}

// ── Core 0: channel hop + PPS accounting ────────────────────────
void security_core0_task() {
  static uint32_t last_hop = 0;
  static uint32_t last_sec = 0;
  uint32_t now = millis();

  // Channel hop
  if (now - last_hop > CHANNEL_HOP_MS) {
    last_hop    = now;
    hop_channel = (hop_channel % 13) + 1;
    esp_wifi_set_channel(hop_channel, WIFI_SECOND_CHAN_NONE);
  }

  // PPS sample every second
  if (now - last_sec > 1000) {
    last_sec        = now;
    s_pps           = s_pps_acc;
    s_pps_acc       = 0;
    pps_hist[pps_idx] = (uint16_t)min((uint32_t)65535, s_pps);
    pps_idx           = (pps_idx + 1) % GRAPH_W;
  }

  vTaskDelay(pdMS_TO_TICKS(10));
}

// ── Core 1: redraw UI every 250 ms ──────────────────────────────
void security_core1_task() {
  if (!_stft) return;
  sec_draw_header();
  sec_draw_left_panel();
  sec_draw_graph();
  vTaskDelay(pdMS_TO_TICKS(250));
}

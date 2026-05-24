// ================================================================
//  Mode3_Aviation.h  —  ADS-B Flight Radar (Aviation HUD Tracker)
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Features:
//    • Connects to home Wi-Fi in STA mode
//    • Polls OpenSky Network REST API every 15 s (rate-limit safe)
//    • Parses JSON state vectors: callsign, altitude, speed, heading
//    • Auto-scrolling flight cards with heading compass rose
//    • Mini radar dot-map scaled to your bounding box
//    • Graceful no-flight / no-WiFi fallback messages
//
//  Display layout adapts to TFT_H (240 square or 320 portrait).
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"

static TFT_eSPI* _atft = nullptr;

// ================================================================
//  FLIGHT DATA STRUCTURES
// ================================================================
struct FlightRecord {
  char    callsign[10];
  float   lat, lon;
  float   alt_m;       // Barometric altitude (metres)
  float   vel_ms;      // Ground speed (m/s)
  float   heading;     // True track (degrees)
  int8_t  on_ground;
  bool    valid;
};

static FlightRecord flights[MAX_FLIGHTS];
static int          flight_count  = 0;
static int          disp_page     = 0;    // Active flight card index
static bool         wifi_ok       = false;
static bool         fetching      = false;
static uint32_t     last_fetch    = 0;
static uint32_t     last_scroll   = 0;
static char         status_msg[48] = "Initialising...";

// ================================================================
//  UTILITY HELPERS
// ================================================================

// Convert heading degrees → short compass label
static const char* heading_label(float h) {
  if (h <  22.5f || h >= 337.5f) return "N";
  if (h <  67.5f)                return "NE";
  if (h < 112.5f)                return "E";
  if (h < 157.5f)                return "SE";
  if (h < 202.5f)                return "S";
  if (h < 247.5f)                return "SW";
  if (h < 292.5f)                return "W";
  return "NW";
}

// Draw a compass rose at (cx,cy) radius r showing heading h (degrees)
static void draw_compass(int cx, int cy, int r, float h) {
  _atft->drawCircle(cx, cy, r, 0x4208);
  _atft->drawCircle(cx, cy, r - 1, 0x2104);

  // Cardinal labels
  const char* cards[] = {"N","E","S","W"};
  int angles[]        = {0, 90, 180, 270};
  for (int i = 0; i < 4; i++) {
    float rad = (angles[i] - 90) * DEG_TO_RAD;
    int tx = cx + (int)((r - 7) * cosf(rad)) - 3;
    int ty = cy + (int)((r - 7) * sinf(rad)) - 3;
    _atft->setTextColor(0x4208);
    _atft->setTextSize(1);
    _atft->setCursor(tx, ty);
    _atft->print(cards[i]);
  }

  // Heading arrow
  float hr = (h - 90.0f) * DEG_TO_RAD;
  int ax = cx + (int)((r - 10) * cosf(hr));
  int ay = cy + (int)((r - 10) * sinf(hr));
  _atft->drawLine(cx, cy, ax, ay, TFT_YELLOW);
  _atft->fillCircle(ax, ay, 3, TFT_YELLOW);
  _atft->fillCircle(cx, cy, 3, TFT_WHITE);
}

// ================================================================
//  MINI RADAR MAP
// ================================================================
static void avi_draw_radar(int x, int y, int r) {
  _atft->fillCircle(x, y, r, 0x0841);
  _atft->drawCircle(x, y, r,     0x0340);
  _atft->drawCircle(x, y, r / 2, 0x0220);
  _atft->drawFastHLine(x - r, y, r * 2, 0x0220);
  _atft->drawFastVLine(x, y - r, r * 2, 0x0220);

  float c_lat = (sys_sky_lamin + sys_sky_lamax) / 2.0f;
  float c_lon = (sys_sky_lomin + sys_sky_lomax) / 2.0f;
  float span  = max(sys_sky_lamax - sys_sky_lamin, sys_sky_lomax - sys_sky_lomin);

  for (int i = 0; i < flight_count; i++) {
    if (!flights[i].valid) continue;
    float nx = (flights[i].lon - c_lon) / span;
    float ny = (c_lat - flights[i].lat) / span;
    int   px = x + (int)(nx * r * 1.8f);
    int   py = y + (int)(ny * r * 1.8f);
    if (abs(px - x) <= r && abs(py - y) <= r) {
      uint16_t col = (i == disp_page) ? TFT_YELLOW : TFT_GREEN;
      _atft->fillCircle(px, py, (i == disp_page) ? 4 : 2, col);
    }
  }
}

// ================================================================
//  FLIGHT CARD RENDERER — adapts to display height
// ================================================================
static void avi_draw_card(int idx) {
  if (!_atft) return;
  _atft->fillScreen(TFT_BLACK);

  // ── Layout constants based on display height ─────────────────
  #if TFT_H >= 300
    // 240×320 portrait — original spacious layout
    const int HDR_H     = 18;
    const int CS_Y      = 22;
    const int CS_SIZE   = 3;
    const int BADGE_Y   = 24;
    const int DIV1_Y    = 52;
    const int ROW_START = 58;
    const int ROW_STEP  = 26;
    const int COMP_R    = 36;
    const int FTR_H     = 12;
  #else
    // 240×240 square — compact layout
    const int HDR_H     = 14;
    const int CS_Y      = 16;
    const int CS_SIZE   = 2;
    const int BADGE_Y   = 18;
    const int DIV1_Y    = 38;
    const int ROW_START = 42;
    const int ROW_STEP  = 22;
    const int COMP_R    = 30;
    const int FTR_H     = 12;
  #endif

  // ── Header strip ─────────────────────────────────────────────
  _atft->fillRect(0, 0, TFT_W, HDR_H, 0x000F);
  _atft->setTextColor(0x07FF);
  _atft->setTextSize(1);
  _atft->setCursor(2, (HDR_H - 8) / 2);
  _atft->printf("FLIGHT RADAR  [%d/%d]",
                flight_count > 0 ? idx + 1 : 0, flight_count);

  if (flight_count == 0) {
    // ── Empty state ───────────────────────────────────────────
    _atft->setTextColor(TFT_YELLOW);
    _atft->setTextSize(1);
    _atft->setCursor(20, TFT_H / 2 - 20);
    _atft->print("No aircraft in radius.");
    _atft->setCursor(20, TFT_H / 2 - 6);
    _atft->print(status_msg);
    _atft->setTextColor(0x4208);
    _atft->setCursor(20, TFT_H / 2 + 10);
    _atft->printf("Box: %.1f-%.1f N  %.1f-%.1f E",
                  sys_sky_lamin, sys_sky_lamax, sys_sky_lomin, sys_sky_lomax);
    return;
  }

  FlightRecord& f = flights[idx];

  // ── Callsign ──────────────────────────────────────────────────
  _atft->setTextColor(TFT_WHITE);
  _atft->setTextSize(CS_SIZE);
  _atft->setCursor(6, CS_Y);
  _atft->print(f.callsign[0] ? f.callsign : "N/A----");

  // Status badge (AIRBORNE / GROUND)
  uint16_t badge_col = f.on_ground ? 0xF800 : 0x07E0;
  _atft->fillRoundRect(TFT_W - 72, BADGE_Y, 68, 14, 4, badge_col);
  _atft->setTextColor(TFT_BLACK);
  _atft->setTextSize(1);
  _atft->setCursor(TFT_W - 68, BADGE_Y + 3);
  _atft->print(f.on_ground ? "  ON GROUND" : "  AIRBORNE");

  // Divider
  _atft->drawFastHLine(0, DIV1_Y, TFT_W, 0x2104);

  // ── Data grid ────────────────────────────────────────────────
  int lx = 6, rx = TFT_W / 2 + 4;
  int r0 = ROW_START;
  int r1 = ROW_START + ROW_STEP;

  // Labels
  _atft->setTextColor(0x528A);
  _atft->setTextSize(1);
  _atft->setCursor(lx, r0);     _atft->print("ALTITUDE");
  _atft->setCursor(lx, r1);     _atft->print("GND SPEED");
  _atft->setCursor(rx, r0);     _atft->print("HEADING");
  _atft->setCursor(rx, r1);     _atft->print("POSITION");

  // Values
  _atft->setTextColor(TFT_YELLOW);
  _atft->setTextSize(2);
  _atft->setCursor(lx, r0 + 10);
  if (f.alt_m > 0)
    _atft->printf("%5.0f m", f.alt_m);
  else
    _atft->print(" N/A  ");

  _atft->setCursor(lx, r1 + 10);
  _atft->printf("%4.0f kn", f.vel_ms * 1.94384f);

  _atft->setCursor(rx, r0 + 10);
  _atft->printf("%3.0f%s", f.heading, heading_label(f.heading));

  _atft->setTextColor(0x07FF);
  _atft->setTextSize(1);
  _atft->setCursor(rx, r1 + 10);
  _atft->printf("%.3f N", f.lat);
  _atft->setCursor(rx, r1 + 20);
  _atft->printf("%.3f E", f.lon);

  // Divider above compass/radar
  int div2_y = r1 + ROW_STEP + 10;
  _atft->drawFastHLine(0, div2_y, TFT_W, 0x2104);
  _atft->drawFastVLine(TFT_W / 2, div2_y, TFT_H - div2_y - FTR_H, 0x2104);

  // ── Compass rose (left lower) ────────────────────────────────
  int widget_cy = div2_y + COMP_R + 6;
  draw_compass(TFT_W / 4, widget_cy, COMP_R, f.heading);

  // ── Radar map (right lower) ──────────────────────────────────
  avi_draw_radar(3 * TFT_W / 4, widget_cy, COMP_R);

  // ── Footer strip ─────────────────────────────────────────────
  _atft->fillRect(0, TFT_H - FTR_H, TFT_W, FTR_H, 0x0008);
  _atft->setTextColor(0x2CA0);
  _atft->setTextSize(1);
  _atft->setCursor(2, TFT_H - FTR_H + 2);
  uint32_t secs_to_next = (FETCH_INTERVAL_MS - min((uint32_t)(millis() - last_fetch), (uint32_t)FETCH_INTERVAL_MS)) / 1000;
  _atft->printf("WiFi:%s | Refresh:%lus %s",
                wifi_ok ? "OK" : "ERR", secs_to_next,
                fetching ? "..." : "");
}

// ================================================================
//  WIFI + API FETCH
// ================================================================

static void avi_connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(sys_wifi_ssid.c_str(), sys_wifi_pass.c_str());
  snprintf(status_msg, sizeof(status_msg), "Connecting to %s ...", sys_wifi_ssid.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 24) {
    vTaskDelay(pdMS_TO_TICKS(500));
    tries++;
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);
  snprintf(status_msg, sizeof(status_msg),
           wifi_ok ? "WiFi OK — %s" : "WiFi FAILED", sys_wifi_ssid.c_str());
}

static void avi_fetch_flights() {
  if (!wifi_ok) {
    avi_connect_wifi();
    if (!wifi_ok) return;
  }

  fetching = true;
  snprintf(status_msg, sizeof(status_msg), "Polling OpenSky...");

  char url[200];
  snprintf(url, sizeof(url),
    "http://opensky-network.org/api/states/all"
    "?lamin=%.2f&lomin=%.2f&lamax=%.2f&lomax=%.2f",
    (double)sys_sky_lamin, (double)sys_sky_lomin,
    (double)sys_sky_lamax, (double)sys_sky_lomax);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code != 200) {
    snprintf(status_msg, sizeof(status_msg), "HTTP error %d", code);
    http.end();
    fetching = false;
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  DynamicJsonDocument doc(20480);
  DeserializationError err = deserializeJson(doc, *stream);
  http.end();

  if (err) {
    snprintf(status_msg, sizeof(status_msg), "JSON err: %s", err.c_str());
    fetching = false;
    return;
  }

  JsonArray states = doc["states"].as<JsonArray>();
  flight_count     = 0;

  for (JsonArray sv : states) {
    if (flight_count >= MAX_FLIGHTS) break;

    FlightRecord& f = flights[flight_count];
    memset(&f, 0, sizeof(f));

    const char* cs = sv[1].as<const char*>();
    if (!cs || strlen(cs) == 0) continue;

    strncpy(f.callsign, cs, 9);
    f.callsign[9] = '\0';
    for (int k = (int)strlen(f.callsign) - 1; k >= 0 && f.callsign[k] == ' '; k--)
      f.callsign[k] = '\0';

    f.lon       = sv[5].isNull() ? 0.0f : sv[5].as<float>();
    f.lat       = sv[6].isNull() ? 0.0f : sv[6].as<float>();
    f.alt_m     = sv[7].isNull() ? 0.0f : sv[7].as<float>();
    f.on_ground = sv[8].as<bool>() ? 1 : 0;
    f.vel_ms    = sv[9].isNull()  ? 0.0f : sv[9].as<float>();
    f.heading   = sv[10].isNull() ? 0.0f : sv[10].as<float>();
    f.valid     = true;

    flight_count++;
  }

  snprintf(status_msg, sizeof(status_msg),
           "OK — %d aircraft found", flight_count);
  fetching = false;
}

// ================================================================
//  PUBLIC API
// ================================================================

void aviation_setup(TFT_eSPI* tft) {
  _atft        = tft;
  flight_count = 0;
  disp_page    = 0;
  wifi_ok      = false;
  fetching     = false;
  last_fetch   = 0;
  last_scroll  = millis();
  snprintf(status_msg, sizeof(status_msg), "Starting up...");

  _atft->fillScreen(TFT_BLACK);
  _atft->setTextColor(TFT_CYAN);
  _atft->setTextSize(2);
  int tx = (TFT_W - 144) / 2;   // Approx center "Flight Radar"
  _atft->setCursor(tx, TFT_H / 2 - 16);
  _atft->print("Flight Radar");
  _atft->setTextSize(1);
  _atft->setTextColor(TFT_WHITE);
  _atft->setCursor(tx, TFT_H / 2 + 6);
  _atft->print("Connecting to WiFi...");
}

void aviation_teardown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  _atft = nullptr;
}

// ── Core 0: asynchronous data fetching ──────────────────────────
void aviation_core0_task() {
  uint32_t now = millis();
  if (now - last_fetch > FETCH_INTERVAL_MS) {
    last_fetch = now;
    avi_fetch_flights();
  }
  vTaskDelay(pdMS_TO_TICKS(500));
}

// ── Core 1: display update + auto-scroll ────────────────────────
void aviation_core1_task() {
  if (!_atft) return;
  uint32_t now = millis();

  if (flight_count > 1 && now - last_scroll > CARD_SCROLL_MS) {
    last_scroll = now;
    disp_page   = (disp_page + 1) % flight_count;
  }

  avi_draw_card(disp_page);
  vTaskDelay(pdMS_TO_TICKS(500));
}

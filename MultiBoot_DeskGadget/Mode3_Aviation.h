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
  // Background
  _atft->fillCircle(x, y, r, 0x0841);
  // Rings
  _atft->drawCircle(x, y, r,     0x0340);
  _atft->drawCircle(x, y, r / 2, 0x0220);
  // Cross-hairs
  _atft->drawFastHLine(x - r, y, r * 2, 0x0220);
  _atft->drawFastVLine(x, y - r, r * 2, 0x0220);

  float c_lat = (SKY_LAMIN + SKY_LAMAX) / 2.0f;
  float c_lon = (SKY_LOMIN + SKY_LOMAX) / 2.0f;
  float span  = max(SKY_LAMAX - SKY_LAMIN, SKY_LOMAX - SKY_LOMIN);

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
//  FLIGHT CARD RENDERER
// ================================================================
static void avi_draw_card(int idx) {
  if (!_atft) return;
  _atft->fillScreen(TFT_BLACK);

  // ── Header strip ─────────────────────────────────────────────
  _atft->fillRect(0, 0, TFT_W, 18, 0x000F);
  _atft->setTextColor(0x07FF);
  _atft->setTextSize(1);
  _atft->setCursor(2, 5);
  _atft->printf("MODE 3: FLIGHT RADAR  [%d/%d]",
                flight_count > 0 ? idx + 1 : 0, flight_count);

  if (flight_count == 0) {
    // ── Empty state ───────────────────────────────────────────
    _atft->setTextColor(TFT_YELLOW);
    _atft->setTextSize(1);
    _atft->setCursor(20, 90);
    _atft->print("No aircraft in radius.");
    _atft->setCursor(20, 105);
    _atft->print(status_msg);
    _atft->setTextColor(0x4208);
    _atft->setCursor(20, 125);
    _atft->printf("Box: %.1f-%.1f N  %.1f-%.1f E",
                  SKY_LAMIN, SKY_LAMAX, SKY_LOMIN, SKY_LOMAX);
    return;
  }

  FlightRecord& f = flights[idx];

  // ── Callsign (large) ──────────────────────────────────────────
  _atft->setTextColor(TFT_WHITE);
  _atft->setTextSize(3);
  _atft->setCursor(6, 22);
  _atft->print(f.callsign[0] ? f.callsign : "N/A----");

  // Status badge (AIRBORNE / GROUND)
  uint16_t badge_col = f.on_ground ? 0xF800 : 0x07E0;
  _atft->fillRoundRect(TFT_W - 72, 24, 68, 14, 4, badge_col);
  _atft->setTextColor(TFT_BLACK);
  _atft->setTextSize(1);
  _atft->setCursor(TFT_W - 68, 28);
  _atft->print(f.on_ground ? "  ON GROUND" : "  AIRBORNE");

  // Divider
  _atft->drawFastHLine(0, 52, TFT_W, 0x2104);

  // ── Data grid — left column ──────────────────────────────────
  int lx = 6, rx = TFT_W / 2 + 4;
  int row[] = {58, 84, 110, 136};

  // Labels
  _atft->setTextColor(0x528A);
  _atft->setTextSize(1);
  _atft->setCursor(lx, row[0]);     _atft->print("ALTITUDE");
  _atft->setCursor(lx, row[1]);     _atft->print("GND SPEED");
  _atft->setCursor(rx, row[0]);     _atft->print("HEADING");
  _atft->setCursor(rx, row[1]);     _atft->print("POSITION");

  // Values
  _atft->setTextColor(TFT_YELLOW);
  _atft->setTextSize(2);
  _atft->setCursor(lx, row[0] + 10);
  if (f.alt_m > 0)
    _atft->printf("%5.0f m", f.alt_m);
  else
    _atft->print(" N/A  ");

  _atft->setCursor(lx, row[1] + 10);
  _atft->printf("%4.0f kn", f.vel_ms * 1.94384f);  // m/s → knots

  _atft->setCursor(rx, row[0] + 10);
  _atft->printf("%3.0f° %s", f.heading, heading_label(f.heading));

  _atft->setTextColor(0x07FF);
  _atft->setTextSize(1);
  _atft->setCursor(rx, row[1] + 10);
  _atft->printf("%.4f N", f.lat);
  _atft->setCursor(rx, row[1] + 22);
  _atft->printf("%.4f E", f.lon);

  // Divider
  _atft->drawFastHLine(0, row[2] + 4, TFT_W, 0x2104);
  _atft->drawFastVLine(TFT_W / 2, row[2] + 4, TFT_H - row[2] - 14, 0x2104);

  // ── Compass rose (left lower) ────────────────────────────────
  int comp_cx = TFT_W / 4, comp_cy = row[2] + 44;
  draw_compass(comp_cx, comp_cy, 36, f.heading);

  // ── Radar map (right lower) ──────────────────────────────────
  int rad_cx = 3 * TFT_W / 4, rad_cy = row[2] + 44;
  avi_draw_radar(rad_cx, rad_cy, 36);

  // ── Footer strip ─────────────────────────────────────────────
  _atft->fillRect(0, TFT_H - 12, TFT_W, 12, 0x0008);
  _atft->setTextColor(0x2CA0);
  _atft->setTextSize(1);
  _atft->setCursor(2, TFT_H - 10);
  uint32_t secs_to_next = (FETCH_INTERVAL_MS - min((uint32_t)(millis() - last_fetch), (uint32_t)FETCH_INTERVAL_MS)) / 1000;
  _atft->printf("WiFi:%s | Refresh in %lus | %s",
                wifi_ok ? "OK" : "ERR", secs_to_next,
                fetching ? "Fetching..." : "");
}

// ================================================================
//  WIFI + API FETCH
// ================================================================

static void avi_connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  snprintf(status_msg, sizeof(status_msg), "Connecting to %s ...", WIFI_SSID);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 24) {
    vTaskDelay(pdMS_TO_TICKS(500));
    tries++;
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);
  snprintf(status_msg, sizeof(status_msg),
           wifi_ok ? "WiFi OK — %s" : "WiFi FAILED", WIFI_SSID);
}

static void avi_fetch_flights() {
  if (!wifi_ok) {
    avi_connect_wifi();
    if (!wifi_ok) return;
  }

  fetching = true;
  snprintf(status_msg, sizeof(status_msg), "Polling OpenSky...");

  // Build URL at runtime (avoids macro-stringify issues with floats)
  char url[200];
  snprintf(url, sizeof(url),
    "http://opensky-network.org/api/states/all"
    "?lamin=%.2f&lomin=%.2f&lamax=%.2f&lomax=%.2f",
    (double)SKY_LAMIN, (double)SKY_LOMIN,
    (double)SKY_LAMAX, (double)SKY_LOMAX);

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

  // Use streaming JSON parser to keep RAM under control
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

    // sv[1] = callsign (string), sv[5]=lon, sv[6]=lat, sv[7]=baro_alt,
    // sv[8]=on_ground, sv[9]=velocity, sv[10]=true_track
    const char* cs = sv[1].as<const char*>();
    if (!cs || strlen(cs) == 0) continue;

    strncpy(f.callsign, cs, 9);
    f.callsign[9] = '\0';
    // Trim trailing spaces
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
  last_fetch   = 0;         // Trigger immediate fetch on first core0 call
  last_scroll  = millis();
  snprintf(status_msg, sizeof(status_msg), "Starting up...");

  _atft->fillScreen(TFT_BLACK);
  _atft->setTextColor(TFT_CYAN);
  _atft->setTextSize(2);
  _atft->setCursor(24, 90);
  _atft->print("Flight Radar");
  _atft->setTextSize(1);
  _atft->setTextColor(TFT_WHITE);
  _atft->setCursor(30, 115);
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

  // Auto-scroll through flight cards every CARD_SCROLL_MS
  if (flight_count > 1 && now - last_scroll > CARD_SCROLL_MS) {
    last_scroll = now;
    disp_page   = (disp_page + 1) % flight_count;
  }

  avi_draw_card(disp_page);
  vTaskDelay(pdMS_TO_TICKS(500));
}

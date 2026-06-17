// ================================================================
//  Aviation/Renderer.h  —  Aviation HUD TFT Renderer
//  AeroSniffer Aviation Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "FlightStore.h"
#include "Config.h"
#include "AeroSnifferOS.h"

static TFT_eSprite* _atft = nullptr;

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

static void draw_compass(int cx, int cy, int r, float h) {
  if (!_atft) return;
  _atft->drawCircle(cx, cy, r, 0x4208);
  _atft->drawCircle(cx, cy, r - 1, 0x2104);

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

  float hr = (h - 90.0f) * DEG_TO_RAD;
  int ax = cx + (int)((r - 10) * cosf(hr));
  int ay = cy + (int)((r - 10) * sinf(hr));
  _atft->drawLine(cx, cy, ax, ay, TFT_YELLOW);
  _atft->fillCircle(ax, ay, 3, TFT_YELLOW);
  _atft->fillCircle(cx, cy, 3, TFT_WHITE);
}

static void avi_draw_radar(int x, int y, int r) {
  if (!_atft) return;
  _atft->fillCircle(x, y, r, 0x0841);
  _atft->drawCircle(x, y, r,     0x0340);
  _atft->drawCircle(x, y, r / 2, 0x0220);
  _atft->drawFastHLine(x - r, y, r * 2, 0x0220);
  _atft->drawFastVLine(x, y - r, r * 2, 0x0220);

  float c_lat = (sys_sky_lamin + sys_sky_lamax) / 2.0f;
  float c_lon = (sys_sky_lomin + sys_sky_lomax) / 2.0f;
  float span  = std::max<float>(sys_sky_lamax - sys_sky_lamin, sys_sky_lomax - sys_sky_lomin);

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

static void avi_draw_card(int idx) {
  if (!_atft) return;
  Serial.printf("[AVI] Rendering %d aircraft\n", flight_count);
  _atft->fillScreen(TFT_BLACK);

  #if TFT_H >= 300
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

  _atft->fillRect(0, 0, TFT_W, HDR_H, 0x000F);
  _atft->setTextColor(0x07FF);
  _atft->setTextSize(1);
  _atft->setCursor(2, (HDR_H - 8) / 2);
  _atft->printf("FLIGHT RADAR  [%d/%d]",
                flight_count > 0 ? idx + 1 : 0, flight_count);

  if (flight_count == 0) {
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

  _atft->setTextColor(TFT_WHITE);
  _atft->setTextSize(CS_SIZE);
  _atft->setCursor(6, CS_Y);
  _atft->print(f.callsign[0] ? f.callsign : "N/A----");

  uint16_t badge_col = f.on_ground ? 0xF800 : 0x07E0;
  _atft->fillRoundRect(TFT_W - 72, BADGE_Y, 68, 14, 4, badge_col);
  _atft->setTextColor(TFT_BLACK);
  _atft->setTextSize(1);
  _atft->setCursor(TFT_W - 68, BADGE_Y + 3);
  _atft->print(f.on_ground ? "  ON GROUND" : "  AIRBORNE");

  _atft->drawFastHLine(0, DIV1_Y, TFT_W, 0x2104);

  int lx = 6, rx = TFT_W / 2 + 4;
  int r0 = ROW_START;
  int r1 = ROW_START + ROW_STEP;

  _atft->setTextColor(0x528A);
  _atft->setTextSize(1);
  _atft->setCursor(lx, r0);     _atft->print("ALTITUDE");
  _atft->setCursor(lx, r1);     _atft->print("GND SPEED");
  _atft->setCursor(rx, r0);     _atft->print("HEADING");
  _atft->setCursor(rx, r1);     _atft->print("POSITION");

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

  int div2_y = r1 + ROW_STEP + 10;
  _atft->drawFastHLine(0, div2_y, TFT_W, 0x2104);
  _atft->drawFastVLine(TFT_W / 2, div2_y, TFT_H - div2_y - FTR_H, 0x2104);

  int widget_cy = div2_y + COMP_R + 6;
  draw_compass(TFT_W / 4, widget_cy, COMP_R, f.heading);
  avi_draw_radar(3 * TFT_W / 4, widget_cy, COMP_R);

  _atft->fillRect(0, TFT_H - FTR_H, TFT_W, FTR_H, 0x0008);
  _atft->setTextColor(0x2CA0);
  _atft->setTextSize(1);
  _atft->setCursor(2, TFT_H - FTR_H + 2);
  uint32_t current_refresh = last_fetch_success ? sys_avi_refresh : std::max<uint32_t>(30000UL, sys_avi_refresh);
  uint32_t secs_to_next = (current_refresh - std::min<uint32_t>((uint32_t)(millis() - last_fetch), current_refresh)) / 1000;
  _atft->printf("WiFi:%s | Refresh:%lus %s",
                wifi_ok ? "OK" : "ERR", secs_to_next,
                fetching ? "..." : "");
}

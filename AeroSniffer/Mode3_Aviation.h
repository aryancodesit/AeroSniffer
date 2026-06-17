// ================================================================
//  Mode3_Aviation.h  —  ADS-B Flight Radar (Aviation HUD Tracker)
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "AeroSnifferOS.h"

// Include modular subcomponents
#include "Aviation/FlightStore.h"
#include "Aviation/Provider.h"
#include "Aviation/Parser.h"
#include "Aviation/Renderer.h"
#include "Aviation/Statistics.h"

static bool wifi_connecting = false;

static void avi_connect_wifi() {
  if (wifi_connecting) {
    Serial.println("[AVI] Already connecting, skipping.");
    return;
  }
  wifi_connecting = true;

  if (sys_wifi_ssid == "YOUR_WIFI_SSID" || sys_wifi_ssid.length() == 0) {
    wifi_ok = false;
    snprintf(status_msg, sizeof(status_msg), "No WiFi set! Use Setup in app.");
    Serial.println("[AVI] WiFi credentials not configured. Use companion app Setup > SET_WIFI.");
    wifi_connecting = false;
    return;
  }

  snprintf(status_msg, sizeof(status_msg), "Connecting to %s ...", sys_wifi_ssid.c_str());

  // Use WiFiService STA connection manager
  wifi_ok = WiFiService.connectSTA(sys_wifi_ssid.c_str(), sys_wifi_pass.c_str());

  Serial.printf("[AVI] Status final=%d  ok=%d\n", WiFiService.getStatus(), wifi_ok ? 1 : 0);
  snprintf(status_msg, sizeof(status_msg),
           wifi_ok ? "WiFi OK -- %s" : "WiFi FAILED: %s", sys_wifi_ssid.c_str());

  wifi_connecting = false;
}

static OpenSkyProvider opensky_provider;

static void avi_fetch_flights() {
  // Enforce WiFiService connectivity layer
  if (!WiFiService.isConnected()) {
    avi_connect_wifi();
    if (!wifi_ok) {
      last_fetch_success = false;
      return;
    }
  }

  fetching = true;
  snprintf(status_msg, sizeof(status_msg), "Polling OpenSky...");

  String payload = "";
  bool success = opensky_provider.fetchFlights(sys_sky_lamin, sys_sky_lomin, sys_sky_lamax, sys_sky_lomax, payload);
  last_fetch_success = success;

  if (success) {
    bool parse_success = parseFlightJSON(payload);
    if (parse_success) {
      snprintf(status_msg, sizeof(status_msg), "OK — %d aircraft found", flight_count);
      
      // Publish event to Emotion Engine
      EventBus.publish(EVENT_FLIGHT_DETECTED, &flight_count);
      
      // Check for rare military/fast flights to trigger rare event
      bool rare_detected = false;
      for (int i = 0; i < flight_count; i++) {
        if (flights[i].valid) {
          float speed_knots = flights[i].vel_ms * 1.94384f;
          if (speed_knots > 510.0f) { // Fast jet
            rare_detected = true;
            break;
          }
          if (strstr(flights[i].callsign, "MIL") || strstr(flights[i].callsign, "RCH") || strstr(flights[i].callsign, "RCAF")) {
            rare_detected = true;
            break;
          }
        }
      }
      if (rare_detected) {
        EventBus.publish(EVENT_FLIGHT_RARE);
      }
    } else {
      last_fetch_success = false;
    }
  }
  fetching = false;
}

// ── Public API ──────────────────────────────────────────────────
void aviation_setup(TFT_eSprite* tft) {
  _atft = tft;
  flight_count = 0;
  disp_page    = 0;
  wifi_ok      = WiFiService.isConnected();
  fetching     = false;
  last_scroll  = millis();
  snprintf(status_msg, sizeof(status_msg), "Starting up...");

  if (sys_aviation_enabled) {
    // Task 8: Startup Bounding Box Diagnostics
    Serial.printf(
      "[AVI] Box: %.2f %.2f %.2f %.2f\n",
      sys_sky_lamin, sys_sky_lomin, sys_sky_lamax, sys_sky_lomax
    );

    // Defer first fetch by 3 s
    last_fetch = millis() - sys_avi_refresh + 3000;
  }
}

void aviation_teardown() {
  _atft = nullptr;
}

void aviation_core0_task() {
  if (!sys_aviation_enabled) { vTaskDelay(pdMS_TO_TICKS(500)); return; }
  uint32_t now = millis();
  // Task 2 & Task 7: If last fetch failed, override to force at least 30s guard. Otherwise, use user rate.
  uint32_t current_refresh = last_fetch_success ? sys_avi_refresh : std::max<uint32_t>(30000UL, sys_avi_refresh);

  if (now - last_fetch > current_refresh) {
    last_fetch = now;
    g_block_touch = true;
    avi_fetch_flights();
    g_block_touch = false;
  }
  vTaskDelay(pdMS_TO_TICKS(500));
}

void aviation_core1_task() {
  if (!_atft) return;
  uint32_t now = millis();

  // Toggle HUD mode via tap
  extern volatile bool g_touch_tap;
  if (g_touch_tap) {
    g_touch_tap = false;
    avi_hud_mode = !avi_hud_mode;
    Serial.printf("[AVI] Toggle HUD mode: %d\n", avi_hud_mode);
    _atft->fillSprite(TFT_BLACK);
  }

  static int frame = 0;
  frame++;

  // Auto-scroll pages if in HUD mode
  if (avi_hud_mode && sys_aviation_enabled && flight_count > 1 && now - last_scroll > CARD_SCROLL_MS) {
    last_scroll = now;
    disp_page   = (disp_page + 1) % flight_count;
  }

  if (avi_hud_mode && sys_aviation_enabled) {
    avi_draw_card(disp_page);
  } else {
    // Render Character Face Mode
    _atft->fillSprite(TFT_BLACK);

    // Draw header bar
    _atft->fillRect(0, 0, 240, 40, 0x0010);
    _atft->setTextColor(0xFFE0); // yellow/orange
    _atft->setTextSize(1);
    _atft->setCursor(10, 8);
    _atft->print("AVIATION OBSERVER");

    _atft->setCursor(10, 24);
    if (!sys_aviation_enabled) {
      _atft->setTextColor(TFT_RED);
      _atft->print("RADAR DISABLED");
    } else if (flight_count > 0) {
      _atft->setTextColor(TFT_GREEN);
      _atft->printf("ACTIVE: %d IN RANGE", flight_count);
    } else {
      _atft->setTextColor(0x5AEB);
      _atft->print("STANDBY: NO FLIGHTS");
    }

    _atft->setTextColor(0x2CA0);
    _atft->setCursor(170, 15);
    if (sys_aviation_enabled && flight_count > 0) {
      _atft->printf("TRK %d/%d", disp_page + 1, flight_count);
    } else {
      _atft->print("OFFLINE");
    }
    _atft->drawFastHLine(0, 39, 240, 0x1c2b22);

    // Dynamic face & text selection
    char l1[48] = "";
    char l2[48] = "";
    if (!sys_aviation_enabled) {
      sys_current_face = "avi_disabled";
      EmotionEngine.setEmotion(EMOTION_SLEEPY);
      snprintf(l1, sizeof(l1), "Aviation Mode Disabled");
      snprintf(l2, sizeof(l2), "Enable in Setup Menu");
    } else if (flight_count == 0) {
      sys_current_face = "sleepy";
      EmotionEngine.setEmotion(EMOTION_SLEEPY);
      snprintf(l1, sizeof(l1), "No flights detected");
      snprintf(l2, sizeof(l2), "%s", status_msg);
    } else {
      // Find the tracked flight
      const FlightRecord* f = &flights[disp_page];
      if (f->valid) {
        snprintf(l1, sizeof(l1), "%s | Alt: %d ft", f->callsign, (int)(f->alt_m * 3.28084f));
        snprintf(l2, sizeof(l2), "Spd: %d kt | Hdg: %d", (int)(f->vel_ms * 1.94384f), f->heading);

        // Check rare jet/mil
        bool rare = false;
        float speed_knots = f->vel_ms * 1.94384f;
        if (speed_knots > 510.0f || strstr(f->callsign, "MIL") || strstr(f->callsign, "RCH") || strstr(f->callsign, "RCAF")) {
          rare = true;
        }

        if (rare) {
          sys_current_face = "surprised";
          EmotionEngine.setEmotion(EMOTION_SURPRISED);
        } else {
          // Locked target face
          sys_current_face = "avi_lock";
          EmotionEngine.setEmotion(EMOTION_EXCITED);
        }
      } else {
        sys_current_face = "avi_radar";
        EmotionEngine.setEmotion(EMOTION_CURIOUS);
        snprintf(l1, sizeof(l1), "Tracking active flights");
        snprintf(l2, sizeof(l2), "Flights: %d in zone", flight_count);
      }
    }

    FaceEngine.setStatusLines(l1, l2);
    FaceEngine.render(_atft, frame);
  }

  DisplayService.commit();
  vTaskDelay(pdMS_TO_TICKS(50));
}

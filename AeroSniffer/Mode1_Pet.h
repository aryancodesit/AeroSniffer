// ================================================================
//  Mode1_Pet.h  —  Vector-style Robot Desk Companion
//  AeroSniffer v2 | XIAO ESP32S3 + ST7789 240×240
//
//  Decoupled to use FaceEngineClass & EmotionEngineClass central state.
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Config.h"

static TFT_eSprite* _rtft = nullptr;
static uint32_t last_serial_rx_ms = 0;
static uint32_t last_standalone_weather_fetch = 0;

static void pet_handle_command(String line) {
  line.trim();
  if (line.startsWith("CMD:")) {
    line = line.substring(4);
  }

  last_serial_rx_ms = millis();

  if (line.startsWith("FACE:")) {
    String faceStr = line.substring(5);
    const char* names[] = {
      "IDLE", "HAPPY", "EXCITED", "SLEEPY", "THINKING",
      "SAD_ERROR", "ALERT_WARNING", "LOVE_BONDING",
      "STARTUP_BOOT", "SURPRISED",
      "SEC_SCANNING", "SEC_INTRUSION", "AVI_RADAR", "AVI_LOCK",
      "SYS_BOOT", "SYS_PREFS", "SYS_ERROR",
      "SEC_MATRIX", "SEC_RETRO", "SEC_RAINBOW"
    };
    const char* face_web_names[] = {
      "idle", "happy", "excited", "sleepy", "thinking",
      "sad", "sec_intrusion", "love",
      "sys_boot", "surprised",
      "sec_scanning", "sec_intrusion", "avi_radar", "avi_lock",
      "sys_boot", "sys_prefs", "sys_error",
      "sec_matrix", "sec_retro", "sec_rainbow"
    };
    EmotionType emo_map[] = {
      EMOTION_CALM, EMOTION_HAPPY, EMOTION_EXCITED, EMOTION_SLEEPY, EMOTION_CURIOUS,
      EMOTION_SAD, EMOTION_ALERT, EMOTION_LOVE,
      EMOTION_HAPPY, EMOTION_SURPRISED,
      EMOTION_CURIOUS, EMOTION_ALERT, EMOTION_CURIOUS, EMOTION_EXCITED,
      EMOTION_HAPPY, EMOTION_CURIOUS, EMOTION_ALERT,
      EMOTION_EXCITED, EMOTION_HAPPY, EMOTION_HAPPY
    };
    for (int i = 0; i < 20; i++) {
      if (faceStr == names[i]) {
        EmotionEngine.setEmotion(emo_map[i]);
        sys_current_face = face_web_names[i];
        break;
      }
    }
  } else if (line.startsWith("STATUS:")) {
    FaceEngine.setStatusText(line.substring(7).c_str());
  } else if (line.startsWith("APP:")) {
    FaceEngine.setAppName(line.substring(4).c_str());
  } else if (line.startsWith("PARTICLE:")) {
    String pType = line.substring(9);
    if (pType == "MUSIC") FaceEngine.triggerParticle(1);
    else if (pType == "SWEAT") FaceEngine.triggerParticle(2);
    else if (pType == "HEART") FaceEngine.triggerParticle(3);
  }
}

static void pet_fetch_standalone_weather() {
  if (sys_wifi_ssid.length() == 0 || sys_wifi_ssid == "YOUR_WIFI_SSID") return;

  g_block_touch = true;
  bool ok = WiFiService.connectSTA(sys_wifi_ssid.c_str(), sys_wifi_pass.c_str());

  if (ok) {
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    int ntp_tries = 0;
    while (!getLocalTime(&timeinfo) && ntp_tries < 6) {
      vTaskDelay(pdMS_TO_TICKS(500));
      ntp_tries++;
    }

    HTTPClient http;
    http.begin("http://wttr.in/Nalco,Angul?format=%c%t");
    http.setTimeout(8000);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      payload.replace("+", " ");
      payload.trim();
      if (payload.length() > 0 && payload.length() < 24) {
        char time_str[12] = "";
        char status_buf[48] = "";
        if (getLocalTime(&timeinfo)) {
          strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
          snprintf(status_buf, sizeof(status_buf), "%s | %s", time_str, payload.c_str());
        } else {
          snprintf(status_buf, sizeof(status_buf), "%s", payload.c_str());
        }
        FaceEngine.setStatusText(status_buf);
      }
    }
    http.end();
  }

  WiFiService.stop();
  g_block_touch = false;
}

void pet_setup(TFT_eSprite* tft) {
  _rtft = tft;
  FaceEngine.begin();
  last_serial_rx_ms = millis();
  
  _rtft->fillScreen(0x10A2); // C_BG
  Serial.println("{\"ready\":1,\"mode\":\"pet\"}");
}

void pet_teardown() {
  _rtft = nullptr;
}

void pet_core0_task() {
  uint32_t now = millis();
  if (now - last_serial_rx_ms > 15000) {
    if (last_standalone_weather_fetch == 0 || now - last_standalone_weather_fetch > 900000) {
      last_standalone_weather_fetch = now;
      // pet_fetch_standalone_weather();  // DISABLED: isolation test for udp_new_ip_type
    }
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void pet_core1_task() {
  if (!_rtft) return;
  static int frame = 0;
  frame++;

  extern volatile uint16_t g_touch_duration_ms;
  if (g_touch_duration_ms > 0) {
    TouchEventData td = { g_touch_duration_ms };
    g_touch_duration_ms = 0;
    EventBus.publish(EVENT_TOUCH_SHORT, &td);
  }

  // Draw face using modular layers inside FaceEngine
  FaceEngine.render(_rtft, frame);
  
  vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
}

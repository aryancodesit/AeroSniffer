// ================================================================
//  MultiBoot_DeskGadget.ino  —  Orchestration Layer
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Architecture: 
//    Core 0 → Background processing (FFT / network / API)
//    Core 1 → UI rendering + sensor reads (display thread)
//
//  Mode cycle (touch long-press on DeskBuddy / BOOT button on DevKitC):
//    0 = Companion (Pet)   →   1 = Security Monitor   →   2 = Flight Radar
//
//  Required Libraries (install via Arduino Library Manager):
//    • TFT_eSPI        by Bodmer
//    • ArduinoFFT      by kosme       (only if HAS_MIC)
//    • ArduinoJson     by Benoit Blanchon
//    • (ESP32 Arduino core ships FreeRTOS, WiFi, HTTPClient, I2S, Wire)
//
//  IMPORTANT: Before flashing, configure TFT_eSPI by copying
//  the provided TFT_eSPI_UserSetup.h content into:
//    Arduino/libraries/TFT_eSPI/User_Setup.h
// ================================================================

#include <WiFi.h>
#include "Config.h"
#include "Companion/AttentionEngine.h"
#include "Companion/MoodEngine.h"
#include "Persistence/PersistenceService.h"
#include "Memory/MemoryEngine.h"
#include "Mode1_Pet.h"
#include "Mode2_Security.h"
#include "Mode3_Aviation.h"

#include <TFT_eSPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ── Global TFT instance ──────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();

// ── Global Settings (loaded from Preferences) ────────────────────
String sys_wifi_ssid;
String sys_wifi_pass;
float sys_sky_lamin;
float sys_sky_lomin;
float sys_sky_lamax;
float sys_sky_lomax;
uint16_t sys_clock_color;
uint16_t sys_weather_color;
String sys_device_name;
String sys_ap_ssid;
String sys_ap_pass;
uint8_t sys_threat_sensitivity;
bool    sys_homeguard_enabled;
bool    sys_aviation_enabled;
String  sys_current_face = "idle";
bool    sec_hud_mode = true;
bool    avi_hud_mode = true;

// ── Memory Event Logger ──────────────────────────────────────────
static void memory_event_callback(EventType event, void* data) {
  (void)data;
  if (event == EVENT_TOUCH_SHORT) {
    MemoryEngine.onTouchEvent();
  }
}

// ── WiFi Event Logger ────────────────────────────────────────────
static void ae_event_callback(EventType event, void* data) {
  AttentionEngine.queueEvent(event, data);
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch(event)
    {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WIFI] Connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("[WIFI] IP=");
            Serial.println(WiFi.localIP());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf(
                "[WIFI] Disconnect reason=%d\n",
                info.wifi_sta_disconnected.reason
            );
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.printf("[WIFI] AP client connected   mac=%02X:%02X:%02X:%02X:%02X:%02X aid=%u total=%u\n",
                info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
                info.wifi_ap_staconnected.aid,
                WiFi.softAPgetStationNum());
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.printf("[WIFI] AP client disconnected mac=%02X:%02X:%02X:%02X:%02X:%02X aid=%u total=%u\n",
                info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5],
                info.wifi_ap_stadisconnected.aid,
                WiFi.softAPgetStationNum());
            break;

        default:
            break;
    }
}

// ── State machine ────────────────────────────────────────────────
volatile uint8_t  g_mode       = 0;      // 0=Pet  1=Security  2=Aviation
volatile uint8_t  g_active_mode = 0;
volatile bool     g_mode_dirty = false;  // Set by ISR/touch, consumed by Core 1
volatile bool     g_mode_transitioning = false; // Core 0/1 sync: skip Core 0 task during transition
volatile uint32_t g_last_btn   = 0;      // Debounce timestamp

// ── Touch interaction signal (shared with Mode 1) ────────────────
volatile bool     g_touch_tap  = false;  // Short tap detected
volatile bool     g_block_touch = false; // Global block touch flag

// ── FreeRTOS task handles ────────────────────────────────────────
static TaskHandle_t h_core0 = nullptr;
static TaskHandle_t h_core1 = nullptr;

// ================================================================
//  INTERRUPT SERVICE ROUTINE — Mode button (DevKitC only)
// ================================================================
#if defined(MODE_BTN_PIN) && MODE_BTN_PIN >= 0
void IRAM_ATTR isr_mode_btn() {
  uint32_t now = millis();
  if (now - g_last_btn < DEBOUNCE_MS) return;
  g_last_btn   = now;
  g_mode       = (g_mode + 1) % TOTAL_MODES;
  g_mode_dirty = true;
}
#endif

// ================================================================
//  CAPACITIVE TOUCH — Long-press mode switch (DeskBuddy 2.0)
// ================================================================
#if HAS_TOUCH
static uint32_t _touch_start   = 0;
static bool     _touch_active  = false;
static uint32_t _touch_last_change = 0;

static void poll_touch_input() {
  if (g_block_touch) {
    _touch_active = false;
    return;
  }
  int raw = digitalRead(TOUCH_PIN);
  bool pressed = (raw == HIGH);
  uint32_t now = millis();
  static uint32_t last_boot_log = 0;

  // Log raw state every 250ms during first 5s of boot for startup diagnostics
  static uint32_t boot_start = 0;
  if (boot_start == 0) boot_start = now;
  if (now - boot_start < 5000 && now - last_boot_log >= 250) {
    last_boot_log = now;
    Serial.printf("[TOUCH] boot raw=%d pressed=%d active=%d ms=%u\n", raw, pressed ? 1 : 0, _touch_active ? 1 : 0, now);
  }

  // Debounce: ignore state changes within TOUCH_DEBOUNCE_MS
  if (pressed != _touch_active) {
    if (now - _touch_last_change < TOUCH_DEBOUNCE_MS) return;
    _touch_last_change = now;
    Serial.printf("[TOUCH] transition raw=%d pressed=%d ms=%u\n", raw, pressed ? 1 : 0, now);
  }

  if (pressed && !_touch_active) {
    // ── Finger DOWN ──────────────────────────────────────────
    _touch_start  = now;
    _touch_active = true;
    Serial.printf("[TOUCH] START raw=%d ms=%u\n", raw, now);
  }

  if (!pressed && _touch_active) {
    // ── Finger UP ────────────────────────────────────────────
    uint32_t duration = now - _touch_start;
    _touch_active = false;
    Serial.printf("[TOUCH] RELEASE raw=%d duration=%u ms=%u\n", raw, duration, now);

    if (duration >= LONG_PRESS_MS) {
      // LONG PRESS → Switch mode
      Serial.println("[TOUCH] LONG_PRESS");
      g_mode       = (g_mode + 1) % TOTAL_MODES;
      g_mode_dirty = true;
    } else if (duration > 20) {
      // SHORT TAP → Context interaction (pet pat, etc.)
      Serial.println("[TOUCH] TAP");
      g_touch_tap = true;
    } else {
      Serial.println("[TOUCH] CHATTER (duration <= 20ms, ignored)");
    }
  }
}
#endif

// ================================================================
//  BOOT SPLASH SCREEN (adapts to display resolution)
// ================================================================
static void show_splash() {
  tft.fillScreen(TFT_BLACK);

  int cx = TFT_W / 2;
  int cy = TFT_H / 2 - 16;

  // Outer glow ring
  tft.fillCircle(cx, cy, 56, 0x000F);
  tft.drawCircle(cx, cy, 56, 0x07FF);
  tft.drawCircle(cx, cy, 54, 0x034B);

  // "ESP" silhouette dots
  tft.fillCircle(cx - 22, cy, 11, 0x001F);
  tft.fillCircle(cx,      cy, 11, 0x0340);
  tft.fillCircle(cx + 22, cy, 11, 0x4000);

  // Title
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  int title_x = cx - 66;   // Approximate centering for "AeroSniffer"
  tft.setCursor(title_x, cy + 30);
  tft.print("AeroSniffer");

  tft.setTextColor(0x07FF);
  tft.setTextSize(1);
  int sub_x = cx - 60;
  tft.setCursor(sub_x, cy + 52);
  tft.print("Multi-Boot Desk Gadget");

  tft.setTextColor(0x4208);
  tft.setCursor(cx - 74, cy + 66);
  #ifdef HW_DESKBUDDY_2
    tft.print("XIAO ESP32S3 | FreeRTOS | v2.0");
  #else
    tft.print("ESP32-S3 | FreeRTOS | v1.0");
  #endif

  // Mode icons
  int iconY = cy + 84;
  tft.setTextSize(1);
  tft.setTextColor(0x07E0); tft.setCursor(cx - 98, iconY); tft.print("Pet");
  tft.setTextColor(0xF800); tft.setCursor(cx - 20, iconY); tft.print("Sec");
  tft.setTextColor(0xFFE0); tft.setCursor(cx + 50, iconY); tft.print("ADS-B");

  delay(2800);
  tft.fillScreen(TFT_BLACK);
}

// ================================================================
//  MODE TEARDOWN HELPER
// ================================================================
static void teardown_mode(uint8_t mode) {
  switch (mode) {
    case 0: pet_teardown();       break;
    case 1: security_teardown();  break;
    case 2: aviation_teardown();  break;
  }
  WiFiService.stop();
}

// ================================================================
//  MODE SETUP HELPER
// ================================================================
static void setup_mode(uint8_t mode) {
  DisplayService.clear();
  switch (mode) {
    case 0: pet_setup(DisplayService.getCanvas());       break;
    case 1: security_setup(DisplayService.getCanvas());  break;
    case 2: aviation_setup(DisplayService.getCanvas());  break;
  }
  DisplayService.commit();
}

// ================================================================
//  GLOBAL & LOCAL SERIAL PROTOCOL ROUTER (Non-Blocking)
// ================================================================

static bool handle_global_command(const String& line) {
  if (line.startsWith("{")) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, line);
    if (!err) {
      if (doc.containsKey("emotion")) {
        String emo = doc["emotion"].as<String>();
        if (emo == "happy") EmotionEngine.setEmotion(EMOTION_HAPPY);
        else if (emo == "sad") EmotionEngine.setEmotion(EMOTION_SAD);
        else if (emo == "angry") EmotionEngine.setEmotion(EMOTION_ANGRY);
        else if (emo == "curious") EmotionEngine.setEmotion(EMOTION_CURIOUS);
        else if (emo == "sleepy") EmotionEngine.setEmotion(EMOTION_SLEEPY);
        else if (emo == "calm") EmotionEngine.setEmotion(EMOTION_CALM);
        else if (emo == "alert") EmotionEngine.setEmotion(EMOTION_ALERT);
        else if (emo == "love") EmotionEngine.setEmotion(EMOTION_LOVE);
        else if (emo == "surprised") EmotionEngine.setEmotion(EMOTION_SURPRISED);
      }
      if (doc.containsKey("activity")) {
        String act = doc["activity"].as<String>();
        if (act == "idle") EmotionEngine.setActivity(ACTIVITY_IDLE);
        else if (act == "coding") {
          EmotionEngine.setActivity(ACTIVITY_CODING);
          EventBus.publish(EVENT_PC_CODING);
        }
        else if (act == "music") EmotionEngine.setActivity(ACTIVITY_MUSIC);
        else if (act == "aviation") EmotionEngine.setActivity(ACTIVITY_WATCHING_FLIGHTS);
        else if (act == "scanning") EmotionEngine.setActivity(ACTIVITY_SCANNING_NETWORKS);
        else if (act == "thinking") EmotionEngine.setActivity(ACTIVITY_THINKING);
        else if (act == "sleeping") EmotionEngine.setActivity(ACTIVITY_SLEEPING);
        else if (act == "typing") {
          EmotionEngine.setActivity(ACTIVITY_TYPING);
          EventBus.publish(EVENT_PC_TYPING);
        }
      }
      if (doc.containsKey("app")) {
        String app = doc["app"].as<String>();
        EventBus.publish(EVENT_PC_STATUS_UPDATE, (void*)app.c_str());
      }
      Serial.println("RES:{\"ok\":true,\"action\":\"json_context\"}");
      return true;
    }
  }

  // Web app commands prefix: "CMD:"
  if (line.startsWith("CMD:")) {
    String cmd = line.substring(4);

    if (cmd == "PING") {
      Serial.printf("RES:{\"ok\":true,\"fw\":\"2.0\",\"mode\":%d,\"hw\":\"deskbuddy2\"}\n", g_mode + 1);
      return true;
    }

    if (cmd == "GET_FLIGHTS") {
      avi_send_flights_serial();
      return true;
    }

    if (cmd == "GET_TIMELINE") {
      Timeline.sendSerial();
      return true;
    }
    
    if (cmd == "GET_PET_STATUS") {
      Serial.printf("RES:{\"emotion\":\"%s\",\"mood\":\"%s\",\"activity\":\"%s\",\"face\":\"%s\",\"wifi\":\"%s\",\"heap\":%d,\"fps\":%d,\"mode\":%d,\"flights\":%lu,\"networks\":%lu,\"coding\":%lu,\"hours\":%lu,\"fl_seen_today\":%lu,\"fl_seen_lifetime\":%lu,\"fl_last_count\":%lu,\"fl_max_seen\":%lu}\n",
                    EmotionEngine.getEmotionStr(), MoodEngine.moodName(g_creature.mood), EmotionEngine.getActivityStr(),
                    sys_current_face.c_str(),
                    WiFiService.isConnected() ? sys_wifi_ssid.c_str() : "disconnected",
                    ESP.getFreeHeap(), 30, g_mode,
                    StorageService.getFlightsSeen(), StorageService.getNetworksDiscovered(),
                    StorageService.getCodingTimeMinutes(), StorageService.getActiveHours(),
                    FlightStats.aircraft_seen_today, FlightStats.aircraft_seen_lifetime,
                    FlightStats.last_aircraft_count, FlightStats.max_aircraft_seen);
      return true;
    }
    
    if (cmd == "GET_CFG") {
      auto rgb565toHex = [](uint16_t color) -> String {
        uint8_t r = (color >> 11) << 3;
        uint8_t g = ((color >> 5) & 0x3F) << 2;
        uint8_t b = (color & 0x1F) << 3;
        char hex[8];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
        return String(hex);
      };
      Serial.printf("RES:{\"ssid\":\"%s\",\"lamin\":%.2f,\"lomin\":%.2f,\"lamax\":%.2f,\"lomax\":%.2f,\"c_col\":\"%s\",\"w_col\":\"%s\",\"refresh\":%lu,\"aviation_enabled\":%s}\n",
                    sys_wifi_ssid.c_str(), sys_sky_lamin, sys_sky_lomin, sys_sky_lamax, sys_sky_lomax,
                    rgb565toHex(sys_clock_color).c_str(), rgb565toHex(sys_weather_color).c_str(), sys_avi_refresh,
                    sys_aviation_enabled ? "true" : "false");
      return true;
    }
    
    if (cmd.startsWith("SET_REFRESH:")) {
      uint32_t val = cmd.substring(12).toInt();
      if (val == 15000 || val == 30000 || val == 60000 || val == 120000) {
        sys_avi_refresh = val;
        Preferences p;
        p.begin("aerosniffer", false);
        p.putUInt("avi_refresh", val);
        p.end();
        Serial.println("RES:{\"ok\":true,\"action\":\"set_refresh\"}");
      } else {
        Serial.println("RES:{\"ok\":false,\"error\":\"invalid refresh rate\"}");
      }
      return true;
    }
    
    if (cmd.startsWith("SET_AVIATION:")) {
      String val = cmd.substring(13);
      bool enable = (val == "1" || val == "true");
      sys_aviation_enabled = enable;
      Preferences p;
      p.begin("aerosniffer", false);
      p.putUChar("avi_enable", enable ? 1 : 0);
      p.end();
      Serial.println("RES:{\"ok\":true,\"action\":\"set_aviation\"}");
      return true;
    }
    
    if (cmd.startsWith("SET_WIFI:")) {
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
      return true;
    }
    
    if (cmd.startsWith("SET_BBOX:")) {
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
      return true;
    }
    
    if (cmd.startsWith("SET_COLOR:")) {
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
      return true;
    }
    
    if (cmd.startsWith("SET_MODE:")) {
      uint8_t target = cmd.substring(9).toInt();
      if (target < TOTAL_MODES) {
        g_mode = target;
        g_mode_dirty = true;
        Serial.printf("RES:{\"ok\":true,\"action\":\"set_mode\",\"mode\":%d}\n", g_mode + 1);
      } else {
        Serial.println("RES:{\"ok\":false,\"error\":\"invalid mode\"}");
      }
      return true;
    }
    
    if (cmd.startsWith("REBOOT")) {
      Serial.println("RES:{\"ok\":true,\"action\":\"reboot\"}");
      delay(100);
      ESP.restart();
      return true;
    }
  } else if (line.startsWith("TELEMETRY:")) {
    String payload = line.substring(10);
    int cpu_val = 0, ram_val = 0;
    if (sscanf(payload.c_str(), "cpu=%d,ram=%d", &cpu_val, &ram_val) == 2) {
      g_creature.pc_cpu = cpu_val;
      g_creature.pc_ram = ram_val;
      Serial.printf("RES:{\"ok\":true,\"action\":\"telemetry\",\"cpu\":%d,\"ram\":%d}\n", cpu_val, ram_val);
    } else {
      Serial.println("RES:{\"ok\":false,\"error\":\"bad_telemetry_format\"}");
    }
    return true;
  } else {
    // Non-CMD web commands, e.g. PC Agent PING
    if (line == "PING") {
      Serial.println("{\"pong\":1}");
      return true;
    }
  }
  
  return false;
}

static void process_serial_commands() {
  static String input_line = "";
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      input_line.trim();
      if (input_line.length() > 0) {
        // Debug trace: log packet type, timestamp, payload size
        String pkt_type = "RAW";
        if (input_line.startsWith("{")) pkt_type = "JSON";
        else if (input_line.startsWith("CMD:")) pkt_type = "CMD";
        else if (input_line.startsWith("FACE:")) pkt_type = "FACE";
        else if (input_line.startsWith("STATUS:")) pkt_type = "STATUS";
        else if (input_line.startsWith("APP:")) pkt_type = "APP";
        else if (input_line.startsWith("TELEMETRY:")) pkt_type = "TELEM";
        else if (input_line.startsWith("PARTICLE:")) pkt_type = "PART";
        else if (input_line == "PING") pkt_type = "PING";
        Serial.printf("[SERIAL] %s t=%lu len=%d\n", pkt_type.c_str(), millis(), input_line.length());

        if (!handle_global_command(input_line)) {
          if (g_mode == 0) {
            pet_handle_command(input_line);
          } else if (g_mode == 1) {
            security_handle_command(input_line);
          }
        }
      }
      input_line = "";
    } else if (c != '\r') {
      input_line += c;
    }
  }
}

// ================================================================
//  FREERTOS TASKS
// ================================================================

// ── Core 0 — Background processing ──────────────────────────────
void task_core0(void*) {
  for (;;) {
    // Skip processing during mode transition to avoid race with Core 1's teardown/setup.
    // Use g_active_mode (only updated by Core 1 after setup completes) to avoid ISR races.
    if (!g_mode_transitioning) {
      switch (g_active_mode) {
        case 0: pet_core0_task();       break;
        case 1: security_core0_task();  break;
        case 2: aviation_core0_task();  break;
      }
    }
    taskYIELD();
  }
}

// ── Core 1 — UI rendering + state management ────────────────────
void task_core1(void*) {
  // Initial mode boot
  setup_mode(g_mode);
  uint32_t last_heartbeat = 0;
  uint32_t last_ae_tick = millis();

  for (;;) {
    // ── Heartbeat (every 1s) ────────────────────────────────
    uint32_t now = millis();
    if (now - last_heartbeat >= 1000) {
      last_heartbeat = now;
      Serial.println("[FACE] heartbeat");
    }

    // ── Tick Attention Engine ────────────────────────────────
    uint32_t ae_delta = now - last_ae_tick;
    last_ae_tick = now;
    AttentionEngine.tick(ae_delta);

    // ── Tick Emotion Engine ──────────────────────────────────
    EmotionEngine.tick();

    // ── Tick Mood Engine ─────────────────────────────────────
    MoodEngine.tick(ae_delta);

    // ── Tick Persistence Service (observer, not pipeline) ────
    CreaturePersistence.tick();

    // ── Tick Memory Engine (observer — reads g_creature) ─────
    MemoryEngine.tick();

    // ── Non-Blocking Serial Processing ───────────────────────
    process_serial_commands();

    // ── Poll capacitive touch (DeskBuddy 2.0) ────────────────
    #if HAS_TOUCH
      poll_touch_input();
    #endif

    // ── Handle mode switch (triggered by ISR or touch) ──────
    if (g_mode_dirty) {
      g_mode_dirty  = false;
      g_mode_transitioning = true;
      Serial.printf("[MODE] Transitioning: %d -> %d\n", g_active_mode, g_mode);
      StorageService.saveMode(g_mode);
      CreaturePersistence.save();
      Serial.println("[DBG] BEFORE teardown");
      teardown_mode(g_active_mode);
      Serial.println("[DBG] AFTER teardown");
      Serial.printf("[MODE] Teardown of mode %d complete\n", g_active_mode);
      AttentionEngine.pause();
      Serial.println("[DBG] AFTER pause");

      // Mode transition splash
      DisplayService.clear();
      TFT_eSprite* canvas = DisplayService.getCanvas();
      canvas->setTextColor(0x07FF);
      canvas->setTextSize(2);
      const char* mnames[] = {"COMPANION", "SECURITY", "AVIATION"};
      // Center text on screen
      int tx = (TFT_W - strlen(mnames[g_mode]) * 12) / 2;
      canvas->setCursor(tx, TFT_H / 2 - 8);
      canvas->print(mnames[g_mode]);
      DisplayService.commit();
      delay(600);

      Serial.println("[DBG] AFTER splash");
      g_active_mode = g_mode;
      Serial.println("[DBG] BEFORE setup");
      setup_mode(g_mode);
      Serial.println("[DBG] AFTER setup");
      #if HAS_TOUCH
        _touch_start = 0;
        _touch_active = false;
        _touch_last_change = millis();
      #endif
      Serial.printf("[MODE] Setup of mode %d complete\n", g_active_mode);
      AttentionEngine.resume();
      Serial.println("[DBG] AFTER resume");
      g_mode_transitioning = false;
    }

    // ── Dispatch UI update for current mode ────────────────────
    switch (g_mode) {
      case 0: pet_core1_task();       break;
      case 1: security_core1_task();  break;
      case 2: aviation_core1_task();  break;
    }

    DisplayService.commit();

    vTaskDelay(pdMS_TO_TICKS(16));
  }
}

// ================================================================
//  ARDUINO SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[AeroSniffer] Booting...");
  WiFi.onEvent(WiFiEvent);

  #ifdef HW_DESKBUDDY_2
    Serial.println("[HW] DeskBuddy 2.0 — XIAO ESP32S3 + ST7789 240x240");
  #else
    Serial.println("[HW] DevKitC — ESP32-S3 + ILI9341 240x320");
  #endif

  // ── TFT init ──────────────────────────────────────────────────
  tft.init();
  tft.setRotation(1);        // Rotate 90 degrees clockwise to make upright
  tft.fillScreen(TFT_BLACK);

  // ── Initialize OS Services ───────────────────────────────────────
  StorageService.begin();
  EmotionEngine.begin();
  MoodEngine.begin();
  CreaturePersistence.begin();
  MemoryEngine.begin();
  AttentionEngine.begin();

  // ── Subscribe to EventBus events ──────────────────────────────
  EventBus.subscribe(EVENT_TOUCH_SHORT, memory_event_callback);

  // ── Subscribe Attention Engine to EventBus events ────────────
  EventBus.subscribe(EVENT_ATTACK_DEAUTH, ae_event_callback);
  EventBus.subscribe(EVENT_ATTACK_EVILTWIN, ae_event_callback);
  EventBus.subscribe(EVENT_FLIGHT_DETECTED, ae_event_callback);
  EventBus.subscribe(EVENT_WIFI_CONNECTING, ae_event_callback);
  EventBus.subscribe(EVENT_WIFI_CONNECTED, ae_event_callback);
  EventBus.subscribe(EVENT_TOUCH_SHORT, ae_event_callback);
  EventBus.subscribe(EVENT_FLIGHT_RARE, ae_event_callback);
  EventBus.subscribe(EVENT_WIFI_DISCONNECTED, ae_event_callback);

  WiFiService.begin();
  DisplayService.begin();

  // Always boot to Companion Mode (ignore saved mode)
  g_mode = 0;
  g_active_mode = 0;

  sys_wifi_ssid = StorageService.getSSID();
  sys_wifi_pass = StorageService.getPassword();
  sys_sky_lamin = StorageService.getSkyLamin();
  sys_sky_lomin = StorageService.getSkyLomin();
  sys_sky_lamax = StorageService.getSkyLamax();
  sys_sky_lomax = StorageService.getSkyLomax();
  sys_clock_color = StorageService.getClockColor();
  sys_weather_color = StorageService.getWeatherColor();
  sys_device_name = StorageService.getDeviceName();
  sys_ap_ssid = StorageService.getAPSsid();
  sys_ap_pass = StorageService.getAPPass();
  sys_threat_sensitivity = StorageService.getThreatSensitivity();
  sys_homeguard_enabled = StorageService.getHomeGuardEnabled();
  sys_aviation_enabled = StorageService.getAviationEnabled();
  Serial.printf("[BOOT] Aviation Enabled preference: %s\n", sys_aviation_enabled ? "true" : "false");

  // ── Backlight (DevKitC only — DeskBuddy BL is always-on) ─────
  #if defined(TFT_BL) && TFT_BL >= 0
    pinMode(TFT_BL, OUTPUT);
    // ESP32 Core v3 API
    ledcAttach(TFT_BL, 5000, 8); // pin, 5 kHz PWM, 8-bit resolution
    ledcWrite(TFT_BL, 255);      // Full brightness (0-255)
  #endif

  // ── I2C for MPU-6050 (only if IMU present) ────────────────────
  #if HAS_IMU
    Wire.begin(I2C_SDA, I2C_SCL);
  #endif

  // ── Input setup ───────────────────────────────────────────────
  #if HAS_TOUCH
    // Capacitive touch — digital input with pull-down
    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    Serial.println("[INPUT] Capacitive touch on GPIO " + String(TOUCH_PIN));
    Serial.println("[INPUT] Short tap = interact | Long press = mode switch");
  #endif

  #if defined(MODE_BTN_PIN) && MODE_BTN_PIN >= 0
    // Physical mode button (DevKitC BOOT button)
    pinMode(MODE_BTN_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(MODE_BTN_PIN), isr_mode_btn, FALLING);
    Serial.println("[INPUT] Mode button on GPIO " + String(MODE_BTN_PIN));
  #endif

  // ── Splash screen ─────────────────────────────────────────────
  show_splash();

  // ── Spawn FreeRTOS tasks (after splash so I2S/WiFi start clean) ─
  // Stack sizes: 16384 to prevent SSL stack overflows; UI engine 8192
  xTaskCreatePinnedToCore(task_core0, "BG_Engine", 16384, nullptr, 1, &h_core0, 0);
  xTaskCreatePinnedToCore(task_core1, "UI_Engine",  8192, nullptr, 2, &h_core1, 1);
}

// ================================================================
//  ARDUINO LOOP  — surrendered to FreeRTOS
// ================================================================
void loop() {
  // All work done in FreeRTOS tasks; delete this Arduino task
  vTaskDelete(nullptr);
}

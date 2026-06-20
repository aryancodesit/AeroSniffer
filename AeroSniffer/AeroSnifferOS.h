// ================================================================
//  AeroSnifferOS.h  —  Modular Services & Central Brain
//  AeroSniffer OS Layer | C++ Shared Infrastructure
// ================================================================
#pragma once
#ifndef AEROSNIFFEROS_H
#define AEROSNIFFEROS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include "esp_wifi.h"

// ── Event Bus Types ─────────────────────────────────────────────
enum EventType {
  EVENT_WIFI_CONNECTING = 0,
  EVENT_WIFI_CONNECTED,
  EVENT_WIFI_DISCONNECTED,
  EVENT_FLIGHT_DETECTED,
  EVENT_FLIGHT_RARE,
  EVENT_ATTACK_DEAUTH,
  EVENT_ATTACK_EVILTWIN,
  EVENT_PC_CODING,
  EVENT_PC_GAMING,
  EVENT_PC_MUSIC,
  EVENT_PC_IDLE,
  EVENT_PC_TYPING,
  EVENT_PC_STATUS_UPDATE,
  EVENT_USER_TAP,
  EVENT_USER_LONG_TAP,
  EVENT_TOUCH_SHORT,
  EVENT_TOUCH_LONG,
  EVENT_MODE_SWITCHED,
  EVENT_NETWORK_DISCOVERED,
  EVENT_DEVICE_TRUSTED,
  EVENT_DEVICE_FAMILIAR,
  EVENT_DEVICE_UNKNOWN,
  EVENT_COUNT
};

typedef void (*EventCallback)(EventType event, void* eventData);

#define MAX_SUBSCRIBERS 32

class EventBusClass {
private:
  struct Subscription {
    EventType event;
    EventCallback cb;
  };
  Subscription subs[MAX_SUBSCRIBERS];
  int sub_count = 0;
public:
  void subscribe(EventType event, EventCallback cb);
  void publish(EventType event, void* eventData = nullptr);
};

// ── Emotion Engine Types ─────────────────────────────────────────
enum EmotionType {
  EMOTION_HAPPY = 0,
  EMOTION_SAD,
  EMOTION_ANGRY,
  EMOTION_CURIOUS,
  EMOTION_SLEEPY,
  EMOTION_CALM,
  EMOTION_ALERT,
  EMOTION_LOVE,
  EMOTION_SURPRISED,
  EMOTION_EXCITED,
  EMOTION_COUNT
};

enum MoodType {
  MOOD_RELAXED  = 0,
  MOOD_PLAYFUL  = 1,
  MOOD_ANXIOUS  = 2,
  MOOD_COUNT    = 3
};

enum ActivityType {
  ACTIVITY_IDLE = 0,
  ACTIVITY_CODING,
  ACTIVITY_MUSIC,
  ACTIVITY_WATCHING_FLIGHTS,
  ACTIVITY_SCANNING_NETWORKS,
  ACTIVITY_THINKING,
  ACTIVITY_SLEEPING,
  ACTIVITY_TYPING,
  ACTIVITY_COUNT
};

// ── Attention Types (V2.5 Structured Attention) ───────────────────
enum AttentionTarget : uint8_t {
    TARGET_NONE    = 0,  // Idle — no focus target
    TARGET_USER    = 1,  // User interaction (touch)
    TARGET_THREAT  = 2,  // Security threat
    TARGET_FLIGHT  = 3   // Aircraft detected
};

enum AttentionSource : uint8_t {
    SOURCE_INTERNAL  = 0,  // Infrastructure: WiFi, mode switch, idle
    SOURCE_TOUCH     = 1,  // Physical user interaction
    SOURCE_SECURITY  = 2,  // Threat detection: deauth, evil twin
    SOURCE_AVIATION  = 3,  // Aircraft observation
    SOURCE_PORTAL    = 4   // Reserved — future Portal-triggered attention
};

// ── Global Creature State (Single Source of Truth) ───────────────
struct CreatureState {
  EmotionType emotion;
  MoodType mood;
  uint8_t mood_strength;
  ActivityType activity;
  bool wifi_connected;
  uint32_t uptime_minutes;
  uint32_t coding_minutes;
  uint32_t gaming_minutes;
  uint32_t music_minutes;
  uint32_t flights_seen_today;
  uint32_t networks_seen_today;
  uint32_t interactions_today;
  uint32_t friendship_level;
  int8_t pc_cpu;  // Last PC CPU telemetry reading
  int8_t pc_ram;  // Last PC RAM telemetry reading
  struct {
    AttentionTarget target;    // What the creature is focused on
    AttentionSource source;    // Which domain triggered focus
    uint8_t         strength;  // 0–100, 0 = idle, 100 = locked
  } attention;
};


extern CreatureState g_creature;

// Debounce guard: prevent rapid emotion toggling from repeated events
#define EMOTION_DEBOUNCE_MS 1000

class EmotionEngineClass {
private:
  EmotionType current_emotion = EMOTION_CALM;
  ActivityType current_activity = ACTIVITY_IDLE;
  uint32_t last_activity_change = 0;
  uint32_t last_emotion_decay = 0;
  uint32_t last_active_sec = 0;
  uint32_t transient_until = 0;
public:
  void begin();
  void tick();
  void handleEvent(EventType event, void* data);
  
  EmotionType getEmotion() const { return current_emotion; }
  ActivityType getActivity() const { return current_activity; }
  
  void setEmotion(EmotionType e);
  void setActivity(ActivityType a);
  void forceIdle();
  
  uint8_t getRobotFace() const; // Maps to Companion Mode Face ID
  const char* getEmotionStr() const;
  const char* getActivityStr() const;
  const char* emotionToDisplayName(EmotionType e) const;
};

// ── Timeline System ──────────────────────────────────────────────
struct TimelineEvent {
  uint32_t time_sec; // Seconds since boot
  char text[48];
  char type[12];     // "system", "flight", "network", "touch", "activity"
};

class TimelineClass {
private:
  TimelineEvent events[100];
  int count = 0;
  int head = 0;
public:
  void logEvent(const char* text, const char* type = "system");
  void clear();
  int getEventsCount() const { return count; }
  void getEvent(int idx, uint32_t& ts, String& txt, String& ty) const;
  void sendSerial(); // Prints JSON array over Serial: RES:{"timeline":[...]}
};

extern TimelineClass Timeline;

// ── Face Engine v2 ───────────────────────────────────────────────
class FaceEngineClass {
private:
  float anim_blink_scale = 1.0f;
  bool anim_is_blinking = false;
  uint32_t anim_blink_next = 0;
  int anim_look_x = 0;
  int anim_look_y = 0;
  uint32_t anim_look_next = 0;
  int anim_bounce_y = 0;
  float anim_pulse_scale = 1.0f;
  char status_l1[48] = "";
  char status_l2[32] = "";

  struct Particle {
    int type; // 0=none, 1=music, 2=sweat, 3=heart
    float x, y;
    float life;
    uint16_t color;
  };
  Particle active_particles[5];

public:
  void begin();
  void updateAnimations(int frame);
  void render(TFT_eSprite* spr, int frame);
  
  // Decoupled architecture layers
  void renderEmotionLayer(TFT_eSprite* spr, int frame);
  void renderActivityLayer(TFT_eSprite* spr, int frame);
  void renderEffectLayer(TFT_eSprite* spr, int frame);
  
  void setStatusText(const char* status);
  void setAppName(const char* app);
  void setStatusLines(const char* l1, const char* l2);
  void triggerParticle(int type);
};

extern FaceEngineClass FaceEngine;

// ── WiFi Service ────────────────────────────────────────────────
class WiFiServiceClass {
private:
  bool is_connecting = false;
  bool is_promiscuous = false;
  uint8_t current_channel = 1;
public:
  void begin();
  bool connectSTA(const char* ssid, const char* pass, uint32_t timeout_ms = 12000);
  void startAP(const char* ssid, const char* pass);
  void stop();
  
  void setPromiscuous(bool enable, wifi_promiscuous_cb_t cb = nullptr);
  void setChannel(uint8_t ch);
  void transmitRaw80211(uint8_t* packet, int len);
  
  bool isConnected();
  int getStatus();
  String getLocalIP();
  String getSoftAPIP();
  
  // Temp ownership tracking
  const char* last_owner = "none";
};

// ── Display Service ─────────────────────────────────────────────
class DisplayServiceClass {
private:
  TFT_eSprite canvas;
public:
  DisplayServiceClass();
  void begin();
  TFT_eSprite* getCanvas();
  void clear();
  void commit();
};

// ── Storage Service ─────────────────────────────────────────────
class StorageServiceClass {
private:
  Preferences prefs;
public:
  void begin();
  
  String getSSID();
  String getPassword();
  void saveWiFi(String ssid, String pass);
  
  float getSkyLamin();
  float getSkyLomin();
  float getSkyLamax();
  float getSkyLomax();
  void saveSkyBounds(float lamin, float lomin, float lamax, float lomax);
  
  uint16_t getClockColor();
  uint16_t getWeatherColor();
  void saveColors(uint16_t clock, uint16_t weather);
  
  uint32_t getSavedMode();
  void saveMode(uint32_t mode);

  // Generic key-value stats subsystem (survives reboot)
  uint32_t getStat(const char* key, uint32_t def = 0);
  void setStat(const char* key, uint32_t val);
  void incrementStat(const char* key, uint32_t amount = 1);

  // ── Security / Control Layer Settings ──
  String getDeviceName();
  void saveDeviceName(String name);
  String getAPSsid();
  String getAPPass();
  void saveAPConfig(String ssid, String pass);
  uint8_t getThreatSensitivity();
  void saveThreatSensitivity(uint8_t level);
  bool getHomeGuardEnabled();
  void saveHomeGuardEnabled(bool enabled);
  bool getAviationEnabled();
  void saveAviationEnabled(bool enabled);

  // Legacy compatibility wraps
  uint32_t getFlightsSeen();
  void incrementFlightsSeen();
  uint32_t getNetworksDiscovered();
  void addNetworksDiscovered(uint32_t count);
  uint32_t getCodingTimeMinutes();
  void addCodingTimeMinutes(uint32_t mins);
  uint32_t getActiveHours();
  void incrementActiveHours();
};

// ── Flight Statistics Service ────────────────────────────────────
class FlightStatsClass {
public:
  uint32_t aircraft_seen_today = 0;
  uint32_t aircraft_seen_lifetime = 0;
  uint32_t last_aircraft_count = 0;
  uint32_t max_aircraft_seen = 0;

  void begin();
  void save();
  void recordFetch(uint32_t current_count);
  void incrementSeen(uint32_t amount);
  void resetDaily();
};

// ── Mood Engine ───────────────────────────────────────────────────
class MoodEngineClass;
extern MoodEngineClass MoodEngine;
class AttentionEngineClass;
extern AttentionEngineClass AttentionEngine;
extern EventBusClass EventBus;
extern EmotionEngineClass EmotionEngine;
extern WiFiServiceClass WiFiService;
extern DisplayServiceClass DisplayService;
extern StorageServiceClass StorageService;
extern FlightStatsClass FlightStats;
extern uint32_t sys_avi_refresh;

#endif // AEROSNIFFEROS_H

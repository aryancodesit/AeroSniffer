// AeroSnifferOS.h  —  OS Layer: CreatureState, EventBus, FaceEngine, Services
#pragma once
#ifndef AEROSNIFFEROS_H
#define AEROSNIFFEROS_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "Memory/MemoryTypes.h"

// Event types
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

struct TouchEventData {
  uint16_t duration_ms;
};

typedef void (*EventCallback)(EventType event, void* eventData);

#define MAX_SUBSCRIBERS 48

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

// Emotion types
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

// Centralized user-facing labels (V2.8 RC Polish) — indexed by EmotionType
static constexpr const char* kEmotionLabels[EMOTION_COUNT] = {
  "Happy", "Sad", "Angry", "Curious", "Sleepy",
  "Calm", "Alert", "Love", "Surprised", "Excited"
};
static constexpr const char* kEmotionShortLabels[EMOTION_COUNT] = {
  "HAPPY", "SAD", "ANGRY", "CURIOUS", "SLEEPY",
  "CALM", "ALERT", "LOVE", "SURPRISED", "EXCITED"
};

enum MoodType {
  MOOD_RELAXED  = 0,
  MOOD_PLAYFUL  = 1,
  MOOD_ANXIOUS  = 2,
  MOOD_COUNT    = 3
};

// Centralized mood labels (V2.8 RC Polish) — indexed by MoodType
static constexpr const char* kMoodLabels[MOOD_COUNT] = {
  "Relaxed", "Playful", "Anxious"
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

// Centralized activity labels (V2.8 RC Polish) — indexed by ActivityType
static constexpr const char* kActivityLabels[ACTIVITY_COUNT] = {
  "Idle", "Coding", "Music", "Aviation", "Scanning",
  "Thinking", "Sleeping", "Typing"
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
  uint8_t engagement_drive;  // 0–100, runtime-only autonomous presence
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

// ── Presentation Intelligence (V2.8 Sprint 1) ────────────────────
// Animation profile — fixed-point multipliers applied on top of mood baseline
struct AnimationProfile {
  uint16_t blink_mod;      // 128 = 1.0x (up to 2.0x = 256)
  uint8_t  gaze_range;     // max wander pixels (absolute)
  uint8_t  idle_speed_mod; // 128 = 1.0x
  uint8_t  bounce_mod;     // 128 = 1.0x
};

// Context bucket — generic activity classification for variant selection
#define PRES_CONTEXT_CALM    0
#define PRES_CONTEXT_ACTIVE  1
#define PRES_CONTEXT_FOCUSED 2
#define PRES_CONTEXT_SOCIAL  3
#define PRES_CONTEXT_ANY     255

// Eye shape indices for PresentationProfile
#define PRES_EYE_DEFAULT   0
#define PRES_EYE_ARCHED    1
#define PRES_EYE_ROUND     2
#define PRES_EYE_FLAT      3
#define PRES_EYE_SQUINT    4
#define PRES_EYE_WIDE      5

// Brow style indices
#define PRES_BROW_NORMAL   0
#define PRES_BROW_RAISED   1
#define PRES_BROW_FURROWED 2
#define PRES_BROW_ONE      3
#define PRES_BROW_WORRIED  4

// Pupil style indices
#define PRES_PUPIL_NORMAL      0
#define PRES_PUPIL_DILATED     1
#define PRES_PUPIL_CONSTRICTED 2
#define PRES_PUPIL_HALF        3
#define PRES_PUPIL_NONE        4

struct PresentationProfile {
  uint8_t  variant_id;
  uint8_t  eye_shape;
  uint8_t  brow_style;
  uint8_t  pupil_style;
  uint8_t  color_override;
  uint8_t  anim_profile_id;
  int8_t   mood_affinity[3];
  uint8_t  engagement_min;
  uint8_t  engagement_max;
  uint8_t  context_bucket;
  uint16_t hold_ms;
};

struct PresentationState {
  uint8_t  current_profile;
  uint8_t  pending_profile;
  uint8_t  transition_progress;
  uint16_t hold_remaining_ms;
  uint32_t selection_seed;
  EmotionType last_emotion;
  MoodType    last_mood;
  uint8_t     last_engagement;
};

// ── Animation Intelligence (V2.8 Sprint 2) ──────────────────────
// PresentationTarget — output contract of Stage 1 (Presentation Intelligence)
// Produced additively alongside PresentationState; consumed by Stage 2.
struct PresentationTarget {
  uint8_t  eye_shape;
  uint8_t  brow_style;
  uint8_t  pupil_style;
  uint16_t color;           // Fully resolved (interpolated) RGB565
  uint8_t  anim_profile_id;
};

// EyePhysics — spring-damper state owned by Animation Intelligence (Stage 2)
struct EyePhysics {
  float current_x;
  float current_y;
  float vel_x;
  float vel_y;
  float target_x;
  float target_y;
};

// TransitionPhase — physical animation stages during emotion/variant transitions
enum class TransitionPhase : uint8_t {
  IDLE,
  ANTICIPATE,
  TRANSITION,
  SETTLE
};

// AnimationPose — Stage 2 output cache, Stage 3 read-only input
// All fields are integer; no floating-point values leak to the renderer.
struct EyePose {
  int x;              // Eye center X (screen pixel)
  int y;              // Eye center Y (screen pixel, includes bounce)
  int height;         // Modulated vertical size (blink/breathing/phase-resolved)
  int pupil_x;        // Pupil X offset within eye (from spring-damper physics)
  int pupil_y;        // Pupil Y offset within eye
  uint8_t shape;       // Resolved eye shape index
  uint8_t pupil_style; // Resolved pupil style index
};

struct BrowPose {
  int y_offset;       // Vertical adjustment (bounce + breathing-resolved)
  uint8_t style;      // Resolved brow style
};

struct GlobalPose {
  int bounce_y;       // Vertical shift (bounce-resolved)
  uint16_t color;     // Interpolated RGB565 color
};

struct AnimationPose {
  EyePose left_eye;
  EyePose right_eye;
  BrowPose brow;
  GlobalPose global;
};

// V2.8 Sprint 3 — Micro-Expression types
enum class MicroExpression : uint8_t {
  NONE,
  DOUBLE_BLINK,
  SQUINT,
  PUPIL_FLICK,
  BROW_TWITCH
};

// ── Face Engine v2 ───────────────────────────────────────────────
class FaceEngineClass {
private:
  float anim_blink_scale = 1.0f;
  uint32_t anim_blink_accum_ms = 0;  // ms accumulator for time-based blink
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

  // Mood-derived presentation modifiers (Sprint 3B)
  struct MoodPresentation {
    uint32_t blink_interval_ms = 4000;
    float idle_amplitude = 1.0f;
    float idle_speed = 1.0f;
    float eye_intensity = 1.0f;
  };
  MoodPresentation _mood_pres;
  MoodType _last_mood = MOOD_RELAXED;
  uint8_t _last_mood_strength = 0;

  float _eyelid_factor = 1.0f;
  int _deep_blink_hold = 0;
  void recomputeMoodPresentation();

  // Presentation Intelligence (V2.8 Sprint 1)
  PresentationState _pres_state;
  void updatePresentation();
  uint8_t selectProfile(EmotionType emotion);
  uint8_t activityToBucket(ActivityType a);
  // xorshift32 mutates seed by reference. Every invocation advances the deterministic RNG state.
  static uint32_t xorshift32(uint32_t& seed);
  static const PresentationProfile* profileTable();
  static int profileCountFor(EmotionType e);

  // Animation Intelligence (V2.8 Sprint 2)
  PresentationTarget _pres_target;       // Stage 1 output contract (additive)
  EyePhysics _eye_physics;               // Spring-damper state
  AnimationPose _anim_pose;              // Stage 2→3 cache (integer only)
  TransitionPhase _transition_phase = TransitionPhase::IDLE;
  uint16_t _transition_timer_ms = 0;     // ms remaining in current phase
  float _breath_phase = 0.0f;            // 0..2π breathing accumulator
  float _breath_accum = 0.0f;            // Sub-pixel breathing fraction
  float _recovery_modifier = 0.0f;       // 1.0→0.0 emotional after-effect
  uint32_t _recovery_start_ms = 0;       // When recovery began
  EmotionType _last_high_emotion = EMOTION_CALM;
  uint32_t _last_frame_us = 0;           // Wall-clock dt tracking
  bool _focus_locked = false;            // Hysteresis state

  // Micro-Expression state (V2.8 Sprint 3)
  MicroExpression _active_micro = MicroExpression::NONE;
  uint32_t _micro_timer_ms = 0;        // Absolute expiration ms (uint32_t for rollover safety)
  uint8_t _micro_phase = 0;            // Phase within active micro-expression
  int8_t _micro_mod_x = 0;             // Pupil X delta (pupil flick)
  int8_t _micro_mod_y = 0;             // Pupil Y delta (pupil flick)
  int8_t _micro_brow_delta = 0;        // Brow Y delta (brow twitch)
  uint32_t _micro_rng_seed = 0;          // Deterministic RNG seed for micro mods
  // Decision-layer edge-detection state (owned by detectMicroTriggers)
  EmotionType _micro_prev_emotion = EMOTION_CALM;
  AttentionTarget _micro_prev_target = TARGET_NONE;
  uint8_t _micro_prev_strength = 0;

  // Animation Intelligence helpers
  void integrateSpringDamper(float dt);
  float computeBreathValue() const;
  void advanceTransitionPhase(uint32_t dt_ms);
  void updateRecovery(uint32_t now);
  void produceAnimationPose();
  void updateMicroExpressions(uint32_t now);  // V2.8 Sprint 3 — execution only
  void detectMicroTriggers();                 // V2.8 Sprint 3 — decision layer

public:
  void begin();
  void updateAnimations(int frame);
  void render(TFT_eSprite* spr, int frame);
  
  // Micro-expression API (V2.8 Sprint 3)
  void queueMicroExpression(MicroExpression type);
  
  // Decoupled architecture layers
  void renderEmotionLayer(TFT_eSprite* spr, int frame);
  void renderActivityLayer(TFT_eSprite* spr, int frame);
  void renderEffectLayer(TFT_eSprite* spr, int frame);
  void renderDebugOverlay(TFT_eSprite* spr);
  
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
class MemoryEngineClass;
extern MemoryEngineClass MemoryEngine;
class AttentionEngineClass;
extern AttentionEngineClass AttentionEngine;
class PersistenceService;
extern PersistenceService CreaturePersistence;
extern EventBusClass EventBus;
extern EmotionEngineClass EmotionEngine;
extern WiFiServiceClass WiFiService;
extern DisplayServiceClass DisplayService;
extern StorageServiceClass StorageService;
extern FlightStatsClass FlightStats;
extern uint32_t sys_avi_refresh;

#endif // AEROSNIFFEROS_H

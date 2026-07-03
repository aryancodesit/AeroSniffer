// AeroSnifferOS.cpp  —  OS Layer: FaceEngine, Storage, WiFi, Display services
#include "AeroSnifferOS.h"
#include "Config.h"
#include "Companion/AttentionEngine.h"
#include "Companion/MoodEngine.h"
#include "Memory/MemoryEngine.h"
#include <math.h>

extern TFT_eSPI tft;

// V2.8 Sprint 2 — Animation Intelligence constants
static constexpr float SPRING_TENSION  = 0.15f;
static constexpr float SPRING_DAMPING  = 0.40f;
static constexpr float SPRING_SETTLE   = 0.2f;
static constexpr float MICRO_DRIFT_AMP = 2.0f;
static constexpr float BREATH_CYCLE_MS = 6000.0f; // 6-second full breath
static constexpr uint8_t FOCUS_LOCK_ON  = 25;
static constexpr uint8_t FOCUS_LOCK_OFF = 20;
static constexpr uint16_t ANTICIPATE_MS = 50;
static constexpr uint16_t TRANSITION_MS = 150;
static constexpr uint16_t SETTLE_MS     = 100;
static constexpr uint16_t RECOVERY_BROW_MS = 1500;
static constexpr uint16_t RECOVERY_EYELID_MS = 2000;
static constexpr uint16_t MIN_SACCADE_HOLD_MS = 1000;

// V2.8 Sprint 3 — Micro-Expression thresholds
static constexpr uint16_t DOUBLE_BLINK_DURATION_MS  = 350;
static constexpr uint16_t SQUINT_DURATION_MS         = 250;
static constexpr uint16_t PUPIL_FLICK_DURATION_MS    = 150;
static constexpr uint16_t BROW_TWITCH_DURATION_MS    = 200;
static constexpr uint8_t  SQUINT_THRESHOLD            = 20;
static constexpr uint8_t  BROW_TWITCH_MIN             = 5;
static constexpr uint8_t  BROW_TWITCH_MAX             = 15;
static constexpr int8_t   PUPIL_FLICK_RANGE           = 2;
static constexpr int8_t   BROW_TWITCH_RANGE           = 2;

// Forward declarations for functions used before defined
static uint16_t lerpColor(uint16_t from, uint16_t to, float t);

// Mood profile accessor (Sprint 3B)
// Future-proof: adding a new mood type requires only a new case here.
struct MoodProfile {
  uint32_t blink_ms;
  float    amp;
  float    speed;
  float    intens;
};

static MoodProfile profileFor(MoodType mood) {
  switch (mood) {
    case MOOD_PLAYFUL: return {3000, 1.5f, 1.3f, 1.4f};
    case MOOD_ANXIOUS: return {2200, 0.5f, 0.8f, 0.6f};
    default:           return {4000, 1.0f, 1.0f, 1.0f};  // RELAXED + unknown/future moods degrade gracefully
  }
}

// ── Face Engine Colors ────────────────────────────────────────────
#define C_BG          0x10A2
#define C_BLACK       0x0000
#define C_IDLE        0x05FF
#define C_HAPPY       0x07E0
#define C_EXCITED     0xFFE0
#define C_SLEEPY      0x421F
#define C_THINKING    0xB31F
#define C_SAD         0xF800
#define C_ALERT       0xFC00
#define C_LOVE        0xF81F
#define C_STARTUP     0x07FF
#define C_SURPRISED   0xFFFF

// ── Presentation Intelligence (V2.8 Sprint 1) ────────────────────

// AnimationProfile table — multipliers on mood baseline (in flash)
static constexpr uint8_t PRES_ANIM_COUNT = 6;
static const AnimationProfile kAnimProfiles[PRES_ANIM_COUNT] = {
  {128, 4, 128, 128},  // 0: normal — 1.0x blink, 4px gaze, 1.0x speed, 1.0x bounce
  { 90, 6, 166, 154},  // 1: alert — 0.7x blink, 6px gaze, 1.3x speed, 1.2x bounce
  {166, 3, 102, 102},  // 2: calm — 1.3x blink, 3px gaze, 0.8x speed, 0.8x bounce
  {256, 2,  64,  64},  // 3: sleepy — 2.0x blink, 2px gaze, 0.5x speed, 0.5x bounce
  {128, 5, 128,  77},  // 4: focused — 1.0x blink, 5px gaze, 1.0x speed, 0.6x bounce
  { 77, 7, 192, 192},  // 5: energetic — 0.6x blink, 7px gaze, 1.5x speed, 1.5x bounce
};

// Profile table (flat 1D): kVariantTable[emotion * MAX_VARIANTS + variant]
// 0 = use emotion default color (-1 sentinel for array)
static const uint8_t PRES_EMO_COLORS[EMOTION_COUNT] = {
  6, 1, 2, 3, 4, 5, 3, 7, 8, 9  // CALM→1, HAPPY→2, SAD→3, etc.
};
// color index → actual 16-bit colors
static uint16_t presColor(uint8_t idx) {
  switch (idx) {
    case 1: return C_HAPPY;
    case 2: return C_SAD;
    case 3: return C_ALERT;
    case 4: return C_THINKING;
    case 5: return C_SLEEPY;
    case 6: return C_IDLE;
    case 7: return C_LOVE;
    case 8: return C_SURPRISED;
    case 9: return C_EXCITED;
    default: return C_IDLE;
  }
}

static constexpr uint8_t MAX_VARIANTS = 3;
// Flattened 1D table: index = emotion * MAX_VARIANTS + variant
// (ESP32 GCC 3.3.10 fails on nested brace-init of 2D arrays with sub-array members)
static const PresentationProfile kVariantTable[EMOTION_COUNT * MAX_VARIANTS] = {
  // EMOTION_CALM = 0
  {0, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 1, 0, 1},  0,100, PRES_CONTEXT_CALM,  3000},
  {1, PRES_EYE_ARCHED,  PRES_BROW_NORMAL,   PRES_PUPIL_HALF,        0,2, { 2, 0, 0},  0, 50, PRES_CONTEXT_CALM,  3000},
  {2, PRES_EYE_ROUND,   PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 0, 0, 0}, 20,100, PRES_CONTEXT_ANY,   3000},
  // EMOTION_HAPPY = 1
  {0, PRES_EYE_ARCHED,  PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,5, { 0, 2, 0}, 40,100, PRES_CONTEXT_SOCIAL, 2500},
  {1, PRES_EYE_WIDE,    PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,5, {-1, 2, 0}, 60,100, PRES_CONTEXT_ACTIVE, 2000},
  {2, PRES_EYE_ARCHED,  PRES_BROW_NORMAL,   PRES_PUPIL_HALF,        0,2, { 1, 0, 0},  0, 40, PRES_CONTEXT_CALM,  3000},
  // EMOTION_SAD = 2
  {0, PRES_EYE_ROUND,   PRES_BROW_WORRIED,  PRES_PUPIL_CONSTRICTED, 0,2, { 1, 0, 0},  0,100, PRES_CONTEXT_ANY,   3500},
  {1, PRES_EYE_FLAT,    PRES_BROW_NORMAL,   PRES_PUPIL_HALF,        0,3, { 2, 0, 0},  0, 50, PRES_CONTEXT_CALM,  3500},
  {2, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 0, 0, 0}, 30,100, PRES_CONTEXT_FOCUSED,3000},
  // EMOTION_ANGRY = 3
  {0, PRES_EYE_SQUINT,  PRES_BROW_FURROWED, PRES_PUPIL_CONSTRICTED, 0,1, { 0, 0, 2}, 30,100, PRES_CONTEXT_ANY,   3000},
  {1, PRES_EYE_ROUND,   PRES_BROW_FURROWED, PRES_PUPIL_NORMAL,      0,1, { 0, 0, 1}, 50,100, PRES_CONTEXT_ACTIVE, 2500},
  {2, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_CONSTRICTED, 0,1, { 1, 0, 0},  0, 40, PRES_CONTEXT_CALM,  3000},
  // EMOTION_CURIOUS = 4
  {0, PRES_EYE_ROUND,   PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,4, { 0, 1, 0}, 40,100, PRES_CONTEXT_FOCUSED,2500},
  {1, PRES_EYE_SQUINT,  PRES_BROW_NORMAL,   PRES_PUPIL_CONSTRICTED, 0,4, { 0, 0, 0}, 30,100, PRES_CONTEXT_FOCUSED,3000},
  {2, PRES_EYE_WIDE,    PRES_BROW_ONE,      PRES_PUPIL_DILATED,     0,0, { 1, 1, 1},  0,100, PRES_CONTEXT_SOCIAL, 2500},
  // EMOTION_SLEEPY = 5
  {0, PRES_EYE_FLAT,    PRES_BROW_NORMAL,   PRES_PUPIL_HALF,        0,3, { 2, 0, 0},  0, 30, PRES_CONTEXT_CALM,  4000},
  {1, PRES_EYE_FLAT,    PRES_BROW_NORMAL,   PRES_PUPIL_NONE,        0,3, { 2, 0, 0},  0, 10, PRES_CONTEXT_CALM,  4000},
  {2, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 0, 0, 0}, 40,100, PRES_CONTEXT_ANY,   3000},
  // EMOTION_ALERT = 6
  {0, PRES_EYE_WIDE,    PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,1, { 0, 0, 2}, 50,100, PRES_CONTEXT_ANY,   2000},
  {1, PRES_EYE_ROUND,   PRES_BROW_FURROWED, PRES_PUPIL_NORMAL,      0,1, { 0, 0, 2}, 30,100, PRES_CONTEXT_FOCUSED,2500},
  {2, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 1, 0, 0},  0, 40, PRES_CONTEXT_CALM,  3000},
  // EMOTION_LOVE = 7
  {0, PRES_EYE_ARCHED,  PRES_BROW_NORMAL,   PRES_PUPIL_DILATED,     0,5, { 1, 2, 0}, 30,100, PRES_CONTEXT_SOCIAL, 2500},
  {1, PRES_EYE_ROUND,   PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 2, 0, 0},  0, 50, PRES_CONTEXT_CALM,  3000},
  {2, PRES_EYE_WIDE,    PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,5, { 0, 2, 0}, 60,100, PRES_CONTEXT_ACTIVE, 2000},
  // EMOTION_SURPRISED = 8
  {0, PRES_EYE_WIDE,    PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,5, { 0, 1, 0}, 30,100, PRES_CONTEXT_ANY,   2000},
  {1, PRES_EYE_ROUND,   PRES_BROW_RAISED,   PRES_PUPIL_CONSTRICTED, 0,5, { 0, 1, 0}, 50,100, PRES_CONTEXT_ANY,   2000},
  {2, PRES_EYE_DEFAULT, PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,2, { 1, 0, 0},  0, 30, PRES_CONTEXT_CALM,  3000},
  // EMOTION_EXCITED = 9
  {0, PRES_EYE_WIDE,    PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,5, { 0, 2, 0}, 50,100, PRES_CONTEXT_ACTIVE, 2000},
  {1, PRES_EYE_ROUND,   PRES_BROW_NORMAL,   PRES_PUPIL_NORMAL,      0,1, { 1, 1, 1}, 20,100, PRES_CONTEXT_SOCIAL, 2500},
  {2, PRES_EYE_ARCHED,  PRES_BROW_RAISED,   PRES_PUPIL_DILATED,     0,5, { 0, 2, 0},  0, 60, PRES_CONTEXT_ANY,   2500},
};

// Inline index helper for flattened 1D variant table
static inline int presIdx(EmotionType e, uint8_t v) { return (int)e * (int)MAX_VARIANTS + v; }

static int variantCountFor(EmotionType e) {
  (void)e;
  return 3; // all emotions have 3 variants for now
}

// Deterministic PRNG — xorshift32, mutates seed by reference.
// Every invocation advances the internal state.
uint32_t FaceEngineClass::xorshift32(uint32_t& seed) {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

// Map ActivityType to generic context bucket
uint8_t FaceEngineClass::activityToBucket(ActivityType a) {
  switch (a) {
    case ACTIVITY_SLEEPING: return PRES_CONTEXT_CALM;
    case ACTIVITY_IDLE:     return PRES_CONTEXT_CALM;
    case ACTIVITY_TYPING:   return PRES_CONTEXT_ACTIVE;
    case ACTIVITY_CODING:   return PRES_CONTEXT_FOCUSED;
    case ACTIVITY_THINKING: return PRES_CONTEXT_FOCUSED;
    case ACTIVITY_WATCHING_FLIGHTS: return PRES_CONTEXT_SOCIAL;
    case ACTIVITY_SCANNING_NETWORKS: return PRES_CONTEXT_SOCIAL;
    case ACTIVITY_MUSIC:    return PRES_CONTEXT_SOCIAL;
    default:                return PRES_CONTEXT_ANY;
  }
}

const PresentationProfile* FaceEngineClass::profileTable() {
  return kVariantTable;
}

int FaceEngineClass::profileCountFor(EmotionType e) {
  if (e >= EMOTION_COUNT) return 0;
  return variantCountFor(e);
}

// Score and select a profile for the given emotion (integer arithmetic)
uint8_t FaceEngineClass::selectProfile(EmotionType emotion) {
  int count = profileCountFor(emotion);
  if (count == 0) return 0;

  uint8_t bucket = activityToBucket(g_creature.activity);
  // scores are Q8 (256 = 1.0) to avoid floating point
  int scores[MAX_VARIANTS];
  int total = 0;

  for (int i = 0; i < count; i++) {
    const PresentationProfile& p = kVariantTable[presIdx(emotion, i)];
    int s = 256; // base score (1.0 in Q8)

    // Mood affinity: −3..+3 scaled by mood_strength (0..100)
    // aff * 256 / 3 * strength / 100 = aff * strength * 256 / 300
    s += ((int)p.mood_affinity[g_creature.mood] * (int)g_creature.mood_strength * 256) / 300;

    // Engagement range: +512 if inside, −256 if outside (Q8: +2.0 / −1.0)
    if (g_creature.engagement_drive >= p.engagement_min &&
        g_creature.engagement_drive <= p.engagement_max) {
      s += 512;
    } else {
      s -= 256;
    }

    // Context bucket: +384 if matched (Q8: +1.5)
    if (p.context_bucket == bucket || p.context_bucket == PRES_CONTEXT_ANY) {
      s += 384;
    }

    if (s < 1) s = 1; // ensure positive for weighted pick
    scores[i] = s;
    total += s;
  }

  // Deterministic weighted pick (integer range)
  uint32_t r = xorshift32(_pres_state.selection_seed) % (uint32_t)total;
  int cumulative = 0;
  for (int i = 0; i < count; i++) {
    cumulative += scores[i];
    if ((int)r < cumulative) return i;
  }
  return count - 1;
}

// Update presentation state — variant selection + transition
void FaceEngineClass::updatePresentation() {
  // Check for emotion change
  bool emotionChanged = (g_creature.emotion != _pres_state.last_emotion);

  // Check for context shift (mood or engagement threshold crossing)
  bool moodChanged = (g_creature.mood != _pres_state.last_mood);
  // Engagement band: LOW (0-33), MED (34-66), HIGH (67-100)
  uint8_t band = g_creature.engagement_drive / 34;
  if (band > 2) band = 2;
  uint8_t lastBand = _pres_state.last_engagement / 34;
  if (lastBand > 2) lastBand = 2;
  bool engagementShifted = (band != lastBand);

  if (emotionChanged) {
    // Emotion change — restart transition
    _pres_state.last_emotion = g_creature.emotion;
    _pres_state.last_mood = g_creature.mood;
    _pres_state.last_engagement = g_creature.engagement_drive;
    uint8_t new_idx = selectProfile(g_creature.emotion);
    _pres_state.pending_profile = new_idx;
    _pres_state.transition_progress = 0;
    _pres_state.hold_remaining_ms = kVariantTable[presIdx(g_creature.emotion, new_idx)].hold_ms;
    return;
  }

  // Decrement hold timer
  if (_pres_state.hold_remaining_ms > 16) { // approximate frame delta
    _pres_state.hold_remaining_ms -= 16;
  } else {
    _pres_state.hold_remaining_ms = 0;
  }

  // Advance transition
  if (_pres_state.transition_progress < 255) {
    _pres_state.transition_progress += 8; // ~250ms at 30fps
    if (_pres_state.transition_progress >= 255) {
      _pres_state.transition_progress = 255;
      _pres_state.current_profile = _pres_state.pending_profile;
    }
    return; // don't re-select during transition
  }

  // Re-select on context shift or hold expiry
  if (moodChanged || engagementShifted || _pres_state.hold_remaining_ms == 0) {
    _pres_state.last_mood = g_creature.mood;
    _pres_state.last_engagement = g_creature.engagement_drive;
    uint8_t new_idx = selectProfile(g_creature.emotion);
    if (new_idx != _pres_state.current_profile) {
      _pres_state.pending_profile = new_idx;
      _pres_state.transition_progress = 0;
      _pres_state.hold_remaining_ms = kVariantTable[presIdx(g_creature.emotion, new_idx)].hold_ms;
    } else if (_pres_state.hold_remaining_ms == 0) {
      // Same profile selected — give it minimum hold to avoid re-picking every frame
      _pres_state.hold_remaining_ms = kVariantTable[presIdx(g_creature.emotion, new_idx)].hold_ms;
    }
  }

  // ── Produce PresentationTarget (V2.8 Sprint 2, additive) ──────
  EmotionType _emo = _pres_state.last_emotion;
  uint8_t _cur_idx = _pres_state.current_profile;
  uint8_t _pen_idx = _pres_state.pending_profile;
  float _t = min(1.0f, _pres_state.transition_progress / 255.0f);
  const PresentationProfile& _curP = kVariantTable[presIdx(_emo, _cur_idx)];
  const PresentationProfile& _penP = kVariantTable[presIdx(_emo, _pen_idx)];
  uint16_t _color_cur = presColor(_curP.color_override ? _curP.color_override : PRES_EMO_COLORS[_emo]);
  uint16_t _color_pen = presColor(_penP.color_override ? _penP.color_override : PRES_EMO_COLORS[_emo]);
  _pres_target.color = (_t >= 1.0f) ? _color_pen : lerpColor(_color_cur, _color_pen, _t);
  _pres_target.eye_shape = _penP.eye_shape;
  _pres_target.brow_style = _penP.brow_style;
  _pres_target.pupil_style = _penP.pupil_style;
  _pres_target.anim_profile_id = _penP.anim_profile_id;
}

// ── Central State Definition ─────────────────────────────────────
CreatureState g_creature = {
  EMOTION_CALM,
  MOOD_RELAXED,
  0,     // mood_strength
  ACTIVITY_IDLE,
  false, // WiFi connected
  0,     // Uptime minutes
  0,     // Coding minutes
  0,     // Gaming minutes
  0,     // Music minutes
  0,     // Flights seen today
  0,     // Networks seen today
  0,     // Interactions today
  0,     // Friendship level
  0,     // pc_cpu
  0,     // pc_ram
  100    // engagement_drive — start fully alert
};

// ── Global instances ─────────────────────────────────────────────
EventBusClass EventBus;
TimelineClass Timeline;
FaceEngineClass FaceEngine;
AttentionEngineClass AttentionEngine;
MoodEngineClass MoodEngine;
WiFiServiceClass WiFiService;
DisplayServiceClass DisplayService;
StorageServiceClass StorageService;
FlightStatsClass FlightStats;
uint32_t sys_avi_refresh = 30000;

// ── Event Bus Implementation ────────────────────────────────────
void EventBusClass::subscribe(EventType event, EventCallback cb) {
  if (sub_count >= MAX_SUBSCRIBERS) {
    Serial.println("[EVENTBUS] WARN: exceeded MAX_SUBSCRIBERS (32)");
    return;
  }
  for (int i = 0; i < sub_count; i++) {
    if (subs[i].event == event && subs[i].cb == cb) return;
  }
  subs[sub_count++] = {event, cb};
}

void EventBusClass::publish(EventType event, void* eventData) {
  for (int i = 0; i < sub_count; i++) {
    if (subs[i].event == event) {
      subs[i].cb(event, eventData);
    }
  }
}

// ── Timeline Class Implementation ───────────────────────────────
void TimelineClass::logEvent(const char* text, const char* type) {
  uint32_t ts = millis() / 1000;
  TimelineEvent& ev = events[head];
  ev.time_sec = ts;
  strncpy(ev.text, text, sizeof(ev.text) - 1);
  ev.text[sizeof(ev.text) - 1] = '\0';
  strncpy(ev.type, type, sizeof(ev.type) - 1);
  ev.type[sizeof(ev.type) - 1] = '\0';
  
  head = (head + 1) % 100;
  if (count < 100) count++;
  
  // Direct event stream output over serial for immediate console telemetry updates
  Serial.printf("EVT:{\"type\":\"timeline\",\"text\":\"%s\",\"tag\":\"%s\"}\n", text, type);

  // Link timeline event directly to TFT status lines
  FaceEngine.setStatusLines(text, type);
}

void TimelineClass::clear() {
  count = 0;
  head = 0;
}

void TimelineClass::getEvent(int idx, uint32_t& ts, String& txt, String& ty) const {
  if (idx < 0 || idx >= count) return;
  int real_idx = (head - count + idx + 100) % 100;
  ts = events[real_idx].time_sec;
  txt = String(events[real_idx].text);
  ty = String(events[real_idx].type);
}

void TimelineClass::sendSerial() {
  Serial.print("RES:{\"timeline\":[");
  for (int i = 0; i < count; i++) {
    int idx = (head - count + i + 100) % 100;
    if (i > 0) Serial.print(",");
    Serial.printf("{\"ts\":%lu,\"text\":\"%s\",\"type\":\"%s\"}",
                  events[idx].time_sec, events[idx].text, events[idx].type);
  }
  Serial.println("]}");
}

// ── Emotion Engine Implementation ────────────────────────────────
EmotionEngineClass EmotionEngine;

void EmotionEngineClass::begin() {
  last_activity_change = millis();
  last_emotion_decay = millis();
  last_active_sec = millis();
  transient_until = 0;

  // Force IDLE on boot
  current_emotion = EMOTION_CALM;
  current_activity = ACTIVITY_IDLE;
  g_creature.emotion = EMOTION_CALM;
  g_creature.activity = ACTIVITY_IDLE;

  // Initialize flight statistics service
  FlightStats.begin();

  // Load persistent stats into g_creature
  g_creature.friendship_level = StorageService.getStat("friendship", 50);
  g_creature.flights_seen_today = StorageService.getStat("td_fl", 0);
  g_creature.networks_seen_today = StorageService.getStat("td_net", 0);
  g_creature.interactions_today = StorageService.getStat("td_int", 0);
  g_creature.coding_minutes = StorageService.getStat("td_cod", 0);
  g_creature.gaming_minutes = StorageService.getStat("td_gam", 0);
  g_creature.music_minutes = StorageService.getStat("td_mus", 0);
  g_creature.uptime_minutes = StorageService.getStat("uptime_min", 0);
  
  // Register single central brain callback subscribing to ALL events
  for (int i = 0; i < EVENT_COUNT; i++) {
    EventBus.subscribe((EventType)i, [](EventType ev, void* d) {
      EmotionEngine.handleEvent(ev, d);
    });
  }
  
  Serial.println("[EMOTION] Boot -> IDLE");
  Timeline.logEvent("Creature Awakened", "system");
}

void EmotionEngineClass::handleEvent(EventType event, void* data) {
  uint32_t now = millis();

  // Debounce: skip events less than 1s apart unless they are different types
  if (now - last_emotion_decay < EMOTION_DEBOUNCE_MS) {
    return;
  }

  switch(event) {
    case EVENT_PC_CODING:
      setActivity(ACTIVITY_CODING);
      setEmotion(EMOTION_CALM);
      transient_until = 0;
      Timeline.logEvent("Started Coding", "activity");
      g_creature.interactions_today++;
      StorageService.incrementStat("td_int", 1);
      g_creature.friendship_level = std::min<uint32_t>(1000U, g_creature.friendship_level + 1);
      StorageService.setStat("friendship", g_creature.friendship_level);
      break;
      
    case EVENT_PC_GAMING:
      setActivity(ACTIVITY_TYPING);
      setEmotion(EMOTION_EXCITED);
      transient_until = now + 5000;
      Timeline.logEvent("Started Gaming", "activity");
      g_creature.interactions_today++;
      StorageService.incrementStat("td_int", 1);
      break;

    case EVENT_PC_MUSIC:
      setActivity(ACTIVITY_MUSIC);
      setEmotion(EMOTION_HAPPY);
      transient_until = now + 5000;
      Timeline.logEvent("Listening to Music", "activity");
      break;

    case EVENT_PC_TYPING:
      setActivity(ACTIVITY_TYPING);
      setEmotion(EMOTION_CURIOUS);
      transient_until = now + 8000;
      Serial.println("[PC] Typing start");
      Timeline.logEvent("Typing detected", "activity");
      break;

    case EVENT_PC_IDLE:
      setActivity(ACTIVITY_IDLE);
      setEmotion(EMOTION_CALM);
      transient_until = 0;
      Timeline.logEvent("PC Idle State", "activity");
      break;

    case EVENT_FLIGHT_DETECTED: {
      int count = data ? *(int*)data : 0;
      
      // Update statistics
      FlightStats.recordFetch(count);
      if (count > 0) {
        g_creature.flights_seen_today += count;
        StorageService.incrementStat("td_fl", count);
        StorageService.incrementStat("flights_seen", count);
        FlightStats.incrementSeen(count);
      }

      setActivity(ACTIVITY_WATCHING_FLIGHTS);
      setEmotion(EMOTION_CURIOUS);
      transient_until = now + 8000;

      if (count == 0) {
        Timeline.logEvent("Watching airspace: clear", "flight");
      }
      else if (count >= 1 && count <= 5) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Airspace: %d flights", count);
        Timeline.logEvent(buf, "flight");
      }
      else if (count >= 6 && count <= 20) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Airspace: %d flights", count);
        Timeline.logEvent(buf, "flight");
      }
      else {
        char buf[48];
        snprintf(buf, sizeof(buf), "Airspace: %d flights", count);
        Timeline.logEvent(buf, "flight");
      }
      break;
    }

    case EVENT_FLIGHT_RARE:
      setEmotion(EMOTION_SURPRISED);
      transient_until = now + 5000;
      Timeline.logEvent("Rare flight detected!", "flight");
      break;

    case EVENT_ATTACK_DEAUTH:
      setActivity(ACTIVITY_SCANNING_NETWORKS);
      setEmotion(EMOTION_ALERT);
      transient_until = now + 5000;
      Timeline.logEvent("Deauth attack alert!", "network");
      break;

    case EVENT_ATTACK_EVILTWIN:
      setActivity(ACTIVITY_SCANNING_NETWORKS);
      setEmotion(EMOTION_ALERT);
      transient_until = now + 5000;
      Timeline.logEvent("Evil twin AP detected!", "network");
      break;

    case EVENT_NETWORK_DISCOVERED: {
      g_creature.networks_seen_today++;
      StorageService.incrementStat("td_net", 1);
      StorageService.incrementStat("nets_seen", 1);
      setEmotion(EMOTION_ALERT);
      transient_until = now + 5000;
      Timeline.logEvent("Discovered AP", "network");
      break;
    }

    case EVENT_DEVICE_TRUSTED:
      setEmotion(EMOTION_HAPPY);
      transient_until = now + 5000;
      Timeline.logEvent("Trusted device detected", "security");
      break;

    case EVENT_DEVICE_FAMILIAR:
      setEmotion(EMOTION_HAPPY);
      transient_until = now + 5000;
      Timeline.logEvent("Familiar device detected", "security");
      break;

    case EVENT_DEVICE_UNKNOWN:
      setEmotion(EMOTION_CURIOUS);
      transient_until = now + 5000;
      Timeline.logEvent("Unknown device detected", "security");
      break;

    case EVENT_TOUCH_SHORT:
    case EVENT_USER_TAP:
      // Random interaction emotion
      {
        EmotionType tapEmotions[] = {EMOTION_HAPPY, EMOTION_LOVE, EMOTION_EXCITED, EMOTION_SURPRISED};
        int idx = random(0, 4);
        setEmotion(tapEmotions[idx]);
      }
      transient_until = now + 5000;
      g_creature.interactions_today++;
      StorageService.incrementStat("td_int", 1);
      g_creature.friendship_level = std::min<uint32_t>(1000U, g_creature.friendship_level + 2);
      StorageService.setStat("friendship", g_creature.friendship_level);
      Timeline.logEvent("User tap interaction", "touch");
      FaceEngine.triggerParticle(3);
      break;

    case EVENT_TOUCH_LONG:
    case EVENT_USER_LONG_TAP:
      setEmotion(EMOTION_SURPRISED);
      transient_until = now + 5000;
      Timeline.logEvent("Mode switch triggered", "touch");
      break;

    case EVENT_WIFI_CONNECTED:
      g_creature.wifi_connected = true;
      Timeline.logEvent("WiFi Connected", "system");
      break;

    case EVENT_WIFI_DISCONNECTED:
      g_creature.wifi_connected = false;
      Timeline.logEvent("WiFi Disconnected", "system");
      break;

    case EVENT_MODE_SWITCHED: {
      int mode = data ? *(int*)data : 0;
      char buf[32];
      snprintf(buf, sizeof(buf), "Switched to Mode %d", mode + 1);
      Timeline.logEvent(buf, "system");
      break;
    }
    
    default:
      break;
  }
}

void EmotionEngineClass::tick() {
  uint32_t now = millis();

  // 1. Transient emotion timeout: return to IDLE after timeout expires
  if (transient_until > 0 && now > transient_until && current_emotion != EMOTION_CALM) {
    forceIdle();
    transient_until = 0;
  }

  // 2. Uptime statistics & persistent active tracking (every 60 seconds)
  if (now - last_active_sec >= 60000) {
    last_active_sec = now;
    g_creature.uptime_minutes++;
    StorageService.setStat("uptime_min", g_creature.uptime_minutes);
    
    // Increment specific activity times
    if (current_activity == ACTIVITY_CODING) {
      g_creature.coding_minutes++;
      StorageService.incrementStat("td_cod", 1);
      StorageService.incrementStat("code_time", 1);
    } else if (current_activity == ACTIVITY_MUSIC) {
      g_creature.music_minutes++;
      StorageService.incrementStat("td_mus", 1);
    } else if (current_activity == ACTIVITY_TYPING) {
      g_creature.gaming_minutes++;
      StorageService.incrementStat("td_gam", 1);
    }
    
    StorageService.incrementStat("active_hrs", 1); // active duration log
    
    // Friendship level increases with uptime (1 level every 10 mins)
    if (g_creature.uptime_minutes % 10 == 0) {
      g_creature.friendship_level = std::min<uint32_t>(1000U, g_creature.friendship_level + 1);
      StorageService.setStat("friendship", g_creature.friendship_level);
    }

    // Daily rollover logic based on uptime (simulated every 24 active hours if NTP is offline)
    if (g_creature.uptime_minutes >= 1440) {
      g_creature.uptime_minutes = 0;
      g_creature.flights_seen_today = 0;
      g_creature.networks_seen_today = 0;
      g_creature.interactions_today = 0;
      g_creature.coding_minutes = 0;
      g_creature.gaming_minutes = 0;
      g_creature.music_minutes = 0;
      StorageService.setStat("td_fl", 0);
      StorageService.setStat("td_net", 0);
      StorageService.setStat("td_int", 0);
      StorageService.setStat("td_cod", 0);
      StorageService.setStat("td_gam", 0);
      StorageService.setStat("td_mus", 0);
      StorageService.setStat("uptime_min", 0);
      FlightStats.resetDaily();
      Timeline.logEvent("New Day Rollover", "system");
    }
  }

  // 3. Activity timeouts (return to IDLE if no activity update in 60 seconds)
  //    Only applies if no transient emotion is active
  if (transient_until == 0 && current_activity != ACTIVITY_IDLE && current_activity != ACTIVITY_SLEEPING && (now - last_activity_change > 60000)) {
    forceIdle();
  }
}

void EmotionEngineClass::setEmotion(EmotionType e) {
  if (current_emotion != e) {
    Serial.printf("[EMOTION] %s -> %s\n", emotionToDisplayName(current_emotion), emotionToDisplayName(e));
    current_emotion = e;
    g_creature.emotion = e;
    last_emotion_decay = millis();

    // Map core emotions to Web FaceState names
    const char* face_names[] = {
      "happy", "sad", "sec_intrusion", "thinking", "sleepy",
      "idle", "sec_intrusion", "love", "surprised", "excited"
    };
    if (e < EMOTION_COUNT) {
      sys_current_face = face_names[e];
    }
  }
}

void EmotionEngineClass::setActivity(ActivityType a) {
  if (current_activity != a) {
    current_activity = a;
    g_creature.activity = a;
    last_activity_change = millis();
  }
}

void EmotionEngineClass::forceIdle() {
  if (current_emotion != EMOTION_CALM) {
    if (current_emotion == EMOTION_CURIOUS) {
      Serial.println("[PC] Typing stop");
    }
    Serial.printf("[EMOTION] %s -> IDLE (timeout)\n", emotionToDisplayName(current_emotion));
    current_emotion = EMOTION_CALM;
    g_creature.emotion = EMOTION_CALM;
    last_emotion_decay = millis();
    sys_current_face = "idle";
  }
  current_activity = ACTIVITY_IDLE;
  g_creature.activity = ACTIVITY_IDLE;
}

uint8_t EmotionEngineClass::getRobotFace() const {
  switch (current_emotion) {
    case EMOTION_CALM:      return 0;  // 01_idle.bmp
    case EMOTION_HAPPY:     return 1;  // 02_happy.bmp
    case EMOTION_EXCITED:   return 2;  // 03_excited.bmp
    case EMOTION_SLEEPY:    return 3;  // 04_sleepy.bmp
    case EMOTION_CURIOUS:   return 4;  // 05_thinking.bmp
    case EMOTION_SAD:       return 5;  // 06_sad_error.bmp
    case EMOTION_ANGRY:     return 6;  // 07_alert_warning.bmp
    case EMOTION_ALERT:     return 6;  // 07_alert_warning.bmp
    case EMOTION_LOVE:      return 7;  // 08_love_bonding.bmp
    case EMOTION_SURPRISED: return 9;  // 10_surprised.bmp
    default:                return 0;  // 01_idle.bmp
  }
}

const char* EmotionEngineClass::emotionToDisplayName(EmotionType e) const {
  const char* names[] = {"HAPPY", "SAD", "ALERT", "THINKING", "SLEEPY", "IDLE", "ALERT", "LOVE", "SURPRISED", "EXCITED"};
  return (e < EMOTION_COUNT) ? names[e] : "Unknown";
}

const char* EmotionEngineClass::getEmotionStr() const {
  return emotionToDisplayName(current_emotion);
}

const char* EmotionEngineClass::getActivityStr() const {
  return (current_activity < ACTIVITY_COUNT) ? kActivityLabels[current_activity] : "Unknown";
}

// ── Face Engine v2 Layer Implementation ─────────────────────────
static void local_draw_heart(TFT_eSprite* spr, int cx, int cy, int size, uint16_t color) {
  int r = size / 2;
  spr->fillCircle(cx - r / 2 + 2, cy - r / 4, r, color);
  spr->fillCircle(cx + r / 2 - 2, cy - r / 4, r, color);
  spr->fillTriangle(cx - r - 7, cy - 2, cx + r + 7, cy - 2, cx, cy + size - 2, color);
}

static void local_draw_music_note(TFT_eSprite* spr, int cx, int cy, uint16_t color) {
  spr->fillCircle(cx, cy, 3, color);
  spr->fillRect(cx + 1, cy - 10, 2, 10, color);
  spr->fillRect(cx + 1, cy - 12, 6, 2, color);
}

static void local_draw_sweat(TFT_eSprite* spr, int cx, int cy, uint16_t color) {
  spr->fillCircle(cx, cy, 4, color);
  spr->fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy - 8, color);
}

static void local_draw_dotted_smile(TFT_eSprite* spr, int cx, int cy, int width, int drop, uint16_t color) {
  spr->fillRect(cx - width / 2, cy - drop, 4, 4, color);
  spr->fillRect(cx - width / 4, cy, 4, 4, color);
  spr->fillRect(cx + width / 4, cy, 4, 4, color);
  spr->fillRect(cx + width / 2, cy - drop, 4, 4, color);
}

static void local_draw_dotted_frown(TFT_eSprite* spr, int cx, int cy, int width, int drop, uint16_t color) {
  spr->fillRect(cx - width / 2, cy + drop, 4, 4, color);
  spr->fillRect(cx - width / 4, cy, 4, 4, color);
  spr->fillRect(cx + width / 4, cy, 4, 4, color);
  spr->fillRect(cx + width / 2, cy + drop, 4, 4, color);
}

void FaceEngineClass::begin() {
  anim_blink_scale = 1.0f;
  anim_is_blinking = false;
  anim_blink_next  = millis() + 3000;
  anim_look_next   = millis() + 2000;
  anim_bounce_y    = 0;
  anim_pulse_scale = 1.0f;
  strcpy(status_l1, "AeroSniffer OS");
  strcpy(status_l2, "system");
  for (int i = 0; i < 5; i++) active_particles[i].type = 0;

  _eyelid_factor = 1.0f;
  _deep_blink_hold = 0;

  _last_mood         = g_creature.mood;
  _last_mood_strength = g_creature.mood_strength;
  recomputeMoodPresentation();

  // Initialize Presentation Intelligence state via deterministic selection
  _pres_state.current_profile = 0;
  _pres_state.pending_profile = 0;
  _pres_state.transition_progress = 255; // fully settled
  _pres_state.hold_remaining_ms = 0;
  _pres_state.selection_seed = g_creature.uptime_minutes + millis();
  _pres_state.last_emotion   = g_creature.emotion;
  // Initialize micro-expression RNG seed (non-zero for xorshift32 liveness)
  _micro_rng_seed = g_creature.uptime_minutes + millis() + 1;
  _pres_state.last_mood      = g_creature.mood;
  _pres_state.last_engagement = g_creature.engagement_drive;
  _pres_state.current_profile = selectProfile(g_creature.emotion);
  _pres_state.pending_profile = _pres_state.current_profile;

  // V2.8 Sprint 2: Animation Intelligence init
  _eye_physics = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  _pres_target = PresentationTarget();
  _anim_pose = AnimationPose();
  _transition_phase = TransitionPhase::IDLE;
  _transition_timer_ms = 0;
  _breath_phase = 0.0f;
  _breath_accum = 0.0f;
  _recovery_modifier = 0.0f;
  _recovery_start_ms = 0;
  _last_high_emotion = EMOTION_CALM;
  _last_frame_us = micros();
  _focus_locked = false;
}

void FaceEngineClass::recomputeMoodPresentation() {
  MoodType m = g_creature.mood;
  if (m >= MOOD_COUNT) m = MOOD_RELAXED;

  MoodProfile relaxed = profileFor(MOOD_RELAXED);
  MoodProfile target  = profileFor(m);

  // RELAXED mood or zero strength always yields RELAXED defaults
  if (m == MOOD_RELAXED || g_creature.mood_strength == 0) {
    _mood_pres.blink_interval_ms = relaxed.blink_ms;
    _mood_pres.idle_amplitude    = relaxed.amp;
    _mood_pres.idle_speed        = relaxed.speed;
    _mood_pres.eye_intensity     = relaxed.intens;
    return;
  }

  // Smoothstep interpolation from RELAXED baseline to target mood at full strength
  float t = g_creature.mood_strength / 50.0f;
  float e = t * t * (3.0f - 2.0f * t);

  int32_t blink_delta = (int32_t)target.blink_ms - (int32_t)relaxed.blink_ms;
  _mood_pres.blink_interval_ms = (uint32_t)((int32_t)relaxed.blink_ms + (int32_t)(blink_delta * e));
  _mood_pres.idle_amplitude    = relaxed.amp    + (target.amp    - relaxed.amp)    * e;
  _mood_pres.idle_speed        = relaxed.speed  + (target.speed  - relaxed.speed)  * e;
  _mood_pres.eye_intensity     = relaxed.intens + (target.intens - relaxed.intens) * e;
}

void FaceEngineClass::updateAnimations(int frame) {
  uint32_t now = millis();

  // ── 1. Compute wall-clock dt ────────────────────────────────
  uint32_t now_us = micros();
  float dt = (float)(now_us - _last_frame_us) / 16666.0f;
  if (dt > 3.0f) dt = 3.0f;
  uint32_t dt_ms = (now_us - _last_frame_us) / 1000;
  if (dt_ms > 50) dt_ms = 50;
  _last_frame_us = now_us;

  // ── 2. Eyelid factor: smooth lerp toward engagement-based drowsiness ──
  float drowsiness = 1.0f - (g_creature.engagement_drive / 100.0f);
  float target_eyelid = 1.0f - pow(drowsiness, 1.5f) * 0.7f;
  _eyelid_factor = _eyelid_factor * 0.95f + target_eyelid * 0.05f;

  // ── 3. Update mood presentation if mood or strength changed ──
  if (g_creature.mood != _last_mood || g_creature.mood_strength != _last_mood_strength) {
    _last_mood         = g_creature.mood;
    _last_mood_strength = g_creature.mood_strength;
    recomputeMoodPresentation();
  }

  // ── 4. Blink with deep blink probability from engagement ──
  if (now > anim_blink_next && !anim_is_blinking) {
    anim_is_blinking = true;
    anim_blink_accum_ms = 0;
    anim_blink_scale = 1.0f;
    int deep_chance = 0;
    if (g_creature.attention.target == TARGET_NONE) {
      deep_chance = max(0, 60 - (int)g_creature.engagement_drive * 75 / 100);
    }
    _deep_blink_hold = (random(100) < deep_chance) ? 2 : 0;
  }

  if (anim_is_blinking) {
    // Time-based blink progress using dt_ms from the animation frame
    anim_blink_accum_ms += dt_ms;

    static constexpr uint32_t BLINK_CLOSE_MS = 80;
    static constexpr uint32_t BLINK_HOLD_MS  = 50;
    static constexpr uint32_t BLINK_OPEN_MS  = 150;
    static constexpr uint32_t BLINK_TOTAL_MS = BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS;

    uint32_t t = anim_blink_accum_ms;
    if (t < BLINK_CLOSE_MS) {
      float p = (float)t / (float)BLINK_CLOSE_MS;
      anim_blink_scale = 1.0f - p * p;
    } else if (t < BLINK_CLOSE_MS + BLINK_HOLD_MS) {
      anim_blink_scale = 0.0f;
    } else if (t < BLINK_TOTAL_MS) {
      float p = (float)(t - BLINK_CLOSE_MS - BLINK_HOLD_MS) / (float)BLINK_OPEN_MS;
      anim_blink_scale = p * (2.0f - p);
    } else {
      anim_blink_scale = 1.0f;
      anim_is_blinking = false;
      anim_blink_accum_ms = 0;
      {
        uint8_t _pid_ = (_pres_state.transition_progress >= 255)
          ? _pres_state.current_profile : _pres_state.pending_profile;
        uint8_t pid = kVariantTable[presIdx(_pres_state.last_emotion, _pid_)].anim_profile_id;
        if (pid >= PRES_ANIM_COUNT) pid = 0;
        uint32_t blink_ms = _mood_pres.blink_interval_ms * kAnimProfiles[pid].blink_mod / 128;
        anim_blink_next = now + blink_ms + random(-500, 500);
      }
    }
  }

  // ── 5. TransitionPhase state machine ─────────────────────────
  advanceTransitionPhase(dt_ms);

  // ── 6. Biological breathing waveform ─────────────────────────
  _breath_phase += dt * (2.0f * PI) / (BREATH_CYCLE_MS / 16.666f);
  if (_breath_phase >= 2.0f * PI) _breath_phase -= 2.0f * PI;

  // ── 7. Emotional recovery curve ──────────────────────────────
  updateRecovery(now);

  // ── 8. Focus lock + gaze target ──────────────────────────────
  // Focus-lock hysteresis: lock >= 25, unlock < 20
  bool want_lock = (g_creature.attention.strength >= FOCUS_LOCK_ON);
  bool want_unlock = (g_creature.attention.strength < FOCUS_LOCK_OFF);
  if (!_focus_locked && want_lock) _focus_locked = true;
  else if (_focus_locked && want_unlock) _focus_locked = false;

  if (g_creature.attention.target == TARGET_THREAT) {
    _eye_physics.target_x = 0.0f;
    _eye_physics.target_y = 0.0f;
  } else if (g_creature.attention.target == TARGET_USER) {
    _eye_physics.target_x = 0.0f;
    _eye_physics.target_y = -(float)(1 + (g_creature.attention.strength / 33));
  } else if (g_creature.attention.target == TARGET_FLIGHT) {
    _eye_physics.target_x = 0.0f;
    _eye_physics.target_y = -(float)(1 + (g_creature.attention.strength / 25));
  } else if (_focus_locked) {
    _eye_physics.target_x = 0.0f;
    _eye_physics.target_y = -1.0f;
  } else {
    // Idle wander — only set new target on saccade timer
    if (now > anim_look_next) {
      if (g_creature.emotion == EMOTION_CALM || g_creature.emotion == EMOTION_CURIOUS) {
        uint8_t _pid_g = (_pres_state.transition_progress >= 255)
          ? _pres_state.current_profile : _pres_state.pending_profile;
        uint8_t pid = kVariantTable[presIdx(_pres_state.last_emotion, _pid_g)].anim_profile_id;
        if (pid >= PRES_ANIM_COUNT) pid = 0;
        int range_x = (int)(kAnimProfiles[pid].gaze_range * _mood_pres.eye_intensity);
        int range_y = max(1, range_x * 2 / 3);
        _eye_physics.target_x = (float)random(-range_x, range_x + 1);
        _eye_physics.target_y = (float)random(-range_y, range_y + 1);
      } else {
        _eye_physics.target_x = 0.0f;
        _eye_physics.target_y = 0.0f;
      }
      // Saccade interval: min hold clamped to MIN_SACCADE_HOLD_MS
      int base_interval = 3000 + (100 - (int)g_creature.engagement_drive) * 270;
      int jitter = base_interval / 2;
      int next_interval = base_interval + random(-jitter, jitter + 1);
      anim_look_next = now + (uint32_t)max((int)MIN_SACCADE_HOLD_MS, next_interval);
    }
  }

  // ── 9. Spring-damper integration ─────────────────────────────
  integrateSpringDamper(dt);

  // ── 10. Resolve bounce from mood + breathing ──────────────────
  uint8_t _pid_i = (_pres_state.transition_progress >= 255)
    ? _pres_state.current_profile : _pres_state.pending_profile;
  uint8_t _pid = kVariantTable[presIdx(_pres_state.last_emotion, _pid_i)].anim_profile_id;
  if (_pid >= PRES_ANIM_COUNT) _pid = 0;
  float mod_bounce = kAnimProfiles[_pid].bounce_mod / 128.0f;

  float breath_val = computeBreathValue();
  float breath_bounce = breath_val * 0.5f;

  if (g_creature.emotion == EMOTION_EXCITED || g_creature.emotion == EMOTION_HAPPY || g_creature.emotion == EMOTION_LOVE) {
    anim_bounce_y = (int)(breath_bounce + breath_val * 4.0f * _mood_pres.idle_amplitude * mod_bounce);
  } else {
    anim_bounce_y = (int)breath_bounce;
  }

  // ── 11. Pulse animation (unchanged) ──────────────────────────
  float mod_speed = _mood_pres.idle_speed * kAnimProfiles[_pid].idle_speed_mod / 128.0f;
  if (g_creature.emotion == EMOTION_EXCITED || g_creature.emotion == EMOTION_ALERT) {
    anim_pulse_scale = 1.0f + sin(frame * 0.3f * mod_speed) * 0.15f;
  } else {
    anim_pulse_scale = 1.0f;
  }

  // AnimationPose production moved to render() — Sprint 3 injects
  // updateMicroExpressions() between updateAnimations() and produceAnimationPose()
}

// ── V2.8 Sprint 3 — Micro-Expression Layer ──────────────────────

void FaceEngineClass::updateMicroExpressions(uint32_t now) {
  // ── Timer expiry: clear active micro ──
  // Rollover-safe: (int32_t)(now - expiry) >= 0 works across uint32_t wraparound
  if (_active_micro != MicroExpression::NONE && (int32_t)(now - _micro_timer_ms) >= 0) {
    _active_micro = MicroExpression::NONE;
    _micro_phase = 0;
    _micro_mod_x = 0;
    _micro_mod_y = 0;
    _micro_brow_delta = 0;
  }

  // ── Active micro phase processing ──
  if (_active_micro == MicroExpression::DOUBLE_BLINK) {
    if (_micro_phase == 0) {
      _micro_phase = 1;
    } else if (_micro_phase == 1 && !anim_is_blinking && anim_blink_scale >= 1.0f) {
      anim_blink_next = 0;
      anim_is_blinking = false;
      _micro_phase = 2;
    }
  }
}

// ── V2.8 Sprint 3 — Public API: queue a micro-expression for execution ──

void FaceEngineClass::queueMicroExpression(MicroExpression type) {
  if (_active_micro != MicroExpression::NONE) return; // One at a time

  switch (type) {
    case MicroExpression::DOUBLE_BLINK:
      _active_micro = MicroExpression::DOUBLE_BLINK;
      _micro_timer_ms = millis() + DOUBLE_BLINK_DURATION_MS;
      _micro_phase = 0;
      anim_blink_next = 0;
      anim_is_blinking = false;
      break;

    case MicroExpression::SQUINT:
      _active_micro = MicroExpression::SQUINT;
      _micro_timer_ms = millis() + SQUINT_DURATION_MS;
      _micro_phase = 8;
      break;

    case MicroExpression::PUPIL_FLICK: {
      _active_micro = MicroExpression::PUPIL_FLICK;
      _micro_timer_ms = millis() + PUPIL_FLICK_DURATION_MS;
      _micro_mod_x = (xorshift32(_micro_rng_seed) & 1) ? PUPIL_FLICK_RANGE : -PUPIL_FLICK_RANGE;
      _micro_mod_y = (xorshift32(_micro_rng_seed) & 1) ? 1 : -1;
      break;
    }
    case MicroExpression::BROW_TWITCH:
      _active_micro = MicroExpression::BROW_TWITCH;
      _micro_timer_ms = millis() + BROW_TWITCH_DURATION_MS;
      _micro_brow_delta = (xorshift32(_micro_rng_seed) & 1) ? BROW_TWITCH_RANGE : -BROW_TWITCH_RANGE;
      break;

    default:
      break;
  }
}

// ── V2.8 Sprint 3 — Decision layer: reads CreatureState, calls queueMicroExpression ──
// Priority order (first match wins):
//   1. SURPRISED rising edge → DOUBLE_BLINK
//   2. Attention strength delta > 20 → SQUINT
//   3. Attention target change → PUPIL_FLICK
//   4. ANXIOUS + small attention delta 5-15 → BROW_TWITCH

void FaceEngineClass::detectMicroTriggers() {
  // Update edge-detection state every frame so thresholds stay current
  EmotionType prev_emotion = _micro_prev_emotion;
  uint8_t prev_strength = _micro_prev_strength;
  AttentionTarget prev_target = _micro_prev_target;
  _micro_prev_emotion = g_creature.emotion;
  _micro_prev_target = g_creature.attention.target;
  _micro_prev_strength = g_creature.attention.strength;

  if (_active_micro != MicroExpression::NONE) return;

  // 1. Double Blink: SURPRISED emotion rising edge
  if (g_creature.emotion == EMOTION_SURPRISED && prev_emotion != EMOTION_SURPRISED) {
    queueMicroExpression(MicroExpression::DOUBLE_BLINK);
  }
  // 2. Squint: rapid attention increase
  else if (g_creature.attention.strength > prev_strength + SQUINT_THRESHOLD) {
    queueMicroExpression(MicroExpression::SQUINT);
  }
  // 3. Pupil Flick: attention target changed
  else if (g_creature.attention.target != prev_target
           && g_creature.attention.target != TARGET_NONE) {
    queueMicroExpression(MicroExpression::PUPIL_FLICK);
  }
  // 4. Brow Twitch: small attention fluctuation during ANXIOUS
  else if (g_creature.mood == MOOD_ANXIOUS
           && g_creature.attention.strength > prev_strength + BROW_TWITCH_MIN
           && g_creature.attention.strength < prev_strength + BROW_TWITCH_MAX) {
    queueMicroExpression(MicroExpression::BROW_TWITCH);
  }
}

// ── V2.8 Sprint 2 Helper Methods ────────────────────────────────

void FaceEngineClass::advanceTransitionPhase(uint32_t dt_ms) {
  // Sync TransitionPhase with Sprint 1 transition_progress
  if (_pres_state.transition_progress < 255) {
    if (_transition_phase == TransitionPhase::IDLE) {
      _transition_phase = TransitionPhase::ANTICIPATE;
      _transition_timer_ms = ANTICIPATE_MS;
    }
    if (_transition_timer_ms > dt_ms) {
      _transition_timer_ms -= dt_ms;
    } else {
      _transition_timer_ms = 0;
      switch (_transition_phase) {
        case TransitionPhase::ANTICIPATE:
          _transition_phase = TransitionPhase::TRANSITION;
          _transition_timer_ms = TRANSITION_MS;
          break;
        case TransitionPhase::TRANSITION:
          _transition_phase = TransitionPhase::SETTLE;
          _transition_timer_ms = SETTLE_MS;
          break;
        default:
          break;
      }
    }
  } else {
    // Transition complete — let SETTLE finish naturally
    if (_transition_phase == TransitionPhase::SETTLE) {
      if (_transition_timer_ms > dt_ms) {
        _transition_timer_ms -= dt_ms;
      } else {
        _transition_phase = TransitionPhase::IDLE;
        _transition_timer_ms = 0;
      }
    } else if (_transition_phase != TransitionPhase::IDLE) {
      _transition_phase = TransitionPhase::IDLE;
      _transition_timer_ms = 0;
    }
  }
}

float FaceEngineClass::computeBreathValue() const {
  float p = _breath_phase / (2.0f * PI);
  if (p < 0.35f) {
    return p / 0.35f;
  } else if (p < 0.45f) {
    return 1.0f;
  } else if (p < 0.80f) {
    return 1.0f - (p - 0.45f) / 0.35f;
  } else {
    return 0.0f;
  }
}

void FaceEngineClass::integrateSpringDamper(float dt) {
  // Micro-drift: sub-pixel Lissajous for organic eye drift
  static float _drift_phase = 0.0f;
  _drift_phase += dt * 0.03f;
  float drift_x = sinf(_drift_phase * 0.7f) * MICRO_DRIFT_AMP;
  float drift_y = cosf(_drift_phase * 0.5f) * MICRO_DRIFT_AMP;

  // Spring-damper toward (target + drift)
  float accel_x = (_eye_physics.target_x + drift_x - _eye_physics.current_x) * SPRING_TENSION
                  - _eye_physics.vel_x * SPRING_DAMPING;
  float accel_y = (_eye_physics.target_y + drift_y - _eye_physics.current_y) * SPRING_TENSION
                  - _eye_physics.vel_y * SPRING_DAMPING;

  _eye_physics.vel_x += accel_x * dt;
  _eye_physics.vel_y += accel_y * dt;
  _eye_physics.current_x += _eye_physics.vel_x * dt;
  _eye_physics.current_y += _eye_physics.vel_y * dt;

  // Settle kill: when near target, snap to prevent micro-jitter
  if (fabsf(_eye_physics.target_x - _eye_physics.current_x) < SPRING_SETTLE &&
      fabsf(_eye_physics.target_y - _eye_physics.current_y) < SPRING_SETTLE) {
    _eye_physics.vel_x = 0.0f;
    _eye_physics.vel_y = 0.0f;
    _eye_physics.current_x = _eye_physics.target_x;
    _eye_physics.current_y = _eye_physics.target_y;
  }
}

void FaceEngineClass::updateRecovery(uint32_t now) {
  bool isHighEnergy = (g_creature.emotion == EMOTION_ALERT || g_creature.emotion == EMOTION_ANGRY);
  if (isHighEnergy) {
    _recovery_modifier = 0.0f;
    _recovery_start_ms = now;
  } else {
    if (_last_high_emotion == EMOTION_ALERT || _last_high_emotion == EMOTION_ANGRY) {
      _recovery_start_ms = now;
      _recovery_modifier = 1.0f;
    }
    if (_recovery_modifier > 0.0f) {
      uint32_t elapsed = now - _recovery_start_ms;
      _recovery_modifier = max(0.0f, 1.0f - (float)elapsed / (float)RECOVERY_BROW_MS);
    }
  }
  _last_high_emotion = g_creature.emotion;
}

void FaceEngineClass::produceAnimationPose() {
  AnimationPose& p = _anim_pose;

  // Eye height: 32px base * blink * pulse * eyelid * transition * breathing * recovery
  float h = 32.0f * anim_blink_scale * anim_pulse_scale * _eyelid_factor;

  // Transition phase physical effects
  if (_transition_phase == TransitionPhase::ANTICIPATE) {
    h *= 0.90f;
  } else if (_transition_phase == TransitionPhase::SETTLE) {
    h *= 1.05f;
  }

  // Breathing modulates vertical scale: +2% inhale, -1% exhale
  float bv = computeBreathValue();
  h *= (1.0f + bv * 0.03f - 0.01f);

  // Recovery: eyelid openness penalty over 2s
  if (_recovery_modifier > 0.0f) {
    uint32_t now = millis();
    float eyelid_decay = min(1.0f, (float)(now - _recovery_start_ms) / (float)RECOVERY_EYELID_MS);
    float penalty = _recovery_modifier * (1.0f - eyelid_decay) * 0.15f;
    h *= (1.0f - penalty);
  }

  // V2.8 Sprint 3 — Micro-expression: Squint (reduce eye height)
  if (_active_micro == MicroExpression::SQUINT) {
    h *= 0.70f;
  }

  // V2.8 RC Polish — blink asymmetry: right eye closes 1px faster for organic feel
  float h_left = h, h_right = h;
  if (anim_is_blinking && h < 28.0f && h > 2.0f) {
    h_right = h - 1.0f;
    if (h_right < 2.0f) h_right = 2.0f;
  }
  if (h_left < 1.0f) h_left = 1.0f;
  if (h_right < 1.0f) h_right = 1.0f;

  // Eye positions
  int bounce_y = anim_bounce_y;
  p.left_eye.x = 80;
  p.right_eye.x = 160;
  int y_base = 90 + bounce_y;
  p.left_eye.y = y_base;
  p.right_eye.y = y_base;
  p.left_eye.height = (int)h_left;
  p.right_eye.height = (int)h_right;

  // Pupil offset from spring-damper physics
  p.left_eye.pupil_x = (int)_eye_physics.current_x;
  p.left_eye.pupil_y = (int)_eye_physics.current_y;
  p.right_eye.pupil_x = p.left_eye.pupil_x;
  p.right_eye.pupil_y = p.left_eye.pupil_y;

  // V2.8 Sprint 3 — Micro-expression: Pupil Flick
  if (_active_micro == MicroExpression::PUPIL_FLICK) {
    p.left_eye.pupil_x += _micro_mod_x;
    p.left_eye.pupil_y += _micro_mod_y;
    p.right_eye.pupil_x += _micro_mod_x;
    p.right_eye.pupil_y += _micro_mod_y;
  }

  // Styles from PresentationTarget (Stage 1 output)
  p.left_eye.shape = _pres_target.eye_shape;
  p.right_eye.shape = _pres_target.eye_shape;
  p.left_eye.pupil_style = _pres_target.pupil_style;
  p.right_eye.pupil_style = _pres_target.pupil_style;

  // Brow
  p.brow.y_offset = bounce_y;
  p.brow.style = _pres_target.brow_style;

  // V2.8 Sprint 3 — Micro-expression: Brow Twitch
  if (_active_micro == MicroExpression::BROW_TWITCH) {
    p.brow.y_offset += _micro_brow_delta;
  }

  // Global
  p.global.bounce_y = bounce_y;
  p.global.color = _pres_target.color;

  // Backward compat for renderActivityLayer / renderEffectLayer
  anim_look_x = p.left_eye.pupil_x;
  anim_look_y = p.left_eye.pupil_y;
}

// Interpolate between two 16-bit RGB565 colors (t: 0.0 = from, 1.0 = to)
static uint16_t lerpColor(uint16_t from, uint16_t to, float t) {
  int r_f = (from >> 11) & 0x1F, g_f = (from >> 5) & 0x3F, b_f = from & 0x1F;
  int r_t = (to >> 11) & 0x1F, g_t = (to >> 5) & 0x3F, b_t = to & 0x1F;
  int r = r_f + (int)((r_t - r_f) * t);
  int g = g_f + (int)((g_t - g_f) * t);
  int b = b_f + (int)((b_t - b_f) * t);
  if (r < 0) r = 0; if (r > 31) r = 31;
  if (g < 0) g = 0; if (g > 63) g = 63;
  if (b < 0) b = 0; if (b > 31) b = 31;
  return (r << 11) | (g << 5) | b;
}

// Draw one eye given shape, position, size, color, pupil type, and lookup
static void drawEye(TFT_eSprite* spr, int cx, int cy, int w, int h, uint16_t color,
                    uint8_t eye_shape, uint8_t pupil_type, int look_x, int look_y,
                    float pulse, uint16_t bg) {
  if (h <= 2) return; // fully closed

  switch (eye_shape) {
    case PRES_EYE_ARCHED: {
      // Arched — two overlapping circles with bottom clipped
      int r = w / 2;
      spr->fillCircle(cx - r / 2 + 2, cy + r / 4, r, color);
      spr->fillCircle(cx + r / 2 - 2, cy + r / 4, r, bg);
      if (h > 6) {
        int ps = (pupil_type == PRES_PUPIL_DILATED) ? 5 :
                 (pupil_type == PRES_PUPIL_CONSTRICTED) ? 2 : 3;
        spr->fillRect(cx - ps + look_x, cy + 2 + look_y, ps * 2, ps * 2, C_BLACK);
      }
      break;
    }
    case PRES_EYE_ROUND: {
      // Round — full circles
      int r = max(8, w / 2);
      spr->fillCircle(cx, cy + h / 2 - r / 2, r, color);
      if (h > 6) {
        int ps = (pupil_type == PRES_PUPIL_DILATED) ? 6 :
                 (pupil_type == PRES_PUPIL_CONSTRICTED) ? 2 : 4;
        if (pupil_type != PRES_PUPIL_NONE && pupil_type != PRES_PUPIL_HALF)
          spr->fillCircle(cx + look_x, cy + h / 2 + look_y, ps, C_BLACK);
      }
      break;
    }
    case PRES_EYE_FLAT: {
      // Flat — thin horizontal bar
      spr->fillRoundRect(cx - w / 2, cy + h / 4, w, max(2, h / 3), 3, color);
      break;
    }
    case PRES_EYE_SQUINT: {
      // Squint — narrow angled
      spr->fillTriangle(cx - w / 2, cy + 2, cx + w / 2, cy + 2, cx, cy + h - 2, color);
      if (h > 4) {
        int ps = (pupil_type == PRES_PUPIL_DILATED) ? 3 :
                 (pupil_type == PRES_PUPIL_CONSTRICTED) ? 1 : 2;
        spr->fillRect(cx - ps + look_x / 2, cy + h / 2 + look_y / 2, ps * 2, ps * 2, C_BLACK);
      }
      break;
    }
    case PRES_EYE_WIDE: {
      // Wide — big circles
      int r = max(8, w * 3 / 4);
      spr->fillCircle(cx, cy + h / 2 - r / 4, r, color);
      if (h > 10) {
        int ps = (pupil_type == PRES_PUPIL_DILATED) ? 7 :
                 (pupil_type == PRES_PUPIL_CONSTRICTED) ? 3 : 5;
        if (pupil_type != PRES_PUPIL_NONE && pupil_type != PRES_PUPIL_HALF)
          spr->fillCircle(cx + look_x, cy + h / 2 + look_y, ps, C_BLACK);
      }
      break;
    }
    default: {
      // DEFAULT — rounded rectangle
      spr->fillRoundRect(cx - w / 2, cy, w, h, 6, color);
      if (h > 6) {
        int ps = (pupil_type == PRES_PUPIL_DILATED) ? 5 :
                 (pupil_type == PRES_PUPIL_CONSTRICTED) ? 2 : 3;
        if (pupil_type == PRES_PUPIL_HALF) {
          // Half pupil — only top half of eye filled with pupil color
          spr->fillRect(cx - w / 4 + look_x / 2, cy + 2 + look_y / 2, w / 2, h / 2 - 2, C_BLACK);
        } else if (pupil_type != PRES_PUPIL_NONE) {
          spr->fillRect(cx - ps + look_x, cy + h / 2 - ps + look_y, ps * 2, ps * 2, C_BLACK);
        }
      }
      break;
    }
  }
}

// Draw eyebrows based on style
static void drawBrows(TFT_eSprite* spr, int y_base, int bounce_y, uint16_t color,
                      uint8_t brow_style) {
  switch (brow_style) {
    case PRES_BROW_RAISED:
      spr->fillTriangle(50, y_base - 6 + bounce_y, 100, y_base + 4 + bounce_y, 100, y_base - 2 + bounce_y, color);
      spr->fillTriangle(190, y_base - 6 + bounce_y, 140, y_base + 4 + bounce_y, 140, y_base - 2 + bounce_y, color);
      break;
    case PRES_BROW_FURROWED:
      spr->fillTriangle(50, y_base + 4 + bounce_y, 100, y_base - 6 + bounce_y, 100, y_base - 2 + bounce_y, color);
      spr->fillTriangle(190, y_base + 4 + bounce_y, 140, y_base - 6 + bounce_y, 140, y_base - 2 + bounce_y, color);
      break;
    case PRES_BROW_ONE:
      // Left raised, right normal
      spr->fillTriangle(50, y_base - 6 + bounce_y, 100, y_base + 4 + bounce_y, 100, y_base - 2 + bounce_y, color);
      break;
    case PRES_BROW_WORRIED:
      spr->fillTriangle(50, y_base - 2 + bounce_y, 100, y_base + 6 + bounce_y, 100, y_base + 2 + bounce_y, color);
      spr->fillTriangle(190, y_base - 2 + bounce_y, 140, y_base + 6 + bounce_y, 140, y_base + 2 + bounce_y, color);
      break;
    default:
      break; // PRES_BROW_NORMAL — nothing drawn
  }
}

void FaceEngineClass::renderEmotionLayer(TFT_eSprite* spr, int frame) {
  // Consume AnimationPose (Stage 2 output) — all values are integer
  const AnimationPose& p = _anim_pose;
  drawEye(spr, p.left_eye.x, p.left_eye.y, 40, p.left_eye.height,
          p.global.color, p.left_eye.shape, p.left_eye.pupil_style,
          p.left_eye.pupil_x, p.left_eye.pupil_y, 1.0f, C_BG);
  drawEye(spr, p.right_eye.x, p.right_eye.y, 40, p.right_eye.height,
          p.global.color, p.right_eye.shape, p.right_eye.pupil_style,
          p.right_eye.pupil_x, p.right_eye.pupil_y, 1.0f, C_BG);
  drawBrows(spr, 78, p.global.bounce_y, p.global.color, p.brow.style);
}

static void local_draw_frown(TFT_eSprite* spr, int32_t cx, int32_t cy, int32_t w, int32_t h, uint16_t color) {
  spr->fillCircle(cx, cy + h, w / 2, color);
  spr->fillCircle(cx, cy + h + 4, w / 2, C_BG);
}

void FaceEngineClass::renderActivityLayer(TFT_eSprite* spr, int frame) {
  uint16_t color = C_IDLE;
  switch (g_creature.emotion) {
    case EMOTION_HAPPY:     color = C_HAPPY; break;
    case EMOTION_SAD:       color = C_SAD; break;
    case EMOTION_ANGRY:     color = C_ALERT; break;
    case EMOTION_CURIOUS:   color = C_THINKING; break;
    case EMOTION_SLEEPY:    color = C_SLEEPY; break;
    case EMOTION_LOVE:      color = C_LOVE; break;
    case EMOTION_SURPRISED: color = C_EXCITED; break;
    case EMOTION_EXCITED:  color = C_EXCITED; break;
    default:                color = C_IDLE; break;
  }
  
  // Mouth drawing based on activity / emotion combo
  if (g_creature.activity == ACTIVITY_SLEEPING) {
    spr->fillRect(105, 160 + anim_bounce_y, 30, 4, C_SLEEPY);
  }
  else if (g_creature.emotion == EMOTION_HAPPY || g_creature.emotion == EMOTION_LOVE) {
    spr->fillCircle(120, 155 + anim_bounce_y, 12, color);
    spr->fillCircle(120, 151 + anim_bounce_y, 12, C_BG);
  }
  else if (g_creature.emotion == EMOTION_SAD) {
    local_draw_frown(spr, 120, 160 + anim_bounce_y, 40, 10, color);
  }
  else if (g_creature.activity == ACTIVITY_WATCHING_FLIGHTS || g_creature.emotion == EMOTION_SURPRISED) {
    spr->fillCircle(120, 160 + anim_bounce_y, 10, color);
    spr->fillCircle(120, 160 + anim_bounce_y, 4, C_BG);
  }
  else {
    spr->fillRect(95, 160 + anim_bounce_y, 50, 6, color);
  }
  
  // Status Bar rendering: status_l1 and status_l2
  spr->fillRect(0, 200, TFT_W, 40, C_BG);
  if (strlen(status_l1) > 0) {
    spr->setTextColor(TFT_WHITE);
    spr->setTextSize(1);
    spr->setCursor((TFT_W - strlen(status_l1) * 6) / 2, 206);
    spr->print(status_l1);
  }
  if (strlen(status_l2) > 0) {
    spr->setTextColor(0x7BEF); // light gray/cyan
    spr->setTextSize(1);
    spr->setCursor((TFT_W - (strlen(status_l2) + 2) * 6) / 2, 222);
    spr->printf("[%s]", status_l2);
  }
}

void FaceEngineClass::renderEffectLayer(TFT_eSprite* spr, int frame) {
  uint32_t now = millis();
  
  // Simulated activity/emotion effect trigger
  if (g_creature.activity == ACTIVITY_MUSIC && frame % 15 == 0) {
    triggerParticle(1); // music note
  }
  if (g_creature.emotion == EMOTION_LOVE && frame % 20 == 0) {
    triggerParticle(3); // heart
  }
  if ((g_creature.emotion == EMOTION_ANGRY || g_creature.emotion == EMOTION_ALERT) && frame % 25 == 0) {
    triggerParticle(2); // sweat
  }
  
  // Zzz sleeping animation overlay
  if (g_creature.activity == ACTIVITY_SLEEPING || g_creature.emotion == EMOTION_SLEEPY) {
    int z = (frame / 10) % 3;
    uint16_t z_col = C_SLEEPY;
    if (z >= 0) spr->fillRect(150, 50, 6, 2, z_col);
    if (z >= 1) spr->fillRect(165, 35, 10, 2, z_col);
    if (z >= 2) spr->fillRect(185, 15, 14, 3, z_col);
  }
  
  // Draw & decay active particles
  for (int i = 0; i < 5; i++) {
    if (active_particles[i].type != 0 && active_particles[i].life > 0) {
      active_particles[i].life -= 0.05f;
      
      if (active_particles[i].type == 1) { // Music note
        active_particles[i].y -= 2.0f;
        local_draw_music_note(spr, (int)active_particles[i].x, (int)active_particles[i].y, active_particles[i].color);
      } else if (active_particles[i].type == 2) { // Sweat drop
        active_particles[i].y += 3.0f;
        local_draw_sweat(spr, (int)active_particles[i].x, (int)active_particles[i].y, active_particles[i].color);
      } else if (active_particles[i].type == 3) { // Heart
        active_particles[i].y -= 2.0f;
        local_draw_heart(spr, (int)active_particles[i].x, (int)active_particles[i].y, 14, active_particles[i].color);
      }
      
      if (active_particles[i].life <= 0) {
        active_particles[i].type = 0;
      }
    }
  }
}

#if ANIM_DEBUG
static const char* emotionToShortStr(EmotionType e) {
  if (e >= 0 && e < EMOTION_COUNT) return kEmotionShortLabels[e];
  return "UNKNOWN";
}

void FaceEngineClass::renderDebugOverlay(TFT_eSprite* spr) {
  // Top-left debug HUD: short emotion name + full label
  spr->setTextColor(TFT_WHITE, C_BG);
  spr->setTextSize(1);
  spr->setCursor(2, 2);
  int e = (int)g_creature.emotion;
  if (e >= 0 && e < EMOTION_COUNT) {
    spr->print(kEmotionShortLabels[e]);
    spr->setCursor(2, 12);
    spr->setTextColor(0x7BEF, C_BG); // muted gray for secondary label
    spr->print(kEmotionLabels[e]);
  } else {
    spr->print("---");
  }
}
#endif

void FaceEngineClass::render(TFT_eSprite* spr, int frame) {
  // Overdraw background
  spr->fillRect(0, 40, TFT_W, 160, C_BG);
  
  // V2.8 Sprint 1 + 3: presentation & micro-expression decision
  updatePresentation();
  detectMicroTriggers();
  updateAnimations(frame);
  // V2.8 Sprint 3: micro-expression execution between animation and rasterization
  updateMicroExpressions(millis());
  produceAnimationPose();
  renderEmotionLayer(spr, frame);
  renderActivityLayer(spr, frame);
  renderEffectLayer(spr, frame);
  
  #if ANIM_DEBUG
  renderDebugOverlay(spr);
  #endif
}

void FaceEngineClass::setStatusText(const char* status) {
  // Status is the primary line, keep secondary tag
  setStatusLines(status, status_l2);
}

void FaceEngineClass::setAppName(const char* app) {
  setStatusLines(status_l1, app);
}

void FaceEngineClass::setStatusLines(const char* l1, const char* l2) {
  strncpy(status_l1, l1, sizeof(status_l1) - 1);
  status_l1[sizeof(status_l1) - 1] = '\0';
  strncpy(status_l2, l2, sizeof(status_l2) - 1);
  status_l2[sizeof(status_l2) - 1] = '\0';
}

void FaceEngineClass::triggerParticle(int type) {
  for (int i = 0; i < 5; i++) {
    if (active_particles[i].type == 0 || active_particles[i].life <= 0) {
      active_particles[i].type = type;
      active_particles[i].life = 1.0f;
      if (type == 1) { // Music
        active_particles[i].x = random(30, 210);
        active_particles[i].y = 160;
        active_particles[i].color = 0x07FF;
      } else if (type == 2) { // Sweat
        active_particles[i].x = random(180, 210);
        active_particles[i].y = 50;
        active_particles[i].color = 0x05FF;
      } else if (type == 3) { // Heart
        active_particles[i].x = random(30, 210);
        active_particles[i].y = 150;
        active_particles[i].color = C_LOVE;
      }
      break;
    }
  }
}

// ── WiFi Service Implementation ──────────────────────────────────
void WiFiServiceClass::begin() {
  is_connecting = false;
  is_promiscuous = false;
}

bool WiFiServiceClass::connectSTA(const char* ssid, const char* pass, uint32_t timeout_ms) {
  if (is_connecting) return false;
  is_connecting = true;
  last_owner = "connectSTA";
  
  Serial.printf("[WiFiService] STA Connect -> '%s' (owner=%s)\n", ssid, last_owner);
  Serial.printf("[WiFiService] WiFi mode before: %d status: %d\n", WiFi.getMode(), WiFi.status());
  EventBus.publish(EVENT_WIFI_CONNECTING);

  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  delay(500);

  WiFi.begin(ssid, pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start < timeout_ms)) {
    delay(200);
  }

  is_connecting = false;
  bool ok = (WiFi.status() == WL_CONNECTED);
  if (ok) {
    Serial.printf("[WiFiService] Connected! IP: %s (owner=%s)\n", WiFi.localIP().toString().c_str(), last_owner);
    EventBus.publish(EVENT_WIFI_CONNECTED);
  } else {
    Serial.printf("[WiFiService] Connection failed. status=%d mode=%d (owner=%s)\n", WiFi.status(), WiFi.getMode(), last_owner);
    EventBus.publish(EVENT_WIFI_DISCONNECTED);
  }
  return ok;
}

void WiFiServiceClass::startAP(const char* ssid, const char* pass) {
  last_owner = "startAP";
  Serial.printf("[WiFiService] Start AP: '%s' (owner=%s, mode=%d)\n", ssid, last_owner, WiFi.getMode());
  WiFi.mode(WIFI_AP);
  delay(200);
  WiFi.softAP(ssid, pass);
  Serial.printf("[WiFiService] AP started. mode=%d softAPIP=%s\n", WiFi.getMode(), WiFi.softAPIP().toString().c_str());
}

void WiFiServiceClass::stop() {
  Serial.printf("[WiFiService] Stop WiFi (owner=%s, mode=%d, promisc=%d)\n", last_owner, WiFi.getMode(), is_promiscuous ? 1 : 0);
  if (is_promiscuous) {
    esp_wifi_set_promiscuous(false);
    is_promiscuous = false;
  }
  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  last_owner = "stopped";
  delay(500);
  Serial.println("[WiFiService] WiFi stopped.");
}

void WiFiServiceClass::setPromiscuous(bool enable, wifi_promiscuous_cb_t cb) {
  if (enable) {
    last_owner = "setPromiscuous";
    Serial.printf("[WiFiService] setPromiscuous(true) mode=%d (owner=%s)\n", WiFi.getMode(), last_owner);
    WiFi.mode(WIFI_AP);
    delay(100);
    esp_wifi_set_promiscuous(false);
    if (cb) {
      esp_wifi_set_promiscuous_rx_cb(cb);
    }
    esp_wifi_set_promiscuous(true);
    is_promiscuous = true;
    Serial.println("[WiFiService] Sniffing active");
  } else {
    Serial.printf("[WiFiService] setPromiscuous(false) mode=%d (owner=%s)\n", WiFi.getMode(), last_owner);
    esp_wifi_set_promiscuous(false);
    is_promiscuous = false;
    Serial.println("[WiFiService] Sniffing stopped");
  }
}

void WiFiServiceClass::setChannel(uint8_t ch) {
  current_channel = ch;
  Serial.printf("[WiFiService] setChannel(%d) mode=%d (owner=%s)\n", ch, WiFi.getMode(), last_owner);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void WiFiServiceClass::transmitRaw80211(uint8_t* packet, int len) {
  esp_wifi_80211_tx(WIFI_IF_AP, packet, len, true);
}

bool WiFiServiceClass::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

int WiFiServiceClass::getStatus() {
  return WiFi.status();
}

String WiFiServiceClass::getLocalIP() {
  return WiFi.localIP().toString();
}

String WiFiServiceClass::getSoftAPIP() {
  return WiFi.softAPIP().toString();
}

// ── Display Service Implementation ────────────────────────────────
DisplayServiceClass::DisplayServiceClass() : canvas(&tft) {}

void DisplayServiceClass::begin() {
  canvas.createSprite(TFT_W, TFT_H);
  canvas.fillSprite(TFT_BLACK);
}

TFT_eSprite* DisplayServiceClass::getCanvas() {
  return &canvas;
}

void DisplayServiceClass::clear() {
  canvas.fillSprite(TFT_BLACK);
}

void DisplayServiceClass::commit() {
  canvas.pushSprite(0, 0);
}

// ── Storage Service Implementation ────────────────────────────────
void StorageServiceClass::begin() {
  Preferences p;
  p.begin("aerosniffer", false);
  if (!p.isKey("ssid")) {
    p.putString("ssid", DEFAULT_WIFI_SSID);
    p.putString("pass", DEFAULT_WIFI_PASSWORD);
  }
  if (!p.isKey("avi_refresh")) {
    p.putUInt("avi_refresh", 30000);
  }
  sys_avi_refresh = p.getUInt("avi_refresh", 30000);
  p.end();
}

String StorageServiceClass::getSSID() {
  Preferences p;
  p.begin("aerosniffer", true);
  String val = p.getString("ssid", DEFAULT_WIFI_SSID);
  p.end();
  return val;
}

String StorageServiceClass::getPassword() {
  Preferences p;
  p.begin("aerosniffer", true);
  String val = p.getString("pass", DEFAULT_WIFI_PASSWORD);
  p.end();
  return val;
}

void StorageServiceClass::saveWiFi(String ssid, String pass) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}

float StorageServiceClass::getSkyLamin() {
  Preferences p;
  p.begin("aerosniffer", true);
  float val = p.getFloat("lamin", DEFAULT_SKY_LAMIN);
  p.end();
  return val;
}

float StorageServiceClass::getSkyLomin() {
  Preferences p;
  p.begin("aerosniffer", true);
  float val = p.getFloat("lomin", DEFAULT_SKY_LOMIN);
  p.end();
  return val;
}

float StorageServiceClass::getSkyLamax() {
  Preferences p;
  p.begin("aerosniffer", true);
  float val = p.getFloat("lamax", DEFAULT_SKY_LAMAX);
  p.end();
  return val;
}

float StorageServiceClass::getSkyLomax() {
  Preferences p;
  p.begin("aerosniffer", true);
  float val = p.getFloat("lomax", DEFAULT_SKY_LOMAX);
  p.end();
  return val;
}

void StorageServiceClass::saveSkyBounds(float lamin, float lomin, float lamax, float lomax) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putFloat("lamin", lamin);
  p.putFloat("lomin", lomin);
  p.putFloat("lamax", lamax);
  p.putFloat("lomax", lomax);
  p.end();
}

uint16_t StorageServiceClass::getClockColor() {
  Preferences p;
  p.begin("aerosniffer", true);
  uint16_t val = p.getUShort("c_col", DEFAULT_CLOCK_COLOR);
  p.end();
  return val;
}

uint16_t StorageServiceClass::getWeatherColor() {
  Preferences p;
  p.begin("aerosniffer", true);
  uint16_t val = p.getUShort("w_col", DEFAULT_WEATHER_COLOR);
  p.end();
  return val;
}

void StorageServiceClass::saveColors(uint16_t clock, uint16_t weather) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUShort("c_col", clock);
  p.putUShort("w_col", weather);
  p.end();
}

uint32_t StorageServiceClass::getSavedMode() {
  Preferences p;
  p.begin("aerosniffer", true);
  uint32_t val = p.getUInt("mode", 0);
  p.end();
  return val;
}

void StorageServiceClass::saveMode(uint32_t mode) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUInt("mode", mode);
  p.end();
}

uint32_t StorageServiceClass::getStat(const char* key, uint32_t def) {
  Preferences p;
  p.begin("aerosniffer", true);
  uint32_t val = p.getUInt(key, def);
  p.end();
  return val;
}

void StorageServiceClass::setStat(const char* key, uint32_t val) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUInt(key, val);
  p.end();
}

void StorageServiceClass::incrementStat(const char* key, uint32_t amount) {
  Preferences p;
  p.begin("aerosniffer", false);
  uint32_t val = p.getUInt(key, 0) + amount;
  p.putUInt(key, val);
  p.end();
}

// ── Security / Control Layer Settings ──────────────────────────
String StorageServiceClass::getDeviceName() {
  Preferences p;
  p.begin("aerosniffer", true);
  String val = p.getString("dev_name", DEFAULT_DEVICE_NAME);
  p.end();
  return val;
}
void StorageServiceClass::saveDeviceName(String name) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putString("dev_name", name);
  p.end();
}
String StorageServiceClass::getAPSsid() {
  Preferences p;
  p.begin("aerosniffer", true);
  String val = p.getString("ap_ssid", DEFAULT_AP_SSID);
  p.end();
  return val;
}
String StorageServiceClass::getAPPass() {
  Preferences p;
  p.begin("aerosniffer", true);
  String val = p.getString("ap_pass", DEFAULT_AP_PASS);
  p.end();
  return val;
}
void StorageServiceClass::saveAPConfig(String ssid, String pass) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putString("ap_ssid", ssid);
  p.putString("ap_pass", pass);
  p.end();
}
uint8_t StorageServiceClass::getThreatSensitivity() {
  Preferences p;
  p.begin("aerosniffer", true);
  uint8_t val = p.getUChar("thr_sens", DEFAULT_THREAT_SENSITIVITY);
  p.end();
  return val;
}
void StorageServiceClass::saveThreatSensitivity(uint8_t level) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUChar("thr_sens", level);
  p.end();
}
bool StorageServiceClass::getHomeGuardEnabled() {
  Preferences p;
  p.begin("aerosniffer", true);
  bool val = p.getUChar("hg_enable", DEFAULT_HOMEGUARD_ENABLED);
  p.end();
  return val;
}
void StorageServiceClass::saveHomeGuardEnabled(bool enabled) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUChar("hg_enable", enabled ? 1 : 0);
  p.end();
}
bool StorageServiceClass::getAviationEnabled() {
  Preferences p;
  p.begin("aerosniffer", true);
  bool val = p.getUChar("avi_enable", DEFAULT_AVIATION_ENABLED);
  p.end();
  return val;
}
void StorageServiceClass::saveAviationEnabled(bool enabled) {
  Preferences p;
  p.begin("aerosniffer", false);
  p.putUChar("avi_enable", enabled ? 1 : 0);
  p.end();
}

// Stats legacy mapping
uint32_t StorageServiceClass::getFlightsSeen() { return getStat("flights_seen", 0); }
void StorageServiceClass::incrementFlightsSeen() { incrementStat("flights_seen", 1); }
uint32_t StorageServiceClass::getNetworksDiscovered() { return getStat("nets_seen", 0); }
void StorageServiceClass::addNetworksDiscovered(uint32_t count) { incrementStat("nets_seen", count); }
uint32_t StorageServiceClass::getCodingTimeMinutes() { return getStat("code_time", 0); }
void StorageServiceClass::addCodingTimeMinutes(uint32_t mins) { incrementStat("code_time", mins); }
uint32_t StorageServiceClass::getActiveHours() { return getStat("active_hrs", 0); }
void StorageServiceClass::incrementActiveHours() { incrementStat("active_hrs", 1); }

// ── Flight Statistics Service Implementation ────────────────────
void FlightStatsClass::begin() {
  Preferences p;
  p.begin("flightstats", true);
  aircraft_seen_today = p.getUInt("seen_today", 0);
  aircraft_seen_lifetime = p.getUInt("seen_lifetime", 0);
  last_aircraft_count = p.getUInt("last_count", 0);
  max_aircraft_seen = p.getUInt("max_seen", 0);
  p.end();
}

void FlightStatsClass::save() {
  Preferences p;
  p.begin("flightstats", false);
  p.putUInt("seen_today", aircraft_seen_today);
  p.putUInt("seen_lifetime", aircraft_seen_lifetime);
  p.putUInt("last_count", last_aircraft_count);
  p.putUInt("max_seen", max_aircraft_seen);
  p.end();
}

void FlightStatsClass::recordFetch(uint32_t current_count) {
  last_aircraft_count = current_count;
  if (current_count > max_aircraft_seen) {
    max_aircraft_seen = current_count;
  }
  save();
}

void FlightStatsClass::incrementSeen(uint32_t amount) {
  aircraft_seen_today += amount;
  aircraft_seen_lifetime += amount;
  save();
}

void FlightStatsClass::resetDaily() {
  aircraft_seen_today = 0;
  save();
}

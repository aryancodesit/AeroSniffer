// ================================================================
//  AeroSnifferOS.cpp  —  Modular Services & Central Brain
//  AeroSniffer OS Layer | C++ Shared Infrastructure
// ================================================================
#include "AeroSnifferOS.h"
#include "Config.h"
#include "Companion/AttentionEngine.h"
#include "Companion/MoodEngine.h"
#include <math.h>

extern TFT_eSPI tft;

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
  0      // pc_ram
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
  if (sub_count >= MAX_SUBSCRIBERS) return;
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
  const char* names[] = {"Idle", "Coding", "Music", "Aviation", "Scanning", "Thinking", "Sleeping", "Typing"};
  return (current_activity < ACTIVITY_COUNT) ? names[current_activity] : "Unknown";
}

// ── Face Engine v2 Layer Implementation ─────────────────────────
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
  
  _last_mood         = g_creature.mood;
  _last_mood_strength = g_creature.mood_strength;
  recomputeMoodPresentation();
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

  // ── Update mood presentation if mood or strength changed (Sprint 3B) ─
  if (g_creature.mood != _last_mood || g_creature.mood_strength != _last_mood_strength) {
    _last_mood         = g_creature.mood;
    _last_mood_strength = g_creature.mood_strength;
    recomputeMoodPresentation();
  }

  if (now > anim_blink_next && !anim_is_blinking) {
    anim_is_blinking = true;
    anim_blink_scale = 1.0f;
  }
  
  if (anim_is_blinking) {
    if (anim_blink_scale > 0.1f && frame % 2 == 0) {
      anim_blink_scale -= 0.4f;
    } else if (anim_blink_scale <= 0.1f) {
      anim_blink_scale = 0.0f;
    }
    if (anim_blink_scale <= 0.0f && frame % 3 == 0) {
      anim_blink_scale = 0.1f;
    } else if (anim_blink_scale > 0.0f && anim_blink_scale < 1.0f) {
      anim_blink_scale += 0.2f;
    }
    if (anim_blink_scale >= 1.0f) {
      anim_blink_scale = 1.0f;
      anim_is_blinking = false;
      anim_blink_next = now + _mood_pres.blink_interval_ms + random(-500, 500);
    }
  }
  
  // ── Attention-driven gaze ──────────────────────────────────
  // Use target for direction, strength (0-100) for magnitude.
  // As strength decays toward 0, gaze gradually returns to
  // neutral, then resumes random wandering.
  // [Sprint 2A] FaceEngine now consumes the V2.5 structured
  // attention model. The V2.4 attention_state shim is retained
  // elsewhere for rollback safety and will be removed in 2B.
  if (g_creature.attention.target == TARGET_THREAT) {
    anim_look_x = 0;
    anim_look_y = 0;                               // dead ahead, locked
  } else if (g_creature.attention.target == TARGET_USER) {
    anim_look_x = 0;
    anim_look_y = -(1 + (g_creature.attention.strength / 33));  // -1 to -3, upward engaged
  } else if (g_creature.attention.target == TARGET_FLIGHT) {
    anim_look_x = 0;
    anim_look_y = -(1 + (g_creature.attention.strength / 25));  // -1 to -4, skyward
  } else if (g_creature.attention.strength >= 25) {
    anim_look_x = 0;
    anim_look_y = -1;                              // mild curiosity
  } else {
    // SOFT_FOCUS & TARGET_NONE + low strength — random wandering
    // Sprint 3B: eye_intensity applies only to idle wander, NOT to attention-driven
    // gaze (THREAT/USER/FLIGHT).  Attention targets are reactive behaviors triggered
    // by external stimuli — they should produce the same response regardless of mood.
    // Mood colors internal/expressive behavior only (idle gaze, blink rate, sway).
    if (now > anim_look_next) {
      if (g_creature.emotion == EMOTION_CALM || g_creature.emotion == EMOTION_CURIOUS) {
        int range_x = (int)(6.0f * _mood_pres.eye_intensity);
        int range_y = (int)(4.0f * _mood_pres.eye_intensity);
        anim_look_x = random(-range_x, range_x + 1);
        anim_look_y = random(-range_y, range_y + 1);
      } else {
        anim_look_x = 0;
        anim_look_y = 0;
      }
      anim_look_next = now + random(1000, 4000);
    }
  }

  // Bounce animation when happy, excited, or loving — mood-modulated (Sprint 3B)
  if (g_creature.emotion == EMOTION_EXCITED || g_creature.emotion == EMOTION_HAPPY || g_creature.emotion == EMOTION_LOVE) {
    anim_bounce_y = (int)(sin(frame * 0.4f * _mood_pres.idle_speed) * 4.0f * _mood_pres.idle_amplitude);
  } else {
    anim_bounce_y = 0;
  }

  // Pulse animation when excited or alert — mood-modulated speed (Sprint 3B)
  if (g_creature.emotion == EMOTION_EXCITED || g_creature.emotion == EMOTION_ALERT) {
    anim_pulse_scale = 1.0f + sin(frame * 0.3f * _mood_pres.idle_speed) * 0.15f;
  } else {
    anim_pulse_scale = 1.0f;
  }
}

void FaceEngineClass::renderEmotionLayer(TFT_eSprite* spr, int frame) {
  uint16_t color = C_IDLE;
  switch (g_creature.emotion) {
    case EMOTION_HAPPY:     color = C_HAPPY; break;
    case EMOTION_SAD:       color = C_SAD; break;
    case EMOTION_ANGRY:     color = C_ALERT; break;
    case EMOTION_CURIOUS:   color = C_THINKING; break;
    case EMOTION_SLEEPY:    color = C_SLEEPY; break;
    case EMOTION_CALM:      color = C_IDLE; break;
    case EMOTION_ALERT:     color = C_ALERT; break;
    case EMOTION_LOVE:      color = C_LOVE; break;
    case EMOTION_SURPRISED: color = C_SURPRISED; break;
    default:                color = C_IDLE; break;
  }
  
  int h = (int)(32 * anim_blink_scale * anim_pulse_scale);
  int y = 90 + anim_bounce_y + (32 - h) / 2;
  
  // Custom eye shapes based on emotion
  if (g_creature.emotion == EMOTION_HAPPY) {
    int h_hap = (int)(24 * anim_blink_scale * anim_pulse_scale);
    int y_hap = 90 + anim_bounce_y + (24 - h_hap) / 2;
    if (h_hap > 10) {
      spr->fillCircle(80, y_hap + 10, (int)(22 * anim_pulse_scale), color);
      spr->fillCircle(80, y_hap + 16, (int)(22 * anim_pulse_scale), C_BG);
      spr->fillCircle(160, y_hap + 10, (int)(22 * anim_pulse_scale), color);
      spr->fillCircle(160, y_hap + 16, (int)(22 * anim_pulse_scale), C_BG);
    } else {
      spr->fillRoundRect(60, y_hap, (int)(40 * anim_pulse_scale), h_hap, 2, color);
      spr->fillRoundRect(140, y_hap, (int)(40 * anim_pulse_scale), h_hap, 2, color);
    }
  }
  else if (g_creature.emotion == EMOTION_SLEEPY) {
    int h_slp = (int)(10 * anim_blink_scale * anim_pulse_scale);
    int y_slp = 100 + anim_bounce_y + (10 - h_slp) / 2;
    spr->fillRoundRect(60, y_slp, (int)(40 * anim_pulse_scale), h_slp, 4, color);
    spr->fillRoundRect(140, y_slp, (int)(40 * anim_pulse_scale), h_slp, 4, color);
  }
  else if (g_creature.emotion == EMOTION_SURPRISED) {
    int h_sur = (int)(50 * anim_blink_scale * anim_pulse_scale);
    int y_sur = 84 + anim_bounce_y + (50 - h_sur) / 2;
    if (h_sur > 30) {
      spr->fillCircle(80, y_sur + h_sur / 2, (int)(h_sur / 2), color);
      spr->fillCircle(160, y_sur + h_sur / 2, (int)(h_sur / 2), color);
      spr->fillCircle(80, y_sur + h_sur / 2, (int)(8 * anim_pulse_scale), C_BLACK);
      spr->fillCircle(160, y_sur + h_sur / 2, (int)(8 * anim_pulse_scale), C_BLACK);
    } else {
      spr->fillRoundRect(55, y_sur, (int)(50 * anim_pulse_scale), h_sur, 8, color);
      spr->fillRoundRect(135, y_sur, (int)(50 * anim_pulse_scale), h_sur, 8, color);
    }
  }
  else {
    // Default round rect eyes
    spr->fillRoundRect(60, y, (int)(40 * anim_pulse_scale), h, 6, color);
    spr->fillRoundRect(140, y, (int)(40 * anim_pulse_scale), h, 6, color);
    if (h > 10) {
      spr->fillRect(76 + anim_look_x, y + h / 2 - 4 + anim_look_y, 8, 8, C_BLACK);
      spr->fillRect(156 + anim_look_x, y + h / 2 - 4 + anim_look_y, 8, 8, C_BLACK);
    }
    
    // Draw eyebrows for angry/alert
    if (g_creature.emotion == EMOTION_ANGRY || g_creature.emotion == EMOTION_ALERT) {
      spr->fillTriangle(50, 74 + anim_bounce_y, 100, 84 + anim_bounce_y, 100, 78 + anim_bounce_y, color);
      spr->fillTriangle(190, 74 + anim_bounce_y, 140, 84 + anim_bounce_y, 140, 78 + anim_bounce_y, color);
    }
  }
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

void FaceEngineClass::render(TFT_eSprite* spr, int frame) {
  // Overdraw background
  spr->fillRect(0, 40, TFT_W, 160, C_BG);
  
  updateAnimations(frame);
  renderEmotionLayer(spr, frame);
  renderActivityLayer(spr, frame);
  renderEffectLayer(spr, frame);
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

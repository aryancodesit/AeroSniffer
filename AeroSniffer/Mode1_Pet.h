// ================================================================
//  Mode1_Pet.h  —  Vector-style Robot Desk Companion
//  AeroSniffer v2 | XIAO ESP32S3 + ST7789 240×240
//
//  Programmatic geometric rendering using TFT_eSPI.
//  No BMPs required! Features smooth blinking and dynamic eyes.
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include "Config.h"
#include <math.h>

static TFT_eSPI* _rtft = nullptr;

// ================================================================
//  COLOUR PALETTE  (RGB565)
// ================================================================
#define C_BG          0x10A2   // Very dark charcoal background
#define C_BLACK       0x0000

#define C_IDLE        0x05FF   // Blue
#define C_HAPPY       0x07E0   // Green
#define C_EXCITED     0xFFE0   // Yellow
#define C_SLEEPY      0x421F   // Indigo/Blue
#define C_THINKING    0xB31F   // Violet
#define C_SAD         0xF800   // Red
#define C_ALERT       0xFC00   // Orange
#define C_LOVE        0xF81F   // Pink
#define C_STARTUP     0x07FF   // Cyan
#define C_SURPRISED   0xFFFF   // White (eyes)

// ================================================================
//  FACE STATE
// ================================================================
enum RobotFace : uint8_t {
  FACE_IDLE = 0,
  FACE_HAPPY,
  FACE_EXCITED,
  FACE_SLEEPY,
  FACE_THINKING,
  FACE_SAD,
  FACE_ALERT,
  FACE_LOVE,
  FACE_STARTUP,
  FACE_SURPRISED,
  FACE_COUNT
};

static RobotFace face_current    = FACE_IDLE;
static RobotFace face_last       = FACE_COUNT;
static uint32_t  last_active_ms  = 0;
static bool      face_dirty      = true;
static uint32_t  _manual_face_override_ms = 0;

// ── Animation state ───────────────────────────────────────────────
static float     anim_blink_scale = 1.0f; // 1.0 = open, 0.0 = closed
static bool      anim_is_blinking = false;
static uint32_t  anim_blink_next  = 0;
static int       anim_look_x      = 0;
static int       anim_look_y      = 0;
static uint32_t  anim_look_next   = 0;

static char      pc_status[40]    = "";
static char      pc_app[32]       = "";

// ── Particle System ───────────────────────────────────────────────
enum ParticleType {
  PT_NONE = 0,
  PT_MUSIC,
  PT_SWEAT,
  PT_HEART
};

struct Particle {
  ParticleType type;
  float x;
  float y;
  float life; // 1.0 to 0.0
  uint16_t color;
};

#define MAX_PARTICLES 5
static Particle active_particles[MAX_PARTICLES];

// ================================================================
//  GEOMETRY HELPERS
// ================================================================
static void draw_heart(int cx, int cy, int size, uint16_t color) {
  int r = size / 2;
  _rtft->fillCircle(cx - r / 2 + 2, cy - r / 4, r, color);
  _rtft->fillCircle(cx + r / 2 - 2, cy - r / 4, r, color);
  _rtft->fillTriangle(cx - r - 7, cy - 2,
                      cx + r + 7, cy - 2,
                      cx, cy + size - 2, color);
}

static void draw_dotted_smile(int cx, int cy, int width, int drop, uint16_t color) {
  _rtft->fillRect(cx - width / 2, cy - drop, 4, 4, color);
  _rtft->fillRect(cx - width / 4, cy, 4, 4, color);
  _rtft->fillRect(cx + width / 4, cy, 4, 4, color);
  _rtft->fillRect(cx + width / 2, cy - drop, 4, 4, color);
}

static void draw_dotted_frown(int cx, int cy, int width, int drop, uint16_t color) {
  _rtft->fillRect(cx - width / 2, cy + drop, 4, 4, color);
  _rtft->fillRect(cx - width / 4, cy, 4, 4, color);
  _rtft->fillRect(cx + width / 4, cy, 4, 4, color);
  _rtft->fillRect(cx + width / 2, cy + drop, 4, 4, color);
}

static void draw_music_note(int cx, int cy, uint16_t color) {
  _rtft->fillCircle(cx, cy, 3, color);
  _rtft->fillRect(cx + 1, cy - 10, 2, 10, color);
  _rtft->fillRect(cx + 1, cy - 12, 6, 2, color);
}

static void draw_sweat(int cx, int cy, uint16_t color) {
  _rtft->fillCircle(cx, cy, 4, color);
  _rtft->fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy - 8, color);
}

// ================================================================
//  FACE RENDERERS
// ================================================================
static void draw_face_idle(int frame) {
  int h = (int)(32 * anim_blink_scale);
  int y = 90 + (32 - h) / 2;

  // Left Eye
  _rtft->fillRoundRect(60, y, 40, h, 6, C_IDLE);
  if (h > 10) _rtft->fillRect(76 + anim_look_x, y + h / 2 - 4 + anim_look_y, 8, 8, C_BLACK);

  // Right Eye
  _rtft->fillRoundRect(140, y, 40, h, 6, C_IDLE);
  if (h > 10) _rtft->fillRect(156 + anim_look_x, y + h / 2 - 4 + anim_look_y, 8, 8, C_BLACK);

  // Mouth
  _rtft->fillRect(95, 160, 50, 6, C_IDLE);
}

static void draw_face_happy(int frame) {
  int h = (int)(24 * anim_blink_scale);
  int y = 90 + (24 - h) / 2;

  if (h > 10) {
    // Happy eyes (perfect half-circle arches)
    _rtft->fillCircle(80, y + 10, 22, C_HAPPY);
    _rtft->fillCircle(80, y + 16, 22, C_BG);

    _rtft->fillCircle(160, y + 10, 22, C_HAPPY);
    _rtft->fillCircle(160, y + 16, 22, C_BG);
  } else {
    // Blinking state
    _rtft->fillRoundRect(60, y, 40, h, 2, C_HAPPY);
    _rtft->fillRoundRect(140, y, 40, h, 2, C_HAPPY);
  }

  // Solid crescent smile
  _rtft->fillCircle(120, 155, 16, C_HAPPY);
  _rtft->fillCircle(120, 149, 16, C_BG);
}

static void draw_face_excited(int frame) {
  int h = (int)(44 * anim_blink_scale);
  int y = 84 + (44 - h) / 2;

  // Big square eyes
  _rtft->fillRoundRect(56, y, 44, h, 8, C_EXCITED);
  if (h > 12) _rtft->fillRoundRect(64, y + h - 16, 12, 12, 2, C_BLACK); // Pupil

  _rtft->fillRoundRect(140, y, 44, h, 8, C_EXCITED);
  if (h > 12) _rtft->fillRoundRect(148, y + h - 16, 12, 12, 2, C_BLACK);

  // O mouth
  _rtft->fillCircle(120, 160, 10, C_EXCITED);
  _rtft->fillCircle(120, 160, 4, C_BG);

  // Sparks
  if (frame % 8 < 4) {
    _rtft->fillRect(40, 50, 4, 4, C_EXCITED);
    _rtft->fillRect(196, 50, 4, 4, C_EXCITED);
  }
}

static void draw_face_sleepy(int frame) {
  int h = (int)(10 * anim_blink_scale);
  int y = 100 + (10 - h) / 2;

  _rtft->fillRoundRect(60, y, 40, h, 4, C_SLEEPY);
  _rtft->fillRoundRect(140, y, 40, h, 4, C_SLEEPY);

  _rtft->fillRect(105, 160, 30, 4, C_SLEEPY);

  // Animated Zzz
  int z = (frame / 10) % 3;
  if (z >= 0) _rtft->fillRect(150, 50, 6, 2, C_SLEEPY); // Tiny z
  if (z >= 1) _rtft->fillRect(165, 35, 10, 2, C_SLEEPY); // Med z
  if (z >= 2) _rtft->fillRect(185, 15, 14, 3, C_SLEEPY); // Big Z
}

static void draw_face_thinking(int frame) {
  // Left eye squint
  int hL = (int)(14 * anim_blink_scale);
  int yL = 100 + (14 - hL) / 2;
  _rtft->fillRoundRect(60, yL, 40, hL, 4, C_THINKING);
  if (hL > 6) _rtft->fillRect(76 + anim_look_x, yL + hL / 2 - 3, 6, 6, C_BLACK);

  // Right eye open
  int hR = (int)(32 * anim_blink_scale);
  int yR = 90 + (32 - hR) / 2;
  _rtft->fillRoundRect(140, yR, 40, hR, 6, C_THINKING);
  if (hR > 10) _rtft->fillRect(156 + anim_look_x, yR + hR / 2 - 4, 8, 8, C_BLACK);

  // Offset mouth
  _rtft->fillRect(130, 160, 20, 6, C_THINKING);

  // Thinking dots
  if (frame % 12 < 6) _rtft->fillRect(40, 50, 6, 6, C_THINKING);
}

static void draw_face_sad(int frame) {
  int h = (int)(36 * anim_blink_scale);
  int y = 90 + (36 - h) / 2;

  _rtft->fillRoundRect(60, y, 36, h, 4, C_SAD);
  if (h > 10) _rtft->fillRect(74, y + 8, 8, 8, C_BLACK);

  _rtft->fillRoundRect(144, y, 36, h, 4, C_SAD);
  if (h > 10) _rtft->fillRect(158, y + 8, 8, 8, C_BLACK);

  // Tears
  if ((frame / 5) % 2 == 0) {
    _rtft->fillRect(76, y + h + 10, 4, 12, C_SAD);
    _rtft->fillRect(160, y + h + 10, 4, 12, C_SAD);
  }

  draw_dotted_frown(120, 160, 40, 10, C_SAD);
}

static void draw_face_alert(int frame) {
  int h = (int)(32 * anim_blink_scale);
  int y = 94 + (32 - h) / 2;

  _rtft->fillRoundRect(60, y, 40, h, 4, C_ALERT);
  if (h > 10) _rtft->fillRect(76, y + 10, 8, 8, C_BLACK);
  // Left eyebrow
  _rtft->fillTriangle(50, 74, 100, 84, 100, 78, C_ALERT);

  _rtft->fillRoundRect(140, y, 40, h, 4, C_ALERT);
  if (h > 10) _rtft->fillRect(156, y + 10, 8, 8, C_BLACK);
  // Right eyebrow
  _rtft->fillTriangle(190, 74, 140, 84, 140, 78, C_ALERT);

  _rtft->fillRect(100, 160, 40, 6, C_ALERT);
}

static void draw_face_love(int frame) {
  int h = (int)(32 * anim_blink_scale);
  int y = 90 + (32 - h) / 2;

  // Pink rounded rect eyes
  _rtft->fillRoundRect(60, y, 40, h, 6, C_LOVE);
  _rtft->fillRoundRect(140, y, 40, h, 6, C_LOVE);

  if (h > 15) {
    if ((frame / 15) % 2 == 0) {
      uint16_t c_dark = 0xA004; // Dark red/pink
      _rtft->fillRect(80 - 10, y + h / 2 - 10, 20, 20, c_dark);
      _rtft->fillRect(160 - 10, y + h / 2 - 10, 20, 20, c_dark);
    }
  }

  // Solid crescent smile
  _rtft->fillCircle(120, 155, 12, C_LOVE);
  _rtft->fillCircle(120, 151, 12, C_BG);

  if (frame % 10 < 5) {
    _rtft->fillRect(40, 60, 4, 4, C_LOVE);
    _rtft->fillRect(196, 60, 4, 4, C_LOVE);
  }
}

static void draw_face_startup(int frame) {
  int h = (int)(36 * anim_blink_scale);
  int y = 90 + (36 - h) / 2;

  if (h > 20) {
    _rtft->fillRect(60, y, 40, 8, C_STARTUP);
    _rtft->fillRect(60, y + 14, 40, 8, C_STARTUP);
    _rtft->fillRect(60, y + 28, 40, 8, C_STARTUP);

    _rtft->fillRect(140, y, 40, 8, C_STARTUP);
    _rtft->fillRect(140, y + 14, 40, 8, C_STARTUP);
    _rtft->fillRect(140, y + 28, 40, 8, C_STARTUP);
  } else {
    _rtft->fillRect(60, y, 40, h, C_STARTUP);
    _rtft->fillRect(140, y, 40, h, C_STARTUP);
  }

  // Progress bar mouth
  _rtft->fillRect(80, 160, 80, 8, 0x0328); // Dark cyan
  int prog = (frame % 20) * 4;
  _rtft->fillRect(80, 160, prog, 8, C_STARTUP);
}

static void draw_face_surprised(int frame) {
  int h = (int)(50 * anim_blink_scale);
  int y = 84 + (50 - h) / 2;

  if (h > 30) {
    _rtft->fillCircle(80, y + h / 2, h / 2, C_SURPRISED);
    _rtft->fillCircle(160, y + h / 2, h / 2, C_SURPRISED);

    _rtft->fillCircle(80, y + h / 2, 8, C_BLACK);
    _rtft->fillCircle(160, y + h / 2, 8, C_BLACK);
  } else {
    _rtft->fillRoundRect(55, y, 50, h, 8, C_SURPRISED);
    _rtft->fillRoundRect(135, y, 50, h, 8, C_SURPRISED);
  }

  _rtft->fillCircle(120, 164, 12, C_EXCITED);
  _rtft->fillCircle(120, 164, 6, C_BG);
}

// ================================================================
//  DISPATCH
// ================================================================
typedef void (*FaceDrawFn)(int);
static FaceDrawFn face_fns[FACE_COUNT] = {
  draw_face_idle,
  draw_face_happy,
  draw_face_excited,
  draw_face_sleepy,
  draw_face_thinking,
  draw_face_sad,
  draw_face_alert,
  draw_face_love,
  draw_face_startup,
  draw_face_surprised
};

// ================================================================
//  STATUS BAR
// ================================================================
static void draw_status_bar() {
  _rtft->fillRect(0, 200, TFT_W, 40, C_BG);

  if (strlen(pc_status) > 0) {
    _rtft->setTextColor(0x7BEF); // Light grey
    _rtft->setTextSize(1);
    _rtft->setCursor((TFT_W - strlen(pc_status) * 6) / 2, 206);
    _rtft->print(pc_status);
  }

  if (strlen(pc_app) > 0) {
    _rtft->setTextColor(0x4208); // Dark grey
    _rtft->setCursor(4, 222);
    _rtft->printf("[%s]", pc_app);
  }
}

// ================================================================
//  SERIAL PARSER
// ================================================================
static void pet_handle_command(String line) {
  line.trim();
  if (line.startsWith("CMD:")) {
    line = line.substring(4);
  }

  // Any command received from serial counts as user activity to keep pet awake
  last_active_ms = millis();
  last_serial_rx_ms = millis();

  if (line.startsWith("FACE:")) {
    if (millis() < _manual_face_override_ms) {
      return; // Ignore PC agent during manual override preview
    }
    String faceStr = line.substring(5);
    const char* names[] = {
      "IDLE", "HAPPY", "EXCITED", "SLEEPY", "THINKING",
      "SAD_ERROR", "ALERT_WARNING", "LOVE_BONDING",
      "STARTUP_BOOT", "SURPRISED"
    };
    for (int i = 0; i < FACE_COUNT; i++) {
      if (faceStr == names[i]) {
        if (face_current != (RobotFace)i) {
          face_current = (RobotFace)i;
          face_dirty   = true;
          last_active_ms = millis();
        }
        break;
      }
    }
  } else if (line.startsWith("STATUS:")) {
    strncpy(pc_status, line.substring(7).c_str(), sizeof(pc_status) - 1);
    face_dirty = true;
  } else if (line.startsWith("APP:")) {
    strncpy(pc_app, line.substring(4).c_str(), sizeof(pc_app) - 1);
    face_dirty = true;
  } else if (line.startsWith("PARTICLE:")) {
    String pType = line.substring(9);
    ParticleType pt = PT_NONE;
    if (pType == "MUSIC") pt = PT_MUSIC;
    else if (pType == "SWEAT") pt = PT_SWEAT;
    else if (pType == "HEART") pt = PT_HEART;

    if (pt != PT_NONE) {
      for (int i = 0; i < MAX_PARTICLES; i++) {
        if (active_particles[i].type == PT_NONE || active_particles[i].life <= 0) {
          active_particles[i].type = pt;
          active_particles[i].life = 1.0f;
          if (pt == PT_MUSIC) {
            active_particles[i].x = random(30, 210);
            active_particles[i].y = 160;
            active_particles[i].color = 0x07FF; // Cyan
          } else if (pt == PT_SWEAT) {
            active_particles[i].x = random(180, 210);
            active_particles[i].y = 50;
            active_particles[i].color = 0x05FF; // Blue
          } else if (pt == PT_HEART) {
            active_particles[i].x = random(30, 210);
            active_particles[i].y = 150;
            active_particles[i].color = C_LOVE;
          }
          break;
        }
      }
    }
  }
}

// ================================================================
//  PUBLIC API
// ================================================================
void pet_setup(TFT_eSPI* tft) {
  _rtft = tft;
  face_current = FACE_STARTUP;
  face_last    = FACE_COUNT;
  face_dirty   = true;
  last_active_ms = millis();
  anim_blink_scale = 1.0f;
  anim_is_blinking = false;
  anim_blink_next  = millis() + 3000;
  anim_look_next   = millis() + 2000;

  memset(pc_status, 0, sizeof(pc_status));
  memset(pc_app,    0, sizeof(pc_app));
  for (int i = 0; i < MAX_PARTICLES; i++) {
    active_particles[i].type = PT_NONE;
    active_particles[i].life = 0;
  }

  _rtft->fillScreen(C_BG);

  Serial.begin(115200);
  Serial.println("{\"ready\":1,\"mode\":\"pet\"}");
}

void pet_teardown() {
  _rtft = nullptr;
}

static uint32_t last_serial_rx_ms = 0;
static uint32_t last_standalone_weather_fetch = 0;

static void pet_fetch_standalone_weather() {
  if (sys_wifi_ssid.length() == 0 || sys_wifi_ssid == "YOUR_WIFI_SSID") return;

  g_block_touch = true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(sys_wifi_ssid.c_str(), sys_wifi_pass.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    vTaskDelay(pdMS_TO_TICKS(500));
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // IST (UTC+5:30)

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
        if (getLocalTime(&timeinfo)) {
          strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
          snprintf(pc_status, sizeof(pc_status), "%s | %s", time_str, payload.c_str());
        } else {
          snprintf(pc_status, sizeof(pc_status), "%s", payload.c_str());
        }
        strncpy(pc_app, "Wireless Mode", sizeof(pc_app) - 1);
      }
    }
    http.end();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  g_block_touch = false;
  face_dirty = true;
}

void pet_core0_task() {
  uint32_t now = millis();
  if (now - last_serial_rx_ms > 15000) {
    if (last_standalone_weather_fetch == 0 || now - last_standalone_weather_fetch > 900000) {
      last_standalone_weather_fetch = now;
      pet_fetch_standalone_weather();
    }
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void pet_core1_task() {
  if (!_rtft) return;
  static int frame = 0;
  uint32_t now = millis();
  frame++;

  // Handle capacitive touch interaction — cycle through faces on single tap
  extern volatile bool g_touch_tap;
  if (g_touch_tap) {
    g_touch_tap = false;
    face_current = (RobotFace)((face_current + 1) % FACE_COUNT);
    if (face_current == FACE_STARTUP) {
      face_current = (RobotFace)((face_current + 1) % FACE_COUNT); // Skip boot animation
    }
    face_dirty = true;
    last_active_ms = now;
    _manual_face_override_ms = now + 8000; // Lock manual selection for 8 seconds
  }

  // ── Animation Logic ───────────────────────────────────────────
  bool anim_changed = false;

  // Blinking physics
  if (now > anim_blink_next && !anim_is_blinking) {
    anim_is_blinking = true;
    anim_blink_scale = 1.0f;
  }

  if (anim_is_blinking) {
    // Snap close, ease open
    if (anim_blink_scale > 0.1f && frame % 2 == 0) {
      anim_blink_scale -= 0.4f; // close fast
    } else if (anim_blink_scale <= 0.1f) {
      anim_blink_scale = 0.0f;
    }

    // If closed, start opening
    if (anim_blink_scale <= 0.0f && frame % 3 == 0) {
      anim_blink_scale = 0.1f; // start open
    } else if (anim_blink_scale > 0.0f && anim_blink_scale < 1.0f) {
      anim_blink_scale += 0.2f; // open slower
    }

    if (anim_blink_scale >= 1.0f) {
      anim_blink_scale = 1.0f;
      anim_is_blinking = false;
      anim_blink_next = now + random(2000, 6000);
    }
    anim_changed = true;
  }

  // Look around (Idle/Thinking only)
  if (now > anim_look_next) {
    if (face_current == FACE_IDLE || face_current == FACE_THINKING) {
      anim_look_x = random(-6, 7);
      anim_look_y = random(-4, 5);
    } else {
      anim_look_x = 0;
      anim_look_y = 0;
    }
    anim_look_next = now + random(1000, 4000);
    anim_changed = true;
  }

  // Auto-sleep fallback
  if (face_current != FACE_SLEEPY && now - last_active_ms > 30000) {
    face_current = FACE_SLEEPY;
    face_dirty = true;
  }

  // ── Render ────────────────────────────────────────────────────
  bool needs_anim = (face_current == FACE_EXCITED || face_current == FACE_SAD ||
                     face_current == FACE_STARTUP || face_current == FACE_THINKING ||
                     face_current == FACE_LOVE    || face_current == FACE_SLEEPY);

  bool has_particles = false;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (active_particles[i].type != PT_NONE && active_particles[i].life > 0) {
      has_particles = true;
      break;
    }
  }

  if (face_dirty || face_current != face_last || anim_changed || has_particles || (needs_anim && frame % 3 == 0)) {
    // Overdraw background to erase previous frame
    _rtft->fillRect(0, 40, TFT_W, 160, C_BG);

    face_dirty = false;
    face_last = face_current;

    if (face_current < FACE_COUNT) {
      face_fns[face_current](frame);
    }

    // Draw particles over face
    if (has_particles) {
      for (int i = 0; i < MAX_PARTICLES; i++) {
        if (active_particles[i].type != PT_NONE && active_particles[i].life > 0) {
          active_particles[i].life -= 0.05f; // decay

          if (active_particles[i].type == PT_MUSIC) {
            active_particles[i].y -= 2.0f; // float up
            draw_music_note((int)active_particles[i].x, (int)active_particles[i].y, active_particles[i].color);
          } else if (active_particles[i].type == PT_SWEAT) {
            active_particles[i].y += 3.0f; // slide down
            draw_sweat((int)active_particles[i].x, (int)active_particles[i].y, active_particles[i].color);
          } else if (active_particles[i].type == PT_HEART) {
            active_particles[i].y -= 2.0f; // float up
            draw_heart((int)active_particles[i].x, (int)active_particles[i].y, 14, active_particles[i].color);
          }

          if (active_particles[i].life <= 0) {
            active_particles[i].type = PT_NONE;
          }
        }
      }
    }

    // Only redraw status bar if it was dirty
    if (anim_changed == false || frame % 15 == 0) {
      draw_status_bar();
    }
  }

  vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
}

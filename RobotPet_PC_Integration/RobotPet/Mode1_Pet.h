// ================================================================
//  Mode1_Pet.h  —  Pixel Art Robot Desk Companion
//  AeroSniffer v2 | XIAO ESP32S3 + ST7789 240×240
//
//  The robot face is drawn on a 24×24 meta-pixel canvas.
//  Each meta-pixel = 4×4 real screen pixels = chunky pixel art.
//  Canvas = 96×96 px, centered on the 240×240 display.
//
//  PC events arrive over USB Serial from pc_agent.py as:
//    FACE:IDLE\n  FACE:TYPING\n  FACE:CODING\n  etc.
//
//  To use your own Piskel art:
//    1. Draw 96×96 sprite in Piskel
//    2. Export → C file (uint16_t, RGB565)
//    3. Replace the draw_face_*() functions with pushImage() calls
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include "Config.h"

static TFT_eSPI* _rtft = nullptr;

// ================================================================
//  COLOUR PALETTE  (RGB565)
// ================================================================
#define C_BG        0x0000   // Background black
#define C_BODY      0x2104   // Robot body — dark charcoal
#define C_BODY_HL   0x31A6   // Body highlight edge
#define C_PLATE     0x1082   // Face plate — slightly lighter
#define C_BOLT      0xC618   // Silver ear bolts
#define C_ANTENNA   0x07FF   // Antenna tip — cyan
#define C_EYE_CYAN  0x07FF   // Eye: calm / idle
#define C_EYE_GRN   0x07E0   // Eye: happy
#define C_EYE_RED   0xF800   // Eye: angry / panic
#define C_EYE_YLW   0xFFE0   // Eye: thinking / surprised
#define C_EYE_PNK   0xF81F   // Eye: excited / notification
#define C_EYE_BLU   0x001F   // Eye: sad
#define C_GLARE     0xFFFF   // White glare dot on eye
#define C_BLUSH     0xF810   // Cheek blush — pink
#define C_MOUTH_ON  0x07FF   // Mouth dot: lit
#define C_MOUTH_OFF 0x2104   // Mouth dot: unlit (off)
#define C_STATUS    0x07FF   // Status text colour
#define C_DIM       0x2124   // Dimmed / unused element

// ================================================================
//  META-PIXEL CANVAS SYSTEM
// ================================================================
#define PX     4                              // Meta-pixel size (px)
#define CW     24                             // Canvas width  (meta-px)
#define CH     24                             // Canvas height (meta-px)
#define CVS_X  ((TFT_W  - CW * PX) / 2)     // 72  (on 240-wide display)
#define CVS_Y  ((TFT_H  - CH * PX) / 2 - 8) // 68  (slightly above center)

// Draw one meta-pixel at canvas (cx, cy)
static inline void mpx(int cx, int cy, uint16_t color) {
  _rtft->fillRect(CVS_X + cx * PX, CVS_Y + cy * PX, PX, PX, color);
}
// Fill a meta-rect from (cx1,cy1) to (cx2,cy2) inclusive
static inline void mrect(int cx1, int cy1, int cx2, int cy2, uint16_t color) {
  _rtft->fillRect(CVS_X + cx1*PX, CVS_Y + cy1*PX,
                  (cx2-cx1+1)*PX, (cy2-cy1+1)*PX, color);
}
// Draw a meta-pixel only if it's a "dot" pattern (checker for grille)
static inline void mgrille(int cx, int cy, bool lit) {
  mpx(cx, cy, lit ? C_MOUTH_ON : C_MOUTH_OFF);
}

// ================================================================
//  FACE STATE
// ================================================================
enum RobotFace : uint8_t {
  FACE_IDLE = 0,
  FACE_HAPPY,
  FACE_SAD,
  FACE_ANGRY,
  FACE_SURPRISED,
  FACE_THINKING,
  FACE_TYPING,
  FACE_SLEEPING,
  FACE_CODING,
  FACE_VIBING,
  FACE_PANIC,
  FACE_NOTIFICATION,
  FACE_COUNT
};

static RobotFace face_current    = FACE_IDLE;
static RobotFace face_last       = FACE_COUNT;  // Forces first draw
static RobotFace face_before_idle = FACE_IDLE;  // Used for blink restore
static uint32_t  face_timer      = 0;
static uint32_t  blink_next      = 0;
static uint32_t  last_active_ms  = 0;
static bool      face_dirty      = true;
static char      pc_status[40]   = "";           // Status text from PC
static char      pc_app[32]      = "";           // Active app name

// ── Touch state (same as before) ─────────────────────────────────
static bool     t_was_pressed = false;
static uint32_t t_press_start = 0;
static uint32_t t_last_tap    = 0;
static uint8_t  t_tap_count   = 0;

// ── Vibing animation ──────────────────────────────────────────────
static int  vibe_y   = 0;
static int  vibe_dir = 1;

// ── Blink state ───────────────────────────────────────────────────
static bool is_blinking = false;

// ================================================================
//  ROBOT BODY  (static base — same for all expressions)
// ================================================================
static void draw_robot_body() {
  // Clear canvas area
  _rtft->fillRect(CVS_X, CVS_Y, CW*PX, CH*PX, C_BG);

  // ── Antenna ────────────────────────────────────────────────────
  // Stem
  mrect(11, 0, 12, 2, C_BODY);
  // Tip glow
  mpx(11, 0, C_ANTENNA);
  mpx(12, 0, C_ANTENNA);

  // ── Outer head shape (pixel art rounded rect) ─────────────────
  // Top edge
  mrect(3, 3, 20, 3, C_BODY);
  // Left/right edges with stepped corners
  for (int y = 4; y <= 20; y++) {
    mpx(2, y, C_BODY);
    mpx(21, y, C_BODY);
  }
  mrect(3, 21, 20, 21, C_BODY);
  // Corner rounding
  mrect(3, 4, 20, 20, C_BODY);
  // Top corners (stepped)
  mrect(4, 3, 19, 3, C_BODY);
  mrect(3, 4, 20, 4, C_BODY);

  // ── Body highlight (top-left edge glow) ────────────────────────
  for (int y = 4; y <= 10; y++) mpx(3, y, C_BODY_HL);
  for (int x = 4; x <= 10; x++) mpx(x, 4, C_BODY_HL);

  // ── Ear bolts ─────────────────────────────────────────────────
  mrect(1, 9,  1, 10, C_BOLT);   // Left bolt
  mrect(22, 9, 22, 10, C_BOLT);  // Right bolt
  mpx(1, 9,  0x8C71); mpx(1, 10, 0x8C71);  // Bolt highlight
  mpx(22, 9, 0x8C71); mpx(22, 10, 0x8C71);

  // ── Face plate ────────────────────────────────────────────────
  mrect(4, 6, 19, 19, C_PLATE);
  // Plate inner border (subtle inset)
  mrect(5, 7, 18, 7, C_BODY_HL);   // Top border
  mrect(5, 7, 5, 18, C_BODY_HL);   // Left border

  // ── Mouth grille (default: alternating dots pattern) ─────────
  // Row 16: speaker grille holes — static base
  mgrille(7,  16, true);  mgrille(8,  16, false); mgrille(9,  16, true);
  mgrille(10, 16, false); mgrille(11, 16, true);  mgrille(12, 16, false);
  mgrille(13, 16, true);  mgrille(14, 16, false); mgrille(15, 16, true);
  mgrille(16, 16, false);

  // ── Eye sockets (dark recess behind eyes) ────────────────────
  mrect(6, 8, 10, 13, C_BODY);   // Left eye socket
  mrect(13, 8, 17, 13, C_BODY);  // Right eye socket
}

// ================================================================
//  EYE DRAWING FUNCTIONS
// ================================================================

// Draw a simple rectangular "screen" eye
// col=left edge, row=top edge, w=width, h=height, in meta-pixels
static void draw_eye(int col, int row, int w, int h, uint16_t color) {
  mrect(col, row, col+w-1, row+h-1, color);
  // Glare dot (top-left of eye)
  mpx(col + 1, row + 1, C_GLARE);
}

// Curved-up eye (happy): fill only top arc portion
static void draw_eye_happy(int col, int row, int w, uint16_t color) {
  // Full width top half = happy curve
  mrect(col, row, col+w-1, row+1, color);
  // Narrower second row (curved feel)
  mrect(col+1, row+2, col+w-2, row+2, color);
  mpx(col+1, row, C_GLARE);
}

// Curved-down eye (sad): fill only bottom
static void draw_eye_sad(int col, int row, int w, uint16_t color) {
  mrect(col+1, row+2, col+w-2, row+2, color);
  mrect(col, row+3, col+w-1, row+4, color);
  mpx(col+1, row+3, C_GLARE);
}

// Squinted eye (angry/determined): thin horizontal slit
static void draw_eye_squint(int col, int row, int w, bool angle_left, uint16_t color) {
  if (angle_left) {
    // Angled: higher on left, lower on right
    mrect(col, row+1, col+2, row+1, color);
    mrect(col+2, row+2, col+w-1, row+2, color);
  } else {
    mrect(col, row+2, col+2, row+2, color);
    mrect(col+2, row+1, col+w-1, row+1, color);
  }
}

// Big round eye (surprised): filled circle-like square
static void draw_eye_wide(int col, int row, uint16_t color) {
  mrect(col, row, col+4, row+4, color);
  // Pupil (smaller dark square inside)
  mrect(col+1, row+1, col+3, row+3, C_BG);
  mpx(col+2, row+2, color); // Pupil center
  mpx(col+1, row+1, C_GLARE); // Glare
}

// Closed eye (sleeping): thin line
static void draw_eye_closed(int col, int row, int w, uint16_t color) {
  mrect(col, row+2, col+w-1, row+2, color);
}

// X eye (panic/dead)
static void draw_eye_X(int col, int row, uint16_t color) {
  mpx(col,   row,   color); mpx(col+4, row,   color);
  mpx(col+1, row+1, color); mpx(col+3, row+1, color);
  mpx(col+2, row+2, color);
  mpx(col+1, row+3, color); mpx(col+3, row+3, color);
  mpx(col,   row+4, color); mpx(col+4, row+4, color);
}

// Code/screen eye (coding): small rectangle with lines inside
static void draw_eye_screen(int col, int row, int w, uint16_t color) {
  mrect(col, row, col+w-1, row+4, color);
  // Inner "code lines" in dark
  mrect(col+1, row+1, col+3, row+1, C_BODY);
  mrect(col+1, row+2, col+w-2, row+2, C_BODY);
  mrect(col+2, row+3, col+w-2, row+3, C_BODY);
  mpx(col+1, row+1, C_GLARE);
}

// Wavy eye (vibing) — shifts with animation frame
static void draw_eye_wave(int col, int row, int w, int offset, uint16_t color) {
  for (int x = 0; x < w; x++) {
    int wave = (int)(sinf((float)(x + offset) * 0.8f) * 1.5f);
    mpx(col + x, row + 2 + wave, color);
    mpx(col + x, row + 3 + wave, color);
  }
}

// ================================================================
//  MOUTH DRAWING FUNCTIONS
// ================================================================
static void draw_mouth_smile() {
  // Happy mouth: upward curve dots
  mpx(7, 17, C_MOUTH_ON); mpx(9, 18, C_MOUTH_ON);
  mpx(11, 18, C_MOUTH_ON); mpx(12, 18, C_MOUTH_ON);
  mpx(14, 18, C_MOUTH_ON); mpx(16, 17, C_MOUTH_ON);
}
static void draw_mouth_frown() {
  mpx(7, 18, C_MOUTH_ON); mpx(9, 17, C_MOUTH_ON);
  mpx(11, 17, C_MOUTH_ON); mpx(12, 17, C_MOUTH_ON);
  mpx(14, 17, C_MOUTH_ON); mpx(16, 18, C_MOUTH_ON);
}
static void draw_mouth_flat() {
  mrect(8, 17, 15, 17, C_MOUTH_ON);
}
static void draw_mouth_open() {
  // O-shape mouth
  mrect(9, 17, 14, 17, C_MOUTH_ON);
  mrect(9, 19, 14, 19, C_MOUTH_ON);
  mpx(8, 18, C_MOUTH_ON); mpx(15, 18, C_MOUTH_ON);
  mrect(9, 18, 14, 18, C_BODY);  // Inside of O
}
static void draw_mouth_zigzag() {
  mpx(8, 18, C_MOUTH_ON); mpx(9, 17, C_MOUTH_ON);
  mpx(10, 18, C_MOUTH_ON); mpx(11, 17, C_MOUTH_ON);
  mpx(12, 18, C_MOUTH_ON); mpx(13, 17, C_MOUTH_ON);
  mpx(14, 18, C_MOUTH_ON); mpx(15, 17, C_MOUTH_ON);
}
static void draw_mouth_ellipsis(int frame) {
  // Thinking dots, animated
  mrect(8, 17, 15, 17, C_BODY);
  for (int i = 0; i < 3; i++) {
    if (i <= frame % 4)
      mpx(9 + i*3, 17, C_MOUTH_ON);
  }
}
static void draw_mouth_zzz(int frame) {
  // Sleeping — just the grille off
  mrect(7, 16, 16, 18, C_BODY);
  // Gentle zzz text using meta-pixels (z pixel art)
  static const uint8_t Z[3][5][3] = {
    {{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1}},
    {{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1}},
    {{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1}},
  };
  uint16_t zcol = (frame % 20 < 10) ? C_EYE_CYAN : C_DIM;
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 3; c++) {
      if (Z[0][r][c]) mpx(8+c, 15+r, zcol);
    }
  }
}

// ================================================================
//  EYEBROW DRAWING
// ================================================================
static void draw_brows_neutral() {
  mrect(6, 7, 10, 7, C_BODY_HL);
  mrect(13, 7, 17, 7, C_BODY_HL);
}
static void draw_brows_angry() {
  // Left brow angled down-left
  mpx(6, 7, C_BODY_HL); mpx(7, 7, C_BODY_HL);
  mpx(8, 8, C_BODY_HL); mpx(9, 8, C_BODY_HL); mpx(10, 8, C_BODY_HL);
  // Right brow angled down-right
  mpx(13, 8, C_BODY_HL); mpx(14, 8, C_BODY_HL);
  mpx(15, 7, C_BODY_HL); mpx(16, 7, C_BODY_HL); mpx(17, 7, C_BODY_HL);
}
static void draw_brows_raised() {
  mrect(6, 6, 10, 6, C_BODY_HL);
  mrect(13, 6, 17, 6, C_BODY_HL);
}
static void draw_brows_thinking() {
  // One brow raised, one furrowed
  mrect(6, 7, 10, 7, C_BODY_HL);  // Left normal
  mrect(13, 6, 17, 6, C_BODY_HL); // Right raised
}

// ================================================================
//  STATUS BAR (below the robot)
// ================================================================
static void draw_status_bar() {
  int barY = CVS_Y + CH*PX + 8;
  _rtft->fillRect(0, barY, TFT_W, 24, C_BG);

  if (strlen(pc_status) > 0) {
    _rtft->setTextColor(C_STATUS);
    _rtft->setTextSize(1);
    _rtft->setCursor((TFT_W - strlen(pc_status)*6) / 2, barY + 6);
    _rtft->print(pc_status);
  }

  if (strlen(pc_app) > 0) {
    _rtft->setTextColor(C_DIM);
    _rtft->setTextSize(1);
    _rtft->setCursor(4, barY + 16);
    _rtft->printf("[%s]", pc_app);
  }
}

// ================================================================
//  NOTIFICATION INDICATOR  (! above head)
// ================================================================
static void draw_notif_indicator(bool visible, uint16_t color) {
  int nx = CVS_X + 18*PX, ny = CVS_Y + 0;
  if (visible) {
    _rtft->fillRect(nx, ny, PX*2, PX*4, color);         // ! stem
    _rtft->fillRect(nx, ny + PX*5, PX*2, PX*2, color);  // ! dot
  } else {
    _rtft->fillRect(nx - 4, ny - 4, PX*2 + 8, PX*8, C_BG);
  }
}

// ================================================================
//  COMPLETE FACE RENDERERS
// ================================================================

static void draw_face_idle(int frame) {
  draw_robot_body();
  draw_brows_neutral();
  draw_eye(6, 9, 5, 3, C_EYE_CYAN);    // L eye: 5×3 rectangle
  draw_eye(13, 9, 5, 3, C_EYE_CYAN);   // R eye
  draw_mouth_flat();
  snprintf(pc_status, sizeof(pc_status), "");
}

static void draw_face_happy(int frame) {
  draw_robot_body();
  draw_brows_raised();
  draw_eye_happy(6, 9, 5, C_EYE_GRN);
  draw_eye_happy(13, 9, 5, C_EYE_GRN);
  draw_mouth_smile();
  // Blush cheeks
  mrect(3, 14, 4, 15, C_BLUSH);
  mrect(19, 14, 20, 15, C_BLUSH);
}

static void draw_face_sad(int frame) {
  draw_robot_body();
  draw_brows_thinking();
  draw_eye_sad(6, 9, 5, C_EYE_BLU);
  draw_eye_sad(13, 9, 5, C_EYE_BLU);
  draw_mouth_frown();
  // Tear pixels (animated drip)
  if ((frame / 8) % 3 == 0) mpx(9, 14, 0x03FF);  // L tear
  if ((frame / 8) % 3 == 1) mpx(16, 14, 0x03FF); // R tear
}

static void draw_face_angry(int frame) {
  draw_robot_body();
  draw_brows_angry();
  draw_eye_squint(6, 9, 5, true, C_EYE_RED);
  draw_eye_squint(13, 9, 5, false, C_EYE_RED);
  draw_mouth_zigzag();
  // Forehead crease
  mpx(11, 7, C_EYE_RED); mpx(12, 7, C_EYE_RED);
}

static void draw_face_surprised(int frame) {
  draw_robot_body();
  draw_brows_raised();
  draw_eye_wide(6, 8, C_EYE_YLW);
  draw_eye_wide(13, 8, C_EYE_YLW);
  draw_mouth_open();
  // Sparks / stars
  if (frame % 4 < 2) {
    mpx(3, 5, C_EYE_YLW); mpx(20, 5, C_EYE_YLW);
    mpx(2, 7, C_EYE_YLW); mpx(21, 7, C_EYE_YLW);
  }
}

static void draw_face_thinking(int frame) {
  draw_robot_body();
  draw_brows_thinking();
  // Left eye: squinting
  draw_eye_squint(6, 9, 5, false, C_EYE_YLW);
  // Right eye: normal
  draw_eye(13, 9, 5, 3, C_EYE_YLW);
  // Animated ... dots
  draw_mouth_ellipsis(frame);
  // Thought bubble (top-right)
  int bx = CVS_X + 20*PX, by = CVS_Y + 1*PX;
  _rtft->fillCircle(bx, by, 4, C_EYE_YLW);
  _rtft->fillCircle(bx - 6, by + 8, 3, C_EYE_YLW);
  _rtft->fillCircle(bx - 10, by + 14, 2, C_EYE_YLW);
}

static void draw_face_typing(int frame) {
  draw_robot_body();
  draw_brows_neutral();
  // Focused/determined eyes — forward squint
  draw_eye_squint(6, 9, 5, false, C_EYE_CYAN);
  draw_eye_squint(13, 9, 5, true, C_EYE_CYAN);
  // Flat focused mouth
  mrect(8, 17, 15, 17, C_EYE_CYAN);
  // Animated cursor blink below status
  if (frame % 16 < 8) {
    int cx = CVS_X + 11*PX, cy = CVS_Y + CH*PX + 8;
    _rtft->fillRect(cx, cy, 2, 10, C_EYE_CYAN);
  }
}

static void draw_face_sleeping(int frame) {
  draw_robot_body();
  // Override face plate darker
  mrect(4, 6, 19, 19, 0x0841);
  draw_eye_closed(6, 11, 5, C_BODY_HL);
  draw_eye_closed(13, 11, 5, C_BODY_HL);
  draw_mouth_zzz(frame);
}

static void draw_face_coding(int frame) {
  draw_robot_body();
  draw_brows_neutral();
  draw_eye_screen(6, 8, 5, C_EYE_GRN);
  draw_eye_screen(13, 8, 5, C_EYE_GRN);
  // Flat serious mouth
  mrect(8, 17, 15, 17, C_EYE_GRN);
  // Glasses bridge
  mpx(11, 11, C_BODY_HL); mpx(12, 11, C_BODY_HL);
  snprintf(pc_status, sizeof(pc_status), "{ CODING... }");
}

static void draw_face_vibing(int frame) {
  draw_robot_body();
  draw_brows_raised();
  // Wavy eyes
  mrect(6, 8, 10, 13, C_BODY);  mrect(13, 8, 17, 13, C_BODY); // Clear sockets
  draw_eye_wave(6, 8, 5, frame, C_EYE_PNK);
  draw_eye_wave(13, 8, 5, frame + 3, C_EYE_PNK);
  draw_mouth_smile();
  // Music note (top right)
  int nx = CVS_X + 18*PX, ny = CVS_Y + 2*PX;
  if (frame % 6 < 3) {
    _rtft->fillCircle(nx, ny + 8, 4, C_EYE_PNK);
    _rtft->drawFastVLine(nx + 3, ny, 8, C_EYE_PNK);
    _rtft->drawFastHLine(nx + 3, ny, 5, C_EYE_PNK);
  }
  // Bob offset for vibing
  int bob = (int)(sinf(frame * 0.3f) * 4);
  if (bob != 0) {
    _rtft->fillRect(CVS_X, CVS_Y + CH*PX + 2, CW*PX, abs(bob), C_BG);
  }
}

static void draw_face_panic(int frame) {
  draw_robot_body();
  draw_brows_angry();
  draw_eye_X(6, 8, C_EYE_RED);
  draw_eye_X(13, 8, C_EYE_RED);
  draw_mouth_zigzag();
  // Flashing ! indicators
  if (frame % 6 < 3) {
    mpx(1, 4, C_EYE_RED); mpx(1, 5, C_EYE_RED); mpx(1, 7, C_EYE_RED);
    mpx(22, 4, C_EYE_RED); mpx(22, 5, C_EYE_RED); mpx(22, 7, C_EYE_RED);
  }
  // Screen edge flash
  if (frame % 4 < 2) {
    _rtft->drawRect(0, 0, TFT_W, TFT_H, C_EYE_RED);
  }
}

static void draw_face_notification(int frame) {
  draw_robot_body();
  draw_brows_raised();
  // Blinking eyes
  if (frame % 8 < 6) {
    draw_eye(6, 9, 5, 3, C_EYE_PNK);
    draw_eye(13, 9, 5, 3, C_EYE_PNK);
  } else {
    draw_eye_closed(6, 11, 5, C_EYE_PNK);
    draw_eye_closed(13, 11, 5, C_EYE_PNK);
  }
  draw_mouth_smile();
  // ! indicator (pulsing)
  draw_notif_indicator(frame % 8 < 6, C_EYE_PNK);
}

// ================================================================
//  FACE DISPATCH  (calls the correct draw function)
// ================================================================
typedef void (*FaceDrawFn)(int);
static FaceDrawFn face_fns[FACE_COUNT] = {
  draw_face_idle,
  draw_face_happy,
  draw_face_sad,
  draw_face_angry,
  draw_face_surprised,
  draw_face_thinking,
  draw_face_typing,
  draw_face_sleeping,
  draw_face_coding,
  draw_face_vibing,
  draw_face_panic,
  draw_face_notification,
};

// ================================================================
//  SERIAL COMMAND PARSER  (called from Core 1)
// ================================================================
static void pet_parse_serial() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("FACE:")) {
      String faceStr = line.substring(5);
      const char* names[] = {
        "IDLE","HAPPY","SAD","ANGRY","SURPRISED",
        "THINKING","TYPING","SLEEPING","CODING",
        "VIBING","PANIC","NOTIFICATION"
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
      strncpy(pc_status, line.substring(7).c_str(), sizeof(pc_status)-1);
      face_dirty = true;

    } else if (line.startsWith("APP:")) {
      strncpy(pc_app, line.substring(4).c_str(), sizeof(pc_app)-1);
      face_dirty = true;

    } else if (line.startsWith("PING")) {
      Serial.println("{\"pong\":1}");
    }
  }
}

// ================================================================
//  TOUCH PROCESSING
// ================================================================
static bool pet_check_short_tap() {
  bool pressed = (digitalRead(TOUCH_PIN) == LOW);
  uint32_t now = millis();
  bool tapped  = false;

  if (pressed && !t_was_pressed) {
    t_press_start = now;
    t_was_pressed = true;
  }
  if (!pressed && t_was_pressed) {
    uint32_t held = now - t_press_start;
    t_was_pressed = false;
    if (held > TOUCH_DEBOUNCE_MS && held < TOUCH_LONG_MS) {
      if (now - t_last_tap < 400) t_tap_count++;
      else t_tap_count = 1;
      t_last_tap = now;
      tapped = true;
    }
  }
  return tapped;
}

bool pet_is_long_pressed() {
  return t_was_pressed && (millis() - t_press_start >= TOUCH_LONG_MS);
}

// ================================================================
//  PUBLIC API
// ================================================================
void pet_setup(TFT_eSPI* tft) {
  _rtft        = tft;
  face_current = FACE_IDLE;
  face_last    = FACE_COUNT;
  face_dirty   = true;
  blink_next   = millis() + random(4000, 8000);
  last_active_ms = millis();
  vibe_y = 0; vibe_dir = 1;
  t_was_pressed = false; t_tap_count = 0;
  memset(pc_status, 0, sizeof(pc_status));
  memset(pc_app,    0, sizeof(pc_app));

  _rtft->fillScreen(TFT_BLACK);
  // Mode header
  _rtft->setTextColor(0x07FF);
  _rtft->setTextSize(1);
  _rtft->setCursor(2, 2);
  _rtft->print("MODE 1: ROBOT PET");
  _rtft->setTextColor(0x4208);
  _rtft->setCursor(148, 2);
  _rtft->print("long=next mode");

  Serial.begin(SERIAL_BAUD);
  Serial.println("{\"ready\":1,\"mode\":\"pet\"}");
}

void pet_teardown() {
  _rtft = nullptr;
}

// Core 0 — idle (no heavy processing in this mode)
void pet_core0_task() {
  vTaskDelay(pdMS_TO_TICKS(50));
}

// Core 1 — animation, touch, serial
void pet_core1_task() {
  if (!_rtft) return;
  static int frame = 0;
  uint32_t now = millis();
  frame++;

  // ── Parse incoming serial commands from PC agent ───────────────
  pet_parse_serial();

  // ── Touch → override face from PC ─────────────────────────────
  bool tapped = pet_check_short_tap();
  if (tapped) {
    last_active_ms = now;
    face_current   = (t_tap_count >= 2) ? FACE_SURPRISED : FACE_HAPPY;
    face_dirty     = true;
    snprintf(pc_status, sizeof(pc_status), "Tap!");
  }

  // ── Auto-sleep when PC is idle (30s no PC events + no touch) ──
  if (face_current != FACE_SLEEPING &&
      now - last_active_ms > 30000) {
    face_before_idle = face_current;
    face_current     = FACE_SLEEPING;
    face_dirty       = true;
  }

  // ── Scheduled blink (IDLE only) ────────────────────────────────
  if (face_current == FACE_IDLE && now > blink_next && !is_blinking) {
    is_blinking  = true;
    face_before_idle = FACE_IDLE;
    // Draw blink
    draw_eye_closed(6, 11, 5, C_EYE_CYAN);
    draw_eye_closed(13, 11, 5, C_EYE_CYAN);
    vTaskDelay(pdMS_TO_TICKS(120));
    face_dirty   = true;
    blink_next   = millis() + random(3000, 7000);
    is_blinking  = false;
  }

  // ── Vibing y-offset animation ──────────────────────────────────
  if (face_current == FACE_VIBING) {
    vibe_y += vibe_dir;
    if (vibe_y >= 6)  vibe_dir = -1;
    if (vibe_y <= -6) vibe_dir =  1;
  } else {
    vibe_y = 0;
  }

  // ── Redraw if needed ──────────────────────────────────────────
  bool needs_anim = (face_current == FACE_THINKING   ||
                     face_current == FACE_TYPING      ||
                     face_current == FACE_SLEEPING    ||
                     face_current == FACE_VIBING      ||
                     face_current == FACE_PANIC       ||
                     face_current == FACE_SURPRISED   ||
                     face_current == FACE_NOTIFICATION);

  if (face_dirty || face_current != face_last || (needs_anim && frame % 4 == 0)) {
    face_dirty  = false;
    face_last   = face_current;

    // Apply vibing y offset by shifting canvas
    int savedY = CVS_Y;
    // (CVS_Y is const, so just pass offset into draw — handled via vibe_y)
    if (face_current < FACE_COUNT) {
      face_fns[face_current](frame);
    }
    draw_status_bar();
  }

  vTaskDelay(pdMS_TO_TICKS(33));   // ~30 fps
}

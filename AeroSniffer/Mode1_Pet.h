// ================================================================
//  Mode1_Pet.h  —  The Interactive Desk Companion
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  Features:
//    • Animated OLED-style eye expressions on TFT
//    • FSR-402 haptic input  → gentle pat / harsh press reactions
//    • I2S mic + arduinoFFT  → music-reactive bobbing animation
//    • MPU-6050 shake detect → startle / surprised reaction
//    • MAX98357A I2S DAC     → purr and alert sound synthesis
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>
#include <Wire.h>
#include "Config.h"

// ── Internal display pointer ─────────────────────────────────────
static TFT_eSPI* _ptft = nullptr;

// ── Emotion State Machine ────────────────────────────────────────
enum PetEmotion : uint8_t {
  EMO_IDLE = 0,
  EMO_HAPPY,
  EMO_ANGRY,
  EMO_SAD,
  EMO_SURPRISED,
  EMO_BLINK,
  EMO_SLEEPING,
  EMO_BOBBING
};

static PetEmotion emo_current   = EMO_IDLE;
static PetEmotion emo_last      = EMO_IDLE;
static bool       emo_dirty     = true;    // Redraw needed?
static uint32_t   emo_timer     = 0;       // When emotion was set
static uint32_t   blink_next    = 0;       // Next scheduled blink

// ── Bobbing animation state ──────────────────────────────────────
static int   bob_offset  = 0;
static int   bob_dir     = 1;    // +1 up / -1 down
static float bass_energy = 0.0f;

// ── FFT buffers (static — Core 0 owns these) ────────────────────
static double fft_real[FFT_SAMPLES];
static double fft_imag[FFT_SAMPLES];
static ArduinoFFT<double> PetFFT(fft_real, fft_imag, FFT_SAMPLES, FFT_SAMPLE_RATE);

// ================================================================
//  SOUND SYNTHESIS
// ================================================================

static void pet_play_purr() {
  // Generates a gentle 80–120 Hz warbling purr tone
  const int RATE   = 8000;
  const int FRAMES = 512;
  int16_t buf[FRAMES];
  float   freq = 90.0f;
  for (int rep = 0; rep < 12; rep++) {
    for (int i = 0; i < FRAMES; i++) {
      buf[i] = (int16_t)(9000.0f * sinf(2.0f * M_PI * freq * i / RATE));
      freq   = 90.0f + 30.0f * sinf(2.0f * M_PI * 4.0f * (rep * FRAMES + i) / (RATE));
    }
    size_t written = 0;
    i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(100));
  }
}

static void pet_play_alert() {
  // Alternating 660 / 880 Hz two-tone alert
  const int RATE   = 8000;
  const int FRAMES = 512;
  int16_t buf[FRAMES];
  float freqs[2] = {660.0f, 880.0f};
  for (int rep = 0; rep < 8; rep++) {
    float f = freqs[rep % 2];
    for (int i = 0; i < FRAMES; i++) {
      buf[i] = (int16_t)(22000.0f * sinf(2.0f * M_PI * f * i / RATE));
    }
    size_t written = 0;
    i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(100));
  }
}

static void pet_play_surprised() {
  // Rising glide tone  220 → 880 Hz
  const int RATE   = 8000;
  const int FRAMES = 1024;
  int16_t buf[FRAMES];
  for (int i = 0; i < FRAMES; i++) {
    float t    = (float)i / FRAMES;
    float freq = 220.0f + 660.0f * t;
    buf[i]     = (int16_t)(16000.0f * sinf(2.0f * M_PI * freq * i / RATE));
  }
  size_t written = 0;
  i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, pdMS_TO_TICKS(200));
}

// ================================================================
//  DRAWING HELPERS
// ================================================================

// Face geometry constants
#define FACE_CX   (TFT_W / 2)
#define FACE_CY   (TFT_H / 2 + 8)
#define FACE_R    78
#define EYE_LX    (FACE_CX - 26)
#define EYE_RX    (FACE_CX + 26)
#define EYE_BASE_Y (FACE_CY - 12)
#define EYE_R     18

// Draw both eyes using the active emotion, shifted by bob_offset
static void _draw_eyes(PetEmotion emo, int yoff) {
  int lx = EYE_LX,   rx = EYE_RX;
  int ey = EYE_BASE_Y + yoff;

  switch (emo) {

    // ── IDLE / HAPPY ──────────────────────────────────────────
    case EMO_IDLE:
    case EMO_HAPPY:
    case EMO_BOBBING:
      // White sclera
      _ptft->fillEllipse(lx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillEllipse(rx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      // Erase bottom half → curved happy eyelid
      _ptft->fillRect(lx - EYE_R, ey + 2, EYE_R * 2, EYE_R, TFT_BLACK);
      _ptft->fillRect(rx - EYE_R, ey + 2, EYE_R * 2, EYE_R, TFT_BLACK);
      // Pupils
      _ptft->fillCircle(lx, ey - 2, EYE_R - 6, 0x1967);  // Dark teal pupils
      _ptft->fillCircle(rx, ey - 2, EYE_R - 6, 0x1967);
      // Glare dots
      _ptft->fillCircle(lx - 5, ey - 8, 4, TFT_WHITE);
      _ptft->fillCircle(rx - 5, ey - 8, 4, TFT_WHITE);
      // Blush marks
      _ptft->fillCircle(lx - 24, ey + 18, 10, 0xF810);
      _ptft->fillCircle(rx + 24, ey + 18, 10, 0xF810);
      // Smile arc
      _ptft->drawArc(FACE_CX, FACE_CY + 28 + yoff, 34, 27, 210, 330, TFT_WHITE, TFT_BLACK);
      break;

    // ── ANGRY ─────────────────────────────────────────────────
    case EMO_ANGRY:
      // Red triangular eyes
      _ptft->fillTriangle(lx - EYE_R,  ey - 4,
                          lx + EYE_R,  ey - EYE_R,
                          lx + EYE_R,  ey + EYE_R - 4, TFT_RED);
      _ptft->fillTriangle(rx - EYE_R,  ey - EYE_R,
                          rx + EYE_R,  ey - 4,
                          rx - EYE_R,  ey + EYE_R - 4, TFT_RED);
      // Angry brow lines
      _ptft->drawWideLine(lx - EYE_R, ey - EYE_R - 6, lx + EYE_R, ey - 6, 3, TFT_RED, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey - 6, rx + EYE_R, ey - EYE_R - 6, 3, TFT_RED, TFT_BLACK);
      // Frown
      _ptft->drawArc(FACE_CX, FACE_CY + 44 + yoff, 28, 21, 30, 150, TFT_WHITE, TFT_BLACK);
      break;

    // ── SAD ───────────────────────────────────────────────────
    case EMO_SAD:
      // Half-open droopy eyes
      _ptft->fillEllipse(lx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillEllipse(rx, ey, EYE_R, EYE_R - 2, TFT_WHITE);
      _ptft->fillRect(lx - EYE_R, ey - EYE_R, EYE_R * 2, EYE_R / 2, TFT_BLACK); // top erase
      _ptft->fillCircle(lx, ey + 4, EYE_R - 8, 0x001F);  // Blue pupils
      _ptft->fillCircle(rx, ey + 4, EYE_R - 8, 0x001F);
      // Tear drops
      _ptft->fillEllipse(lx + 8, ey + EYE_R + 6, 4, 8, 0x03FF);
      _ptft->fillEllipse(rx - 8, ey + EYE_R + 6, 4, 8, 0x03FF);
      // Frown
      _ptft->drawArc(FACE_CX, FACE_CY + 44 + yoff, 28, 21, 30, 150, TFT_WHITE, TFT_BLACK);
      break;

    // ── SURPRISED ─────────────────────────────────────────────
    case EMO_SURPRISED:
      // Big wide circular eyes
      _ptft->fillCircle(lx, ey, EYE_R + 4, TFT_WHITE);
      _ptft->fillCircle(rx, ey, EYE_R + 4, TFT_WHITE);
      _ptft->fillCircle(lx, ey, EYE_R - 4, 0x1967);
      _ptft->fillCircle(rx, ey, EYE_R - 4, 0x1967);
      _ptft->fillCircle(lx - 4, ey - 4, 5, TFT_WHITE);  // Glare
      _ptft->fillCircle(rx - 4, ey - 4, 5, TFT_WHITE);
      // Open "O" mouth
      _ptft->fillCircle(FACE_CX, FACE_CY + 32 + yoff, 14, TFT_WHITE);
      _ptft->fillCircle(FACE_CX, FACE_CY + 32 + yoff, 8,  TFT_BLACK);
      break;

    // ── BLINK ─────────────────────────────────────────────────
    case EMO_BLINK:
      // Thin closed lines
      _ptft->drawWideLine(lx - EYE_R, ey, lx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey, rx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      // Smile
      _ptft->drawArc(FACE_CX, FACE_CY + 28 + yoff, 34, 27, 210, 330, TFT_WHITE, TFT_BLACK);
      break;

    // ── SLEEPING ──────────────────────────────────────────────
    case EMO_SLEEPING:
      // Closed curved lines (zz eyelids)
      _ptft->drawWideLine(lx - EYE_R, ey, lx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      _ptft->drawWideLine(rx - EYE_R, ey, rx + EYE_R, ey, 3, TFT_WHITE, TFT_BLACK);
      // Small curved frown / neutral
      _ptft->drawWideLine(FACE_CX - 12, FACE_CY + 28 + yoff,
                          FACE_CX + 12, FACE_CY + 28 + yoff, 2, TFT_WHITE, TFT_BLACK);
      // "Zzz"
      _ptft->setTextColor(TFT_CYAN);
      _ptft->setTextSize(1);
      _ptft->setCursor(FACE_CX + 52, FACE_CY - 40 + yoff); _ptft->print("z");
      _ptft->setTextSize(2);
      _ptft->setCursor(FACE_CX + 62, FACE_CY - 58 + yoff); _ptft->print("Z");
      _ptft->setTextSize(3);
      _ptft->setCursor(FACE_CX + 76, FACE_CY - 82 + yoff); _ptft->print("Z");
      break;

    default: break;
  }
}

// Render the full face: background, base circle, then eyes
static void pet_draw_face(PetEmotion emo, int yoff) {
  if (!_ptft) return;

  int cy = FACE_CY + yoff;

  // Clear previous face bounding box
  _ptft->fillRect(FACE_CX - FACE_R - 12, FACE_CY - FACE_R - 15,
                  (FACE_R + 12) * 2, (FACE_R + 15) * 2, TFT_BLACK);

  // Face base — dark slate with soft rim
  _ptft->fillCircle(FACE_CX, cy, FACE_R, 0x2124);
  _ptft->drawCircle(FACE_CX, cy, FACE_R, 0x6B4D);
  _ptft->drawCircle(FACE_CX, cy, FACE_R - 1, 0x4208);

  // Music notes banner during bobbing
  if (emo == EMO_BOBBING) {
    _ptft->setTextColor(0x07FF);
    _ptft->setTextSize(2);
    _ptft->setCursor(8, 16);
    _ptft->print("♪  AeroSniffer  ♫");
  }

  _draw_eyes(emo, yoff);
}

// ================================================================
//  I2S INITIALISATION
// ================================================================

static void pet_i2s_mic_init() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = FFT_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
}

static void pet_i2s_dac_init() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = 8000,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_DAC_BCLK,
    .ws_io_num    = I2S_DAC_LRC,
    .data_out_num = I2S_DAC_DIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_1, &pins);
}

// ================================================================
//  MPU-6050 SHAKE DETECTION  (raw I2C — no extra library needed)
// ================================================================
static int16_t mpu_ax = 0, mpu_ay = 0, mpu_az = 0;
static int16_t mpu_last_ax = 0, mpu_last_ay = 0;

static void mpu_wake() {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(0x6B);   // PWR_MGMT_1
  Wire.write(0x00);   // Wake up
  Wire.endTransmission();
}

static void mpu_read_accel() {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(0x3B);   // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (uint8_t)6, (uint8_t)true);
  if (Wire.available() >= 6) {
    mpu_ax = (Wire.read() << 8) | Wire.read();
    mpu_ay = (Wire.read() << 8) | Wire.read();
    mpu_az = (Wire.read() << 8) | Wire.read();
  }
}

static bool mpu_shake_detected() {
  int16_t dx = abs(mpu_ax - mpu_last_ax);
  int16_t dy = abs(mpu_ay - mpu_last_ay);
  mpu_last_ax = mpu_ax;
  mpu_last_ay = mpu_ay;
  return (dx > 8000 || dy > 8000);   // ~0.5 g sudden delta
}

// ================================================================
//  PUBLIC API — called from MultiBoot_DeskGadget.ino
// ================================================================

void pet_setup(TFT_eSPI* tft) {
  _ptft          = tft;
  emo_current    = EMO_IDLE;
  emo_last       = EMO_IDLE;
  emo_dirty      = true;
  bob_offset     = 0;
  bob_dir        = 1;
  blink_next     = millis() + random(3000, 7000);
  emo_timer      = millis();

  _ptft->fillScreen(TFT_BLACK);

  // Header bar
  _ptft->setTextColor(0x07FF);
  _ptft->setTextSize(1);
  _ptft->setCursor(2, 2);
  _ptft->print("MODE 1: COMPANION");
  _ptft->setTextColor(0x4208);
  _ptft->setCursor(162, 2);
  _ptft->print("[BOOT]=next mode");

  pet_i2s_mic_init();
  pet_i2s_dac_init();
  mpu_wake();

  pet_draw_face(EMO_IDLE, 0);
}

void pet_teardown() {
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_1);
  _ptft = nullptr;
}

// ── Core 0 — FFT / Audio analysis (background) ─────────────────
void pet_core0_task() {
  // Collect FFT_SAMPLES 32-bit frames from the INMP441
  int32_t raw[FFT_SAMPLES];
  size_t  bytes = 0;
  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes, pdMS_TO_TICKS(40));

  int got = bytes / 4;
  for (int i = 0; i < FFT_SAMPLES; i++) {
    fft_real[i] = (i < got) ? (double)(raw[i] >> 14) : 0.0;
    fft_imag[i] = 0.0;
  }

  PetFFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  PetFFT.compute(FFT_FORWARD);
  PetFFT.complexToMagnitude();

  // Sum energy in bass band
  int bassEndBin = (int)((BASS_HZ_MAX * FFT_SAMPLES) / (double)FFT_SAMPLE_RATE);
  bassEndBin = min(bassEndBin, FFT_SAMPLES / 2);
  double sum = 0;
  for (int i = 2; i < bassEndBin; i++) sum += fft_real[i];
  bass_energy = (float)(sum / max(bassEndBin - 2, 1));
}

// ── Core 1 — UI rendering, FSR, MPU (display thread) ───────────
void pet_core1_task() {
  if (!_ptft) return;

  uint32_t now = millis();

  // ─ MPU-6050 shake ──────────────────────────────────────────
  mpu_read_accel();
  if (mpu_shake_detected() && emo_current == EMO_IDLE) {
    emo_current = EMO_SURPRISED;
    emo_timer   = now;
    emo_dirty   = true;
    // Fire sound in one-shot task so UI thread isn't blocked
    xTaskCreate([](void*) { pet_play_surprised(); vTaskDelete(nullptr); },
                "snd_surp", 2048, nullptr, 1, nullptr);
  }

  // ─ FSR read ────────────────────────────────────────────────
  int fsr = analogRead(FSR_PIN);

  if (fsr >= FSR_HARSH_MIN) {
    if (emo_current != EMO_ANGRY) {
      emo_current = EMO_ANGRY;
      emo_timer   = now;
      emo_dirty   = true;
      xTaskCreate([](void*) { pet_play_alert(); vTaskDelete(nullptr); },
                  "snd_alert", 2048, nullptr, 1, nullptr);
    }
  } else if (fsr >= FSR_GENTLE_MIN && fsr <= FSR_GENTLE_MAX) {
    if (emo_current != EMO_HAPPY) {
      emo_current = EMO_HAPPY;
      emo_timer   = now;
      emo_dirty   = true;
      xTaskCreate([](void*) { pet_play_purr(); vTaskDelete(nullptr); },
                  "snd_purr", 2048, nullptr, 1, nullptr);
    }
  }

  // ─ Bass-reactive bobbing ────────────────────────────────────
  if (bass_energy > BASS_THRESHOLD &&
      emo_current != EMO_ANGRY && emo_current != EMO_SAD) {
    emo_current = EMO_BOBBING;
    emo_timer   = now;
  }

  // ─ Auto-return to IDLE after 3 s ───────────────────────────
  if ((emo_current == EMO_HAPPY   || emo_current == EMO_ANGRY  ||
       emo_current == EMO_BOBBING || emo_current == EMO_SURPRISED) &&
      now - emo_timer > 3000) {
    emo_current = EMO_IDLE;
    emo_dirty   = true;
  }

  // ─ Scheduled blink ─────────────────────────────────────────
  if (emo_current == EMO_IDLE && now > blink_next) {
    emo_current = EMO_BLINK;
    emo_dirty   = true;
    vTaskDelay(pdMS_TO_TICKS(140));
    emo_current = EMO_IDLE;
    blink_next  = millis() + random(3000, 7000);
    emo_dirty   = true;
  }

  // ─ Bobbing animation step ───────────────────────────────────
  if (emo_current == EMO_BOBBING) {
    bob_offset += bob_dir * 3;
    if (bob_offset >= 10)  bob_dir = -1;
    if (bob_offset <= -10) bob_dir =  1;
    emo_dirty = true;
  } else if (bob_offset != 0) {
    bob_offset = 0;
    emo_dirty  = true;
  }

  // ─ Redraw only when state changed ──────────────────────────
  if (emo_dirty || emo_current != emo_last) {
    emo_dirty  = false;
    emo_last   = emo_current;
    pet_draw_face(emo_current, bob_offset);
  }

  vTaskDelay(pdMS_TO_TICKS(30));   // ≈ 33 fps
}

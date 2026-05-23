// ================================================================
//  TFT_eSPI_UserSetup.h
//  AeroSniffer — TFT_eSPI Library Configuration
//
//  INSTALLATION:
//    Copy the content below into:
//    Arduino/libraries/TFT_eSPI/User_Setup.h
//    (overwrite the existing file)
//
//  Then in the same folder, open User_Setup_Select.h and make sure
//  only this line is uncommented:
//      #include <User_Setup.h>
// ================================================================

// ── Display Driver Selection ─────────────────────────────────────
// Uncomment ONE of the following to match your exact display:

#define ILI9341_DRIVER   // 240x320 rectangular — default
// #define ST7789_DRIVER       // 240x240 square (most common "GC9A01 style" round or square)

// ── Display Dimensions ───────────────────────────────────────────
#define TFT_WIDTH   240
#define TFT_HEIGHT  320   // ← Change to 240 if using ST7789

// ── SPI Pin Mapping (must match Config.h) ────────────────────────
#define TFT_MOSI    11    // Data Out
#define TFT_SCLK    12    // Clock
#define TFT_CS      10    // Chip Select
#define TFT_DC      13    // Data / Command
#define TFT_RST     14    // Reset

// Backlight is controlled separately via PWM in firmware, but
// define it here so the library can toggle it during init:
#define TFT_BL      15
#define TFT_BACKLIGHT_ON  HIGH

// ── Font Loading — keep all for max flexibility ──────────────────
#define LOAD_GLCD     // Adafruit GFX built-in 5x7 font
#define LOAD_FONT2    // Small — good for status bars
#define LOAD_FONT4    // Medium — good for data readouts
#define LOAD_FONT6    // Large numerals
#define LOAD_FONT7    // 7-segment style
#define LOAD_FONT8    // Very large numerals (high RAM cost)
#define LOAD_GFXFF    // FreeFonts — access to 48 font variants

#define SMOOTH_FONT   // Anti-aliased font rendering (uses more Flash)

// ── SPI Bus Speed ────────────────────────────────────────────────
// ESP32-S3 can sustain 80 MHz on SPI2; 40 MHz is a safe default.
// Increase to 80000000 if you see no display glitches.
#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

// ── Colour Order ─────────────────────────────────────────────────
// Most ST7789 panels are BGR. If colours look wrong (red ↔ blue),
// comment out this line:
// #define TFT_RGB_ORDER  TFT_RGB   // uncomment for RGB panel
// The default (no define) is BGR — correct for most ST7789 panels.

// ── Transactions ─────────────────────────────────────────────────
// Enables SPI transaction support — required for ESP32 multi-device SPI
#define SUPPORT_TRANSACTIONS

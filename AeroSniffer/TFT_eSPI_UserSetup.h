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
//
//  HARDWARE VARIANTS:
//    - HW_DESKBUDDY_2: XIAO ESP32S3 + ST7789 240×240
//    - HW_DEVKITC:     ESP32-S3 DevKitC + ILI9341 240×320
//
//  Toggle the #define below to match your build.
// ================================================================

// ── Hardware Variant ─────────────────────────────────────────────
// Must match the setting in Config.h!
// #define HW_DEVKITC
#define HW_DESKBUDDY_2

// ================================================================
//  DESKBUDDY 2.0 — XIAO ESP32S3 + ST7789 240×240
// ================================================================
#ifdef HW_DESKBUDDY_2

// ── Display Driver ──────────────────────────────────────────────
#define ST7789_DRIVER        // 240×240 square IPS — DeskBuddy 2.0 display

// ── Display Dimensions ──────────────────────────────────────────
#define TFT_WIDTH   240
#define TFT_HEIGHT  240      // Square!

// ── SPI Pin Mapping (XIAO ESP32S3 hardware SPI) ────────────────
#define TFT_MOSI     9      // D10 → GPIO 9
#define TFT_SCLK     7      // D8  → GPIO 7
#define TFT_CS       4      // D3  → GPIO 4
#define TFT_DC       3      // D2  → GPIO 3
#define TFT_RST      8      // D9  → GPIO 8

// Backlight is wired to VCC on DeskBuddy carrier — always on
// No BL pin defined (saves a GPIO)

// ================================================================
//  ORIGINAL DEVKITC — ESP32-S3 DevKitC + ILI9341 240×320
// ================================================================
#elif defined(HW_DEVKITC)

// ── Display Driver ──────────────────────────────────────────────
#define ILI9341_DRIVER       // 240×320 rectangular — original build

// ── Display Dimensions ──────────────────────────────────────────
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ── SPI Pin Mapping (DevKitC) ───────────────────────────────────
#define TFT_MOSI    11       // Data Out
#define TFT_SCLK    12       // Clock
#define TFT_CS      10       // Chip Select
#define TFT_DC      13       // Data / Command
#define TFT_RST     14       // Reset

// Backlight on GPIO 15
#define TFT_BL      15
#define TFT_BACKLIGHT_ON  HIGH

#else
  #error "Define HW_DESKBUDDY_2 or HW_DEVKITC in TFT_eSPI_UserSetup.h"
#endif

// ================================================================
//  SHARED SETTINGS (both variants)
// ================================================================

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

#!/usr/bin/env bash
# ================================================================
#  AeroSniffer — Arduino Library Installer
#  Supports: Linux / macOS
#  Usage:  bash tools/install_libraries.sh
# ================================================================

set -e

# Detect arduino-cli or fall back to manual instructions
if ! command -v arduino-cli &> /dev/null; then
  echo ""
  echo "📦 arduino-cli not found. Installing it first..."
  echo ""
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
  export PATH="$PATH:$HOME/bin"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "   AeroSniffer — Library Installer"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Update core index
echo "▶ Updating Arduino core index..."
arduino-cli core update-index

# Add ESP32 board support
echo "▶ Adding ESP32 board support URL..."
arduino-cli config add board_manager.additional_urls \
  "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json" \
  2>/dev/null || true

echo "▶ Installing ESP32 Arduino core (this may take a few minutes)..."
arduino-cli core install esp32:esp32

echo ""
echo "▶ Installing required libraries..."
echo ""

LIBS=(
  "TFT_eSPI"
  "ArduinoFFT"
  "ArduinoJson"
  "NimBLE-Arduino"
  "AnimatedGIF"
)

for lib in "${LIBS[@]}"; do
  echo -n "  Installing $lib ... "
  arduino-cli lib install "$lib" 2>/dev/null && echo "✅" || echo "⚠️  Already installed or not found by exact name — check manually"
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅  All libraries installed."
echo ""
echo "⚠️  IMPORTANT — Final manual step:"
echo "    Copy MultiBoot_DeskGadget/TFT_eSPI_UserSetup.h"
echo "    into your Arduino libraries folder:"
echo ""

# Detect libraries path
if [[ "$OSTYPE" == "darwin"* ]]; then
  LIB_PATH="$HOME/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h"
else
  LIB_PATH="$HOME/Arduino/libraries/TFT_eSPI/User_Setup.h"
fi

echo "    $LIB_PATH"
echo ""

# Auto-copy if path exists
if [ -f "MultiBoot_DeskGadget/TFT_eSPI_UserSetup.h" ]; then
  LIB_DIR=$(dirname "$LIB_PATH")
  if [ -d "$LIB_DIR" ]; then
    cp MultiBoot_DeskGadget/TFT_eSPI_UserSetup.h "$LIB_PATH"
    echo "    ✅  Auto-copied TFT_eSPI_UserSetup.h → User_Setup.h"
  fi
fi

echo ""
echo "Next step: Edit MultiBoot_DeskGadget/Config.h with your WiFi credentials."
echo "Then open MultiBoot_DeskGadget/MultiBoot_DeskGadget.ino in Arduino IDE."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

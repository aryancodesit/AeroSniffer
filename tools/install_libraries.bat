@echo off
:: ================================================================
::  AeroSniffer — Arduino Library Installer (Windows)
::  Usage: Double-click this file or run from Command Prompt
:: ================================================================

echo.
echo ====================================================
echo    AeroSniffer -- Library Installer (Windows)
echo ====================================================
echo.

:: Check for arduino-cli
where arduino-cli >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo [!] arduino-cli not found in PATH.
  echo.
  echo Please download it from:
  echo https://arduino.github.io/arduino-cli/latest/installation/
  echo.
  echo Or install manually via Arduino IDE 2.x:
  echo   Sketch ^> Include Library ^> Manage Libraries
  echo.
  echo Libraries to install:
  echo   - TFT_eSPI          by Bodmer
  echo   - ArduinoFFT        by kosme
  echo   - ArduinoJson       by Benoit Blanchon  (v7)
  echo   - NimBLE-Arduino    by h2zero
  echo   - AnimatedGIF       by Larry Bank
  echo.
  pause
  exit /b 1
)

echo [>] Updating index...
arduino-cli core update-index

echo [>] Adding ESP32 board URL...
arduino-cli config add board_manager.additional_urls ^
  "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

echo [>] Installing ESP32 core (may take a few minutes)...
arduino-cli core install esp32:esp32

echo.
echo [>] Installing libraries...
arduino-cli lib install "TFT_eSPI"
arduino-cli lib install "ArduinoFFT"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "NimBLE-Arduino"
arduino-cli lib install "AnimatedGIF"

echo.
echo [>] Copying TFT_eSPI User_Setup.h...
set LIB_PATH=%USERPROFILE%\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
if exist AeroSniffer\TFT_eSPI_UserSetup.h (
  if exist "%USERPROFILE%\Documents\Arduino\libraries\TFT_eSPI\" (
    copy /Y "AeroSniffer\TFT_eSPI_UserSetup.h" "%LIB_PATH%"
    echo [OK] Copied to %LIB_PATH%
  )
)

echo.
echo ====================================================
echo [OK] All done!
echo.
echo Next: Edit AeroSniffer\Config.h
echo       Set your WIFI_SSID and WIFI_PASSWORD
echo       Then open the .ino in Arduino IDE and flash.
echo ====================================================
echo.
pause

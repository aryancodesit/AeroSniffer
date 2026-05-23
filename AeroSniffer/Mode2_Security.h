// ================================================================
//  Mode2_Security.h  —  Integration Wrapper for ESP32 Marauder
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
//
//  NOTE: This mode hands over control to the ESP32 Marauder firmware.
//  Marauder is a massive standalone application that expects to own
//  the microcontroller entirely. Once started, it cannot be cleanly
//  stopped. Therefore, to switch out of Mode 2 back to Mode 0 or 3,
//  this wrapper will simply restart the ESP32.
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Extern declarations for Marauder ────────────────────────────
// You MUST rename Marauder's setup() and loop() to these names
// in its main .ino / .cpp file before compiling!
extern void marauder_setup(TFT_eSPI* tft) __attribute__((weak));
extern void marauder_loop() __attribute__((weak));

// Provide dummy weak implementations so the project compiles
// even before you copy the Marauder files.
void marauder_setup(TFT_eSPI* tft) {
  if (tft) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(2);
    tft->setCursor(10, 100);
    tft->print("Marauder files");
    tft->setCursor(10, 130);
    tft->print("not found yet.");
  }
}
void marauder_loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

void security_setup(TFT_eSPI* tft) {
  // Let Marauder perform its own hardware initialization.
  // It will likely initialize its own TFT_eSPI instance, but we pass
  // the pointer just in case the weak dummy needs it.
  marauder_setup(tft);
}

void security_teardown() {
  // ESP32 Marauder cannot be cleanly torn down because it
  // spins up numerous FreeRTOS tasks and hardware interrupts
  // that do not have de-initialization routines.
  //
  // We rely on the MultiBoot orchestration layer to save the next
  // mode to EEPROM/Preferences and then we restart.
  ESP.restart();
}

void security_core0_task() {
  // Marauder typically relies heavily on the main loop() (Core 1)
  // and spawns its own WiFi sniffer tasks on Core 0.
  // We yield this task to free up Core 0 for Marauder's use.
  vTaskDelay(pdMS_TO_TICKS(500));
}

void security_core1_task() {
  // Drive Marauder's main loop
  marauder_loop();
  
  // Yield occasionally to prevent watchdog resets if Marauder 
  // hogs the CPU without yielding natively.
  taskYIELD();
}

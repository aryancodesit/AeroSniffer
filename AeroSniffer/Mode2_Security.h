// ================================================================
//  Mode2_Security.h  —  Wi-Fi Security Operations Center
//  AeroSniffer | ESP32-S3 Multi-Boot Desk Gadget
// ================================================================
#pragma once

#include <TFT_eSPI.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
using fs::FS;
#include <WebServer.h>
#include <Preferences.h>
#include "Config.h"

#if defined(HW_DESKBUDDY_2)

#if ENABLE_PORTAL_V2

// ── Portal v2 ────────────────────────────────────────────────────
// Mobile-first SPA dashboard with 4 read-only sections.
// Foundation layer: data stubs, no POST endpoints yet.
#include "Security/Portal.h"

static void security_handle_command(String line) {
    line.trim();
    if (!line.startsWith("CMD:")) return;
    String cmd = line.substring(4);
    if (cmd == "PING") {
        Serial.println("RES:{\"ok\":true,\"fw\":\"2.0\",\"mode\":2,\"hw\":\"deskbuddy2\"}");
    }
}

void security_setup(TFT_eSprite* tft) {
    Serial.println("[SEC] === security_setup START (PORTAL v2) ===");
    WiFiService.startAP(sys_ap_ssid.c_str(), sys_ap_pass.c_str());
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_AP, &conf);
    Serial.printf("[SEC] AP config: channel=%d auth=%d max_conn=%d\n",
        conf.ap.channel, conf.ap.authmode, conf.ap.max_connection);
    portal_setup(tft);
    Serial.println("[SEC] === security_setup DONE ===");
}

void security_teardown() { portal_teardown(); }
void security_core0_task() { portal_core0_task(); }
void security_core1_task() { portal_core1_task(); }

#else

// ── Minimal AP Diagnostic Mode (immutable baseline) ──────────────
// No BLE, no promiscuous mode, no device tracking, no threat analysis.
// Only: WiFi SoftAP + WebServer + connection event logs.

static TFT_eSprite* _stft = nullptr;
static WebServer* _webserver = nullptr;

static void diag_handle_root() {
    Serial.printf("[SEC] WEB REQ client=%s uri=%s\n",
        _webserver->client().remoteIP().toString().c_str(), _webserver->uri().c_str());
    String html = "<!DOCTYPE html><html><head><title>AeroSniffer AP Test</title>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>body{background:#070a08;color:#39ff88;font-family:monospace;padding:2em}"
        "h1{color:#c084fc}h2{color:#ffb000}</style></head><body>"
        "<h1>AeroSniffer AP Test</h1>"
        "<h2>Connected Clients</h2>"
        "<p>Stations: <strong>" + String(WiFi.softAPgetStationNum()) + "</strong></p>"
        "<p>AP IP: <strong>" + WiFi.softAPIP().toString() + "</strong></p>"
        "<p>SSID: <strong>AeroSniffer-Test</strong></p>"
        "</body></html>";
    _webserver->send(200, "text/html", html);
}

static void security_handle_command(String line) {
    line.trim();
    if (!line.startsWith("CMD:")) return;
    String cmd = line.substring(4);
    if (cmd == "PING") {
        Serial.println("RES:{\"ok\":true,\"fw\":\"2.0\",\"mode\":2,\"hw\":\"deskbuddy2\"}");
    }
}

static void sec_draw_minimal() {
    _stft->fillScreen(TFT_BLACK);

    _stft->setTextColor(0x07FF);
    _stft->setTextSize(2);
    _stft->setCursor(10, 10);
    _stft->print("SECURITY");

    _stft->drawFastHLine(10, 32, 220, 0x528A);

    _stft->setTextSize(1);
    _stft->setTextColor(0x07E0);
    _stft->setCursor(10, 44);
    _stft->print("Status: AP ACTIVE");

    _stft->setTextColor(0xFFFF);
    _stft->setCursor(10, 62);
    _stft->print("SSID: ");
    _stft->setTextColor(0x07FF);
    _stft->print(sys_ap_ssid.c_str());

    _stft->setTextColor(0xFFFF);
    _stft->setCursor(10, 80);
    _stft->print("IP: ");
    _stft->setTextColor(0x07FF);
    _stft->print(WiFiService.getSoftAPIP());

    _stft->setTextColor(0xFFFF);
    _stft->setCursor(10, 98);
    _stft->print("Clients: ");
    _stft->setTextColor(0xFFE0);
    _stft->print(WiFi.softAPgetStationNum());

    _stft->setTextColor(0xFFFF);
    _stft->setCursor(10, 116);
    _stft->print("Portal: ");
    _stft->setTextColor(0x07E0);
    _stft->print("READY");

    _stft->fillRect(0, 226, 240, 14, 0x0008);
    _stft->setTextColor(0x2CA0);
    _stft->setTextSize(1);
    _stft->setCursor(4, 228);
    _stft->print("http://192.168.4.1");
}

void security_setup(TFT_eSprite* tft) {
    Serial.println("[SEC] === security_setup START (DIAGNOSTIC) ===");
    _stft = tft;

    Serial.println("[SEC] Starting AP...");
    WiFiService.startAP(sys_ap_ssid.c_str(), sys_ap_pass.c_str());
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_AP, &conf);
    Serial.printf("[SEC] AP started: SSID=\"%s\" pass=\"%s\"\n", sys_ap_ssid.c_str(), sys_ap_pass.c_str());
    Serial.printf("[SEC] AP config: channel=%d auth=%d max_conn=%d\n",
        conf.ap.channel, conf.ap.authmode, conf.ap.max_connection);
    Serial.printf("[SEC] Web UI: http://%s\n", WiFiService.getSoftAPIP().c_str());

    Serial.println("[SEC] Starting WebServer...");
    _webserver = new WebServer(SEC_WEB_PORT);
    _webserver->on("/", diag_handle_root);
    _webserver->begin();
    Serial.println("[SEC] WebServer started.");

    sec_draw_minimal();

    Serial.println("[SEC] === security_setup DONE ===");
}

void security_teardown() {
    if (_webserver) {
        _webserver->stop();
        delete _webserver;
        _webserver = nullptr;
    }
    _stft = nullptr;
}

void security_core0_task() {
    vTaskDelay(pdMS_TO_TICKS(500));
}

void security_core1_task() {
    if (!_stft) return;

    if (_webserver) _webserver->handleClient();

    uint32_t now = millis();
    static uint32_t last_sta_log = 0;
    if (now - last_sta_log >= 5000) {
        last_sta_log = now;
        int n = WiFi.softAPgetStationNum();
        Serial.printf("[SEC] AP stations=%d\n", n);
    }

    sec_draw_minimal();

    vTaskDelay(pdMS_TO_TICKS(100));
}

#endif

#elif defined(HW_DEVKITC)

static void security_handle_command(String line) {}

static TFT_eSPI* _stft = nullptr;

void security_setup(TFT_eSPI* tft) {
  _stft = tft;
  if (tft) {
    tft->fillScreen(TFT_BLACK);
    tft->setTextColor(TFT_WHITE);
    tft->setTextSize(2);
    tft->setCursor(10, 100);
    tft->print("DevKitC target");
    tft->setCursor(10, 130);
    tft->print("no SOC firmware");
  }
}

void security_teardown() {
  ESP.restart();
}

void security_core0_task() {
  vTaskDelay(pdMS_TO_TICKS(500));
}

void security_core1_task() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif

// ================================================================
//  Aviation/Provider.h  —  Flight Data Providers Interface
//  AeroSniffer Aviation Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include "../Config.h"
#include "FlightStore.h"

class FlightProvider {
public:
  virtual ~FlightProvider() {}
  virtual bool fetchFlights(float lamin, float lomin, float lamax, float lomax, String& out_payload) = 0;
};

class OpenSkyProvider : public FlightProvider {
public:
  bool fetchFlights(float lamin, float lomin, float lamax, float lomax, String& out_payload) override {
    char url[250];
    snprintf(url, sizeof(url),
      "https://opensky-network.org/api/states/all"
      "?lamin=%.2f&lomin=%.2f&lamax=%.2f&lomax=%.2f",
      (double)lamin, (double)lomin, (double)lamax, (double)lomax);

    DEBUG_PRINTF("[AVI] Fetching URL: %s\n", url);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.begin(url);
    http.setAuthorization(OPENSKY_USER, OPENSKY_PASS);
    
    const char* headerKeys[] = {"X-Rate-Limit-Limit", "X-Rate-Limit-Remaining", "X-Rate-Limit-Reset"};
    http.collectHeaders(headerKeys, 3);

    http.setTimeout(10000);
    int code = http.GET();

    if (code > 0) {
      Serial.printf("[AVI] HTTP Response Code: %d\n", code);
      if (http.hasHeader("X-Rate-Limit-Limit")) {
        Serial.printf("[AVI] Rate Limit: %s\n", http.header("X-Rate-Limit-Limit").c_str());
      }
      if (http.hasHeader("X-Rate-Limit-Remaining")) {
        Serial.printf("[AVI] Credits Remaining: %s\n", http.header("X-Rate-Limit-Remaining").c_str());
      }
      if (http.hasHeader("X-Rate-Limit-Reset")) {
        Serial.printf("[AVI] Rate Limit Reset: %s\n", http.header("X-Rate-Limit-Reset").c_str());
      }
    } else {
      Serial.printf("[AVI] HTTP GET Failed: %s\n", http.errorToString(code).c_str());
    }

    if (code != 200) {
      Serial.printf("[AVI] Fetch Failed. HTTP Code: %d\n", code);
      snprintf(status_msg, sizeof(status_msg), "HTTP error %d", code);
      http.end();
      return false;
    }

    Serial.println("[AVI] Fetch Complete");
    out_payload = http.getString();
    http.end();
    return true;
  }
};

// Future Provider: ADSB.lol (Stub for expansion)
class ADSBlolProvider : public FlightProvider {
public:
  bool fetchFlights(float lamin, float lomin, float lamax, float lomax, String& out_payload) override {
    // Stub implementation
    snprintf(status_msg, sizeof(status_msg), "ADSB.lol not configured");
    return false;
  }
};

// Future Provider: Airplanes.Live (Stub for expansion)
class AirplanesLiveProvider : public FlightProvider {
public:
  bool fetchFlights(float lamin, float lomin, float lamax, float lomax, String& out_payload) override {
    // Stub implementation
    snprintf(status_msg, sizeof(status_msg), "Airplanes.Live not configured");
    return false;
  }
};

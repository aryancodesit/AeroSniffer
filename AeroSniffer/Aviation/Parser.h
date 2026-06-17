// ================================================================
//  Aviation/Parser.h  —  JSON State Vector Parser
//  AeroSniffer Aviation Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "FlightStore.h"
#include "AeroSnifferOS.h"

static bool parseFlightJSON(const String& payload) {
  Serial.printf("[AVI] Payload size = %d\n", payload.length());

  DynamicJsonDocument doc(49152);
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[AVI] JSON Error: %s\n", err.c_str());
    snprintf(status_msg, sizeof(status_msg), "JSON err: %s", err.c_str());
    return false;
  }

  Serial.println("[AVI] Parse Complete");

  if (!doc.containsKey("states") || doc["states"].isNull()) {
    Serial.println("[AVI] No flights found in bounding box.");
    flight_count = 0;
    snprintf(status_msg, sizeof(status_msg), "No flights in area");
    Serial.println("[AVI] Parsed 0 aircraft");
    Serial.println("[AVI] Stored 0 aircraft");
    return true;
  }

  JsonArray states = doc["states"].as<JsonArray>();
  flight_count     = 0;

  for (JsonArray sv : states) {
    if (flight_count >= MAX_FLIGHTS) break;

    FlightRecord& f = flights[flight_count];
    memset(&f, 0, sizeof(f));

    const char* cs = sv[1].as<const char*>();
    if (!cs || strlen(cs) == 0) continue;

    strncpy(f.callsign, cs, 9);
    f.callsign[9] = '\0';
    // Trim spaces
    for (int k = (int)strlen(f.callsign) - 1; k >= 0 && f.callsign[k] == ' '; k--) {
      f.callsign[k] = '\0';
    }

    f.lon       = sv[5].isNull() ? 0.0f : sv[5].as<float>();
    f.lat       = sv[6].isNull() ? 0.0f : sv[6].as<float>();
    f.alt_m     = sv[7].isNull() ? 0.0f : sv[7].as<float>();
    f.on_ground = sv[8].as<bool>() ? 1 : 0;
    f.vel_ms    = sv[9].isNull()  ? 0.0f : sv[9].as<float>();
    f.heading   = sv[10].isNull() ? 0.0f : sv[10].as<float>();
    f.valid     = true;

    flight_count++;
  }

  Serial.printf("[AVI] Parsed %d aircraft\n", flight_count);
  Serial.printf("[AVI] Stored %d aircraft\n", flight_count);
  return true;
}

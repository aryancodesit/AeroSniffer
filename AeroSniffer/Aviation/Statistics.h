// ================================================================
//  Aviation/Statistics.h  —  Aviation Statistics & Serial Dump
//  AeroSniffer Aviation Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "FlightStore.h"

static void avi_send_flights_serial() {
  Serial.printf("[AVI] Emitting %d aircraft over serial\n", flight_count);

  // Build JSON in a String to send as a single atomic line
  String json = "RES:{\"flights\":[";
  for (int i = 0; i < flight_count; i++) {
    if (i > 0) json += ",";
    FlightRecord& f = flights[i];
    char buf[200];
    snprintf(buf, sizeof(buf),
      "{\"callsign\":\"%s\",\"lat\":%.4f,\"lon\":%.4f,\"alt\":%.1f,\"spd\":%.1f,\"hdg\":%.1f,\"gnd\":%d}",
      f.callsign, f.lat, f.lon, f.alt_m, f.vel_ms, f.heading, f.on_ground);
    json += buf;
  }
  json += "]}";

  Serial.printf("[AVI] Serial payload size = %d bytes\n", json.length());
  Serial.println(json);
}

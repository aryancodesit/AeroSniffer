// ================================================================
//  Aviation/FlightStore.h  —  Active Flight Storage State
//  AeroSniffer Aviation Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include "Config.h"

struct FlightRecord {
  char    callsign[10];
  float   lat, lon;
  float   alt_m;       // Barometric altitude (metres)
  float   vel_ms;      // Ground speed (m/s)
  float   heading;     // True track (degrees)
  int8_t  on_ground;
  bool    valid;
};

static FlightRecord flights[MAX_FLIGHTS];
static int          flight_count  = 0;
static int          disp_page     = 0;
static bool         wifi_ok       = false;
static bool         fetching      = false;
static bool         last_fetch_success = true;
static uint32_t     last_fetch    = 0;
static uint32_t     last_scroll   = 0;
static char         status_msg[48] = "Initialising...";

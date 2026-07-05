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

// Flight state is kept in module-level variables rather than a class because
// this header forms a single translation unit with the rest of the sketch.
// Namespaced to avoid accidental name collision — a full FlightStateManager
// class would be warranted if multiple instances or unit tests were needed.
namespace FlightStore {
inline FlightRecord flights[MAX_FLIGHTS];
inline int          flight_count  = 0;
inline int          disp_page     = 0;
inline bool         wifi_ok       = false;
inline bool         fetching      = false;
inline bool         last_fetch_success = true;
inline uint32_t     last_fetch    = 0;
inline uint32_t     last_scroll   = 0;
inline char         status_msg[48] = "Initialising...";
}

// Project convention: module-level state uses unqualified access within the
// single-translation-unit sketch. External consumers may qualify with FlightStore::.
using namespace FlightStore;

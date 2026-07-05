// ================================================================
//  Security/HomeGuard.h  —  Whitelist & Welcome Home Engine
//  AeroSniffer Security Layer | Modular Subcomponent
// ================================================================
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "AeroSnifferOS.h"
#include "Config.h"

// ── Home Guard & Privacy States ──────────────────────────────────
namespace HomeGuardState {
inline uint8_t  welcome_mac[6] = {0};
inline char     welcome_name[32] = "";
inline volatile bool welcome_triggered = false;
inline uint32_t welcome_triggered_ms = 0;
inline uint32_t welcome_last_seen_ms = 0;

#define MAX_KNOWN_DEVICES 50

struct KnownDevice {
  char mac[18];        // "AA:BB:CC:DD:EE:FF"
  char name[32];       // Friendly name
  uint32_t firstSeen;  // timestamp (seconds from boot)
  uint32_t lastSeen;   // timestamp
  uint32_t sightings;  // counter
  bool trusted;        // true if trusted
};

inline KnownDevice known_devices[MAX_KNOWN_DEVICES];
inline int known_device_count = 0;

inline char     find_ssid[33] = "";
inline volatile int8_t find_rssi = -100;
inline uint32_t find_last_seen_ms = 0;
inline volatile bool find_mode_active = false;
}

using namespace HomeGuardState;

static bool parse_mac_string(const String& mac_str, uint8_t* mac_bytes) {
  int values[6];
  if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) == 6) {
    for (int i = 0; i < 6; i++) {
      mac_bytes[i] = (uint8_t)values[i];
    }
    return true;
  }
  return false;
}

static String format_mac_bytes(const uint8_t* mac_bytes) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_bytes[0], mac_bytes[1], mac_bytes[2],
           mac_bytes[3], mac_bytes[4], mac_bytes[5]);
  return String(buf);
}

static void save_known_devices() {
  Preferences prefs;
  prefs.begin("aerosniffer", false);
  prefs.putInt("kd_cnt", known_device_count);
  if (known_device_count > 0) {
    prefs.putBytes("kd_db", known_devices, sizeof(KnownDevice) * known_device_count);
  }
  prefs.end();
}

static void load_known_devices() {
  Preferences prefs;
  prefs.begin("aerosniffer", true);
  known_device_count = prefs.getInt("kd_cnt", 0);
  if (known_device_count > MAX_KNOWN_DEVICES) known_device_count = MAX_KNOWN_DEVICES;
  if (known_device_count > 0) {
    prefs.getBytes("kd_db", known_devices, sizeof(KnownDevice) * known_device_count);
  }
  prefs.end();
}

static int find_device_index(const char* mac) {
  for (int i = 0; i < known_device_count; i++) {
    if (strcasecmp(known_devices[i].mac, mac) == 0) {
      return i;
    }
  }
  return -1;
}

static void enroll_device(const uint8_t* mac_bytes, const char* name = nullptr) {
  if (!sys_homeguard_enabled) return;
  char mac_str[18];
  snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_bytes[0], mac_bytes[1], mac_bytes[2],
           mac_bytes[3], mac_bytes[4], mac_bytes[5]);

  int idx = find_device_index(mac_str);
  uint32_t now_sec = millis() / 1000;

  if (idx != -1) {
    // Device already known
    known_devices[idx].sightings++;
    uint32_t elapsed = now_sec - known_devices[idx].lastSeen;
    known_devices[idx].lastSeen = now_sec;
    
    // Only trigger returning message if they haven't been seen in the last 300 seconds
    if (elapsed > 300) {
      if (known_devices[idx].trusted) {
        EventBus.publish(EVENT_DEVICE_TRUSTED);
        Serial.printf("EVT:{\"type\":\"security_alert\",\"mac\":\"%s\",\"classification\":\"TRUSTED\",\"msg\":\"🛡 Trusted device returned: %s\"}\n",
                      mac_str, known_devices[idx].name);
      } else {
        EventBus.publish(EVENT_DEVICE_FAMILIAR);
        Serial.printf("EVT:{\"type\":\"security_alert\",\"mac\":\"%s\",\"classification\":\"FAMILIAR\",\"msg\":\"🤔 I don't recognize this device (%s)\"}\n",
                      mac_str, strlen(known_devices[idx].name) > 0 ? known_devices[idx].name : "Familiar");
      }
    }
    return;
  }

  // Check if randomized MAC
  bool is_randomized = (mac_bytes[0] & 0x02) != 0;
  
  // New device discovered
  if (known_device_count < MAX_KNOWN_DEVICES) {
    idx = known_device_count++;
  } else {
    // Find the oldest untrusted familiar/unknown device to evict
    int oldest_idx = -1;
    uint32_t oldest_time = 0xFFFFFFFF;
    for (int i = 0; i < known_device_count; i++) {
      if (!known_devices[i].trusted && known_devices[i].lastSeen < oldest_time) {
        oldest_time = known_devices[i].lastSeen;
        oldest_idx = i;
      }
    }
    if (oldest_idx != -1) {
      idx = oldest_idx;
    } else {
      return; // All devices are trusted and the database is full
    }
  }

  // Initialize new KnownDevice slot
  strcpy(known_devices[idx].mac, mac_str);
  if (name && strlen(name) > 0) {
    strncpy(known_devices[idx].name, name, sizeof(known_devices[idx].name));
  } else if (is_randomized) {
    strcpy(known_devices[idx].name, "Android Private MAC");
  } else {
    known_devices[idx].name[0] = '\0';
  }
  known_devices[idx].firstSeen = now_sec;
  known_devices[idx].lastSeen = now_sec;
  known_devices[idx].sightings = 1;
  known_devices[idx].trusted = false;

  save_known_devices();

  if (is_randomized) {
    EventBus.publish(EVENT_DEVICE_UNKNOWN);
    Serial.printf("EVT:{\"type\":\"security_alert\",\"mac\":\"%s\",\"classification\":\"UNKNOWN\",\"msg\":\"👀 New device nearby: Android Private MAC\"}\n", mac_str);
  } else {
    EventBus.publish(EVENT_DEVICE_UNKNOWN);
    Serial.printf("EVT:{\"type\":\"security_alert\",\"mac\":\"%s\",\"classification\":\"UNKNOWN\",\"msg\":\"👀 New device nearby\"}\n", mac_str);
  }
}

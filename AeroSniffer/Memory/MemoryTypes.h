#pragma once
#ifndef MEMORY_TYPES_H
#define MEMORY_TYPES_H

#include <stdint.h>

enum MemoryCategory : uint8_t {
  MEM_CAT_NONE        = 0,
  MEM_CAT_EPISODIC    = 1,
  MEM_CAT_FAMILIARITY = 2,
  MEM_CAT_SIGNIFICANT = 3
};

enum MemoryDomain : uint8_t {
  DOMAIN_TOUCH     = 0,
  DOMAIN_SECURITY  = 1,
  DOMAIN_AVIATION  = 2,
  DOMAIN_MOOD      = 3,
  DOMAIN_ATTENTION = 4,
  DOMAIN_COUNT
};

enum TouchSubtype : uint8_t {
  TOUCH_TAP       = 0x01,
  TOUCH_DOUBLE    = 0x02,
  TOUCH_HOLD      = 0x03,
  TOUCH_BURST     = 0x04,
  TOUCH_LONG_HOLD = 0x05
};

enum SecuritySubtype : uint8_t {
  SEC_DEVICE_APPEARED       = 0x10,
  SEC_SUSPICIOUS_ACTIVITY   = 0x11,
  SEC_THREAT_DETECTED       = 0x12
};

enum AviationSubtype : uint8_t {
  AVI_AIRCRAFT_SPOTTED      = 0x20,
  AVI_QUIET_SKY_PERIOD      = 0x21,
  AVI_UNUSUAL_TRAFFIC_EVENT = 0x22
};

enum MoodSubtype : uint8_t {
  MOOD_LONG_RELAXED_PERIOD  = 0x30,
  MOOD_LONG_PLAYFUL_PERIOD  = 0x31,
  MOOD_LONG_ANXIOUS_PERIOD  = 0x32,
  MOOD_TRANSITION           = 0x33
};

struct MemoryRecord {
  uint32_t id;
  uint32_t source_id;
  uint32_t formed_at_ms;
  uint8_t  category;
  uint8_t  domain;
  uint8_t  subtype;
  uint8_t  salience;
  uint8_t  strength;
  uint8_t  recall_count;
  uint8_t  mood_at_formation;
  uint8_t  attention_at_form;
  uint8_t  mode_at_formation;
  uint8_t  padding;
  union {
    struct {
      uint16_t touch_duration_ms;
      uint8_t  tap_count_window;
      uint8_t  _pad[13];
    } touch;
    struct {
      uint8_t  threat_level;
      uint8_t  event_count;
      uint8_t  _pad[14];
    } security;
    struct {
      uint32_t icao24;
      int16_t  altitude_ft;
      uint8_t  is_rare;
      uint8_t  _pad[9];
    } aviation;
    struct {
      uint8_t  mood;
      uint8_t  prev_mood;
      uint16_t duration_min;
      uint8_t  _pad[12];
    } mood_event;
    uint8_t raw[16];
  } data;
};

struct MemorySummary {
  uint8_t  domain_strength[DOMAIN_COUNT];
  uint8_t  episodic_count;
  uint8_t  familiarity_count;
  uint8_t  significant_present;
  uint16_t ms_since_last_touch;
  uint16_t total_records;
  uint16_t dropped_touch_events;
};

#define MEMORY_MAX_RECORDS 64

static_assert(sizeof(MemoryRecord) == 40, "MemoryRecord must be 40 bytes");
static_assert(sizeof(MemoryRecord::data) == 16, "MemoryRecord data union must be 16 bytes");

#endif

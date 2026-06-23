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

struct MemoryRecord {
  uint32_t id;
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
};

#define MEMORY_MAX_RECORDS 64

#endif

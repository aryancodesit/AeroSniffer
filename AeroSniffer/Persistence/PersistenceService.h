#pragma once
#ifndef PERSISTENCE_SERVICE_H
#define PERSISTENCE_SERVICE_H

#include <Arduino.h>
#include <Preferences.h>
#include "AeroSnifferOS.h"

struct CreatureProfile
{
    uint32_t schema_version;

    char name[32];

    uint32_t total_boots;
    uint32_t total_runtime_minutes;
    uint32_t total_touches;

    uint32_t playful_entries;
    uint32_t anxious_entries;

    uint32_t first_boot_unix;
    uint32_t last_seen_unix;
};

class PersistenceService
{
public:
    void begin();
    void tick();
    void save();
    void factoryReset();

    const CreatureProfile& profile() const;

private:
    CreatureProfile _profile;
    uint32_t _last_save_ms = 0;
    uint32_t _last_runtime_flush_ms = 0;
    MoodType _last_seen_mood = MOOD_RELAXED;
    bool     _prev_was_touch_source = false;

    void onTouch();
    void flushRuntimeMinutes();
    void detectMoodTransition();
    bool load();
    void migrate(uint32_t from_version);

    static constexpr uint32_t SCHEMA_VERSION = 1;
    static constexpr uint32_t SAVE_INTERVAL_MS = 300000;
    static constexpr const char* NVS_NS = "creature";
    static constexpr const char* NVS_KEY_PROFILE = "profile";
    static constexpr const char* NVS_KEY_SCHEMA = "schema";
};

extern PersistenceService CreaturePersistence;

#endif

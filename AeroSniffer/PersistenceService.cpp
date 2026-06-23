#include "Persistence/PersistenceService.h"

PersistenceService CreaturePersistence;

void PersistenceService::begin() {
    _last_save_ms = millis();
    _last_runtime_flush_ms = millis();
    _last_seen_mood = g_creature.mood;
    _prev_was_touch_source = false;

    Preferences p;
    bool exists = p.begin(NVS_NS, true) && p.isKey(NVS_KEY_PROFILE);
    p.end();

    if (exists && load()) {
        _profile.total_boots++;
    } else {
        factoryReset();
        _profile.total_boots = 1;
    }

    save();
    Serial.printf("[PERSIST] Boot %u | touches=%u | runtime=%u min | playful=%u | anxious=%u\n",
        _profile.total_boots, _profile.total_touches, _profile.total_runtime_minutes,
        _profile.playful_entries, _profile.anxious_entries);
}

void PersistenceService::tick() {
    onTouch();
    detectMoodTransition();

    uint32_t now = millis();
    flushRuntimeMinutes();

    if (now - _last_save_ms >= SAVE_INTERVAL_MS) {
        flushRuntimeMinutes();
        save();
        _last_save_ms = now;
    }
}

void PersistenceService::onTouch() {
    bool currently_touch = (g_creature.attention.source == SOURCE_TOUCH);
    if (currently_touch && !_prev_was_touch_source) {
        _profile.total_touches++;
    }
    _prev_was_touch_source = currently_touch;
}

void PersistenceService::detectMoodTransition() {
    if (g_creature.mood != _last_seen_mood) {
        switch (g_creature.mood) {
            case MOOD_PLAYFUL: _profile.playful_entries++; break;
            case MOOD_ANXIOUS: _profile.anxious_entries++; break;
            default: break;
        }
        _last_seen_mood = g_creature.mood;
    }
}

void PersistenceService::flushRuntimeMinutes() {
    uint32_t now_ms = millis();
    uint32_t elapsed_ms = now_ms - _last_runtime_flush_ms;

    if (elapsed_ms > 3600000) {
        _last_runtime_flush_ms = now_ms;
        return;
    }

    uint32_t delta_minutes = elapsed_ms / 60000;
    if (delta_minutes > 0) {
        _profile.total_runtime_minutes += delta_minutes;
        _last_runtime_flush_ms += delta_minutes * 60000;
    }
}

bool PersistenceService::load() {
    Preferences p;
    if (!p.begin(NVS_NS, true)) return false;

    uint32_t stored_version = p.getUInt(NVS_KEY_SCHEMA, 0);
    size_t stored_size = p.getBytesLength(NVS_KEY_PROFILE);

    if (stored_version != SCHEMA_VERSION) {
        p.end();
        migrate(stored_version);
        if (!p.begin(NVS_NS, true)) return false;
        stored_version = p.getUInt(NVS_KEY_SCHEMA, 0);
        if (stored_version != SCHEMA_VERSION) {
            p.end();
            return false;
        }
        stored_size = p.getBytesLength(NVS_KEY_PROFILE);
    }

    if (stored_size != sizeof(CreatureProfile)) {
        p.end();
        return false;
    }

    size_t actual = p.getBytes(NVS_KEY_PROFILE, &_profile, sizeof(CreatureProfile));
    p.end();
    return actual == sizeof(CreatureProfile);
}

void PersistenceService::save() {
    Preferences p;
    if (!p.begin(NVS_NS, false)) return;
    _profile.schema_version = SCHEMA_VERSION;
    p.putBytes(NVS_KEY_PROFILE, &_profile, sizeof(CreatureProfile));
    p.putUInt(NVS_KEY_SCHEMA, SCHEMA_VERSION);
    p.end();

    _last_save_ms = millis();
}

void PersistenceService::factoryReset() {
    _profile = CreatureProfile{};
    _profile.schema_version = SCHEMA_VERSION;
    _last_seen_mood = g_creature.mood;
    _prev_was_touch_source = false;
    save();
    Serial.println("[PERSIST] Factory reset -> profile cleared");
}

const CreatureProfile& PersistenceService::profile() const {
    return _profile;
}

void PersistenceService::migrate(uint32_t from_version) {
    switch (from_version) {
        case 1:
            break;
        default:
            factoryReset();
            break;
    }
}

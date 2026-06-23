// ================================================================
//  p7_injector.ino  —  P7 Schema Recovery Certification Tool
//  AeroSniffer V2.5 — Standalone ESP32-S3 injector
//
//  NOT part of AeroSniffer firmware.  Throwaway certification
//  tool.  Upload, inject, discard.
//
//  NVS target: namespace "creature", keys "schema" (u32) and
//  "profile" (blob, 64 bytes).  Must match PersistenceService
//  constants at PersistenceService.h:49-53.
// ================================================================

#include <Preferences.h>

static constexpr const char* NVS_NS          = "creature";
static constexpr const char* NVS_KEY_SCHEMA  = "schema";
static constexpr const char* NVS_KEY_PROFILE = "profile";

// Must match PersistenceService::CreatureProfile exactly
struct CreatureProfile {
    uint32_t schema_version;
    char     name[32];
    uint32_t total_boots;
    uint32_t total_runtime_minutes;
    uint32_t total_touches;
    uint32_t playful_entries;
    uint32_t anxious_entries;
    uint32_t first_boot_unix;
    uint32_t last_seen_unix;
};

static void inject_old_schema() {
    Preferences p;
    p.begin(NVS_NS, false);
    CreatureProfile profile = {};
    profile.schema_version = 0;
    p.putBytes(NVS_KEY_PROFILE, &profile, sizeof(profile));
    p.putUInt(NVS_KEY_SCHEMA, 0);
    p.end();
    Serial.println();
    Serial.println(F("=== P7 Test 1: Old Schema (version=0) ==="));
    Serial.println(F("Injected: schema=0, valid 64-byte profile"));
    Serial.println(F("Action:   Power-cycle or re-upload AeroSniffer"));
    Serial.println(F("Expect:   [PERSIST] Factory reset -> profile cleared"));
    Serial.println(F("          [PERSIST] Boot 1 | touches=0 | runtime=0 | ..."));
}

static void inject_future_schema() {
    Preferences p;
    p.begin(NVS_NS, false);
    CreatureProfile profile = {};
    profile.schema_version = 255;
    p.putBytes(NVS_KEY_PROFILE, &profile, sizeof(profile));
    p.putUInt(NVS_KEY_SCHEMA, 255);
    p.end();
    Serial.println();
    Serial.println(F("=== P7 Test 2: Future Schema (version=255) ==="));
    Serial.println(F("Injected: schema=255, valid 64-byte profile"));
    Serial.println(F("Action:   Power-cycle or re-upload AeroSniffer"));
    Serial.println(F("Expect:   [PERSIST] Factory reset -> profile cleared"));
    Serial.println(F("          [PERSIST] Boot 1 | touches=0 | runtime=0 | ..."));
}

static void inject_corrupted_blob() {
    Preferences p;
    p.begin(NVS_NS, false);
    // Blob 20 bytes larger than sizeof(CreatureProfile) = 84 bytes
    uint8_t junk[sizeof(CreatureProfile) + 20];
    memset(junk, 0xFF, sizeof(junk));
    p.putBytes(NVS_KEY_PROFILE, junk, sizeof(junk));
    p.putUInt(NVS_KEY_SCHEMA, 1);
    p.end();
    Serial.println();
    Serial.println(F("=== P7 Test 3: Corrupted Blob ==="));
    Serial.printf("Injected: schema=1, profile=%u bytes (expected %u)\n",
                  sizeof(junk), sizeof(CreatureProfile));
    Serial.println(F("Action:   Power-cycle or re-upload AeroSniffer"));
    Serial.println(F("Expect:   [PERSIST] Factory reset -> profile cleared"));
    Serial.println(F("          [PERSIST] Boot 1 | touches=0 | runtime=0 | ..."));
}

static void show_state() {
    Preferences p;
    p.begin(NVS_NS, true);
    uint32_t sv = p.getUInt(NVS_KEY_SCHEMA, 0xFFFFFFFF);
    size_t   sz = p.getBytesLength(NVS_KEY_PROFILE);
    p.end();
    Serial.println();
    Serial.println(F("=== Current NVS State ==="));
    Serial.printf("  Namespace: %s\n",      NVS_NS);
    Serial.printf("  schema key: %u\n",     sv);
    Serial.printf("  profile blob: %u B\n", sz);
    Serial.printf("  expected:     %u B\n", sizeof(CreatureProfile));
}

static void inject_clean() {
    Preferences p;
    p.begin(NVS_NS, false);
    CreatureProfile profile = {};
    profile.schema_version = 1;
    profile.total_boots = 0;
    p.putBytes(NVS_KEY_PROFILE, &profile, sizeof(profile));
    p.putUInt(NVS_KEY_SCHEMA, 1);
    p.end();
    Serial.println(F("Injected: clean schema=1, zeroed profile"));
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    Serial.println(F("\n========================================="));
    Serial.println(F("  P7 Schema Recovery — NVS Injector"));
    Serial.println(F("  AeroSniffer V2.5 Certification Tool"));
    Serial.println(F("========================================="));

    show_state();

    Serial.println(F("\nSelect test:"));
    Serial.println(F("  1 — Old Schema (version=0)"));
    Serial.println(F("  2 — Future Schema (version=255)"));
    Serial.println(F("  3 — Corrupted Blob (wrong size)"));
    Serial.println(F("  s — Show current NVS state"));
    Serial.println(F("  r — Reset to clean state"));
    Serial.println();
    Serial.print(F("> "));
}

void loop() {
    if (!Serial.available()) return;
    char c = Serial.read();
    Serial.println(c);

    switch (c) {
        case '1': inject_old_schema();       break;
        case '2': inject_future_schema();    break;
        case '3': inject_corrupted_blob();   break;
        case 's': show_state();              break;
        case 'r': inject_clean();            break;
        default:
            Serial.println(F("Unknown.  1/2/3/s/r"));
            break;
    }
    Serial.print(F("\n> "));
}

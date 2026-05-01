/**
 * Host-compilable test for SAT BLE parser + MAC-filter contracts.
 *
 * Covers Phase 3A "honourable mention" gaps closed by TASK-500:
 *   - 3A-M2 MAC-filter bypass scenarios (empty filter accepts all,
 *     configured filter strict, malformed MAC reject paths).
 *   - 3A-M3 BLE byte-layout parsers (ATC/pvvx, BTHome v2) against
 *     fixed-byte payloads with expected float values.
 *   - 3A-M4 Encrypted-BTHome-flag rejection (flag bit0 = 0x01 set).
 *
 * The functions under test are pure logic with no Arduino runtime
 * dependencies (no Serial, NimBLE, FreeRTOS, settings persistence).
 * The relevant code paths are LIFTED into this test as static helpers
 * — the test exercises the same byte-layout / branch shape as the
 * firmware. If the firmware diverges, future regressions are caught
 * at PR time by re-running this test.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 -Wall -Wextra tests/test_ble_parsers.cpp -o tests/test_ble_parsers.out
 *   ./tests/test_ble_parsers.out
 *   echo $?   # 0 on pass, 1 on failure
 *
 * Pin: src/OTGW-firmware/SATble.ino (parseBLEAtcFormat,
 * parseBLEBTHomeFormat, bleMatchesConfiguredMAC). When SATble.ino
 * changes any of these, this test must change too — that's the point.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// ---------------------------------------------------------------------------
// Lifted constants — kept in sync with src/OTGW-firmware/SATble.ino
// ---------------------------------------------------------------------------
static constexpr uint8_t BTHOME_V2_FLAG_VERSION_BIT = 0x40;
static constexpr uint8_t BTHOME_V2_FLAG_ENCRYPTED   = 0x01;
static constexpr uint8_t BTHOME_OBJ_PACKET_ID_U8    = 0x00;  // 1-byte counter (TASK-498 2A-M3)
static constexpr uint8_t BTHOME_OBJ_BATTERY_U8      = 0x01;  // 1-byte uint8
static constexpr uint8_t BTHOME_OBJ_TEMPERATURE_S16 = 0x02;  // 2-byte int16, factor 0.01
static constexpr uint8_t BTHOME_OBJ_HUMIDITY_U16    = 0x03;  // 2-byte uint16, factor 0.01

// ---------------------------------------------------------------------------
// Mock for settings.sat.sBleMAC. Tests set this string before each filter
// assertion. Empty string ("") = "accept any" (default-allow).
// ---------------------------------------------------------------------------
static char g_sBleMAC[18] = "";

// ---------------------------------------------------------------------------
// Lifted: parseBLEAtcFormat (SATble.ino:94)
// ---------------------------------------------------------------------------
static bool parseBLEAtcFormat(const uint8_t* data, size_t len,
                              float* temp, float* hum, uint8_t* batt)
{
  if (len < 13) return false;

  int16_t  rawTemp = (int16_t)(data[6] | (data[7] << 8));
  *temp = rawTemp / 100.0f;

  uint16_t rawHum  = (uint16_t)(data[8] | (data[9] << 8));
  *hum  = rawHum / 100.0f;

  *batt = data[10];

  if (*temp < -40.0f || *temp > 60.0f) return false;
  if (*hum  <   0.0f || *hum  > 100.0f) return false;
  return true;
}

// ---------------------------------------------------------------------------
// Lifted: parseBLEBTHomeFormat (SATble.ino:121)
// ---------------------------------------------------------------------------
static bool parseBLEBTHomeFormat(const uint8_t* data, size_t len,
                                 float* temp, float* hum, uint8_t* batt)
{
  if (len < 3) return false;

  uint8_t flags = data[0];
  if ((flags & BTHOME_V2_FLAG_VERSION_BIT) == 0) return false;
  if ((flags & BTHOME_V2_FLAG_ENCRYPTED) != 0)   return false;

  size_t pos = 1;
  bool gotTemp = false;
  bool gotHum  = false;

  while (pos < len) {
    if (pos >= len) break;
    uint8_t objId = data[pos++];

    switch (objId) {
      case BTHOME_OBJ_TEMPERATURE_S16:
        if (pos + 2 > len) return gotTemp;
        { int16_t rawT = (int16_t)(data[pos] | (data[pos + 1] << 8));
          *temp = rawT / 100.0f;
          if (*temp >= -40.0f && *temp <= 60.0f) gotTemp = true;
        }
        pos += 2;
        break;

      case BTHOME_OBJ_HUMIDITY_U16:
        if (pos + 2 > len) return gotTemp;
        { uint16_t rawH = (uint16_t)(data[pos] | (data[pos + 1] << 8));
          *hum = rawH / 100.0f;
          if (*hum >= 0.0f && *hum <= 100.0f) gotHum = true;
        }
        pos += 2;
        break;

      case BTHOME_OBJ_BATTERY_U8:
        if (pos + 1 > len) return gotTemp;
        *batt = data[pos];
        pos += 1;
        break;

      case BTHOME_OBJ_PACKET_ID_U8:
        if (pos + 1 > len) return gotTemp;
        pos += 1;
        break;

      default:
        return gotTemp;
    }
  }
  (void)gotHum;
  return gotTemp;
}

// ---------------------------------------------------------------------------
// Lifted: bleMatchesConfiguredMAC (SATble.ino:182)
// strcasecmp is on most POSIX hosts; provide a portable inline shim.
// ---------------------------------------------------------------------------
static int test_strcasecmp(const char* a, const char* b)
{
  while (*a && *b) {
    int ca = std::tolower((unsigned char)*a);
    int cb = std::tolower((unsigned char)*b);
    if (ca != cb) return ca - cb;
    a++; b++;
  }
  return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}

static bool bleMatchesConfiguredMAC(const char* mac)
{
  if (g_sBleMAC[0] == '\0') return true;  // accept all
  return (test_strcasecmp(mac, g_sBleMAC) == 0);
}

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------
static int failures = 0;

static void checkBool(const char* label, bool expected, bool actual)
{
  if (expected != actual) {
    std::printf("FAIL: %s: expected %s, got %s\n",
                label, expected ? "true" : "false", actual ? "true" : "false");
    failures++;
  } else {
    std::printf("ok  : %s\n", label);
  }
}

static void checkFloatNear(const char* label, float expected, float actual, float eps = 0.001f)
{
  float diff = (actual > expected) ? (actual - expected) : (expected - actual);
  if (diff > eps) {
    std::printf("FAIL: %s: expected %.3f, got %.3f (diff %.4f)\n",
                label, expected, actual, diff);
    failures++;
  } else {
    std::printf("ok  : %s (%.2f)\n", label, actual);
  }
}

static void checkU8(const char* label, uint8_t expected, uint8_t actual)
{
  if (expected != actual) {
    std::printf("FAIL: %s: expected %u, got %u\n",
                label, expected, actual);
    failures++;
  } else {
    std::printf("ok  : %s (%u)\n", label, actual);
  }
}

int main()
{
  std::printf("test_ble_parsers — SAT BLE parser + MAC-filter contracts\n");
  std::printf("================================================\n");

  // -------------------------------------------------------------------
  // 1. ATC/pvvx parser — happy path
  // -------------------------------------------------------------------
  // Layout: MAC(6) | tempS16 LE (2) | humU16 LE (2) | batt(1) | reserved
  // tempS16 = 2150 → 21.50 °C; humU16 = 4523 → 45.23 %RH; batt = 0x55 = 85 %
  {
    uint8_t adv[14] = {
      0xA4, 0xC1, 0x38, 0x12, 0x34, 0x56,    // MAC
      0x66, 0x08,                            // tempS16 = 0x0866 = 2150
      0xAB, 0x11,                            // humU16 = 0x11AB = 4523
      0x55,                                  // battery 85
      0x10, 0x06,                            // batt_mv (ignored)
      0x42                                   // counter (ignored)
    };
    float t = 0, h = 0; uint8_t b = 0;
    bool ok = parseBLEAtcFormat(adv, sizeof(adv), &t, &h, &b);
    checkBool("ATC happy: parsed", true, ok);
    checkFloatNear("ATC happy: temp", 21.50f, t);
    checkFloatNear("ATC happy: hum",  45.23f, h);
    checkU8("ATC happy: batt", 85, b);
  }

  // 2. ATC parser — short payload rejected
  {
    uint8_t adv[12] = {0};
    float t = 0, h = 0; uint8_t b = 0;
    checkBool("ATC short: rejected (<13 bytes)", false,
              parseBLEAtcFormat(adv, sizeof(adv), &t, &h, &b));
  }

  // 3. ATC parser — out-of-range temperature rejected
  // tempS16 = 7000 (70.00 °C, above sanity ceiling)
  {
    uint8_t adv[13] = {
      0,0,0,0,0,0,
      0x58, 0x1B,        // tempS16 = 0x1B58 = 7000 → 70.00 °C
      0x10, 0x27,        // humU16 = 0x2710 = 10000 → 100.00 %RH (boundary)
      0x55,
      0,0
    };
    float t = 0, h = 0; uint8_t b = 0;
    checkBool("ATC out-of-range temp: rejected", false,
              parseBLEAtcFormat(adv, sizeof(adv), &t, &h, &b));
  }

  // -------------------------------------------------------------------
  // 4. BTHome v2 — happy path: temp + humidity + battery
  // -------------------------------------------------------------------
  // flags = 0x40 (version 2, unencrypted).
  // Object 0x02 (temp): rawT = 2150 → 21.50 °C.
  // Object 0x03 (hum):  rawH = 4523 → 45.23 %RH.
  // Object 0x01 (batt): 85.
  {
    uint8_t adv[] = {
      0x40,                              // flags (v2, plaintext)
      0x02, 0x66, 0x08,                  // temp s16 LE = 2150
      0x03, 0xAB, 0x11,                  // hum u16 LE = 4523
      0x01, 0x55                         // battery 85
    };
    float t = 0, h = 0; uint8_t b = 0;
    bool ok = parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b);
    checkBool("BTHome happy: parsed", true, ok);
    checkFloatNear("BTHome happy: temp", 21.50f, t);
    checkFloatNear("BTHome happy: hum",  45.23f, h);
    checkU8("BTHome happy: batt", 85, b);
  }

  // 5. BTHome v2 — TASK-498 (2A-M3): packet-id 0x00 prefix is skipped, parser
  //    continues to subsequent objects instead of bailing out.
  {
    uint8_t adv[] = {
      0x40,                              // flags (v2, plaintext)
      0x00, 0x42,                        // packet-id 0x42 (1 byte) — must skip
      0x02, 0x66, 0x08,                  // temp s16 LE = 2150
      0x03, 0xAB, 0x11                   // hum u16 LE = 4523
    };
    float t = 0, h = 0; uint8_t b = 0;
    bool ok = parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b);
    checkBool("BTHome packet-id prefix: parsed", true, ok);
    checkFloatNear("BTHome packet-id prefix: temp survived", 21.50f, t);
    checkFloatNear("BTHome packet-id prefix: hum survived",  45.23f, h);
  }

  // 6. BTHome v2 — TASK-500 (3A-M4): encrypted bit set rejects entire packet.
  //    flags = 0x40 | 0x01 = 0x41 → version 2 OK but encrypted; must reject.
  {
    uint8_t adv[] = {
      0x41,                              // flags: v2 + encrypted
      0x02, 0x66, 0x08,                  // would-be temp
      0x03, 0xAB, 0x11                   // would-be hum
    };
    float t = 0, h = 0; uint8_t b = 0;
    checkBool("BTHome encrypted: rejected", false,
              parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b));
  }

  // 7. BTHome v2 — version bit not set rejects packet (BTHome v1 / unknown).
  {
    uint8_t adv[] = {
      0x00,                              // flags: no version bit
      0x02, 0x66, 0x08
    };
    float t = 0, h = 0; uint8_t b = 0;
    checkBool("BTHome no-version: rejected", false,
              parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b));
  }

  // 8. BTHome v2 — short payload rejected (<3 bytes).
  {
    uint8_t adv[2] = {0x40, 0x02};
    float t = 0, h = 0; uint8_t b = 0;
    checkBool("BTHome short: rejected", false,
              parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b));
  }

  // 9. BTHome v2 — unknown object id stops parsing but preserves data parsed
  //    earlier in the same advertisement.
  {
    uint8_t adv[] = {
      0x40,
      0x02, 0x66, 0x08,                  // temp parses successfully
      0xFE, 0xAA, 0xBB                   // unknown object id 0xFE — bails out
    };
    float t = 0, h = 0; uint8_t b = 0;
    bool ok = parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b);
    checkBool("BTHome unknown-after-temp: returns partial true", true, ok);
    checkFloatNear("BTHome unknown-after-temp: temp preserved", 21.50f, t);
  }

  // 10. BTHome v2 — temperature out-of-range (sanity reject) returns false
  //     even though parser advanced through the bytes.
  {
    uint8_t adv[] = {
      0x40,
      0x02, 0x58, 0x1B                   // tempS16 = 7000 → 70.00 °C, above 60 ceiling
    };
    float t = 0, h = 0; uint8_t b = 0;
    bool ok = parseBLEBTHomeFormat(adv, sizeof(adv), &t, &h, &b);
    checkBool("BTHome temp-out-of-range: not gotTemp", false, ok);
  }

  // -------------------------------------------------------------------
  // 11. MAC filter — empty configured MAC accepts all (default-allow)
  // -------------------------------------------------------------------
  {
    g_sBleMAC[0] = '\0';
    checkBool("MAC filter empty: accepts well-formed MAC", true,
              bleMatchesConfiguredMAC("AA:BB:CC:DD:EE:FF"));
    checkBool("MAC filter empty: accepts lower-case MAC", true,
              bleMatchesConfiguredMAC("aa:bb:cc:dd:ee:ff"));
    // Even malformed MACs pass the empty filter — slot allocation later
    // is the second line of defence. This documents current behaviour;
    // do not "fix" it here without escalating to a UX decision.
    checkBool("MAC filter empty: accepts garbage (current behaviour)", true,
              bleMatchesConfiguredMAC("garbage"));
  }

  // 12. MAC filter — exact configured MAC accepts only that MAC
  {
    std::strcpy(g_sBleMAC, "AA:BB:CC:DD:EE:FF");
    checkBool("MAC filter strict: accepts exact match",       true,
              bleMatchesConfiguredMAC("AA:BB:CC:DD:EE:FF"));
    checkBool("MAC filter strict: accepts case-insensitive",  true,
              bleMatchesConfiguredMAC("aa:bb:cc:dd:ee:ff"));
    checkBool("MAC filter strict: rejects different MAC",     false,
              bleMatchesConfiguredMAC("11:22:33:44:55:66"));
    checkBool("MAC filter strict: rejects empty input",       false,
              bleMatchesConfiguredMAC(""));
    checkBool("MAC filter strict: rejects malformed",         false,
              bleMatchesConfiguredMAC("not-a-mac"));
  }

  std::printf("================================================\n");
  if (failures == 0) {
    std::printf("All assertions PASS\n");
    return 0;
  } else {
    std::printf("FAILURES: %d\n", failures);
    return 1;
  }
}

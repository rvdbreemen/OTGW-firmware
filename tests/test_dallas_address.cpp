/**
 * Host-compilable test for getDallasAddress()
 *
 * Verifies that the Dallas DS18B20 address conversion produces the expected
 * hex-string output. The function under test is pure byte-to-hex logic: no
 * Arduino runtime dependencies (no Serial, millis, delay, OneWire). Arduino
 * types/macros are stubbed below so the test compiles and runs on any host
 * toolchain (gcc, g++, clang++, MSVC).
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out
 *   ./tests/test_dallas_address.out
 *   echo $?   # 0 on pass, 1 on failure
 *
 * See tests/README.md for details.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

// ---- Arduino compatibility stubs (host-only) ----
// PROGMEM is a no-op on the host; flash and RAM are the same address space.
#ifndef PROGMEM
#define PROGMEM
#endif
// pgm_read_byte() on ESP8266 performs an aligned flash read. On the host
// the pointer is a regular RAM pointer, so a plain dereference is correct.
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#endif

// DeviceAddress mirrors the OneWire library typedef (8-byte Dallas ROM code).
typedef uint8_t DeviceAddress[8];

// ---- Function under test (copy of the firmware implementation) ----
static const char hexchars[] PROGMEM = "0123456789ABCDEF";

char* getDallasAddress(DeviceAddress deviceAddress)
{
  static char dest[17]; // 8 bytes * 2 chars + 1 null

  for (uint8_t i = 0; i < 8; i++)
  {
    uint8_t b = deviceAddress[i];
    dest[i*2]   = pgm_read_byte(&hexchars[b >> 4]);
    dest[i*2+1] = pgm_read_byte(&hexchars[b & 0x0F]);
  }
  dest[16] = '\0';
  return dest;
}

// ---- Tiny test harness ----
static int failures = 0;

static void check(const char* name, const char* expected, const char* got)
{
  bool ok = (std::strcmp(expected, got) == 0);
  std::printf("%-32s expected=%s got=%s %s\n",
              name, expected, got, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

int main()
{
  std::printf("=== Dallas Address Conversion Test ===\n");

  // Test 1: standard Dallas address
  DeviceAddress addr1 = {0x28, 0xFF, 0x64, 0x1E, 0x82, 0x16, 0xC3, 0xA1};
  check("standard address", "28FF641E8216C3A1", getDallasAddress(addr1));

  // Test 2: leading-zero bytes (exercises nibble zero-padding)
  DeviceAddress addr2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  check("leading zero bytes", "0102030405060708", getDallasAddress(addr2));

  // Test 3: all zeros
  DeviceAddress addr3 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  check("all zeros", "0000000000000000", getDallasAddress(addr3));

  // Test 4: all 0xFF
  DeviceAddress addr4 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const char* result4 = getDallasAddress(addr4);
  check("all 0xFF", "FFFFFFFFFFFFFFFF", result4);

  // Test 5: length is exactly 16 (buffer contract)
  {
    size_t len = std::strlen(result4);
    bool ok = (len == 16);
    std::printf("%-32s expected=%d got=%zu %s\n",
                "length check", 16, len, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }

  // Test 6: mixed nibbles + NUL terminator
  DeviceAddress addr6 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
  const char* result6 = getDallasAddress(addr6);
  check("mixed nibbles", "AABBCCDDEEFF1122", result6);
  {
    bool ok = (std::strlen(result6) == 16 && result6[16] == '\0');
    std::printf("%-32s %s\n", "NUL terminator at index 16", ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }

  std::printf("=== %s (failures=%d) ===\n",
              failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
              failures);
  return failures == 0 ? 0 : 1;
}

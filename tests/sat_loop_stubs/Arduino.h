// Host stand-in for the Arduino core (tests/test_sat_loop_integration.py). Provides the types, the pgmspace macros and the C library
// the real SAT headers include; no hardware, no Wire, no WiFi. The stub clock lives in sat_loop_stubs.h (millis()).
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <ctime>
#include <string>
#include <algorithm>
#include <type_traits>
typedef uint8_t byte;                 // Arduino typedef
typedef bool boolean;
typedef const char* PGM_P;
#define PROGMEM
#define PGM_P_DEFINED 1
#define PSTR(x) (x)
struct __FlashStringHelper;           // incomplete on purpose, as on the real core
#define F(x) (reinterpret_cast<const __FlashStringHelper*>(x))
#define snprintf_P snprintf
#define vsnprintf_P vsnprintf
#define strcmp_P strcmp
#define strncmp_P strncmp
#define strcasecmp_P _stricmp
#define strncasecmp_P _strnicmp
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strlen_P strlen
#define strcpy_P strcpy
#define strncpy_P strncpy
#define strstr_P strstr
#define memcpy_P memcpy
#define memcmp_P memcmp
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_float(a) (*(const float*)(a))
#define pgm_read_ptr(a) (*(const void* const*)(a))
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_NOINIT_ATTR
#define ICACHE_FLASH_ATTR
#define __attribute__(x)
static inline size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static inline size_t strlcat(char* d, const char* s, size_t n) { size_t dl = strlen(d); if (dl >= n) return n + strlen(s); return dl + strlcpy(d + dl, s, n - dl); }
using std::min;
using std::max;
#define constrain(a, lo, hi) ((a) < (lo) ? (lo) : ((a) > (hi) ? (hi) : (a)))
#ifndef PI
#define PI 3.14159265358979f
#endif
// The stub clock: the test drives it. millis() wraps like the real one (uint32).
static uint32_t g_ms = 0;
static inline unsigned long millis() { return g_ms; }
static inline long random(long a, long b) { return a + (rand() % (b - a > 0 ? b - a : 1)); }   // STUB: seeded pseudo-random, not the hardware RNG
static inline long random(long b) { return rand() % (b > 0 ? b : 1); }

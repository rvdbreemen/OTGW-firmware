/*
***************************************************************************
**  Program  : test/host/ha_disc_shim/pgmspace.h
**
**  Stand-in for <pgmspace.h> so test_ha_discovery_json.py can compile the
**  real MQTTHaDiscovery.cpp and MQTTstuff.h on the host (TASK-1202). The
**  macros follow cores/esp32/pgmspace.h of framework-arduinoespressif32:
**  PROGMEM is empty, PGM_P is a macro, every _P function is its RAM twin.
**  pgm_read_word / pgm_read_ptr are plain casts here because MSVC has no
**  typeof or statement expressions. strlcpy_P is NOT defined, as on the
**  device, so MQTTstuff.h compiles its own fallback. strlcpy, strlcat and
**  strcasecmp come from newlib on the device; MSVC lacks them, so they are
**  provided here. Platform surface only: no discovery code is reimplemented.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#define PROGMEM
#define PGM_P        const char *
#define PGM_VOID_P   const void *
#define PSTR(s)      (s)

#define pgm_read_byte(addr)  (*(const unsigned char *)(addr))
#define pgm_read_word(addr)  (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_ptr(addr)   (*(void *const *)(addr))

#define memcmp_P      memcmp
#define memcpy_P      memcpy
#define strcpy_P      strcpy
#define strncpy_P     strncpy
#define strcmp_P      strcmp
#define strncmp_P     strncmp
#define strcasecmp_P  strcasecmp
#define strlen_P      strlen
#define strstr_P      strstr
#define sprintf_P     sprintf
#define snprintf_P    snprintf

// newlib (the device C library) provides these three; MSVC does not.
inline size_t strlcpy(char *dst, const char *src, size_t size) {
  const size_t n = strlen(src);
  if (size) {
    const size_t c = (n >= size) ? size - 1 : n;
    memcpy(dst, src, c);
    dst[c] = '\0';
  }
  return n;
}
inline size_t strlcat(char *dst, const char *src, size_t size) {
  const size_t d = strnlen(dst, size);
  if (d == size) return size + strlen(src);
  return d + strlcpy(dst + d, src, size - d);
}
#define strcasecmp _stricmp

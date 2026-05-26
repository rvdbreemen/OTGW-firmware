/*
***************************************************************************
**  Program  : debugStuff.ino
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Debug-output machinery IMPLEMENTATION (ADR-079 split).
**  Companion header: debugStuff.h (macros + function declarations).
**  Data companion  : Debugtypes.h (state.debug trace toggles).
**
**  Two helpers live here so that only one translation unit instantiates
**  the function bodies (matches the project-wide stuff.h/.ino pattern
**  and removes the prior multi-definition fragility of Debug.h):
**
**    - _debugPrintf_P(fmt, ...)
**        PROGMEM-format-string helper. vsnprintf_P into a 256-byte stack
**        buffer, then debugTelnet.print(buf). Strings longer than 255
**        chars are silently truncated -- acceptable for debug output.
**
**    - _debugBOL(fn, line)
**        Beginning-of-line prefix: "HH:MM:SS.uuuuuu (heap|block) fn(line): ".
**        Caches the timezone object, timestamp, and heap stats for a full
**        second so high-volume flags (bMQTTGate) do not burn CPU on
**        ZonedDateTime conversions or free-list walks on every debug line.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

// SimpleTelnet inherits from Stream/Print but printf_P() is used here as a
// standalone helper for PROGMEM format strings via vsnprintf_P into a
// 256-byte stack buffer, then sent via debugTelnet.print().
// Debug strings that exceed 255 chars are silently truncated -- acceptable.
void _debugPrintf_P(PGM_P fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf_P(buf, sizeof(buf), fmt, args);
    va_end(args);
    debugTelnet.print(buf);
}

void _debugBOL(const char *fn, int line)
{
   static char _bol[160];  // Increased size + static for stack reduction

   // Cache timezone manager calls to avoid recreating objects
   static TimeZone cachedTz;
   static time_t lastTzUpdate = 0;
   static bool tzInitialized = false;

   // Per-second cache: timezone conversion + heap stats only change once/sec.
   // Avoids ~30-50 ZonedDateTime conversions and free-list walks/sec when
   // high-volume debug flags (bMQTTGate) are enabled.
   // Pre-formatted prefix avoids repeating integer-to-string work per call.
   static time_t lastCachedSec = 0;
   static char cachedPrefix[35] = "";  // "HH:MM:SS" + " ( heap| block) "

   time_t now_sec = time(nullptr);

   // Initialize timezone on first call or refresh every 5 minutes (300 seconds)
   // Check now_sec > 0 to ensure time is set
   if (now_sec > 0 && (!tzInitialized || now_sec - lastTzUpdate > 300)) {
     TimeZone newTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
     // Only update cache if timezone is valid
     if (!newTz.isError()) {
       cachedTz = newTz;
       lastTzUpdate = now_sec;
       tzInitialized = true;
     }
     // If timezone creation fails, keep using previous cached timezone
   }

   timeval now;
   gettimeofday(&now, nullptr);

   // If timezone not yet initialized, try to initialize it now (first call fallback)
   // This handles cases when time is not set yet (now_sec <= 0) or when primary initialization failed
   if (!tzInitialized) {
     cachedTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
     tzInitialized = true;  // Mark as initialized to avoid repeated attempts on every call
     // Note: Even if timezone creation fails, the error object is safe to use
   }

   // Refresh time decomposition and heap stats at most once per second.
   // ZonedDateTime::forUnixSeconds64() computes DST rules and UTC offset;
   // platformMaxFreeBlock() walks the entire free list -- both are too
   // expensive to run on every debug line under high-volume flags.
   if (now_sec != lastCachedSec) {
     ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, cachedTz);
     snprintf_P(cachedPrefix, sizeof(cachedPrefix), PSTR("%02d:%02d:%02d.%%06d (%7u|%6u) "),
              myTime.hour(), myTime.minute(), myTime.second(),
              platformFreeHeap(), platformMaxFreeBlock());
     lastCachedSec  = now_sec;
   }

   // Per-call: expand cached prefix (fills in tv_usec), append fn(line).
   // HH:MM:SS, heap, and maxblock are baked into cachedPrefix once/sec;
   // only microseconds, function name, and line number vary per call.
   // Note: the first snprintf() below cannot be snprintf_P — its format
   // argument is the runtime RAM buffer cachedPrefix, not a string literal,
   // so snprintf_P (which requires a PROGMEM format) does not apply here.
   int written = snprintf(_bol, sizeof(_bol), cachedPrefix, (int)now.tv_usec);
   written += snprintf_P(_bol + written, sizeof(_bol) - written,
                       PSTR("%-12.12s(%4d): "), fn, line);


   // Ensure null termination even if truncated
   if (written >= (int)sizeof(_bol)) {
       _bol[sizeof(_bol) - 1] = '\0';
   }

   debugTelnet.print(_bol);
}

void enableDebugForPrerelease() {
  state.debug.bRestAPI    = true;
  state.debug.bMQTT       = true;
  state.debug.bMQTTGate   = true;
  state.debug.bSensors    = true;
  state.debug.bSATBLE     = true;
  DebugTln(F("[prerelease] verbose debug enabled: restapi mqtt mqtt_gate sensors sat_ble"));
}

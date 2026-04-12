/* 
***************************************************************************  
**  Program  : Debug.h
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Met dank aan Willem Aandewiel en Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
** Modified: as OTGW actually uses the Serial interface, so no more debug to serial please.
*/

 #ifndef DEBUG_H
 #define DEBUG_H

/*---- start macro's ------------------------------------------------------------------*/


#define Debug(...)      ({ debugTelnet.print(__VA_ARGS__);    })
#define Debugln(...)    ({ debugTelnet.println(__VA_ARGS__);  })
#define Debugf(...)     ({ _debugPrintf_P(__VA_ARGS__);       })

#define DebugFlush()    ({ debugTelnet.flush(); })


#define DebugT(...)     ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debug(__VA_ARGS__);                 \
                        })
#define DebugTln(...)   ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugln(__VA_ARGS__);        \
                        })
#define DebugTf(...)    ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugf(__VA_ARGS__);                \
                        })

/*---- einde macro's ------------------------------------------------------------------*/

// Module-specific conditional debug macros (ADR-051: uses state.debug.* flags)
// Each .ino file defines its own set with a per-module flag and prefix.
// Pattern (intentionally duplicated per-file — Arduino single-TU, no conflict):
//
//   #define XxxDebugTln(...) ({ if (state.debug.bXxx) DebugTln(__VA_ARGS__); })
//   #define XxxDebugTf(...)  ({ if (state.debug.bXxx) DebugTf(__VA_ARGS__);  })
//   ... (Tln, ln, Tf, f, T, plain)
//
// Modules: OTGWDebug* (bOTmsg), MQTTDebug* (bMQTT), RESTDebug* (bRestAPI),
//          SensorDebug* (bSensors) — see each .ino file header.

// needs extern SimpleTelnet<1> debugTelnet;   // declared in OTGW-firmware.h, defined in networkStuff.ino

//#include <sys/time.h>
// #include <time.h>
// extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);


// SimpleTelnet inherits from Stream/Print but printf_P() is used here as a
// standalone helper for PROGMEM format strings via vsnprintf_P into a
// 256-byte stack buffer, then sent via debugTelnet.print().
// Debug strings that exceed 255 chars are silently truncated — acceptable.
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
   // ESP.getMaxFreeBlockSize() walks the entire free list — both are too
   // expensive to run on every debug line under high-volume flags.
   if (now_sec != lastCachedSec) {
     ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, cachedTz);
     snprintf(cachedPrefix, sizeof(cachedPrefix), "%02d:%02d:%02d.%%06d (%7u|%6u) ",
              myTime.hour(), myTime.minute(), myTime.second(),
              ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
     lastCachedSec  = now_sec;
   }

   // Per-call: expand cached prefix (fills in tv_usec), append fn(line).
   // HH:MM:SS, heap, and maxblock are baked into cachedPrefix once/sec;
   // only microseconds, function name, and line number vary per call.
   int written = snprintf(_bol, sizeof(_bol), cachedPrefix, (int)now.tv_usec);
   written += snprintf(_bol + written, sizeof(_bol) - written,
                       "%-12.12s(%4d): ", fn, line);

   // Ensure null termination even if truncated
   if (written >= (int)sizeof(_bol)) {
       _bol[sizeof(_bol) - 1] = '\0';
   }

   debugTelnet.print(_bol);
}
#endif

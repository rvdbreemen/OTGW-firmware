/* 
***************************************************************************  
**  Program  : Debug.h
**
**  Copyright (c) 2021-2024 Robert van den Breemen
**  Met dank aan Willem Aandewiel en Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
** Modified: as OTGW actually uses the Serial interface, so no more debug to serial please.
*/

 #ifndef DEBUG_H
 #define DEBUG_H

/*---- start macro's ------------------------------------------------------------------*/


#define Debug(...)      ({ TelnetStream.print(__VA_ARGS__);    })
#define Debugln(...)    ({ TelnetStream.println(__VA_ARGS__);  })
#define Debugf(...)     ({ TelnetStream.printf(__VA_ARGS__);   })

#define DebugFlush()    ({ TelnetStream.flush(); })


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

// needs #include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream

//#include <sys/time.h>
// #include <time.h>
// extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);


void _debugBOL(const char *fn, int line)
{
   static char _bol[160];  // Increased size + static for stack reduction
   
   // Cache timezone manager calls to avoid recreating objects
   static TimeZone cachedTz;
   static time_t lastTzUpdate = 0;
   static bool tzInitialized = false;
   time_t now_sec = time(nullptr);
   
   // Initialize timezone on first call or refresh every 5 minutes (300 seconds)
   // Check now_sec > 0 to ensure time is set
   if (now_sec > 0 && (!tzInitialized || now_sec - lastTzUpdate > 300)) {
     TimeZone newTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
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
   if (!tzInitialized) {
     cachedTz = timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
     tzInitialized = true;  // Set to true even if it fails to avoid repeated attempts
   }
   
   ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, cachedTz);
   
   // Use snprintf safely with return value checking
   int written = snprintf(_bol, sizeof(_bol), 
                 "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): ", 
                 myTime.hour(), myTime.minute(), myTime.second(), (int)now.tv_usec, 
                 ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),
                 fn, line);
   
   // Ensure null termination even if truncated
   if (written >= (int)sizeof(_bol)) {
       _bol[sizeof(_bol) - 1] = '\0';
   }
   
   TelnetStream.print(_bol);
}
#endif

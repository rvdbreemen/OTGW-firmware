/* 
***************************************************************************  
**  Program  : Debug.h
**
**  Copyright (c) 2021-2022 Robert van den Breemen
**  Met dank aan Willem Aandewiel en Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
** Modified: as OTGW actually uses the Serial interface, so no more debug to serial please.
*/

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

char _bol[128];
void _debugBOL(const char *fn, int line)
{
   // This commented out code is using mix of system time and acetime to print, but it will not work on microsecond level correctly
   // // //calculate fractional seconds to millis fraction
   // double fractional_seconds;
   // int microseconds;
   // struct timespec tp;   //to enable clock_gettime()  
   // clock_gettime(CLOCK_REALTIME, &tp); 
   // fractional_seconds = (double) tp.tv_nsec;
   // fractional_seconds /= 1e3;
   // fractional_seconds = round(fractional_seconds);
   // microseconds = (int) fractional_seconds;
     
   /* snprintf(_bol, sizeof(_bol), "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): ", \
                 hour(), minute(), second(), microseconds, \
                 ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),\
                 fn, line);
   */
                 
   //Alternative based on localtime function
   timeval now;
   //struct tm *tod;
   gettimeofday(&now, nullptr);
   //tod = localtime(&now.tv_sec);

   /*
   snprintf(_bol, sizeof(_bol), "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): ", \
                  tod->tm_hour, tod->tm_min, tod->tm_sec, (int)now.tv_usec, \
                  ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),\
                  fn, line);
   */

   TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settingNTPtimezone));
   ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
   
   //DebugTf("%02d:%02d:%02d %02d-%02d-%04d\r\n", myTime.hour(), myTime.minute(), myTime.second(), myTime.day(), myTime.month(), myTime.year());

   snprintf(_bol, sizeof(_bol), "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): ", \
                  myTime.hour(), myTime.minute(), myTime.second(), (int)now.tv_usec, \
                  ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),\
                  fn, line);

   TelnetStream.print (_bol);
}

/* 
***************************************************************************  
**  Program  : Debug.h
**
**  Copyright (c) 2021 Robert van den Breemen
**  Met dank aan Willem Aandewiel en Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
** Modified: as OTGW actually uses the Serial interface, so no more debug to serial please.
*/

/*---- sto wait
art macro's ------------------------------------------------------------------*/

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

char _bol[128];
void _debugBOL(const char *fn, int line)
{
   
  snprintf(_bol, sizeof(_bol), "[%02d:%02d:%02d][%7u|%6u] %-12.12s(%4d): ", \
                hour(), minute(), second(), \
                ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),\
                fn, line);
                 
  TelnetStream.print (_bol);
}

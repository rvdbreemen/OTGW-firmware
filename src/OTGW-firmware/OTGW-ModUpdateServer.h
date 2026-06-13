/*
***************************************************************************  
**  Program  : OTGW-MonUpdateServer.h
**  Modified to work with OTGW Nodoshop Hardware Watchdog
** 
**  This is the ESP8266HTTPUpdateServer.h file 
**  Created and modified by Ivan Grokhotkov, Miguel Angel Ajo, Earle Philhower and many others 
**  see: https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPUpdateServer
**
**  ... and then modified by Willem Aandewiel
**
** License and credits
** Arduino IDE is developed and maintained by the Arduino team. The IDE is licensed under GPL.
**
** ESP8266 core includes an xtensa gcc toolchain, which is also under GPL.
**
***************************************************************************
*/

#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

// ---- ESP32 implementation -------------------------------------------------
// ESP32-S3 is the only supported target; the update server is the ESP32
// WebServer-based implementation. (The original ESP8266 template lived here
// behind an #if defined(ESP8266) guard and was removed with the ESP8266 port.)

#include "OTGW-ModUpdateServer-esp32.h"

#endif // __HTTP_UPDATE_SERVER_H

/*
***************************************************************************
**  Program  : platform.h
**  Version  : v1.4.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Platform abstraction layer — all #ifdef ESP8266/ESP32 differences live
**  here (and in the two platform_*.h headers it includes).  Application
**  code includes only this header, never platform-specific ones directly.
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef PLATFORM_H
#define PLATFORM_H

#include <Arduino.h>

// ---- Platform selection ---------------------------------------------------
#if defined(ESP8266)
  #include "platform_esp8266.h"
#elif defined(ESP32)
  #include "platform_esp32.h"
#else
  #error "Unsupported platform — only ESP8266 and ESP32 are supported."
#endif

// ---- Common includes (identical API on both platforms) --------------------
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <WiFiClient.h>

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/

#endif // PLATFORM_H

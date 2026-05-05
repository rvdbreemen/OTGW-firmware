/*
***************************************************************************
**  Program  : platform.h
**  Version  : v2.0.0-alpha.2
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
#include <WiFiClient.h>

// ---- Unified directory iteration -----------------------------------------
// Wraps ESP8266 Dir and ESP32 File-based directory APIs into one interface.
class PlatformDir {
public:
  explicit PlatformDir(const char* path)
#if defined(ESP8266)
    : _dir(LittleFS.openDir(path)) {}
#elif defined(ESP32)
  {
    _dirFile = LittleFS.open(path);
  }
#endif

#if defined(ESP32)
  ~PlatformDir() {
    if (_entry) _entry.close();
    if (_dirFile) _dirFile.close();
  }
#endif

  bool valid() {
#if defined(ESP8266)
    return true;  // ESP8266 Dir is always valid; empty dir has no entries
#elif defined(ESP32)
    return (_dirFile && _dirFile.isDirectory());
#endif
  }

  bool next() {
#if defined(ESP8266)
    return _dir.next();
#elif defined(ESP32)
    if (_entry) _entry.close();  // close previous entry before opening next
    _entry = _dirFile.openNextFile();
    return (bool)_entry;
#endif
  }

  String fileName() {
#if defined(ESP8266)
    return _dir.fileName();
#elif defined(ESP32)
    // ESP32 File::name() returns full path; strip to basename for consistency
    const char* name = _entry.name();
    const char* slash = strrchr(name, '/');
    return String(slash ? slash + 1 : name);
#endif
  }

  size_t fileSize() {
#if defined(ESP8266)
    return _dir.fileSize();
#elif defined(ESP32)
    return _entry.size();
#endif
  }

  bool isDirectory() {
#if defined(ESP8266)
    return _dir.isDirectory();
#elif defined(ESP32)
    return _entry.isDirectory();
#endif
  }

private:
#if defined(ESP8266)
  Dir _dir;
#elif defined(ESP32)
  File _dirFile;
  File _entry;
#endif
};

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

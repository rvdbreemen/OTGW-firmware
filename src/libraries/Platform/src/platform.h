/*
***************************************************************************
**  Program  : platform.h
**  Version  : v2.0.0-alpha.137
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
#if defined(ESP32)
  #include "platform_esp32.h"
#else
  #error "Unsupported platform — only ESP32 is supported."
#endif

// ---- Integer-type model (TASK-745) ----------------------------------------
// On xtensa-esp32, int32_t is `long` — a DISTINCT type from `int` (both 32-bit),
// so sendJsonMapEntry(int) / (unsigned int) need their own overloads alongside
// the int32_t/uint32_t ones. jsonStuff.ino gates the extra overloads on this
// flag, keeping the platform conditional inside the abstraction layer. Always 1
// on ESP32 (the only supported target).
#define PLATFORM_INT_DISTINCT_FROM_INT32 1

// ---- Common includes (identical API on both platforms) --------------------
#include <WiFiUdp.h>
#include <WiFiClient.h>

// ---- Unified directory iteration -----------------------------------------
// Wraps the ESP32 File-based directory API into a small iteration interface.
class PlatformDir {
public:
  explicit PlatformDir(const char* path)
  {
    _dirFile = LittleFS.open(path);
  }

  ~PlatformDir() {
    if (_entry) _entry.close();
    if (_dirFile) _dirFile.close();
  }

  bool valid() {
    return (_dirFile && _dirFile.isDirectory());
  }

  bool next() {
    if (_entry) _entry.close();  // close previous entry before opening next
    _entry = _dirFile.openNextFile();
    return (bool)_entry;
  }

  String fileName() {
    // ESP32 File::name() returns full path; strip to basename for consistency
    const char* name = _entry.name();
    const char* slash = strrchr(name, '/');
    return String(slash ? slash + 1 : name);
  }

  size_t fileSize() {
    return _entry.size();
  }

  bool isDirectory() {
    return _entry.isDirectory();
  }

private:
  File _dirFile;
  File _entry;
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

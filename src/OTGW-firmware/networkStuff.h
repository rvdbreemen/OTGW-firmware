/*
***************************************************************************
**  Program : networkStuff.h
**  Version  : v2.0.0-alpha.304
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
**  Usage:
**      setup() {
**        startTelnet();
**        startWiFi(_HOSTNAME, 240);  // timeout 4 minuten
**        startMDNS(_HOSTNAME);
**        setupFSexplorer();   // registers routes on AsyncWebServer `server`
**        startWebserver();    // server.begin()
**      }
**      loop() {
**        handleWiFi();
**        MDNS.update();
**        // No handleClient() — HTTP serves on the AsyncTCP task.
**      }
**
**  Implementations live in networkStuff.ino.
*/

#ifndef NETWORKSTUFF_H
#define NETWORKSTUFF_H

#include <platform.h>           // Unified ESP8266/ESP32 abstraction layer
#include "webServerCompat.h"        // ESPAsyncWebServer bridge (TASK-865.9): AsyncWebServer server(80)
#include "OTGW-ModUpdateServer.h"   // <<special version for Nodoshop Watchdog needed>>
#include "updateServerHtml.h"
// Disable WiFiManager debug: the debug block in handleWifiSave() builds Strings
// from raw string literals stored in ESP8266 flash at potentially misaligned
// addresses. strlen() then generates an unaligned word-load → Exception 3
// (LoadStoreAlignmentCause). WM_NODEBUG undefines WM_DEBUG_LEVEL so those
// #ifdef WM_DEBUG_LEVEL blocks are excluded at compile time.
#define WM_NODEBUG
#include <WiFiManager.h>        // version 2.0.4-beta

/*
 * WebSocket log viewer
 * --------------------
 * The firmware exposes an AsyncWebSocket endpoint at ws://<host>/ws (TASK-865.10,
 * ADR-123 Phase 3) attached to the shared port-80 AsyncWebServer. It streams
 * OpenTherm Gateway log messages to the Web UI: the browser connects and receives
 * a live feed of OTGW traffic without polling. The endpoint and its global
 * (otLogWs) live in webSocketStuff.ino.
 *
 * Security considerations:
 * - The WebSocket log stream is intended for LOCAL NETWORK USE ONLY.
 * - There is no authentication on the WebSocket endpoint.
 * - Do NOT expose the OTGW HTTP/WebSocket port directly to the internet.
 */

//=====[ Types ]===============================================================

enum NtpStatus_t {
  TIME_NOTSET,
  TIME_SYNC,
  TIME_WAITFORSYNC,
  TIME_NEEDSYNC
};

//=====[ Constants ]===========================================================

static const time_t EPOCH_2000_01_01 = 946684800;
// Upper sanity bound: ESP8266 SDK initialises time() to 0xFFFFFFFF before
// SNTP sync. That value (year 2106) is > EPOCH_2000_01_01, so the old check
// incorrectly treated it as a valid synced time. INT32_MAX (2038-01-19) is
// safely below 0xFFFFFFFF and above any realistic current date.
static const time_t EPOCH_2038_01_19 = 2147483647UL;

//=====[ Extern variable declarations ]========================================
// Defined in networkStuff.ino

extern NtpStatus_t       NtpStatus;
extern time_t            NtpLastSync;
// AsyncWebServer server(80) is declared in webServerCompat.h (extern) and
// defined in networkStuff.ino. seq10 (WebSocket) and seq11 (OTA) attach to it.
extern OTGWUpdateServer  httpUpdater;
extern FSInfo            LittleFSinfo;
extern bool              LittleFSmounted;
#define WM_DEBUG_PORT debugTelnet

//=====[ Forward declarations ]================================================

void feedWatchDog();   // defined in OTGW-firmware.ino

void startWiFi(const char* hostname, int timeOut, bool forcePortal = false);
void resetWiFiSettings(void);
void startTelnet();
void startMDNS(const char *hostname);
void startLLMNR(const char *hostname);
void startNTP();
void loopNTP();
bool isNTPtimeSet();
const char* getMacAddress();
const char* getUniqueId();
bool checkHttpAuth();

#endif // NETWORKSTUFF_H

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

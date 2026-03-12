/*
***************************************************************************
**  Program : networkStuff.h
**  Version  : v1.3.0-beta
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
**        httpServer.on("/index", <sendIndexPage>);
**        httpServer.begin();
**      }
**      loop() {
**        handleWiFi();
**        MDNS.update();
**        httpServer.handleClient();
**      }
**
**  Implementations live in networkStuff.ino.
*/

#ifndef NETWORKSTUFF_H
#define NETWORKSTUFF_H

#include <ESP8266WiFi.h>        // ESP8266 Core WiFi Library
#include <ESP8266WebServer.h>   // Version 1.0.0 - part of ESP8266 Core
#include <ESP8266mDNS.h>        // part of ESP8266 Core
#include <ESP8266HTTPClient.h>
#include <ESP8266LLMNR.h>

#include <WiFiUdp.h>            // part of ESP8266 Core
#include <LittleFS.h>
#include "OTGW-ModUpdateServer.h"   // <<special version for Nodoshop Watchdog needed>>
#include "updateServerHtml.h"
#include <WiFiManager.h>        // version 2.0.4-beta

// Optimize WebSocket memory usage: reduce per-client buffer from 512 to 256 bytes
// This must be defined BEFORE including WebSocketsServer.h
// Saves ~256 bytes per client (768 bytes with 3 clients)
#define WEBSOCKETS_MAX_DATA_SIZE 256

#include <WebSocketsServer.h>   // WebSocket server for streaming OT log messages to WebUI

/*
 * WebSocket log viewer
 * --------------------
 * The firmware exposes a WebSocket endpoint that streams OpenTherm Gateway log
 * messages to the Web UI. The browser connects to this WebSocket and receives
 * a live feed of OTGW traffic without polling.
 *
 * Security considerations:
 * - The WebSocket log stream is intended for LOCAL NETWORK USE ONLY.
 * - There is no authentication on the WebSocket endpoint.
 * - Do NOT expose the OTGW HTTP/WebSocket ports directly to the internet.
 */

extern "C" int clock_gettime(clockid_t unused, struct timespec *tp);

//=====[ Types ]===============================================================

enum NtpStatus_t {
  TIME_NOTSET,
  TIME_SYNC,
  TIME_WAITFORSYNC,
  TIME_NEEDSYNC
};

//=====[ Constants ]===========================================================

static const time_t EPOCH_2000_01_01 = 946684800;

//=====[ Extern variable declarations ]========================================
// Defined in networkStuff.ino

extern NtpStatus_t       NtpStatus;
extern time_t            NtpLastSync;
extern ESP8266WebServer  httpServer;
extern ESP8266HTTPUpdateServer httpUpdater;
extern FSInfo            LittleFSinfo;
extern bool              LittleFSmounted;
extern bool              isConnected;

#define WM_DEBUG_PORT TelnetStream

//=====[ Forward declarations ]================================================

void feedWatchDog();   // defined in OTGW-firmware.ino

void startWiFi(const char* hostname, int timeOut);
void resetWiFiSettings(void);
void startTelnet();
void startMDNS(const char *hostname);
void startLLMNR(const char *hostname);
void startNTP();
void getNTPtime();
void loopNTP();
bool isNTPtimeSet();
const char* getMacAddress();
const char* getUniqueId();

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

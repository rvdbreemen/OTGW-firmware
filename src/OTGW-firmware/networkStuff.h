/*
***************************************************************************
**  Program : networkStuff.h
**  Version  : v1.6.0-beta.10
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

// Note: user_interface.h was previously included for wifi_station_dhcpc_stop/start.
// Those SDK calls were removed (TASK-432) because they take DHCP ownership away
// from the SDK and break subsequent setAutoReconnect-driven DHCP behaviour.
// See networkStuff.ino loopWifi() WIFI_DISCONNECTED for the rationale.

#include <WiFiUdp.h>            // part of ESP8266 Core
#include <LittleFS.h>
#include "OTGW-ModUpdateServer.h"   // <<special version for Nodoshop Watchdog needed>>
#include "updateServerHtml.h"
#include <WiFiManager.h>        // version 2.0.17

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
// Upper sanity bound: ESP8266 SDK initialises time() to 0xFFFFFFFF before
// SNTP sync. That value (year 2106) is > EPOCH_2000_01_01, so the old check
// incorrectly treated it as a valid synced time. INT32_MAX (2038-01-19) is
// safely below 0xFFFFFFFF and above any realistic current date.
static const time_t EPOCH_2038_01_19 = 2147483647UL;

//=====[ Extern variable declarations ]========================================
// Defined in networkStuff.ino

extern NtpStatus_t       NtpStatus;
extern time_t            NtpLastSync;
extern ESP8266WebServer  httpServer;
extern ESP8266HTTPUpdateServer httpUpdater;
extern FSInfo            LittleFSinfo;
extern bool              LittleFSmounted;
#define WM_DEBUG_PORT debugTelnet

//=====[ Forward declarations ]================================================

void feedWatchDog();   // defined in OTGW-firmware.ino

void startWiFi(const char* hostname, int timeOut, bool forcePortal = false);
void resetWiFiSettings(void);
void refreshServicesAfterWifiReconnect();
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

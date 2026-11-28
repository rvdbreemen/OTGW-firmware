/* 
***************************************************************************  
**  Program  : webSocketStuff.ino
**  Version  : v2.0.0-beta.487
**
**  Copyright (c) 2021-2025 Robert van den Breemen
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.                                                            
***************************************************************************      
**  WebSocket handler for streaming OpenTherm log messages to WebUI
**
**  This module provides real-time streaming of OT log messages to the WebUI
**  using WebSockets. Simplified version with direct text broadcasting.
**
**  Features:
**  - AsyncWebSocket attached to the shared port-80 AsyncWebServer at path /ws
**    (TASK-865.10, ADR-123 Phase 3): no separate TCP listener, no dedicated port, no loop poll
**  - Broadcasts log messages directly to all connected clients
**  - Minimal memory footprint
**  - Auto-cleanup of disconnected clients (cleanupClients() from a periodic timer)
**
**  Security:
**  - The WebSocket endpoint is UNAUTHENTICATED: any client on the reachable
**    network can connect to ws://<host>/ws and receive OpenTherm log messages.
**  - This is intended for use ONLY on trusted local networks. Do NOT expose
**    the /ws endpoint directly to the internet or untrusted networks.
**  - OpenTherm logs may reveal details about your heating system usage and
**    configuration. Protect network access accordingly (firewall, VLAN, etc.).
***************************************************************************
*/

#include <TelnetStream.h>
#include "debugStuff.h"
#include "webServerCompat.h"   // extern AsyncWebServer server (port 80, TASK-865.9)

// AsyncWebSocket live-log endpoint at path /ws on the shared port-80 server.
// AsyncWebSocket is NOT a server: startWebSocket() attaches it via
// server.addHandler(&otLogWs). No built-in authentication; local network use only.
AsyncWebSocket otLogWs("/ws");
// TASK-3119: defined further down (next to wsNoteSendStatus); the send sites above them need the names first.
static void wsNoteSendStatus(AsyncWebSocket::SendStatus st);
static AsyncWebSocket::SendStatus wsTextAllNoThrow(const char* msg);

// Close wrapper for prepareForReboot() in helperStuff.ino. The otLogWs global
// is defined here (not extern'd in a header) because it needs ESPAsyncWebServer
// visibility; wrapping the call keeps helperStuff.ino free of that include chain.
// Same pattern as doMqttDisconnect() in MQTTstuff.ino.
void doWebSocketClose() {
  otLogWs.closeAll();
}

// emergencyHeapRecovery() action 1 (helperStuff.ino), ADR-295 (partly supersedes ADR-107 action 1, TASK-3025): the
// /ws connections are aborted in lwIP (RST, their send queues freed at once) instead of a WebSocket close, whose close
// frame may never leave at CRITICAL heap. Browsers reconnect on their own (v2.js connectWs retries after 5 s).
// Returns the number of connections aborted.
uint8_t doWebSocketDisconnectAll() {
  const uint8_t n = webSocketAbortAll();
  state.heapdiag.iWsRecoveryAborts += n;
  return n;
}

// Maximum number of simultaneous WebSocket clients
// Rationale: Each client uses ~700 bytes (256 byte buffer + overhead)
// Limiting to 3 clients prevents heap exhaustion (3 × 700 = 2100 bytes max)
#define MAX_WEBSOCKET_CLIENTS 3

// Track WebSocket initialization state. Also enforces startWebSocket()
// idempotency: the handler is attached to the persistent port-80 server exactly
// once, even though startWebSocket() is called on every WiFi/Ethernet transition.
static bool wsInitialized = false;

// Application-level keepalive tracking
static unsigned long lastKeepaliveMs = 0;
const unsigned long KEEPALIVE_INTERVAL_MS = 30000; // 30 seconds

// TASK-3017 (AC3 run, 2026-11-21): a /ws client can leave WS_CONNECTED while its TCP connection stays open
// (WS_DISCONNECTING with a close frame that never leaves). count() then skips it, so nothing is sent to it and
// cleanupClients() never removes it (it only deletes clients whose TCP is gone, AsyncWebSocket.cpp:1072-1085); a
// reader that only reads waits forever. Every accepted /ws connection gets AsyncTCP's rx timeout: AsyncTCP closes
// the connection on its own task when for WS_RX_TIMEOUT_S nothing arrived AND nothing we sent was acknowledged
// (AsyncClient::_poll, AsyncTCP.cpp:1106-1108; an ACK of our data resets the timer too, :1049). A healthy client
// acknowledges at least the 30 s keepalive, so only a client that receives nothing (stuck outside WS_CONNECTED),
// stopped reading, or vanished without a FIN is closed. The library then removes it as for any disconnect.
// The /ws connection also gets the two rules every other port-80 connection gets in webBeginRequest()
// (webServerCompat.h), which the /ws upgrade does not pass through: ADR-216, its lwIP send queue capped at 2 x MSS
// (WEB_TCP_SEND_CAP), and ADR-189, an ack timeout (5 s, CONFIG_ASYNC_TCP_MAX_ACK_TIME) aborts the connection with a
// RST instead of the library's graceful close (AsyncWebSocketClient::_onTimeout -> AsyncClient::close(), which queues
// the FIN behind the unacked data, so the pcb kept its queue and kept retransmitting). 09's baseline on 56dd422: one
// half-open /ws client stalled a fresh healthy one; the abort frees its queue at once.
// The table below is only for device/info and the disconnect line; it is written on the async_tcp task (the /ws
// events and the REST handler run there).
// --- wsTrack begin
static const uint32_t WS_RX_TIMEOUT_S = 70;   // more than two keepalive intervals
#define WS_TRACK_SLOTS (MAX_WEBSOCKET_CLIENTS + 1)
struct WsTrackRow { uint32_t id; uint32_t connectMs; uint8_t seenStatus; uint32_t seenSinceMs; uint32_t remoteIp; uint16_t remotePort; };
static WsTrackRow wsTrack[WS_TRACK_SLOTS];

static const char* wsStatusName(uint8_t st) {
  switch (st) {
    case WS_CONNECTED:     return "connected";
    case WS_DISCONNECTING: return "disconnecting";
    case WS_DISCONNECTED:  return "disconnected";
    case 0xFE:             return "not_connected";   // WS_STATUS_NOT_CONNECTED: tracked, not returned by client(id)
    default:               return "unknown";
  }
}

static void wsTrackConnect(AsyncWebSocketClient* client, uint32_t now) {
  AsyncClient* tcp = client->client();
  if (tcp != nullptr) {
    tcp->setRxTimeout(WS_RX_TIMEOUT_S);                         // case B: nothing sent, so no ack timeout runs
    platformTcpCapSendBuffer(tcp->pcb(), WEB_TCP_SEND_CAP);     // ADR-216
    tcp->onTimeout([](void*, AsyncClient* c, uint32_t) {        // ADR-189: replaces the library's graceful close
      state.heapdiag.iWsAckAbortCount++;
      c->abort();
    }, nullptr);
  }
  for (uint8_t i = 0; i < WS_TRACK_SLOTS; i++) {
    if (wsTrack[i].id == 0) {
      const IPAddress ip = client->remoteIP();   // TASK-3025: the 4-tuple ADR-295's abort matches on
      wsTrack[i] = { client->id(), now, (uint8_t)WS_CONNECTED, now, (uint32_t)ip, client->remotePort() };
      return;
    }
  }
}

// Returns the row's data for the disconnect line; false when the client was never tracked (a rejected one).
static bool wsTrackDisconnect(uint32_t id, uint32_t now, uint32_t* lifeS, uint8_t* lastStatus, uint32_t* lastAgeS) {
  for (uint8_t i = 0; i < WS_TRACK_SLOTS; i++) {
    if (wsTrack[i].id == id) {
      *lifeS      = (now - wsTrack[i].connectMs) / 1000UL;
      *lastStatus = wsTrack[i].seenStatus;
      *lastAgeS   = (now - wsTrack[i].seenSinceMs) / 1000UL;
      wsTrack[i].id = 0;
      return true;
    }
  }
  return false;
}

// device/info (async_tcp): one row per tracked /ws client. The library's locked client(id) lookup returns only a client
// in WS_CONNECTED (AsyncWebSocket.cpp:1043-1053), so a tracked client it does not return is still in the list (no
// disconnect event yet) but outside WS_CONNECTED: shown as "not_connected" without a queue length. That is the state
// of the AC3 stall. status_age_s counts from the first time this read (or the connect) saw the current status.
static const uint8_t WS_STATUS_NOT_CONNECTED = 0xFE;
uint8_t webSocketSnapshotClients(WsClientSnap* out, uint8_t max, uint32_t now) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < WS_TRACK_SLOTS && n < max; i++) {
    if (wsTrack[i].id == 0) continue;
    AsyncWebSocketClient* c = otLogWs.client(wsTrack[i].id);
    const uint8_t st = (c != nullptr) ? (uint8_t)c->status() : WS_STATUS_NOT_CONNECTED;
    if (st != wsTrack[i].seenStatus) { wsTrack[i].seenStatus = st; wsTrack[i].seenSinceMs = now; }
    out[n].id         = wsTrack[i].id;
    out[n].status     = (c != nullptr) ? wsStatusName(st) : "not_connected";
    out[n].queueLen   = (c != nullptr) ? (uint16_t)c->queueLen() : (uint16_t)0xFFFF;
    out[n].statusAgeS = (now - wsTrack[i].seenSinceMs) / 1000UL;
    out[n].ageS       = (now - wsTrack[i].connectMs) / 1000UL;
    n++;
  }
  return n;
}
// TASK-3025 (ADR-295): the /ws connections ADR-107's recovery aborts: local port 80 (the port-80 server /ws runs on)
// with the remote address and port of a tracked client. Runs under the lwIP core lock inside platformTcpAbortWhere()
// on the loop task. The rows are plain words written on async_tcp and read here without a lock: a row being written at
// that moment is missed, or can match a lingering old pcb of the same slot, which is an already dead /ws connection and
// harmless (lwIP keeps one pcb per 4-tuple). Only IPv4 clients are matched: remoteIp is the IPv4 word, so an IPv6 /ws
// client is not aborted here (missed, never a wrong match).
static const uint16_t WS_LOCAL_PORT = 80;
static bool wsAbortMatch(uint16_t localPort, uint32_t remoteIp, uint16_t remotePort) {
  if (localPort != WS_LOCAL_PORT) return false;
  for (uint8_t i = 0; i < WS_TRACK_SLOTS; i++) {
    if (wsTrack[i].id != 0 && wsTrack[i].remoteIp == remoteIp && wsTrack[i].remotePort == remotePort) return true;
  }
  return false;
}

uint8_t webSocketAbortAll() {
  return platformTcpAbortWhere(wsAbortMatch);
}

// Tracked /ws connections, connected or not: count() sees only WS_CONNECTED ones, so a client stuck outside it
// (TASK-3017) would be skipped by a recovery gated on hasWebSocketClients().
uint8_t webSocketTrackedCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < WS_TRACK_SLOTS; i++) if (wsTrack[i].id != 0) n++;
  return n;
}
// --- wsTrack end

static const uint8_t WS_BURST_CONNECTED = 1;
static const uint8_t WS_BURST_DISCONNECTED = 2;
static const uint8_t WS_BURST_REJECTED_MAX = 3;
static const uint8_t WS_BURST_REJECTED_HEAP = 4;
static const uint8_t WS_BURST_ERROR = 5;
static const uint8_t WS_BURST_REJECTED_COOLDOWN = 6;   // TASK-3123

static unsigned long wsBurstWindowStartMs = 0;
static uint8_t wsBurstConnects = 0;
static uint8_t wsBurstDisconnects = 0;
static uint8_t wsBurstMaxRejects = 0;
static uint8_t wsBurstHeapRejects = 0;
static uint8_t wsBurstErrors = 0;
static uint8_t wsBurstCooldownRejects = 0;   // TASK-3123
static bool wsBurstLogged = false;
const unsigned long WS_BURST_WINDOW_MS = 5000;
const uint8_t WS_BURST_LOG_THRESHOLD = 3;

static void resetWebSocketBurstWindow(unsigned long now) {
  wsBurstWindowStartMs = now;
  wsBurstConnects = 0;
  wsBurstDisconnects = 0;
  wsBurstMaxRejects = 0;
  wsBurstHeapRejects = 0;
  wsBurstErrors = 0;
  wsBurstCooldownRejects = 0;
  wsBurstLogged = false;
}

static void noteWebSocketBurstEvent(uint8_t eventType) {
  const unsigned long now = millis();
  if ((wsBurstWindowStartMs == 0) || ((now - wsBurstWindowStartMs) > WS_BURST_WINDOW_MS)) {
    resetWebSocketBurstWindow(now);
  }

  switch (eventType) {
    case WS_BURST_CONNECTED:
      wsBurstConnects++;
      break;
    case WS_BURST_DISCONNECTED:
      wsBurstDisconnects++;
      break;
    case WS_BURST_REJECTED_MAX:
      wsBurstMaxRejects++;
      break;
    case WS_BURST_REJECTED_HEAP:
      wsBurstHeapRejects++;
      break;
    case WS_BURST_ERROR:
      wsBurstErrors++;
      break;
    case WS_BURST_REJECTED_COOLDOWN:
      wsBurstCooldownRejects++;
      break;
  }

  const uint8_t totalEvents = wsBurstConnects + wsBurstDisconnects + wsBurstMaxRejects + wsBurstHeapRejects + wsBurstErrors + wsBurstCooldownRejects;
  if (!wsBurstLogged && (totalEvents >= WS_BURST_LOG_THRESHOLD)) {
    DebugTf(PSTR("[%lu] WebSocket burst window=%lums total=%u conn=%u disc=%u rejMax=%u rejHeap=%u err=%u clients=%u heap=%u maxBlk=%u\r\n"),
            now,
            WS_BURST_WINDOW_MS,
            totalEvents,
            wsBurstConnects,
            wsBurstDisconnects,
            wsBurstMaxRejects,
            wsBurstHeapRejects,
            wsBurstErrors,
            (unsigned)otLogWs.count(),
            platformFreeHeap(),
            platformMaxFreeBlock());
    wsBurstLogged = true;
  }
}

bool hasWebSocketClients() {
  return wsInitialized && (otLogWs.count() > 0);
}

// Connected /ws clients. They share port 80 with the HTTP server, so the web
// connection cap (ADR-188) subtracts them from its port-80 count.
uint16_t webSocketClientCount() {
  return wsInitialized ? (uint16_t)otLogWs.count() : 0;
}

//===========================================================================================
// Handshake gate (TASK-3119)
//===========================================================================================
// AsyncWebSocket builds the client object (AsyncWebSocketClient, with a std::recursive_mutex whose constructor calls
// pthread_mutex_init(): a malloc plus a FreeRTOS semaphore) BEFORE it fires WS_EVT_CONNECT. Under a critical heap that
// construction can fail silently and the first lock of the unwritten mutex panics async_tcp (bench 2026-11-27, K hold,
// tabs reconnecting every 5 s: close() in the 'Low heap' branch below locked it). So a connection that would be refused
// anyway is refused here, in AsyncWebSocket::handleRequest(), before the 101 response and before the client exists
// (the library answers 401, the web UI reconnects after 5 s). The branches in webSocketEvent() stay as the second line:
// the heap can still fall between the handshake and the client construction, one round trip later.
// connected = otLogWs.count() at the handshake: the new client is not in the list yet, so a full list is count >= MAX.
// TASK-3123: after an emergency recovery that aborted /ws clients the tabs reconnect after 5 s (v2.js), and on a unit without PSRAM that
// drove the heap under 3 KB again within about 50 s, so the next recovery aborted them again (bench .76, B1 run 2). No handshake is
// admitted for WS_RECOVERY_COOLDOWN_MS after such a recovery. lastRecoveryMs is millis() of that recovery, 0 = none yet; the unsigned
// difference survives the wrap of millis().
static const uint32_t WS_RECOVERY_COOLDOWN_MS = 30000;
static uint32_t g_wsRecoveryMs = 0;
void webSocketNoteRecoveryAbort() {   // called by emergencyHeapRecovery() (helperStuff.ino) when it aborted at least one /ws client
  const uint32_t t = millis();
  g_wsRecoveryMs = t ? t : 1;
}
static uint32_t wsCooldownLeftMs(uint32_t nowMs, uint32_t lastRecoveryMs) {
  if (lastRecoveryMs == 0) return 0;
  const uint32_t since = (uint32_t)(nowMs - lastRecoveryMs);
  return since < WS_RECOVERY_COOLDOWN_MS ? (WS_RECOVERY_COOLDOWN_MS - since) : 0;
}
static bool wsAdmitHandshake(uint32_t freeHeap, uint16_t connected, uint32_t nowMs, uint32_t lastRecoveryMs) {
  if (wsCooldownLeftMs(nowMs, lastRecoveryMs) > 0) return false;
  if (freeHeap < HEAP_WARNING_THRESHOLD) return false;
  if (connected >= MAX_WEBSOCKET_CLIENTS) return false;
  return true;
}

// Lane A bench arms (prerelease builds only, RAM only, both on at boot): telnet keys m (wscool) and t (wsabort), REST names wscool and wsabort.
// A release build has no switch: it runs what WS_RELEASE_COOLDOWN and WS_RELEASE_ABORT say, one constant per measure. The rule was: an arm
// is on by default only if it is no worse than base on stall length and panics. Lane A matrix (TASK-3123 AC5, .76, no PSRAM, n = 5 per arm, 20
// holds, 0 panics): base dead median 159.2 s (123-273), cooldown 120.1 s (116-152) and no alloc fail: ON; abort alone median 120.3 s but one hold
// of 304.5 s, the longest of all: OFF until n >= 20; both not better than base (148.9 s): OFF. With a switch off the handler behaves exactly
// like the handler of the send-guard build 3f9176298 (tests/test_ws_handshake_lowmem_arms.py compares the two on a grid, byte for byte).
static const bool WS_RELEASE_COOLDOWN = true;
static const bool WS_RELEASE_ABORT = false;
static bool wsCooldownArmed() {
#if defined(_VERSION_PRERELEASE)
  return state.debug.bWsCooldown;
#else
  return WS_RELEASE_COOLDOWN;
#endif
}
static bool wsAbortArmed() {
#if defined(_VERSION_PRERELEASE)
  return state.debug.bWsAbort;
#else
  return WS_RELEASE_ABORT;
#endif
}
static bool wsHandshakeHandler(AsyncWebServerRequest* request) {
  const uint32_t freeHeap = platformFreeHeap();
  const uint16_t connected = (uint16_t)otLogWs.count();
  const uint32_t now = millis();
  const uint32_t lastRec = wsCooldownArmed() ? g_wsRecoveryMs : 0;
  if (wsAdmitHandshake(freeHeap, connected, now, lastRec)) return true;
  const uint32_t leftMs = wsCooldownLeftMs(now, lastRec);
  if (leftMs > 0) {
    noteWebSocketBurstEvent(WS_BURST_REJECTED_COOLDOWN);
    DebugTf(PSTR("[%lu] WebSocket: refused at the handshake (recovery cooldown, %lu s left)\r\n"),
            (unsigned long)now, (unsigned long)((leftMs + 999UL) / 1000UL));
  } else {
    noteWebSocketBurstEvent(freeHeap < HEAP_WARNING_THRESHOLD ? WS_BURST_REJECTED_HEAP : WS_BURST_REJECTED_MAX);
    DebugTf(PSTR("[%lu] WebSocket: refused at the handshake (free heap %u, %u connected)\r\n"),
            (unsigned long)now, (unsigned)freeHeap, (unsigned)connected);
  }
  // The refusal ends the connection with a RST: tcp_abort() queues nothing and frees the pcb at once. Returning false alone makes the
  // library build and write a 401 through a path that does not pass webBeginRequest() (no send cap, no ADR-189 abort on the ack
  // timeout, graceful close, rx timeout 0): at a floor heap that answer can stay unsent and hold a half-closed connection (bench
  // 2026-11-27, 455: :80 and :23 silent for about 54 s after the hold; mechanism inferred, the lane A arms decide it). After the abort
  // AsyncClient::add() writes nothing (pcb is NULL), so the 401 the library still creates is never sent. Not request->abort(): it sets
  // _sent, and the library's send(401) would then drop its response object without deleting it.
  if (wsAbortArmed() && request && request->client()) request->client()->abort();
  return false;
}

//===========================================================================================
// WebSocket event handler (AsyncWebSocket AwsEventHandler signature)
//===========================================================================================
// NOTE: AsyncWebSocket maintains its own client list (otLogWs.count()), so there is
// no parallel wsClientCount to keep in sync. The new client is already in the list
// when WS_EVT_CONNECT fires (AsyncWebSocket.cpp: _clients.emplace_back() precedes
// the event dispatch), so the cap test is "count() > MAX" — count() already includes
// the just-arrived client.
void webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                    AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_DISCONNECT:
      {
        noteWebSocketBurstEvent(WS_BURST_DISCONNECTED);
        state.heapdiag.iWsDisconnects++;   // TASK-3017
        uint32_t lifeS = 0, lastAgeS = 0;
        uint8_t lastSt = 0;
        if (wsTrackDisconnect(client->id(), millis(), &lifeS, &lastSt, &lastAgeS)) {
          DebugTf(PSTR("[%lu] WebSocket[%u] disconnected after %lus (last seen %s for %lus). Clients: %u\r\n"),
                  millis(), client->id(), (unsigned long)lifeS, wsStatusName(lastSt), (unsigned long)lastAgeS,
                  (unsigned)otLogWs.count());
        } else {
          DebugTf(PSTR("[%lu] WebSocket[%u] disconnected. Clients: %u\r\n"),
                  millis(), client->id(), (unsigned)otLogWs.count());
        }
      }
      break;

    case WS_EVT_CONNECT:
      {
        // Check client limit. count() already includes this new client.
        if (otLogWs.count() > MAX_WEBSOCKET_CLIENTS) {
          noteWebSocketBurstEvent(WS_BURST_REJECTED_MAX);
          DebugTf(PSTR("[%lu] WebSocket[%u]: Max clients (%u) reached, rejecting connection\r\n"),
            millis(), client->id(), MAX_WEBSOCKET_CLIENTS);
          client->close();
          return;
        }

        // Check heap health before accepting connection.
        // Use WARNING threshold to be conservative.
        if (platformFreeHeap() < HEAP_WARNING_THRESHOLD) {
          noteWebSocketBurstEvent(WS_BURST_REJECTED_HEAP);
          DebugTf(PSTR("[%lu] WebSocket[%u]: Low heap (%u bytes), rejecting connection\r\n"),
            millis(), client->id(), platformFreeHeap());
          client->close();
          return;
        }

        wsTrackConnect(client, millis());   // TASK-3017: AsyncTCP rx timeout + the device/info row
        IPAddress ip = client->remoteIP();
        noteWebSocketBurstEvent(WS_BURST_CONNECTED);
        DebugTf(PSTR("[%lu] WebSocket[%u] connected from %d.%d.%d.%d. Clients: %u\r\n"),
          millis(), client->id(), ip[0], ip[1], ip[2], ip[3], (unsigned)otLogWs.count());
      }
      break;

    case WS_EVT_DATA:
      // Handle incoming data from client (currently not used, but available for future commands)
      DebugTf(PSTR("[%lu] WebSocket[%u] received data (%u bytes)\r\n"),
              millis(), client->id(), static_cast<unsigned>(len));
      break;

    case WS_EVT_ERROR:
      noteWebSocketBurstEvent(WS_BURST_ERROR);
      DebugTf(PSTR("[%lu] WebSocket[%u] error\r\n"), millis(), client->id());
      break;

    case WS_EVT_PING:
      // Ping/pong handled automatically by the library
      DebugTf(PSTR("[%lu] WebSocket[%u] ping\r\n"), millis(), client->id());
      break;

    case WS_EVT_PONG:
      // Ping/pong handled automatically by the library
      DebugTf(PSTR("[%lu] WebSocket[%u] pong\r\n"), millis(), client->id());
      break;
  }
}

//===========================================================================================
// Send JSON message to all connected clients
// Used only for firmware upgrade progress notifications
//===========================================================================================
void sendWebSocketJSON(const char *json) {
  if (otLogWs.count() > 0) {
    Debugf(PSTR("[%lu] WebSocket broadcast JSON (%u bytes)\r\n"),
           millis(), static_cast<unsigned>(strlen(json)));
    wsNoteSendStatus(wsTextAllNoThrow(json));
  }
}

//===========================================================================================
// Start WebSocket endpoint (attach the /ws AsyncWebSocket to the port-80 server)
//===========================================================================================
// Idempotent: attaches the handler to the persistent port-80 AsyncWebServer exactly
// once. Called from setup() (OTGW-firmware.ino) and again on every WiFi/Ethernet
// transition (networkStuff.ino, Ethernet.ino). Those transitions do NOT tear down
// the AsyncWebServer (they never call startWebserver()/server.begin() again), so the
// handler survives across them — re-binding would be the bug, the no-op is correct.
// addHandler() after server.begin() is safe: begin() only starts the TCP listener;
// _handlers is consulted live per request (ESPAsyncWebServer WebServer.cpp).
void startWebSocket() {
  if (wsInitialized) return;

  otLogWs.onEvent(webSocketEvent);
  otLogWs.handleHandshake(wsHandshakeHandler);   // TASK-3119: refuse before the library builds the client object
  server.addHandler(&otLogWs);

  wsInitialized = true;
  Debugf(PSTR("[%lu] WebSocket endpoint attached at ws://<host>/ws (port 80)\r\n"), millis());
}

//===========================================================================================
// Periodic WebSocket housekeeping (call from the main loop / background tasks)
//===========================================================================================
// AsyncWebSocket is serviced on the AsyncTCP task — there is no per-loop library poll.
// This timer-driven helper only does the two things the library cannot do on its own:
//  1. cleanupClients() reaps closed/stale client slots, capped at MAX_WEBSOCKET_CLIENTS.
//  2. A 30s application-level keepalive keeps watchdogs fed when no OT log flows and
//     works around Safari WebSocket ping/pong quirks (ADR-025). Shape is identical to
//     the old broadcast so existing clients' keepalive handling is unchanged.
void handleWebSocket() {
  if (!wsInitialized) return;

  otLogWs.cleanupClients(MAX_WEBSOCKET_CLIENTS);

  unsigned long now = millis();
  if (otLogWs.count() > 0 &&
      (now - lastKeepaliveMs) >= KEEPALIVE_INTERVAL_MS) {
    wsNoteSendStatus(wsTextAllNoThrow("{\"type\":\"keepalive\"}"));
    lastKeepaliveMs = now;
  }
}

//===========================================================================================
// Send log message directly to all connected WebSocket clients
// This is called from OTGW-Core.ino when a new log line is ready
// Simplified: no queue, no JSON, just direct text broadcasting.
// Heap backpressure: gated by canSendWebSocket() so the live-log throttles/blocks
// under heap pressure instead of broadcasting every OT frame unconditionally.
// (Restores parity with dev, where this gate is wired; it had never been ported to 2.0.0.)
//===========================================================================================
#include "WsUtf8.h"
// Sanitised copy of an OT-log line that is not valid UTF-8 (TASK-1300). A named
// global per ADR-053; owned by sendLogToWebSocket() and only used during that call
// (textAll copies the data).
char wsSafeBuf[OT_LOG_BUFFER_SIZE];
// ADR-090 in-use flag: the loop task and an async REST handler can both reach
// sendLogToWebSocket(). On contention the frame is dropped rather than sent with
// bytes of two lines mixed in one buffer.
static volatile bool wsSafeBufBusy = false;

// TASK-3017: textAll() reports per message whether every connected client queued it. A client whose queue is full
// (WS_MAX_QUEUED_MESSAGES, ESPAsyncWebServer AsyncWebSocketClient::_queueMessage()) gets the message silently
// discarded; such a client keeps its socket and receives nothing. Count every message that did not reach every client
// in ws_drops, so a stalled client is visible instead of a drop counter that stays 0.
// --- wsNoteSendStatus begin
static void wsNoteSendStatus(AsyncWebSocket::SendStatus st) {
  if (st != AsyncWebSocket::ENQUEUED) state.heapdiag.iWsDropsTotal++;
}
// --- wsNoteSendStatus end

// TASK-3119: a broadcast that cannot allocate is dropped, not thrown. textAll() allocates a shared buffer and a deque node per client
// (AsyncWebSocketClient::_queueMessage); at a floor heap operator new throws std::bad_alloc, and an exception that nothing catches ends
// in std::terminate and a panic (bench 2026-11-27 on .76, K hold: loopTask, release a9aa6a9 and 456). canSendWebSocket() blocks only
// below 3072 B of total free heap and the allocation follows a moment later, so the window cannot be closed by a gate: every send site
// goes through this helper, which drops the message, counts it (hd_ws_alloc_fail) and logs once per interval. The library keeps its
// state sound: the queue lock is an RAII guard and std::deque::emplace_back gives the strong guarantee (read, not run).
static const uint32_t WS_ALLOC_LOG_INTERVAL_MS = 10000;
static AsyncWebSocket::SendStatus wsTextAllNoThrow(const char* msg) {
  try {
    return otLogWs.textAll(msg);
  } catch (const std::bad_alloc&) {
    static uint32_t lastLogMs = 0;
    static uint32_t sinceLog = 0;
    static bool everLogged = false;
    state.heapdiag.iWsAllocFail++;
    sinceLog++;
    const uint32_t now = millis();
    if (!everLogged || (uint32_t)(now - lastLogMs) > WS_ALLOC_LOG_INTERVAL_MS) {
      everLogged = true;
      DebugTf(PSTR("[%lu] WebSocket: broadcast dropped, out of memory (%u dropped since the last line, free heap %u, max block %u)\r\n"),
              (unsigned long)now, (unsigned)sinceLog, (unsigned)platformFreeHeap(), (unsigned)platformMaxFreeBlock());
      lastLogMs = now;
      sinceLog = 0;
    }
    return AsyncWebSocket::DISCARDED;
  }
}

void sendLogToWebSocket(const char* logMessage) {
  if (hasWebSocketClients() && logMessage != nullptr && canSendWebSocket()) {
    // Chrome closes the socket on a text frame that is not valid UTF-8 (TASK-1300):
    // raw PIC bytes in an unparsed line, or a log line cut inside a multi-byte
    // character. Sanitise a copy at this transport boundary, so the PIC bytes stay
    // verbatim on serial, telnet, port 25238 and the raw diagnose path (ADR-177).
    if (wsUtf8Valid(logMessage)) {
      wsNoteSendStatus(wsTextAllNoThrow(logMessage));
    } else {
      if (wsSafeBufBusy) return;
      wsSafeBufBusy = true;
      wsUtf8Sanitize(logMessage, wsSafeBuf, sizeof(wsSafeBuf));
      wsNoteSendStatus(wsTextAllNoThrow(wsSafeBuf));
      wsSafeBufBusy = false;
    }
  }
}



/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
* 
****************************************************************************
*/

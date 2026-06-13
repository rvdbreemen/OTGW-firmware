// mqtt_discovery_verify.h -- discovery-verify state machine (ADR-062)
// Part of OTGW-firmware, MIT license
//
// Copyright (c) 2021-2026 Robert van den Breemen
//
// Public API for the MQTT retained-config verification pass (TASK-363
// extraction; logic originally lived in MQTTstuff.ino). The implementation
// lives in mqtt_discovery_verify.cpp, which is a separate TU from the sketch
// and therefore does NOT include OTGW-firmware.h. All access to the sketch-
// side globals (state, settings, MQTTclient, NodeId, etc.) and to helpers
// like markAllMQTTConfigPending() goes through the narrow accessor surface
// declared in this header and implemented in MQTTstuff.ino (see
// "discovery-verify TU accessors" block).
//
// Callers (MQTTstuff.ino, OTGW-firmware.ino, restAPI.ino, handleDebug.ino)
// interact via the four entry points at the top of this header only.

#pragma once
#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Public entry points -- used by the sketch side.
// ---------------------------------------------------------------------------

// Start a verify pass. Returns false if any precondition fails (already
// verifying, MQTT not connected, PIC flashing, NTP not set, uptime < 3600s,
// drip pending, heap/max-block too small, wildcard truncated, setBufferSize
// or subscribe failed). Preconditions documented in the .cpp.
bool startDiscoveryVerification();

// True while the verify window is open (between successful start and close).
// Read by restAPI.ino for the /api/v2/discovery GET status field.
bool isDiscoveryVerificationActive();

// Poll this from handleMQTT() each iteration: closes on timeout, MQTT
// disconnect, heap-abort, or early-success. Cheap no-op when inactive.
void tickDiscoveryVerification();

// MQTT callback filter hook: call as the first thing inside handleMQTTcallback.
// Returns true when the incoming topic was consumed by the verify window
// (retained-config under <haprefix>/) and the caller must return immediately
// without falling through to the command-topic dispatcher. Returns false
// when the verify window is inactive or the topic prefix does not match,
// in which case normal dispatch proceeds.
bool handleDiscoveryVerifyMessage(const char *topic, unsigned int length);

// ---------------------------------------------------------------------------
// Discovery-verify TU accessors -- implemented in MQTTstuff.ino where the
// full OTGWState/OTGWSettings types are visible (via OTGW-firmware.h, which
// the sketch TU includes). The separate-TU .cpp calls these instead of
// touching the globals directly.
//
// Outcome values are passed as uint8_t to avoid pulling the VerifyOutcome
// enum type into this header (it is declared inside OTGW-firmware.h). The
// mapping matches VerifyOutcome in OTGW-firmware.h:
//   0 = UNKNOWN, 1 = CLEAN, 2 = MISSING, 3 = ABORTED_HEAP, 4 = ABORTED_DISCONNECT
// A static_assert in MQTTstuff.ino pins the numeric mapping at compile time
// so the enum and the wire mapping cannot drift silently.
// ---------------------------------------------------------------------------

// Read-side snapshot helpers.
bool         verifyAccessorMqttConnected();
bool         verifyAccessorPicFlashing();
bool         verifyAccessorNtpTimeSet();
uint32_t     verifyAccessorUptimeSeconds();
uint32_t     verifyAccessorPublishedTopicCount();
uint16_t     verifyAccessorCountPendingDiscoveryIds();
const char*  verifyAccessorHaPrefix();
const char*  verifyAccessorNodeId();

// Write-side helpers: update the state.discovery counters.
void         verifyAccessorSetOutcome(uint8_t outcome);
uint8_t      verifyAccessorGetOutcome();
void         verifyAccessorIncVerifyRunCount();
void         verifyAccessorIncRepublishTriggeredCount();
void         verifyAccessorSetLastVerifyEpoch(uint32_t epoch);
void         verifyAccessorSetLastMissingCount(uint16_t missing);
void         verifyAccessorSetLastOrphanCount(uint16_t orphan);

// Trigger a full discovery re-announce after a MISSING verify.
void         verifyAccessorMarkAllMQTTConfigPending();

// Logging bridge -- routes to DebugTf/DebugTln (telnet port 23). Kept narrow
// so the verify TU does not depend on Debug.h (which defines function bodies
// that would cause ODR violations if re-included here). The .cpp pre-formats
// with snprintf_P into a small stack buffer and hands the result as a RAM
// c-string; the accessor prepends the usual BOL prefix and writes to telnet.
void         verifyAccessorLogLine(const char* ramMessage);

// MQTT client operations exposed without surfacing the MQTT client type here.
// TASK-865.8: the setBufferSize/restore pair was removed: under espMqttClient
// the RX buffer is a fixed EMC_RX_BUFFER_SIZE (1440 B) with no per-window resize.
// Subscribe/unsubscribe to/from the verify wildcard (QoS 0).
bool         verifyAccessorMqttSubscribe(const char *topic);
bool         verifyAccessorMqttUnsubscribe(const char *topic);

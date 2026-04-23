// mqtt_discovery_verify.cpp -- discovery-verify state machine (ADR-062)
// Part of OTGW-firmware, MIT license
//
// Copyright (c) 2021-2026 Robert van den Breemen
//
// Extracted from MQTTstuff.ino under TASK-363. Pure move: NO functional or
// behavior changes vs. the prior file-local implementation. All sketch-side
// globals (state, settings, MQTTclient, NodeId, isFlashing, etc.) are reached
// via the narrow accessor surface declared in mqtt_discovery_verify.h and
// implemented in MQTTstuff.ino. This keeps the new TU completely decoupled
// from OTGW-firmware.h, which is not multi-TU safe (it defines, rather than
// declares, the sketch's top-level globals).
//
// Outcome-enum numeric mapping (matches VerifyOutcome in OTGW-firmware.h):
//   0 = UNKNOWN, 1 = CLEAN, 2 = MISSING, 3 = ABORTED_HEAP, 4 = ABORTED_DISCONNECT
// Maintained in lock-step with MQTTstuff.ino's static_assert on the enum
// values.

#include "mqtt_discovery_verify.h"

#include <Arduino.h>          // millis(), ESP.getFreeHeap()
#include "platform.h"         // platformMaxFreeBlock() (ESP8266 + ESP32)
#include <pgmspace.h>
#include <string.h>
#include <time.h>             // time(nullptr) for iLastVerifyEpoch
#include <stdio.h>            // snprintf_P

// No include of Debug.h here: that header defines function bodies
// (_debugPrintf_P / _debugBOL) and would produce ODR violations in a separate
// TU. Use verifyAccessorLogLine() to emit telnet-side trace lines; it already
// prepends the BOL prefix on the sketch side.

// Outcome codes (mirror VerifyOutcome). Kept local-static so this file does
// not need OTGW-firmware.h just to name the enum.
namespace {
  constexpr uint8_t OUTCOME_UNKNOWN            = 0;
  constexpr uint8_t OUTCOME_CLEAN              = 1;
  constexpr uint8_t OUTCOME_MISSING            = 2;
  constexpr uint8_t OUTCOME_ABORTED_HEAP       = 3;
  constexpr uint8_t OUTCOME_ABORTED_DISCONNECT = 4;
}

// =====================================================================
// MQTT auto-discovery verification (ADR-062, TASK-349)
// Subscribe briefly to <haprefix>/+/<nodeId>/# and count retained configs
// delivered by the broker. If received < expected, trigger a full re-announce.
//
// RAM-tuned: PubSubClient RX buffer raised 384->1024 only during the 15s
// window, then restored. Peak transient delta: +640 bytes.
// =====================================================================
static bool            verifyActive          = false;
static unsigned long   verifyStartMs         = 0;
static uint16_t        verifyReceivedCount   = 0;
static uint16_t        verifyOrphanCount     = 0;
static bool            verifyBufferResized   = false;
// sHaprefix[41] + "/+/" + sUniqueid[41] + "/#" + NUL = 88 bytes worst case.
// Sized to 128 gives comfortable headroom for any future field-size bump.
static char            verifyWildcard[128]   = "";
// Cached once in startDiscoveryVerification() so the MQTT callback filter does
// not recompute strlen() on every incoming retained-config message during the
// 15s verify window (HIGH/Perf review finding).
static size_t          verifyPrefixLen       = 0;
static size_t          verifyNodeLen         = 0;

// ADR-062 tuning table: see docs/adr/ADR-062-retained-discovery-verification.md.
// Per-line rationale below; values are co-tuned with HEAP_LOW=5120 / HEAP_WARNING=3072.
static constexpr unsigned long VERIFICATION_WINDOW_MS      = 15000; // accommodates slow brokers; early-close fires when received>=expected
static constexpr uint16_t      VERIFICATION_BUFFER_BYTES   = 1024;  // worst realistic retained-config payload ~900B; +~100B headroom
// VERIFICATION_MIN_HEAP_START lives in MQTTstuff.h (cross-TU: restAPI.ino needs it).
// Value: 6000. clears the 1024B buffer-resize with comfortable margin above the ABORT floor.
// We read it here by re-declaring the same constant; keeping it file-local
// mirrors the original structure and avoids pulling MQTTstuff.h for just
// this one value. If the two ever drift the heap-abort path will simply use
// the local value.
static constexpr uint32_t      VERIFICATION_MIN_HEAP_START_LOCAL = 6000;
static constexpr uint32_t      VERIFICATION_MIN_HEAP_ABORT = 4500;  // aligned just below HEAP_LOW=5120; abort suppresses false-missing republish
// Max single-segment length the node-id parse accepts. Caps DoS-window CPU
// from malicious broker flooding with long-segment topics (MEDIUM/Sec finding).
static constexpr size_t        VERIFICATION_MAX_NODE_SEGMENT_LEN = 64;

// Forward declaration so tickDiscoveryVerification() can call it.
static void endDiscoveryVerification();

// Begin a verify pass. Returns false if any precondition fails.
//
// Preconditions enforced here (TASK-359 keeps the caller comment in
// OTGW-firmware.ino honest):
//   1. Not already verifying (verifyActive).
//   2. MQTT connected (state.mqtt.bConnected).
//   3. PIC not flashing (isFlashing()).
//   4. NTP time is set (isNTPtimeSet()) -- retained-config dedupe and
//      iLastVerifyEpoch are meaningless with an unsynced clock.
//   5. Uptime >= 3600s (state.uptime.iSeconds) -- avoids running during the
//      startup drip storm when discovery topics are still being published.
//   6. No drip pending (countPendingDiscoveryIds() == 0) -- drip-race guard.
//   7. Heap headroom >= VERIFICATION_MIN_HEAP_START.
//   8. Contiguous max-block >= VERIFICATION_BUFFER_BYTES + 256 so the
//      PubSubClient buffer realloc does not fragment the heap further.
bool startDiscoveryVerification() {
  if (verifyActive) return false;
  if (!verifyAccessorMqttConnected()) return false;
  if (verifyAccessorPicFlashing()) return false;
  if (!verifyAccessorNtpTimeSet()) return false;                              // TASK-359
  if (verifyAccessorUptimeSeconds() < 3600) return false;                     // TASK-359
  if (verifyAccessorCountPendingDiscoveryIds() > 0) return false;             // drip-race guard
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_START_LOCAL) return false;
  // Max-block precheck: umm_malloc realloc of PubSubClient's buffer needs a
  // contiguous 1024-byte block. Avoid the realloc entirely when the heap is
  // fragmented (Perf review: setBufferSize grow/shrink fragments over long uptime).
  if (platformMaxFreeBlock() < (VERIFICATION_BUFFER_BYTES + 256U)) return false;

  const char* haPrefix = verifyAccessorHaPrefix();
  const char* nodeId   = verifyAccessorNodeId();
  const int wrote = snprintf_P(verifyWildcard, sizeof(verifyWildcard),
                               PSTR("%s/+/%s/#"),
                               haPrefix ? haPrefix : "",
                               nodeId   ? nodeId   : "");
  if (wrote <= 0 || (size_t)wrote >= sizeof(verifyWildcard)) {
    // Arch review (HIGH): refuse to start on wildcard truncation, which would
    // subscribe to a malformed topic and yield zero matches -> false-positive
    // full republish every run.
    char logBuf[96];
    snprintf_P(logBuf, sizeof(logBuf),
               PSTR("[verify] wildcard truncated (%d/%u), refusing start"),
               wrote, (unsigned)sizeof(verifyWildcard));
    verifyAccessorLogLine(logBuf);
    return false;
  }

  // Cache segment lengths for callback fast-path.
  verifyPrefixLen = haPrefix ? strlen(haPrefix) : 0;
  verifyNodeLen   = nodeId   ? strlen(nodeId)   : 0;

  // Raise RX buffer BEFORE subscribe so oversize configs fit.
  if (!verifyAccessorSetMqttBufferSize(VERIFICATION_BUFFER_BYTES)) {
    verifyAccessorLogLine("[verify] setBufferSize failed");
    return false;
  }
  verifyBufferResized = true;

  if (!verifyAccessorMqttSubscribe(verifyWildcard)) {
    verifyAccessorLogLine("[verify] subscribe failed");
    verifyAccessorRestoreMqttBufferSize();
    verifyBufferResized = false;
    return false;
  }

  verifyReceivedCount = 0;
  verifyOrphanCount   = 0;
  verifyStartMs       = millis();
  verifyActive        = true;
  // TASK-361: mark outcome UNKNOWN for the new pass so endDiscoveryVerification
  // can distinguish "abort path already wrote an outcome" from "normal close,
  // must classify as CLEAN/MISSING now".
  verifyAccessorSetOutcome(OUTCOME_UNKNOWN);
  verifyAccessorIncVerifyRunCount();
  {
    char logBuf[160];
    snprintf_P(logBuf, sizeof(logBuf),
               PSTR("[verify] started: wildcard=%s expected=%lu"),
               verifyWildcard,
               (unsigned long)verifyAccessorPublishedTopicCount());
    verifyAccessorLogLine(logBuf);
  }
  return true;
}

// End the verify window, reconcile, trigger republish if missing.
// TASK-361: writes an honest VerifyOutcome into state.discovery.eLastOutcome
// based on the received/expected tally. Republish now keys off MISSING only,
// so ABORTED_* paths (heap/disconnect) no longer need the
// verifyReceivedCount=expected hack that lied to telemetry.
static void endDiscoveryVerification() {
  if (!verifyActive) return;
  verifyActive = false;
  verifyAccessorSetLastVerifyEpoch((uint32_t)time(nullptr));

  const uint16_t expected = (uint16_t)verifyAccessorPublishedTopicCount();
  const uint16_t missing  = (verifyReceivedCount >= expected) ? 0
                                                              : (uint16_t)(expected - verifyReceivedCount);
  verifyAccessorSetLastMissingCount(missing);
  verifyAccessorSetLastOrphanCount(verifyOrphanCount);

  // Preserve ABORTED_* outcomes already written by the abort paths (tick's
  // heap-abort / disconnect fast-path set the outcome before calling this).
  // Only classify as CLEAN / MISSING when the outcome is still the neutral
  // sentinel (UNKNOWN, set at startDiscoveryVerification) for a normal-close
  // pass. This avoids a stale ABORTED_* from a prior run leaking forward.
  if (verifyAccessorGetOutcome() == OUTCOME_UNKNOWN) {
    verifyAccessorSetOutcome((missing == 0) ? OUTCOME_CLEAN : OUTCOME_MISSING);
  }

  if (verifyAccessorMqttConnected()) {
    verifyAccessorMqttUnsubscribe(verifyWildcard);
  }
  if (verifyBufferResized) {
    verifyAccessorRestoreMqttBufferSize();
    verifyBufferResized = false;
  }

  const uint8_t outcome = verifyAccessorGetOutcome();
  {
    char logBuf[144];
    snprintf_P(logBuf, sizeof(logBuf),
               PSTR("[verify] done: expected=%u received=%u orphans=%u missing=%u outcome=%u"),
               expected, verifyReceivedCount, verifyOrphanCount, missing,
               (unsigned)outcome);
    verifyAccessorLogLine(logBuf);
  }

  // TASK-361: republish ONLY when outcome is MISSING. ABORTED_HEAP and
  // ABORTED_DISCONNECT suppress republish by design (don't fight for RAM or a
  // dead broker), but keep reporting the real missing count for telemetry.
  if (outcome == OUTCOME_MISSING) {
    verifyAccessorLogLine("[verify] missing configs detected, triggering markAllMQTTConfigPending");
    verifyAccessorIncRepublishTriggeredCount();
    verifyAccessorMarkAllMQTTConfigPending();
  }
}

// Polled from handleMQTT() to close the window on timeout / disconnect / heap-abort.
void tickDiscoveryVerification() {
  if (!verifyActive) return;
  if (!verifyAccessorMqttConnected()) {
    // Fast-path close. Restore the buffer defensively even on a dead client:
    // setBufferSize is a local realloc, safe when not connected. Prevents the
    // 1024B allocation from leaking across a reconnect path that does not
    // re-size (Arch/Sec review finding).
    // TASK-361: report the honest outcome for telemetry; still skip the
    // full reconcile/republish path (MQTT is dead anyway).
    if (verifyBufferResized) {
      verifyAccessorRestoreMqttBufferSize();
      verifyBufferResized = false;
    }
    verifyAccessorSetOutcome(OUTCOME_ABORTED_DISCONNECT);
    verifyAccessorSetLastVerifyEpoch((uint32_t)time(nullptr));
    verifyActive = false;
    return;
  }
  const unsigned long now = millis();
  const uint16_t expected = (uint16_t)verifyAccessorPublishedTopicCount();

  // Heap-abort gate: avoid fighting for RAM mid-window.
  // TASK-361: set ABORTED_HEAP outcome; endDiscoveryVerification then records
  // the real missing/orphan counts but skips republish (republish is gated on
  // MISSING). Earlier code forged verifyReceivedCount=expected to suppress
  // the republish, which also lied to telemetry -- hack removed.
  if (ESP.getFreeHeap() < VERIFICATION_MIN_HEAP_ABORT) {
    verifyAccessorSetOutcome(OUTCOME_ABORTED_HEAP);
    verifyAccessorLogLine("[verify] heap-abort: closing window early");
    endDiscoveryVerification();
    return;
  }
  // Early-close when everything arrived (with tiny settling delay).
  if (verifyReceivedCount >= expected && (unsigned long)(now - verifyStartMs) > 500UL) {
    endDiscoveryVerification();
    return;
  }
  // Timeout.
  if ((unsigned long)(now - verifyStartMs) >= VERIFICATION_WINDOW_MS) {
    endDiscoveryVerification();
  }
}

bool isDiscoveryVerificationActive() { return verifyActive; }

// MQTT callback filter for retained discovery configs.
// Extracted from handleMQTTcallback in MQTTstuff.ino (TASK-363). Returns true
// once the topic has been consumed by the verify window, so the caller knows
// to skip its normal command-topic dispatch path.
//
// Semantics preserved from TASK-357 / TASK-349:
//   Once the haprefix byte-prefix matches, ALWAYS consume. A topic delivered
//   under <haprefix>/ with malformed substructure must not fall through into
//   the OT command dispatcher -- a crafted retained topic could otherwise
//   sneak into the command path. Any substructure that is not a well-formed
//   <haprefix>/<component>/<nodeId>/... shape is counted as an orphan and
//   consumed here.
bool handleDiscoveryVerifyMessage(const char *topic, unsigned int /*length*/) {
  if (!verifyActive || verifyPrefixLen == 0 || topic == nullptr) return false;
  const char* haPrefix = verifyAccessorHaPrefix();
  if (!haPrefix) return false;
  if (strncmp(topic, haPrefix, verifyPrefixLen) != 0) return false;
  if (topic[verifyPrefixLen] != '/') return false;

  // Topic format: <haprefix>/<component>/<nodeId>/.../config
  const char *rest   = topic + verifyPrefixLen + 1;
  const char *slash1 = strchr(rest, '/');
  if (slash1) {
    const char *nodeStart = slash1 + 1;
    const char *slash2    = strchr(nodeStart, '/');
    if (slash2) {
      const size_t nodeLen = (size_t)(slash2 - nodeStart);
      // DoS-cap: refuse overly long node segments from malicious/misconfigured
      // brokers (Sec review). Anything beyond our own nodeid length window is
      // noise we don't need to strncmp.
      const char* nodeId = verifyAccessorNodeId();
      if (nodeLen > VERIFICATION_MAX_NODE_SEGMENT_LEN) {
        verifyOrphanCount++;
      } else if (nodeId != nullptr && nodeLen == verifyNodeLen &&
                 strncmp(nodeStart, nodeId, nodeLen) == 0) {
        verifyReceivedCount++;
      } else {
        verifyOrphanCount++;
      }
    } else {
      // Malformed under prefix: missing second slash (no nodeId/... segment).
      verifyOrphanCount++;
    }
  } else {
    // Malformed under prefix: missing first slash after <haprefix>/.
    verifyOrphanCount++;
  }
  return true;  // handled by verify -- never fall through to command dispatcher
}

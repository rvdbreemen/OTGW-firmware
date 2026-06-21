/*********
**  Program  : webhook.ino
**  Version  : v2.0.0-alpha.235
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Sends an HTTP GET request to a configured URL when an OpenTherm
**  status bit changes state (ON or OFF).  The trigger bit uses the
**  same Statusflags layout as the GPIO outputs feature:
**    Bits 0-7  : Slave status  (bit 1 = CH mode / central heating active)
**    Bits 8-15 : Master status (bit 8 = CH enable)
**
**  TERMS OF USE: MIT License. See bottom of file.
*********/

static bool webhookLastState = false;
static bool webhookInitialized = false;

//=======================================================================
// Validate that a webhook URL targets a local-network host.
// Enforces the ADR-003/ADR-032 security model: the device is local-only
// and must not make outbound calls to public Internet addresses.
//
// Rules:
//  - Scheme must be http:// (ADR-003: no HTTPS on ESP8266)
//  - Dotted-decimal IPv4 host must be RFC1918 or link-local; loopback
//    and public IPs are rejected
//  - Hostnames (letters/digits/dashes/dots) are allowed — local DNS
//    is trusted on a private network (ADR-032 trust model)
//=======================================================================
static bool isLocalUrl(const char* url) {
  // Scheme must be http://
  if (strncasecmp_P(url, PSTR("http://"), 7) != 0) {
    DebugTln(F("Webhook: URL rejected (scheme must be http://)"));
    return false;
  }

  // Extract host: scan from after "http://" to first '/', ':' or NUL
  const char* host = url + 7;
  size_t hostLen = 0;
  while (host[hostLen] && host[hostLen] != '/' && host[hostLen] != ':') {
    hostLen++;
  }
  if (hostLen == 0 || hostLen >= 64) {
    DebugTln(F("Webhook: URL rejected (invalid host)"));
    return false;
  }

  // Copy into a small stack buffer for analysis
  char hostBuf[64];
  memcpy(hostBuf, host, hostLen);
  hostBuf[hostLen] = '\0';

  // Determine whether the host is a bare IPv4 dotted-decimal address
  bool isIp = true;
  int dotCount = 0;
  for (size_t i = 0; i < hostLen; i++) {
    char c = hostBuf[i];
    if (c == '.') { dotCount++; }
    else if (c < '0' || c > '9') { isIp = false; break; }
  }
  // Must have exactly 3 dots to be a valid IPv4 (rejects "1.2.3.4.5", "...")
  if (isIp && dotCount != 3) isIp = false;

  unsigned int o1 = 0, o2 = 0, o3 = 0, o4 = 0;

  if (!isIp) {
    // Hostname — resolve to IP and validate it's local (prevents SSRF via DNS rebinding)
    IPAddress resolved;
    if (!WiFi.hostByName(hostBuf, resolved)) {
      DebugTf(PSTR("Webhook: URL rejected (DNS lookup failed for %s)\r\n"), hostBuf);
      return false;
    }
    o1 = resolved[0]; o2 = resolved[1]; o3 = resolved[2]; o4 = resolved[3];
    DebugTf(PSTR("Webhook: hostname %s resolved to %u.%u.%u.%u\r\n"), hostBuf, o1, o2, o3, o4);
  } else {
    // Parse the four octets and validate range
    if (sscanf(hostBuf, "%u.%u.%u.%u", &o1, &o2, &o3, &o4) != 4 ||
        o1 > 255 || o2 > 255 || o3 > 255 || o4 > 255) {
      DebugTln(F("Webhook: URL rejected (malformed IP)"));
      return false;
    }
  }

  if (o1 == 10)                            return true;  // 10.0.0.0/8
  if (o1 == 172 && o2 >= 16 && o2 <= 31)  return true;  // 172.16.0.0/12
  if (o1 == 192 && o2 == 168)             return true;  // 192.168.0.0/16
  if (o1 == 169 && o2 == 254)             return true;  // link-local
  // 127.x.x.x loopback excluded: calling self could create a feedback loop

  DebugTf(PSTR("Webhook: URL rejected (non-local IP %u.%u.%u.%u)\r\n"),
          o1, o2, o3, o4);
  return false;
}

//=======================================================================
// Expand a payload template by replacing {variable} placeholders with
// current OpenTherm state values.
//
// Supported variables:
//   {state}      — "ON" or "OFF" (the current trigger-bit state)
//   {tboiler}    — boiler flow temperature (°C, 1 decimal)
//   {tr}         — room temperature (°C, 1 decimal)
//   {tset}       — CH water setpoint (°C, 1 decimal)
//   {tdhw}       — DHW temperature (°C, 1 decimal)
//   {relmod}     — relative modulation level (%, 0 decimals)
//   {chpressure} — CH circuit pressure (bar, 2 decimals)
//   {flameon}    — "true"/"false" — burner flame active (slave status bit 3)
//   {chmode}     — "true"/"false" — CH active (slave status bit 1)
//   {dhwmode}    — "true"/"false" — DHW active (slave status bit 2)
//
// Unknown {variables} are passed through unchanged.
//
// Note: HTTPS / public-internet targets (e.g. Discord) require a local
// relay such as a Node-RED flow or Home Assistant webhook automation, as
// this device only makes outbound HTTP calls to local-network hosts.
//=======================================================================
static bool expandPayload(const char* tmpl, char* out, size_t outLen, bool stateOn) {
  bool truncated = false;
  size_t di = 0;
  const char* p = tmpl;
  while (*p && di < outLen - 1) {
    if (*p != '{') { out[di++] = *p++; continue; }

    // Scan for the closing brace
    const char* end = strchr(p + 1, '}');
    if (!end) { out[di++] = *p++; continue; }   // no closing brace — literal '{'

    size_t nameLen = (size_t)(end - p - 1);
    if (nameLen == 0 || nameLen >= 20) { out[di++] = *p++; continue; }

    char varName[20];
    memcpy(varName, p + 1, nameLen);
    varName[nameLen] = '\0';

    char val[16] = "";
    if      (strcmp_P(varName, PSTR("state"))      == 0) { snprintf_P(val, sizeof(val), stateOn ? PSTR("ON") : PSTR("OFF")); }
    else if (strcmp_P(varName, PSTR("tboiler"))    == 0) { snprintf_P(val, sizeof(val), PSTR("%.1f"), OTcurrentSystemState.Tboiler); }
    else if (strcmp_P(varName, PSTR("tr"))         == 0) { if (isnan(OTcurrentSystemState.Tr)) strlcpy_P(val, PSTR("--"), sizeof(val)); else snprintf_P(val, sizeof(val), PSTR("%.1f"), OTcurrentSystemState.Tr); }
    else if (strcmp_P(varName, PSTR("tset"))       == 0) { snprintf_P(val, sizeof(val), PSTR("%.1f"), OTcurrentSystemState.TSet); }
    else if (strcmp_P(varName, PSTR("tdhw"))       == 0) { snprintf_P(val, sizeof(val), PSTR("%.1f"), OTcurrentSystemState.Tdhw); }
    else if (strcmp_P(varName, PSTR("relmod"))     == 0) { snprintf_P(val, sizeof(val), PSTR("%.0f"), OTcurrentSystemState.RelModLevel); }
    else if (strcmp_P(varName, PSTR("chpressure")) == 0) { snprintf_P(val, sizeof(val), PSTR("%.2f"), OTcurrentSystemState.CHPressure); }
    else if (strcmp_P(varName, PSTR("flameon"))    == 0) { snprintf_P(val, sizeof(val), (OTcurrentSystemState.SlaveStatus & (1U << 3)) ? PSTR("true") : PSTR("false")); }
    else if (strcmp_P(varName, PSTR("chmode"))     == 0) { snprintf_P(val, sizeof(val), (OTcurrentSystemState.SlaveStatus & (1U << 1)) ? PSTR("true") : PSTR("false")); }
    else if (strcmp_P(varName, PSTR("dhwmode"))    == 0) { snprintf_P(val, sizeof(val), (OTcurrentSystemState.SlaveStatus & (1U << 2)) ? PSTR("true") : PSTR("false")); }
    else { out[di++] = *p++; continue; }  // unknown variable — pass '{' literally

    size_t valLen = strlen(val);
    size_t avail  = outLen - 1 - di;
    if (valLen > avail) {
      valLen = avail;
      truncated = true;
    }
    memcpy(out + di, val, valLen);
    di += valLen;
    p = end + 1;  // advance past '}'
  }
  if (*p != '\0') truncated = true;
  out[di] = '\0';
  return truncated;
}

//=======================================================================
// ADR-123 Phase-4 webhook sender task plumbing. WebhookJob (the value-copy
// queue item) is declared in Webhooktypes.h so the type precedes the Arduino
// auto-generated prototypes for the helpers below. The sender touches NOTHING
// but the job — no settings, no OTGWState, no lock — so it can block on the HTTP
// round-trip without stalling drainOTFrameQueue or the AsyncTCP task (the
// load-bearing rule: never hold the ADR-129 mutex across blocking I/O).
//=======================================================================
// Depth 2: at most one edge is ever "in flight" (the webhookInFlight gate in
// evalWebhook() enforces ADR-057 §5 one-pending-at-a-time), and the test
// endpoint may enqueue once independently. 2 leaves slack without retaining a
// backlog the best-effort contract never promised.
#define WEBHOOK_QUEUE_DEPTH 2
static PlatformQueue webhookQueue   = nullptr;  // loop/AsyncTCP producer -> webhook task
static PlatformTask  g_webhookTask  = nullptr;  // dedicated sender task handle
static volatile bool webhookInFlight = false;   // ADR-057 §5: one pending transition at a time

//=======================================================================
// Build a WebhookJob for the given state. Reads settings + live OpenTherm
// state and expands the payload template HERE (at enqueue time) so the sender
// task never reads cross-task state. Returns false when there is nothing to
// send (no URL configured for this state) — a non-error "nothing to do".
//
// MUST be called with OpenTherm state reads consistent: loop context (where
// the OT consumer also runs) needs no lock; the AsyncTCP test endpoint wraps
// this in an OTStateLock. expandPayload() only READS OTcurrentSystemState.
//=======================================================================
static bool buildWebhookJob(bool stateOn, WebhookJob& job) {
  const char* url = stateOn ? settings.webhook.sURLon : settings.webhook.sURLoff;
  if (strlen(url) == 0) {
    DebugTf(PSTR("Webhook: no URL configured for state %s\r\n"), stateOn ? "ON" : "OFF");
    return false; // nothing to send
  }

  job.stateOn    = stateOn;
  strlcpy(job.sURL, url, sizeof(job.sURL));

  job.hasPayload = (settings.webhook.sPayload[0] != '\0');
  if (job.hasPayload) {
    bool wasTruncated = expandPayload(settings.webhook.sPayload,
                                      job.sPayloadExpanded, sizeof(job.sPayloadExpanded),
                                      stateOn);
    if (wasTruncated) {
      DebugTf(PSTR("Webhook: expanded payload truncated to %u bytes for %s\r\n"),
              static_cast<unsigned int>(sizeof(job.sPayloadExpanded) - 1), url);
    }
    const char* ct = (settings.webhook.sContentType[0] != '\0')
                     ? settings.webhook.sContentType
                     : "application/json";
    strlcpy(job.sContentType, ct, sizeof(job.sContentType));
  } else {
    job.sPayloadExpanded[0] = '\0';
    job.sContentType[0]     = '\0';
  }
  return true;
}

//=======================================================================
// Send one already-built webhook job. Returns true on HTTP 2xx, false on any
// retryable error. Pure sender: reads only the job, no settings/OTGWState/lock.
//
// Runs ON THE WEBHOOK TASK, which MAY block freely now (ADR-123 Phase-4) — so
// the cooperative-era 500 ms loop-stall cap is gone; a 1000 ms timeout (the
// ADR-057 §4 contract value) is used, with the 3-attempt/30s backoff above it.
//
// Behaviour preserved from the cooperative path:
//   - hasPayload == false → HTTP GET  (Shelly-style URL-encoded command)
//   - hasPayload == true  → HTTP POST with the pre-expanded body
//   - isLocalUrl() enforced before EVERY send (ADR-003/ADR-032, ADR-057 §3);
//     a policy block is non-retryable and reported as success.
//   - ADR-004 error path: no String from http.errorToString().
//=======================================================================
static bool sendWebhookJob(const WebhookJob& job) {
  // Enforce local-network-only policy on every send (ADR-003/ADR-032, ADR-057 §3)
  if (!isLocalUrl(job.sURL)) {
    DebugTln(F("Webhook: blocked (must target local subnet; see ADR-032)"));
    return true; // policy block is not a retryable error
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(1000); // ADR-057 §4; the webhook task may block, no loop stall

  if (!http.begin(client, job.sURL)) {
    DebugTln(F("Webhook: http.begin() failed (invalid URL?)"));
    return false;
  }

  int code;
  if (job.hasPayload) {
    DebugTf(PSTR("Webhook: POST [%s] payload=%s\r\n"), job.sURL, job.sPayloadExpanded);
    http.addHeader(F("Content-Type"), job.sContentType);
    // HTTPClient::POST(uint8_t*, size_t) takes a non-const pointer but does not
    // modify the buffer; the cast is safe and mirrors the pre-Phase-4 cMsg path.
    code = http.POST(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(job.sPayloadExpanded)),
                     strlen(job.sPayloadExpanded));
  } else {
    DebugTf(PSTR("Webhook: GET  [%s] (state=%s)\r\n"), job.sURL, job.stateOn ? "ON" : "OFF");
    code = http.GET();
  }
  http.end();

  if (code >= 200 && code < 300) {
    DebugTf(PSTR("Webhook: HTTP response code: %d\r\n"), code);
    return true;
  }
  // ADR-004 compliant: no String from http.errorToString()
  char errBuf[32];
  snprintf_P(errBuf, sizeof(errBuf), PSTR("HTTP error %d"), code);
  DebugTf(PSTR("Webhook: %s %s failed: %s\r\n"),
          job.hasPayload ? "POST" : "GET", job.sURL, errBuf);
  return false;
}

//=======================================================================
// The dedicated webhook sender task (ADR-123 Phase-4). Blocks on the job queue
// and, per job, runs the ADR-057 §5 retry contract: up to 3 attempts, 30 s
// backoff between them. Because the task blocks, the retry "wait" is a real
// vTaskDelay (platformTaskDelay), not a loop-spread WH_RETRY_WAIT state.
//
// "Latest-state-supersedes": a fresh edge that arrives while a retry is pending
// already replaced webhookLastState loop-side, and the in-flight gate keeps a
// second job out of the queue until this one finishes — so the device always
// ends in the most recently observed state without queueing a backlog.
//=======================================================================
static void webhookTaskBody(void *arg) {
  (void)arg;
  WebhookJob job;
  for (;;) {
    // Block indefinitely until an edge is enqueued; zero CPU while idle.
    if (!platformQueueReceive(webhookQueue, &job, PLATFORM_QUEUE_WAIT_FOREVER)) {
      continue;
    }
    bool sent = false;
    for (uint8_t attempt = 1; attempt <= 3 && !sent; attempt++) {
      sent = sendWebhookJob(job);
      if (sent) break;
      if (attempt < 3) {
        DebugTf(PSTR("Webhook: send failed, retry %u/3 in 30s\r\n"), attempt);
        platformTaskDelay(30000); // 30 s backoff (ADR-057 §5); task blocks, loop unaffected
      } else {
        DebugTln(F("Webhook: max retries reached, giving up"));
      }
    }
    webhookInFlight = false; // release the gate so the next edge can enqueue
  }
}

//=======================================================================
// startWebhookTask — create the job queue + sender task exactly once (ADR-044),
// called once from setup(). Safe to call before settings load: the task simply
// blocks on an empty queue until evalWebhook()/testWebhook() enqueue a job.
//=======================================================================
void startWebhookTask() {
  if (g_webhookTask != nullptr) return;                 // create exactly once
  if (webhookQueue == nullptr) {
    webhookQueue = platformQueueCreate(WEBHOOK_QUEUE_DEPTH, sizeof(WebhookJob));
  }
  if (webhookQueue == nullptr) {
    DebugTln(F("ERROR: failed to create webhook queue"));
    return;
  }
  // 8192-byte stack: the HTTPClient GET/POST + TLS-free WiFiClient chain is
  // deeper than the PIC task's shallow serial byte-I/O, and the task body also
  // holds a ~644-byte WebhookJob local. This matches the Arduino loop task's
  // default 8 KB stack the send previously ran on (pre-Phase-4).
  g_webhookTask = platformTaskCreatePinned(
      webhookTaskBody, "webhook", 8192, nullptr, 1);
  if (g_webhookTask == nullptr) {
    DebugTln(F("ERROR: failed to create webhook task"));
  } else {
    DebugTln(F("Webhook sender task started"));
  }
}

//=======================================================================
// Fire the webhook for a specific state on demand (for testing).
// Called from the AsyncTCP web-server task (restAPI test endpoint), so the
// OpenTherm-state read inside buildWebhookJob() is wrapped in an OTStateLock.
// Enqueues a job and returns immediately — the blocking send happens on the
// webhook task, never on the AsyncTCP task.
//=======================================================================
void testWebhook(bool testOn) {
  DebugTf(PSTR("Webhook: test requested for state %s\r\n"), testOn ? "ON" : "OFF");
  if (webhookQueue == nullptr) return;
  WebhookJob job;
  bool built;
  {
    // TASK-879: bounded acquire (never the default 0 == portMAX_DELAY). testWebhook
    // runs on the async_tcp task (POST /api/v2/webhook/test); the loop-task writer
    // (processOT) holds otStateMutex across slow I/O, so an unbounded wait here can
    // wedge the WDT-subscribed service task. On timeout buildWebhookJob proceeds
    // unlocked — at worst a slightly-stale test payload.
    OTStateLock stateLock(OT_STATE_READ_LOCK_MS);
    built = buildWebhookJob(testOn, job);
  }
  if (built) {
    if (!platformQueueSend(webhookQueue, &job)) {
      DebugTln(F("Webhook: test enqueue dropped (queue full)"));
    }
  }
}

//=======================================================================
// Evaluate trigger bit state from current OpenTherm status flags.
// Returns the boolean value of the configured trigger bit.
//=======================================================================
static bool evalTriggerBit() {
  int8_t rawTriggerBit = static_cast<int8_t>(settings.webhook.iTriggerBit);
  int8_t clampedTriggerBit = rawTriggerBit;
  if (clampedTriggerBit < 0) clampedTriggerBit = 0;
  else if (clampedTriggerBit > 15) clampedTriggerBit = 15;
  if (clampedTriggerBit != rawTriggerBit) {
    DebugTf(PSTR("Webhook: invalid trigger bit %d, clamped to %d\r\n"),
            rawTriggerBit, clampedTriggerBit);
    settings.webhook.iTriggerBit = clampedTriggerBit;
  }
  return (OTcurrentSystemState.Statusflags & (1U << static_cast<uint8_t>(clampedTriggerBit))) != 0;
}

//=======================================================================
// Edge detection + enqueue (ADR-123 Phase-4; supersedes the cooperative
// WH_IDLE/WH_PENDING/WH_RETRY_WAIT state machine of ADR-048).
//
// What SURVIVES from ADR-048/057, here in the loop:
//   - evalTriggerBit() change detection (intrinsic, cheap, no I/O)
//   - edge-triggered semantics: fire only on OFF->ON / ON->OFF transitions
//   - latest-state-supersedes / convergence: webhookLastState is the last
//     state we ACTED on (enqueued), not merely the last bit observed. It is
//     latched ONLY when a job is enqueued — never while a send is in flight —
//     so once the in-flight gate clears the next tick re-detects the LIVE bit
//     and converges on it. This mirrors the old FSM, which assigned
//     webhookLastState only in WH_IDLE (never during PENDING/RETRY_WAIT).
//   - one-pending-transition-at-a-time (ADR-057 §5): the webhookInFlight gate
//     keeps a second job out of the queue until the task finishes the current
//     send+retry cycle; no backlog is built (best-effort contract).
//
// What COLLAPSED into the task (webhookTaskBody): the loop-spread retry/backoff
// and the blocking HTTP send. evalWebhook() no longer touches HTTPClient and
// never blocks; it runs in the same loop context as the OT consumer so its
// OpenTherm-state reads need no OTStateLock.
//=======================================================================
void evalWebhook() {
  if (!settings.webhook.bEnabled) return;
  if (strlen(settings.webhook.sURLon) == 0 && strlen(settings.webhook.sURLoff) == 0) return;
  if (webhookQueue == nullptr) return;  // task not started yet

  bool bitState = evalTriggerBit();

  // Prime on first observation: latch the initial level without firing.
  if (!webhookInitialized) {
    webhookLastState = bitState;
    webhookInitialized = true;
    return;
  }

  // Compare against the last ACTED-ON state (not the last bit observed).
  if (bitState == webhookLastState) return;  // no edge vs last delivered state

  // A send+retry cycle is still running. Do NOT latch: leave webhookLastState at
  // the last acted state so that when the gate clears the next tick re-detects
  // the live bit and converges on it (handles a toggle during a long retry).
  if (webhookInFlight) return;

  // Free to act on this edge. Latch NOW (matches the old FSM, which latched on
  // the IDLE->PENDING transition). A missing URL for this state is "nothing to
  // send" — still latched, so it does not re-fire the same edge every tick.
  webhookLastState = bitState;

  WebhookJob job;
  if (!buildWebhookJob(bitState, job)) return;  // no URL configured for this state

  DebugTf(PSTR("Webhook: bit changed -> %s, queuing send\r\n"), bitState ? "ON" : "OFF");
  webhookInFlight = true;
  if (!platformQueueSend(webhookQueue, &job)) {
    DebugTln(F("Webhook: enqueue dropped (queue full)"));
    webhookInFlight = false;
  }
}

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

/*********
**  Program  : webhook.ino
**  Version  : v1.3.0-beta
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

  if (!isIp) {
    // Hostname (e.g. shelly.local, shelly-relay) — allowed per ADR-032
    // local-only trust model; DNS resolution is on the trusted LAN.
    return true;
  }

  // Parse the four octets and validate range (use %u to reject negatives)
  unsigned int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
  if (sscanf(hostBuf, "%u.%u.%u.%u", &o1, &o2, &o3, &o4) != 4 ||
      o1 > 255 || o2 > 255 || o3 > 255 || o4 > 255) {
    DebugTln(F("Webhook: URL rejected (malformed IP)"));
    return false;
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
static void expandPayload(const char* tmpl, char* out, size_t outLen, bool stateOn) {
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
    if      (strcmp_P(varName, PSTR("state"))      == 0) { strcpy(val, stateOn ? "ON" : "OFF"); }
    else if (strcmp_P(varName, PSTR("tboiler"))    == 0) { snprintf(val, sizeof(val), "%.1f", OTcurrentSystemState.Tboiler); }
    else if (strcmp_P(varName, PSTR("tr"))         == 0) { snprintf(val, sizeof(val), "%.1f", OTcurrentSystemState.Tr); }
    else if (strcmp_P(varName, PSTR("tset"))       == 0) { snprintf(val, sizeof(val), "%.1f", OTcurrentSystemState.TSet); }
    else if (strcmp_P(varName, PSTR("tdhw"))       == 0) { snprintf(val, sizeof(val), "%.1f", OTcurrentSystemState.Tdhw); }
    else if (strcmp_P(varName, PSTR("relmod"))     == 0) { snprintf(val, sizeof(val), "%.0f", OTcurrentSystemState.RelModLevel); }
    else if (strcmp_P(varName, PSTR("chpressure")) == 0) { snprintf(val, sizeof(val), "%.2f", OTcurrentSystemState.CHPressure); }
    else if (strcmp_P(varName, PSTR("flameon"))    == 0) { strcpy(val, (OTcurrentSystemState.SlaveStatus & (1U << 3)) ? "true" : "false"); }
    else if (strcmp_P(varName, PSTR("chmode"))     == 0) { strcpy(val, (OTcurrentSystemState.SlaveStatus & (1U << 1)) ? "true" : "false"); }
    else if (strcmp_P(varName, PSTR("dhwmode"))    == 0) { strcpy(val, (OTcurrentSystemState.SlaveStatus & (1U << 2)) ? "true" : "false"); }
    else { out[di++] = *p++; continue; }  // unknown variable — pass '{' literally

    size_t valLen = strlen(val);
    size_t avail  = outLen - 1 - di;
    if (valLen > avail) valLen = avail;
    memcpy(out + di, val, valLen);
    di += valLen;
    p = end + 1;  // advance past '}'
  }
  out[di] = '\0';
}

//=======================================================================
// Send the webhook for the given state, regardless of current state.
// Used by the test endpoint and called from evalWebhook() on change.
//
// Behaviour:
//   - settings.webhook.sPayload empty  → HTTP GET (compatible with Shelly
//     and other devices that accept URL-encoded commands)
//   - settings.webhook.sPayload set    → HTTP POST with Content-Type:
//     application/json; {variable} placeholders in the template are
//     expanded to live OpenTherm values before sending.
//
// Example payload for a local Home Assistant webhook:
//   {"state":"{state}","tboiler":{tboiler},"tr":{tr}}
//
// See expandPayload() for the full list of supported variables.
//=======================================================================
// Attempt to send the webhook. Returns true on HTTP 2xx, false on any error.
// Timeout reduced from 3000ms to 1000ms (ADR-048: webhook state machine / minimize main loop blocking).
static bool attemptSendWebhook(bool stateOn) {
  const char* url = stateOn ? settings.webhook.sURLon : settings.webhook.sURLoff;
  if (strlen(url) == 0) {
    DebugTf(PSTR("Webhook: no URL configured for state %s\r\n"), stateOn ? "ON" : "OFF");
    return true; // nothing to send is "success"
  }

  // Enforce local-network-only policy (ADR-003/ADR-032)
  if (!isLocalUrl(url)) {
    DebugTln(F("Webhook: blocked (must target local subnet; see ADR-032)"));
    return true; // policy block is not a retryable error
  }

  bool hasPayload = (settings.webhook.sPayload[0] != '\0');

  // Expand the payload template (static to keep off the 4 KB CONT stack)
  static char expandedPayload[384];
  if (hasPayload) {
    expandPayload(settings.webhook.sPayload, expandedPayload, sizeof(expandedPayload), stateOn);
    DebugTf(PSTR("Webhook: POST [%s] payload=%s\r\n"), url, expandedPayload);
  } else {
    DebugTf(PSTR("Webhook: GET  [%s] (state=%s)\r\n"), url, stateOn ? "ON" : "OFF");
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(1000); // 1-second timeout (was 3s; local LAN should respond <500ms)

  if (!http.begin(client, url)) {
    DebugTln(F("Webhook: http.begin() failed (invalid URL?)"));
    return false;
  }

  yield();
  int code;
  if (hasPayload) {
    const char* ct = (settings.webhook.sContentType[0] != '\0')
                     ? settings.webhook.sContentType
                     : "application/json";
    http.addHeader(F("Content-Type"), ct);
    code = http.POST(expandedPayload);
  } else {
    code = http.GET();
  }
  yield();
  http.end();
  feedWatchDog();

  if (code >= 200 && code < 300) {
    DebugTf(PSTR("Webhook: HTTP response code: %d\r\n"), code);
    return true;
  }
  // ADR-004 compliant: no String from http.errorToString()
  char errBuf[32];
  snprintf_P(errBuf, sizeof(errBuf), PSTR("HTTP error %d"), code);
  DebugTf(PSTR("Webhook: %s %s failed: %s\r\n"),
          hasPayload ? "POST" : "GET", url, errBuf);
  return false;
}

//=======================================================================
// Fire the webhook for a specific state on demand (for testing).
//=======================================================================
void testWebhook(bool testOn) {
  DebugTf(PSTR("Webhook: test requested for state %s\r\n"), testOn ? "ON" : "OFF");
  attemptSendWebhook(testOn);
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
// Non-blocking webhook state machine with retry (ADR-048)
// Replaces evalWebhook() — decouples detection from sending.
// States: WH_IDLE -> WH_PENDING -> WH_RETRY_WAIT -> WH_IDLE
//=======================================================================
enum WebhookState_t { WH_IDLE, WH_PENDING, WH_RETRY_WAIT };
static WebhookState_t webhookState = WH_IDLE;
static bool    webhookPendingStateOn = false;
static uint8_t webhookRetryCount = 0;

void evalWebhook() {
  if (!settings.webhook.bEnabled) return;
  if (strlen(settings.webhook.sURLon) == 0 && strlen(settings.webhook.sURLoff) == 0) return;

  DECLARE_TIMER_SEC(timerWebhookRetry, 30, SKIP_MISSED_TICKS);

  // Always evaluate trigger bit (track latest state even during retry wait)
  bool bitState = evalTriggerBit();

  switch (webhookState) {
    case WH_IDLE:
      if (!webhookInitialized) {
        webhookLastState = bitState;
        webhookInitialized = true;
        break;
      }
      if (bitState != webhookLastState) {
        webhookLastState = bitState;
        webhookPendingStateOn = bitState;
        webhookRetryCount = 0;
        webhookState = WH_PENDING;
        DebugTf(PSTR("Webhook: bit changed -> %s, queuing send\r\n"),
                bitState ? "ON" : "OFF");
      }
      break;

    case WH_PENDING:
      if (WiFi.status() != WL_CONNECTED) break; // wait for WiFi
      {
        bool ok = attemptSendWebhook(webhookPendingStateOn);
        if (ok) {
          webhookState = WH_IDLE;
          webhookRetryCount = 0;
        } else {
          webhookRetryCount++;
          if (webhookRetryCount >= 3) {
            DebugTln(F("Webhook: max retries reached, giving up"));
            webhookState = WH_IDLE;
          } else {
            RESTART_TIMER(timerWebhookRetry);
            webhookState = WH_RETRY_WAIT;
            DebugTf(PSTR("Webhook: send failed, retry %d/3 in 30s\r\n"), webhookRetryCount);
          }
        }
      }
      break;

    case WH_RETRY_WAIT:
      if (DUE(timerWebhookRetry)) webhookState = WH_PENDING;
      break;
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

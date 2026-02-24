/*********
**  Program  : webhook.ino
**  Version  : v1.1.0-beta
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
void evalWebhook() {
  if (!settingWebhookEnabled) return;
  if (strlen(settingWebhookURLon) == 0 && strlen(settingWebhookURLoff) == 0) return;

  // Compute current bit state using the same Statusflags layout as GPIO outputs
  bool bitState = (OTcurrentSystemState.Statusflags & (1U << settingWebhookTriggerBit)) != 0;

  // On first call, initialise the last-known state without firing the webhook
  if (!webhookInitialized) {
    webhookLastState = bitState;
    webhookInitialized = true;
    return;
  }

  // Only act when the state has changed
  if (bitState == webhookLastState) return;
  webhookLastState = bitState;

  const char* url = bitState ? settingWebhookURLon : settingWebhookURLoff;
  if (strlen(url) == 0) {
    DebugTf(PSTR("Webhook: state changed to %s but no URL configured\r\n"), bitState ? "ON" : "OFF");
    return;
  }

  DebugTf(PSTR("Webhook: bit %d changed to %s, calling [%s]\r\n"),
          settingWebhookTriggerBit, bitState ? "ON" : "OFF", url);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(3000); // 3 second timeout to avoid stalling the main loop

  if (http.begin(client, url)) {
    int code = http.GET();
    if (code > 0) {
      DebugTf(PSTR("Webhook: HTTP response code: %d\r\n"), code);
    } else {
      DebugTf(PSTR("Webhook: HTTP GET failed, error: %s\r\n"), http.errorToString(code).c_str());
    }
    http.end();
  } else {
    DebugTln(F("Webhook: http.begin() failed (invalid URL?)"));
  }
  feedWatchDog(); // ensure watchdog is fed after potentially slow HTTP request
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

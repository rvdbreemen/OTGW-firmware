/*
***************************************************************************
**  Program  : test/host/test_json_and_webhook.cpp
**
**  Host harness for two shipped functions:
**    - extractJsonField()  (src/OTGW-firmware/jsonStuff.ino)
**    - expandPayload()     (src/OTGW-firmware/webhook.ino)
**
**  The functions are NOT copied here. build_and_run.ps1 slices them out of
**  the real sources between the "host-testable ...: BEGIN/END" sentinels and
**  writes them to generated/*.inc, which this file includes. Editing the
**  shipped source therefore changes what this test compiles.
**
**  Run: powershell -File test/host/build_and_run.ps1     (non-zero on failure)
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include "arduino_shim.h"

HostOTState OTcurrentSystemState;

// ---- code under test, sliced verbatim from the shipped sources -------------
#include "generated/json_scanner.inc"
#include "generated/expand_payload.inc"

// ---- tiny assert framework -------------------------------------------------
static int g_failures = 0;
static int g_checks   = 0;

static void check(bool ok, const char* name, const char* detail) {
  g_checks++;
  if (ok) {
    printf("  ok   %s\n", name);
  } else {
    g_failures++;
    printf("  FAIL %s\n         %s\n", name, detail);
  }
}

static void checkStr(const char* got, const char* want, const char* name) {
  char detail[512];
  snprintf(detail, sizeof(detail), "expected [%s] but got [%s]", want, got);
  check(strcmp(got, want) == 0, name, detail);
}

int main() {
  printf("== extractJsonField (src/OTGW-firmware/jsonStuff.ino) ==\n");

  // (a) a 200-char quoted value must round-trip byte-identically into a
  //     201-byte destination (settings.webhook.sPayload is char[201]).
  {
    char big[201];
    memset(big, 'x', 200);
    big[200] = '\0';
    char body[512];
    snprintf(body, sizeof(body), "{\"name\":\"webhookpayload\",\"value\":\"%s\"}", big);
    char out[201];
    bool ok = extractJsonField(body, F("value"), out, sizeof(out));
    check(ok, "a1 200-char value into char[201] returns true", "returned false");
    checkStr(out, big, "a2 200-char value round-trips byte-identically");
  }

  // (b) a value one byte too long must be REJECTED, not truncated-and-accepted.
  //     This is the defect: the old code stored 200 of 201 chars and said true.
  {
    char big[202];
    memset(big, 'y', 201);
    big[201] = '\0';
    char body[512];
    snprintf(body, sizeof(body), "{\"value\":\"%s\"}", big);
    char out[201];
    memset(out, 0x7f, sizeof(out));
    bool ok = extractJsonField(body, F("value"), out, sizeof(out));
    check(!ok, "b1 201-char value into char[201] returns FALSE",
          "returned true -> value was silently truncated while reporting success");
    check(out[0] == '\0', "b2 rejected value leaves the destination empty",
          "destination holds a truncated value after a false return");
  }

  // (b'') an unterminated string value must also leave a NUL-terminated (empty)
  //       destination - callers such as handleSAT only test result[0].
  {
    const char* body = "{\"value\":\"abc";     // no closing quote
    char out[32];
    memset(out, 0x7f, sizeof(out));
    bool ok = extractJsonField(body, F("value"), out, sizeof(out));
    check(!ok, "b4 unterminated value returns FALSE", "returned true");
    check(out[0] == '\0', "b5 unterminated value leaves the destination empty and terminated",
          "destination holds un-terminated bytes after a false return");
  }

  // (b') the same rule for a bare (unquoted) token.
  {
    char body[64];
    snprintf(body, sizeof(body), "{\"value\":123456789}");
    char out[5];
    bool ok = extractJsonField(body, F("value"), out, sizeof(out));
    check(!ok, "b3 oversized bare token returns FALSE",
          "returned true -> number silently halved");
  }

  // (c) short values still work (regression guard).
  {
    const char* body = "{\"name\":\"hostname\",\"value\":\"otgw\",\"other\":42}";
    char out[64];
    check(extractJsonField(body, F("value"), out, sizeof(out)), "c1 short value returns true", "returned false");
    checkStr(out, "otgw", "c2 short value content");
    check(extractJsonField(body, F("other"), out, sizeof(out)), "c3 bare token returns true", "returned false");
    checkStr(out, "42", "c4 bare token content");
    check(!extractJsonField(body, F("absent"), out, sizeof(out)), "c5 absent key returns false", "returned true");
    // exact fit: 4 chars into a 5-byte buffer
    char tight[5];
    check(extractJsonField(body, F("value"), tight, sizeof(tight)), "c6 exact-fit value returns true", "returned false");
    checkStr(tight, "otgw", "c7 exact-fit value content");
  }

  // (d) escape handling still works, including a \u escape and an escape that
  //     lands exactly on the capacity boundary.
  {
    const char* body = "{\"value\":\"a\\\"b\\\\c\\nd\\u00e9\"}";
    char out[32];
    check(extractJsonField(body, F("value"), out, sizeof(out)), "d1 escaped value returns true", "returned false");
    checkStr(out, "a\"b\\c\nd\xc3\xa9", "d2 escaped value decodes (quote, backslash, newline, U+00E9)");

    // "abé" decodes to 4 bytes; a 5-byte buffer fits it exactly.
    const char* b2 = "{\"value\":\"ab\\u00e9\"}";
    char five[5];
    check(extractJsonField(b2, F("value"), five, sizeof(five)), "d3 multibyte exactly filling the buffer returns true", "returned false");
    checkStr(five, "ab\xc3\xa9", "d4 multibyte exact-fit content");

    // the same value into a 4-byte buffer cannot fit the 2-byte U+00E9.
    char four[4];
    check(!extractJsonField(b2, F("value"), four, sizeof(four)), "d5 multibyte that does not fit returns FALSE",
          "returned true -> value truncated mid-character");
  }

  // (e) design guard: a key longer than the internal 48-byte key buffer must
  //     still be SKIPPED, so a later key in the same object is found. This is
  //     what a "return NULL on truncation" shortcut in xjfReadString would break.
  {
    char longKey[80];
    memset(longKey, 'k', 79);
    longKey[79] = '\0';
    char body[256];
    snprintf(body, sizeof(body), "{\"%s\":\"junk\",\"value\":\"found\"}", longKey);
    char out[32];
    check(extractJsonField(body, F("value"), out, sizeof(out)), "e1 key after an over-long key is still found", "returned false");
    checkStr(out, "found", "e2 value after an over-long key");
  }

  // (f) a key-looking substring inside a value must never match (pre-existing
  //     contract, guarded so the truncation change cannot regress it).
  {
    const char* body = "{\"other\":\"value\\\":\\\"decoy\",\"value\":\"real\"}";
    char out[32];
    check(extractJsonField(body, F("value"), out, sizeof(out)), "f1 decoy body still parses", "returned false");
    checkStr(out, "real", "f2 key inside a string value is not matched");
  }

  printf("== expandPayload (src/OTGW-firmware/webhook.ino) ==\n");

  // Task B: with Tr never observed (NAN-init, OTGW-Core.h:73) the documented
  // template must still yield parseable JSON.
  {
    OTcurrentSystemState = HostOTState();           // Tr = NAN
    char out[128];
    expandPayload("{\"tr\":{tr}}", out, sizeof(out), true);
    checkStr(out, "{\"tr\":null}", "g1 {tr} with no reading expands to the JSON literal null");
    check(strstr(out, "--") == NULL, "g2 {tr} never emits the non-JSON placeholder --",
          "body contains '--', which no JSON parser accepts in a numeric position");
  }

  // A real reading is unchanged.
  {
    OTcurrentSystemState = HostOTState();
    OTcurrentSystemState.Tr = 21.5f;
    char out[128];
    expandPayload("{\"tr\":{tr}}", out, sizeof(out), true);
    checkStr(out, "{\"tr\":21.5}", "g3 {tr} with a reading is unchanged");
  }

  // The 0.0f-initialised numeric variables are deliberately untouched.
  {
    OTcurrentSystemState = HostOTState();
    char out[256];
    expandPayload("{\"tb\":{tboiler},\"ts\":{tset},\"td\":{tdhw},\"rm\":{relmod},\"cp\":{chpressure},\"st\":\"{state}\",\"fl\":{flameon}}",
                  out, sizeof(out), true);
    checkStr(out,
             "{\"tb\":0.0,\"ts\":0.0,\"td\":0.0,\"rm\":0,\"cp\":0.00,\"st\":\"ON\",\"fl\":false}",
             "g4 0.0f-initialised numerics and the boolean/string vars are unchanged");
  }

  printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}

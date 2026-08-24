//=======================================================================
// test_extractJsonField.cpp — host-compiled regression test for the
// silent-truncation defect in extractJsonField()'s quoted-string branch.
//
// The REAL src/OTGW-firmware/jsonStuff.ino is included below; nothing under
// test is copied or reimplemented here. Only the platform is emulated
// (arduino_shim.h).
//
// Build + run: test\run_tests.bat   (exits non-zero on failure)
//=======================================================================
#include "arduino_shim.h"

// ---- code under test, verbatim ----------------------------------------
#include "../../src/OTGW-firmware/jsonStuff.ino"
// -----------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <string>

static int g_failures = 0;
static int g_checks   = 0;

static void check(bool ok, const char* what) {
  g_checks++;
  if (ok) {
    printf("  [ OK ] %s\n", what);
  } else {
    g_failures++;
    printf("  [FAIL] %s\n", what);
  }
}

// Build {"name":"settings.webhook.sPayload","value":"<val>"} with val
// inserted verbatim (val must already be JSON-escaped by the caller).
static std::string body(const std::string& escapedValue) {
  return std::string("{\"name\":\"settings.webhook.sPayload\",\"value\":\"")
       + escapedValue + "\"}";
}

int main() {
  printf("extractJsonField() host tests\n");

  //--------------------------------------------------------------------
  // (a) A ~200 char value round-trips byte-identical into a 201-byte dest,
  //     matching settings.webhook.sPayload[201] / index.js maxlen 200.
  //--------------------------------------------------------------------
  printf("\n(a) 200-char payload into char[201]\n");
  {
    std::string payload;
    for (int i = 0; i < 200; i++) payload += char('A' + (i % 26));
    std::string json = body(payload);

    char dest[201];
    memset(dest, 0x7F, sizeof(dest));
    bool ok = extractJsonField(json.c_str(), F("value"), dest, sizeof(dest));
    check(ok, "returns true");
    check(strlen(dest) == 200, "length is 200 (not truncated)");
    check(strcmp(dest, payload.c_str()) == 0, "value round-trips byte-identical");
  }

  //--------------------------------------------------------------------
  // (b) THE DEFECT: a value that does not fit must return false, not a
  //     silently truncated string with true. 200 chars into char[150] is
  //     exactly the shipped postSettings() case before the fix.
  //--------------------------------------------------------------------
  printf("\n(b) 200-char payload into an undersized char[150]\n");
  {
    std::string payload;
    for (int i = 0; i < 200; i++) payload += char('A' + (i % 26));
    std::string json = body(payload);

    char dest[150];
    memset(dest, 0x7F, sizeof(dest));
    bool ok = extractJsonField(json.c_str(), F("value"), dest, sizeof(dest));
    check(!ok, "returns false when the value does not fit");
    check(!(ok && strlen(dest) == 149),
          "does NOT report success with a truncated 149-char value");
  }

  //--------------------------------------------------------------------
  // (c) Exact-fit boundary: N chars into char[N+1] must still succeed.
  //--------------------------------------------------------------------
  printf("\n(c) exact-fit boundary\n");
  {
    std::string payload(32, 'x');
    std::string json = body(payload);

    char fits[33];
    check(extractJsonField(json.c_str(), F("value"), fits, sizeof(fits)),
          "32 chars into char[33] succeeds");
    check(strcmp(fits, payload.c_str()) == 0, "exact-fit value is intact");

    char oneShort[32];
    check(!extractJsonField(json.c_str(), F("value"), oneShort, sizeof(oneShort)),
          "32 chars into char[32] fails (one byte short)");
  }

  //--------------------------------------------------------------------
  // (d) No regression on the short/simple cases.
  //--------------------------------------------------------------------
  printf("\n(d) short values and the unquoted branch (regression)\n");
  {
    char dest[64];
    check(extractJsonField("{\"name\":\"settingHostname\",\"value\":\"otgw\"}",
                           F("value"), dest, sizeof(dest)) &&
          strcmp(dest, "otgw") == 0, "short quoted string");

    check(extractJsonField("{\"name\":\"x\",\"value\":true}",
                           F("value"), dest, sizeof(dest)) &&
          strcmp(dest, "true") == 0, "unquoted bool literal");

    check(extractJsonField("{\"name\":\"x\",\"value\":42}",
                           F("value"), dest, sizeof(dest)) &&
          strcmp(dest, "42") == 0, "unquoted number");

    check(extractJsonField("{\"name\":\"x\",\"value\":\"\"}",
                           F("value"), dest, sizeof(dest)) &&
          dest[0] == '\0', "empty quoted string");

    check(!extractJsonField("{\"name\":\"x\"}", F("value"), dest, sizeof(dest)),
          "missing field returns false");

    check(!extractJsonField("{\"name\":\"x\",\"value\":\"unterminated}",
                            F("value"), dest, sizeof(dest)),
          "missing closing quote returns false");
  }

  //--------------------------------------------------------------------
  // (e) Escape handling still works, and escapes count toward the fit.
  //--------------------------------------------------------------------
  printf("\n(e) escape handling\n");
  {
    char dest[64];
    check(extractJsonField("{\"value\":\"{\\\"temp\\\": 21.5}\"}",
                           F("value"), dest, sizeof(dest)) &&
          strcmp(dest, "{\"temp\": 21.5}") == 0,
          "escaped quotes unescape (realistic webhook payload template)");

    check(extractJsonField("{\"value\":\"a\\\\b\\nc\\td\"}",
                           F("value"), dest, sizeof(dest)) &&
          strcmp(dest, "a\\b\nc\td") == 0,
          "backslash, newline and tab escapes");

    // 20 escaped quotes decode to 20 chars: fits in [21], not in [20].
    std::string esc;
    for (int i = 0; i < 20; i++) esc += "\\\"";
    std::string json = std::string("{\"value\":\"") + esc + "\"}";

    char fits[21];
    check(extractJsonField(json.c_str(), F("value"), fits, sizeof(fits)) &&
          strlen(fits) == 20, "20 unescaped chars fit char[21]");

    char tooSmall[20];
    check(!extractJsonField(json.c_str(), F("value"), tooSmall, sizeof(tooSmall)),
          "escaped value that does not fit returns false");
  }

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  if (g_failures) { printf("RESULT: FAIL\n"); return 1; }
  printf("RESULT: PASS\n");
  return 0;
}

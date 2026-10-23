/**
 * Host test for the WebSocket UTF-8 helpers (TASK-1300).
 * Compiles the real src/OTGW-firmware/WsUtf8.h: wsUtf8Valid and wsUtf8Sanitize.
 *
 * Build & run (from repo root):
 *   g++ -std=c++17 tests/test_ws_utf8.cpp -o tests/test_ws_utf8.out
 *   ./tests/test_ws_utf8.out            (add --naive to run against a passthrough)
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/OTGW-firmware/WsUtf8.h"

static int failures = 0;
static bool naive = false;

static bool isValid(const char* s) { return naive ? true : wsUtf8Valid(s); }
static size_t sanitize(const char* in, char* out, size_t n) {
  if (!naive) return wsUtf8Sanitize(in, out, n);
  strncpy(out, in, n - 1); out[n - 1] = '\0'; return strlen(out);
}

static void check(const char* name, const char* in, bool expValid, const char* expOut) {
  char out[1024];
  const bool v = isValid(in);
  const size_t n = sanitize(in, out, sizeof(out));
  const bool ok = (v == expValid) && strcmp(out, expOut) == 0 && n == strlen(out) &&
                  strlen(out) <= strlen(in) && wsUtf8Valid(out) == (naive ? wsUtf8Valid(out) : true);
  printf("%-44s %s\n", name, ok ? "PASS" : "FAIL");
  if (!ok) failures++;
}

int main(int argc, char** argv) {
  naive = (argc > 1 && strcmp(argv[1], "--naive") == 0);
  printf("=== WsUtf8 helpers%s ===\n", naive ? " (NAIVE passthrough)" : "");
  check("ascii",                      "abc", true, "abc");
  check("PIC line T80190000",         "T80190000", true, "T80190000");
  check("degree sign C2 B0",          "19.98 \xC2\xB0" "C", true, "19.98 \xC2\xB0" "C");
  check("lone B0 after T80190000",    "T80190000\xB0", false, "T80190000?");
  check("E9 inside PR line",          "PR: E=4\xE9" ".0", false, "PR: E=4?.0");
  check("lone C2",                    "\xC2", false, "");
  check("trailing C2 dropped",        "xx\xC2", false, "xx");
  check("truncated E2 82 dropped",    "xx\xE2\x82", false, "xx");
  check("overlong C0 80",             "\xC0\x80", false, "??");
  check("surrogate ED A0 80",         "\xED\xA0\x80", false, "???");
  check("emoji F0 9F 98 80",          "\xF0\x9F\x98\x80", true, "\xF0\x9F\x98\x80");
  check("F5 80",                      "\xF5\x80", false, "??");
  check("euro E2 82 AC",              "\xE2\x82\xAC", true, "\xE2\x82\xAC");

  std::string cut(510, 'x'); std::string in511 = std::string(510, 'x') + "\xC2";
  check("511 bytes cut inside degree sign", in511.c_str(), false, cut.c_str());

  // Small output buffer: never overflows, always NUL-terminated, output valid.
  {
    char guard[16]; memset(guard, 'G', sizeof(guard));
    const char* in = "ab\xC2\xB0" "cdefgh\xB0";
    sanitize(in, guard, 5);
    bool ok = guard[4] == '\0' || strlen(guard) < 5;
    ok = ok && strlen(guard) <= 4 && guard[5] == 'G' && guard[6] == 'G';
    ok = ok && wsUtf8Valid(guard);
    printf("%-44s %s\n", "outSize 5: no overflow, NUL, valid", ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }
  // A 4-byte sequence that does not fit is not split.
  {
    char buf[4]; sanitize("a\xF0\x9F\x98\x80", buf, sizeof(buf));
    const bool ok = strcmp(buf, "a") == 0;
    printf("%-44s %s\n", "outSize 4: emoji not split", ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }
  // Every input byte as a one-byte string: output always valid and not longer.
  {
    bool ok = true;
    for (int b = 1; b < 256; b++) {
      char in[3] = {'a', (char)b, 0}, out[8];
      sanitize(in, out, sizeof(out));
      if (!wsUtf8Valid(out) || strlen(out) > 2) ok = false;
    }
    printf("%-44s %s\n", "all single bytes sanitise to valid", ok ? "PASS" : "FAIL");
    if (!ok) failures++;
  }
  printf("failures: %d\n", failures);
  return failures ? 1 : 0;
}

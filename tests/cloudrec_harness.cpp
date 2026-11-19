// Host harness for src/OTGW-firmware/CloudRecCodec.h (TASK-2901). Compiled by tests/test_cloudrec_codec.py.
//
// Reads events from stdin, one per line, feeds them to the real crec::Recorder (closing a record and opening a
// new one when an append does not fit, as the firmware does) and prints every closed record as
// "REC <t0_ms> <hex payload>". Events:
//   B <t0_ms>                                          open the first record
//   O <t_ms> <letter> <frame hex>                      OT frame
//   S <t_ms> <format index> <text hex>                 SATTICK line
//   H <heap> <block>                                   heap values of the next line prefix
//   P <t_us> <fmt addr> <fn addr> <line> <fmt hex> <printed text hex> <args...>   printf line; args <type>:<value>,
//                                                     type d u f c s(hex); the text is the line raw fallback
//   L <t_us> <str addr> <fn addr> <line>               println line
//   X <t_us> <fn addr> <line> <text hex>               line raw
//   V <t_ms> <type> <payload hex>                      event (SEC_EVT, TASK-1473)
//   G <items> <records>                                count items/records as lost (SEC_LOST in the next record)
//   E                                                  close the last record
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../src/OTGW-firmware/CloudRecCodec.h"

using namespace crec;

static std::string unhex(const char *h) {
  std::string s;
  for (; h[0] && h[1]; h += 2) { unsigned v; sscanf(h, "%2x", &v); s.push_back((char)v); }
  return s;
}

struct VecSrc {
  std::vector<std::string> args;
  size_t i = 0;
  std::string strbuf;
  const char *next() { return i < args.size() ? args[i++].c_str() : "d:0"; }
  int64_t sint(int) { return strtoll(next() + 2, nullptr, 10); }
  uint64_t uint(int) { return strtoull(next() + 2, nullptr, 10); }
  double dbl() { return strtod(next() + 2, nullptr); }
  int chr() { return (int)strtol(next() + 2, nullptr, 10); }
  const char *str() { strbuf = unhex(next() + 2); return strbuf.c_str(); }
};

static Recorder rec;
static uint8_t out[REC_MAX_BYTES];
static uint32_t heapNow, blockNow;

static void emit() {
  if (!rec.open || rec.empty()) return;
  size_t n = rec.close(out, sizeof out);
  printf("REC %lld ", (long long)rec.t0Ms);
  for (size_t i = 0; i < n; i++) printf("%02x", out[i]);
  printf("\n");
}

// Runs an append; when it does not fit, closes the record, opens one at tMs and tries once more.
template <typename F> static void append(int64_t tMs, F f) {
  if (f()) return;
  emit();
  rec.begin(tMs);
  if (!f()) fprintf(stderr, "item does not fit an empty record\n");
}

int main() {
  static char line[1 << 16];
  while (fgets(line, sizeof line, stdin)) {
    line[strcspn(line, "\r\n")] = 0;
    std::vector<char *> tok;
    for (char *s = strtok(line, " "); s; s = strtok(nullptr, " ")) tok.push_back(s);
    if (tok.empty()) continue;
    char k = tok[0][0];
    if (k == 'B') { rec.begin(strtoll(tok[1], nullptr, 10)); continue; }
    if (k == 'E') { emit(); continue; }
    if (k == 'H') { heapNow = strtoul(tok[1], nullptr, 10); blockNow = strtoul(tok[2], nullptr, 10); continue; }
    if (k == 'V') {
      int64_t t = strtoll(tok[1], nullptr, 10);
      uint8_t ty = (uint8_t)atoi(tok[2]);
      std::string pl = tok.size() > 3 ? unhex(tok[3]) : std::string();
      append(t, [&] { return rec.event(t, ty, (const uint8_t *)pl.data(), pl.size()); });
      continue;
    }
    if (k == 'G') { rec.lostItems += strtoul(tok[1], nullptr, 10); rec.lostRecords += strtoul(tok[2], nullptr, 10); continue; }
    if (k == 'O') {
      int64_t t = strtoll(tok[1], nullptr, 10);
      uint32_t f = strtoul(tok[3], nullptr, 16);
      char L = tok[2][0];
      append(t, [&] { return rec.otFrame(t, L, f); });
    } else if (k == 'S') {
      int64_t t = strtoll(tok[1], nullptr, 10);
      int fi = atoi(tok[2]);
      std::string text = unhex(tok[3]);
      append(t, [&] { return rec.satTick(t, fi, text.c_str()); });
    } else if (k == 'P' || k == 'L' || k == 'X') {
      int64_t tUs = strtoll(tok[1], nullptr, 10);
      append(tUs / 1000, [&] {
        size_t mark = rec.dbgMark();
        DbgState before = rec.dbgSt;
        dbgHeap(rec.dbg, rec.dbgSt, heapNow, blockNow);
        if (k == 'P') {
          uint32_t fa = strtoul(tok[2], nullptr, 10), na = strtoul(tok[3], nullptr, 10), ln = strtoul(tok[4], nullptr, 10);
          std::string fmt = unhex(tok[5]), text = unhex(tok[6]);
          VecSrc src;
          for (size_t i = 7; i < tok.size(); i++) src.args.push_back(tok[i]);
          if (rec.dbg.full) return rec.dbgCommit(mark, before);
          TokResult tr = dbgPrintfTokens(rec, tUs, fa, na, ln, fmt.c_str(), src, false);
          if (tr == TOK_OK) return true;
          if (tr == TOK_FULL) { rec.dbg.n = mark; rec.dbgSt = before; return false; }
          dbgLineRaw(rec.dbg, rec.dbgSt, tUs, na, ln, text.data(), text.size());
        } else
        if (k == 'L') {
          dbgPrintln(rec.dbg, rec.dbgSt, tUs, strtoul(tok[2], nullptr, 10), strtoul(tok[3], nullptr, 10),
                     strtoul(tok[4], nullptr, 10));
        } else {
          std::string raw = unhex(tok[4]);
          dbgLineRaw(rec.dbg, rec.dbgSt, tUs, strtoul(tok[2], nullptr, 10), strtoul(tok[3], nullptr, 10), raw.data(), raw.size());
        }
        return rec.dbgCommit(mark, before);
      });
    }
  }
  return 0;
}

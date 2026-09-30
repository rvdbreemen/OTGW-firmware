/*
***************************************************************************
**  Program  : test/host/test_loopback_range.cpp
**
**  Host harness for simulateLoopbackResponse(), the OT-Direct loopback
**  boiler simulator in src/OTGW-firmware/OTDirect.ino (TASK-1072).
**
**  The code under test is NOT copied here. build_and_run_loopback.ps1
**  slices it out of OTDirect.ino by anchor (a signature line up to its
**  matching closing brace, or one declaration line) and writes it, in
**  source order, to generated/loopback/<rev>/loopback_slice.inc:
**    - the forward declaration of buildOTResponse()
**    - setOTParityBit()
**    - the otLoopbackData[] table
**    - simulateLoopbackResponse()
**    - buildOTResponse()
**  The runner compiles this same file once per revision, with that
**  revision's directory on the include path.
**
**  pgm_read_word() and pgm_read_byte() map to a reader that checks the
**  address against otLoopbackData[]. An in-range read returns the table
**  value. An out-of-range read is counted and returns 0xBEEF without
**  dereferencing. On the ESP32 the same read returns whatever flash follows
**  the table; 0xBEEF is a deterministic stand-in for that value.
**
**  Cases: every MsgID 0-255 x every message type 0-7 x request data
**  0x0000, 0x1234 and 0xFFFF (6144 calls). Contract checks C1-C6 below.
**  Exit 0 when every check holds, 1 otherwise.
**
**  Usage: test_loopback_range.exe [dump-dir]
**    With a dump-dir it writes one line per case to
**    <dump-dir>/cases_ids000-127.txt and <dump-dir>/cases_ids128-255.txt,
**    so two revisions can be compared byte for byte.
**
**  Run: powershell -NoProfile -ExecutionPolicy Bypass -File test/host/build_and_run_loopback.ps1
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

// ---- platform surface the slice needs ---------------------------------------
// One address space on the host: PROGMEM is empty, as it is on ESP32 Arduino.
#define PROGMEM

static unsigned long  g_tableReads = 0;          // every pgm_read_* call
static unsigned long  g_oobReads   = 0;          // pgm_read_* calls outside the table
static const uint16_t kOobStandIn  = 0xBEEF;     // returned for an out-of-range read

// Integer compares only, so the check never forms an out-of-range pointer.
static bool hostInTable(const void* addr, size_t width, const void* base, size_t size) {
  const uintptr_t a  = (uintptr_t)addr;
  const uintptr_t lo = (uintptr_t)base;
  return a >= lo && a + width <= lo + size;
}
static uint16_t hostPgmReadWord(const void* addr, const void* base, size_t size) {
  g_tableReads++;
  if (!hostInTable(addr, sizeof(uint16_t), base, size)) { g_oobReads++; return kOobStandIn; }
  return *(const uint16_t*)addr;
}
static uint8_t hostPgmReadByte(const void* addr, const void* base, size_t size) {
  g_tableReads++;
  if (!hostInTable(addr, 1, base, size)) { g_oobReads++; return (uint8_t)(kOobStandIn & 0xFF); }
  return *(const uint8_t*)addr;
}
// The macros expand inside the sliced function, where otLoopbackData is in scope.
#define pgm_read_word(addr) hostPgmReadWord((const void*)(addr), otLoopbackData, sizeof(otLoopbackData))
#define pgm_read_byte(addr) hostPgmReadByte((const void*)(addr), otLoopbackData, sizeof(otLoopbackData))

// ---- code under test, sliced by anchor from OTDirect.ino ---------------------
#include "loopback_slice.inc"

#ifndef LOOPBACK_SLICE_ORIGIN
#define LOOPBACK_SLICE_ORIGIN "(unknown)"
#endif

// ---- harness ------------------------------------------------------------------
// Parity is computed here by counting bits, independent of setOTParityBit().
static bool hasEvenParity(uint32_t f) {
  int n = 0;
  for (int b = 0; b < 32; b++) n += (int)((f >> b) & 1u);
  return (n & 1) == 0;
}
static uint32_t withParity(uint32_t f) {
  f &= 0x7FFFFFFFu;
  return hasEvenParity(f) ? f : (f | 0x80000000u);
}

static const char* typeName(unsigned t) {
  static const char* const names[8] = {
    "READ_DATA", "WRITE_DATA", "INVALID_DATA", "RESERVED_3",
    "READ_ACK", "WRITE_ACK", "DATA_INVALID", "UNKNOWN_DATA_ID"
  };
  return names[t & 7];
}

enum : unsigned { T_READ_DATA = 0, T_WRITE_DATA = 1, T_READ_ACK = 4, T_WRITE_ACK = 5, T_UNKNOWN_DATA_ID = 7 };

struct Category {
  const char*   id;
  const char*   title;
  unsigned long checks;
  unsigned long fails;
};
static Category g_cat[] = {
  { "C1", "no read outside otLoopbackData[] (every id, type and data)", 0, 0 },
  { "C2", "every response has even parity (bit count over all 32 bits)", 0, 0 },
  { "C3", "every response echoes the request MsgID, spare bits 24-27 zero", 0, 0 },
  { "C4", "WRITE_DATA of ids 0-255 answered WRITE_ACK echoing the data (current behaviour, OT spec 4.4.2 open point)", 0, 0 },
  { "C5", "READ_DATA of ids inside the table answered from otLoopbackData[]", 0, 0 },
  { "C6", "non-write request for an id past the table answered UNKNOWN_DATA_ID with data 0", 0, 0 },
};
static const int kCats = (int)(sizeof(g_cat) / sizeof(g_cat[0]));

static void check(int c, bool ok, unsigned id, unsigned type, uint16_t data,
                  unsigned long resp, const char* want) {
  g_cat[c].checks++;
  if (ok) return;
  g_cat[c].fails++;
  if (g_cat[c].fails <= 3) {   // a few examples per category, not thousands of lines
    printf("  FAIL %s id=%3u %-12s data=0x%04X -> 0x%08lX (%s, data 0x%04lX); want %s\n",
           g_cat[c].id, id, typeName(type), (unsigned)data, resp,
           typeName((unsigned)(resp >> 28) & 7u), resp & 0xFFFFul, want);
  }
}

static constexpr uint16_t kData[]    = { 0x0000, 0x1234, 0xFFFF };   // request data values
static constexpr unsigned kDataCount = (unsigned)(sizeof(kData) / sizeof(kData[0]));

int main(int argc, char** argv) {
  const unsigned tableEntries = (unsigned)(sizeof(otLoopbackData) / sizeof(otLoopbackData[0]));

  printf("== simulateLoopbackResponse (src/OTGW-firmware/OTDirect.ino) ==\n");
  printf("slice origin : %s\n", LOOPBACK_SLICE_ORIGIN);
  printf("table entries: %u (ids 0-%u inside, %u-255 past the end)\n",
         tableEntries, tableEntries - 1, tableEntries);

  FILE* dumpLo = nullptr;
  FILE* dumpHi = nullptr;
  if (argc > 1) {
    std::string dir = argv[1];
    dumpLo = fopen((dir + "/cases_ids000-127.txt").c_str(), "wb");
    dumpHi = fopen((dir + "/cases_ids128-255.txt").c_str(), "wb");
    if (!dumpLo || !dumpHi) { printf("cannot open dump files in %s\n", argv[1]); return 3; }
  }

  static unsigned long resp[256][8][kDataCount];
  unsigned long cases = 0;
  unsigned long readHiCases = 0, readHiReadAck = 0, readHiStandIn = 0;
  unsigned long nonWriteHiCases = 0, nonWriteHiUnknown0 = 0;
  unsigned long writeHiCases = 0, writeHiEcho = 0;
  unsigned long writeTableReads = 0;

  for (unsigned id = 0; id < 256; id++) {
    for (unsigned type = 0; type < 8; type++) {
      for (unsigned d = 0; d < kDataCount; d++) {
        const uint16_t data = kData[d];
        const uint32_t req  = withParity(((uint32_t)type << 28) | ((uint32_t)id << 16) | data);

        const unsigned long readsBefore = g_tableReads;
        const unsigned long oobBefore   = g_oobReads;
        const unsigned long r = simulateLoopbackResponse((unsigned long)req);
        const unsigned long oobThis   = g_oobReads - oobBefore;
        const unsigned long readsThis = g_tableReads - readsBefore;
        resp[id][type][d] = r;
        cases++;

        const unsigned rType  = (unsigned)(r >> 28) & 7u;
        const unsigned rId    = (unsigned)(r >> 16) & 0xFFu;
        const unsigned rSpare = (unsigned)(r >> 24) & 0x0Fu;
        const uint16_t rData  = (uint16_t)(r & 0xFFFFu);
        const bool     inside = id < tableEntries;

        FILE* dump = inside ? dumpLo : dumpHi;
        if (dump) {
          fprintf(dump, "%03u %u %04X req=%08lX -> %08lX oob=%lu\n",
                  id, type, (unsigned)data, (unsigned long)req, r, oobThis);
        }

        check(0, oobThis == 0, id, type, data, r, "no out-of-range table read");
        check(1, hasEvenParity((uint32_t)r), id, type, data, r, "even parity");
        check(2, rId == id && rSpare == 0, id, type, data, r, "same MsgID, spare bits 0");

        if (type == T_WRITE_DATA) {
          writeTableReads += readsThis;
          check(3, rType == T_WRITE_ACK && rData == data, id, type, data, r, "WRITE_ACK echoing the data");
          if (!inside) {
            writeHiCases++;
            if (rType == T_WRITE_ACK && rData == data) writeHiEcho++;
          }
          continue;
        }

        if (type == T_READ_DATA && inside) {
          const uint16_t t = otLoopbackData[id];     // in range: id < tableEntries
          const bool ok = (t == 0xFFFF) ? (rType == T_UNKNOWN_DATA_ID && rData == 0)
                                        : (rType == T_READ_ACK && rData == t);
          check(4, ok, id, type, data, r,
                (t == 0xFFFF) ? "UNKNOWN_DATA_ID data 0 (table holds 0xFFFF)" : "READ_ACK with the table value");
        }

        if (!inside) {
          nonWriteHiCases++;
          if (rType == T_UNKNOWN_DATA_ID && rData == 0) nonWriteHiUnknown0++;
          if (type == T_READ_DATA) {
            readHiCases++;
            if (rType == T_READ_ACK) readHiReadAck++;
            if (rType == T_READ_ACK && rData == kOobStandIn) readHiStandIn++;
          }
          check(5, rType == T_UNKNOWN_DATA_ID && rData == 0, id, type, data, r, "UNKNOWN_DATA_ID data 0");
        }
      }
    }
  }
  if (dumpLo) fclose(dumpLo);
  if (dumpHi) fclose(dumpHi);

  printf("\nspot checks (request data 0x0000 unless noted):\n");
  static const unsigned kSpot[] = { 0, 25, 29, 127, 128, 131, 132, 133, 255 };
  for (unsigned i = 0; i < sizeof(kSpot) / sizeof(kSpot[0]); i++) {
    const unsigned id = kSpot[i];
    const unsigned long r = resp[id][T_READ_DATA][0];
    printf("  READ_DATA  id %3u -> 0x%08lX  %-15s data 0x%04lX\n",
           id, r, typeName((unsigned)(r >> 28) & 7u), r & 0xFFFFul);
  }
  {
    const unsigned long r = resp[131][T_WRITE_DATA][1];
    printf("  WRITE_DATA id 131 data 0x1234 -> 0x%08lX  %-15s data 0x%04lX\n",
           r, typeName((unsigned)(r >> 28) & 7u), r & 0xFFFFul);
  }

  printf("\ncontract:\n");
  unsigned long failures = 0;
  std::string failed;
  for (int c = 0; c < kCats; c++) {
    printf("  %s %-4s %5lu/%-5lu %s\n", g_cat[c].fails ? "FAIL" : "ok  ", g_cat[c].id,
           g_cat[c].checks - g_cat[c].fails, g_cat[c].checks, g_cat[c].title);
    failures += g_cat[c].fails;
    if (g_cat[c].fails) { if (!failed.empty()) failed += ","; failed += g_cat[c].id; }
  }

  printf("\nSUMMARY cases=%lu table_reads=%lu oob_reads=%lu write_table_reads=%lu"
         " read_hi_cases=%lu read_hi_readack=%lu read_hi_readack_standin=%lu"
         " nonwrite_hi_cases=%lu nonwrite_hi_unknown0=%lu"
         " write_hi_cases=%lu write_hi_echo=%lu failures=%lu failed=%s\n",
         cases, g_tableReads, g_oobReads, writeTableReads,
         readHiCases, readHiReadAck, readHiStandIn,
         nonWriteHiCases, nonWriteHiUnknown0,
         writeHiCases, writeHiEcho, failures, failed.empty() ? "none" : failed.c_str());

  printf("%s\n", failures == 0 ? "harness: contract holds" : "harness: contract VIOLATED");
  return failures == 0 ? 0 : 1;
}

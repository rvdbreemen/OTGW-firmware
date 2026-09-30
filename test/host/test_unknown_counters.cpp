/*
***************************************************************************
**  Program  : test/host/test_unknown_counters.cpp
**
**  Host proof for the 3-strike unknown-ID counters in OTDirect.ino
**  (TASK-1173).
**
**  otUnknownCounters holds 2 bits per MsgID, 32 bytes for MsgIDs 0-127.
**  handleMasterResponse() passes the response MsgID, (response >> 16) & 0xFF,
**  to getUnknownCount(), incUnknownCount() and clearUnknownCount(). That
**  MsgID can be 0-255: the thermostat pass-through relays Test & Diagnostic
**  ids (Remeha 131-133), and nothing checks a reply's MsgID against the
**  request. Without a bound, MsgIDs 128-255 reach up to 32 bytes past the
**  array.
**
**  This is the guard-region build, the default of run_unknown_counters.ps1.
**  run_unknown_counters.ps1 -Asan builds test_unknown_counters_asan.cpp
**  instead: the same slices as a real global under AddressSanitizer.
**
**  The code under test is NOT copied into this file. run_unknown_counters.ps1
**  slices the real declaration and the three helpers out of OTDirect.ino
**  (the working tree or a git revision) into generated/*.inc. This file only
**  supplies the storage and the checks:
**
**   - The sliced declaration is compiled unchanged inside a nested namespace.
**     Its element type and size are taken from it with decltype.
**   - The sliced helpers are compiled with otUnknownCounters rebound by a
**     macro to arena.counters, where arena is { counters[N]; guard[64]; }.
**     The guard therefore sits directly after the counters, whatever the
**     linker does, and sizeof(otUnknownCounters) stays N.
**   - The build uses /Od. An index past N is then plain address arithmetic
**     into the guard, which is what the firmware does on the ESP32, where
**     the next RAM symbol takes the hit instead.
**
**  Checks on the system under test (SUT). N comes from the sliced
**  declaration, so the array covers MsgIDs 0 to 4*N-1 (0-127 for N = 32):
**   1. inc, the UNKNOWN_DATA_ID branch, MsgIDs 0-255: no byte past the
**      array changes
**   2. clear, the READ_ACK / WRITE_ACK branch, MsgIDs 0-255: the same
**   3. get, the 3-strike test, MsgIDs 0-255: no value is read from past the
**      array, and get() changes no byte at all
**   4. inc, MsgIDs 4*N-255: no counter inside the array changes
**   5. clear, MsgIDs 4*N-255: the same
**   6. get, MsgIDs 4*N-255: returns 0, also when every counter holds 3
**   7. MsgIDs 0 to 4*N-1 keep the 2-bit counter contract (TASK-151): from
**      zero, four inc calls give get() == 3, and a clear then gives 0
**  Checks 4-6 pin "the helpers ignore 128-255". They reject a fix that
**  stays inside the array but lands on another MsgID's counter, such as
**  msgId &= 0x7F (an ACK for MsgID 131 would clear MsgID 3) or a clamp to
**  127. Check 7 rejects a bound that is off by one.
**  With UC_HAVE_REF (a reference revision, run_unknown_counters.ps1
**  -DiffAgainst <rev>) also:
**   8. SUT and REF leave identical counters and guard bytes and return the
**      same get() values for MsgIDs 0-127, from fixed fill patterns and a
**      seeded random run of get/inc/clear
**   9. the same comparison for MsgIDs 128-255, printed as information only.
**      Against the unbounded code it must show differences, which proves
**      check 8 can see a difference at all.
**  REF's own results for checks 1-7 are printed, not asserted, so the test
**  keeps working once the fix is committed and HEAD is clean.
**
**  Exit code: 0 = every SUT check (and check 8) passes, 1 = a check failed.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "generated/uc_labels.inc"   // UC_SUT_LABEL, UC_REF_LABEL

// ---- guard layout -----------------------------------------------------------
static constexpr size_t kGuardBytes = 64;

// Write paths: counters and guard both start at 0xA5. Every 2-bit field of
// 0xA5 holds 1 or 2 (bits 1-0 = 01, 3-2 = 01, 5-4 = 10, 7-6 = 10).
// incUnknownCount() writes a field only while it is below 3, and
// clearUnknownCount() changes a field only while it is non-zero, so 0xA5
// makes a stray increment or clear visible wherever it lands. A 0x00 fill
// would hide clears, a 0xFF fill would hide increments.
static constexpr uint8_t kFieldFill = 0xA5;

struct CounterImpl {
  const char* label;
  uint8_t*    counters;
  size_t      counterBytes;
  uint8_t*    guard;
  uint8_t   (*get)(uint8_t);
  void      (*inc)(uint8_t);
  void      (*clear)(uint8_t);
};

// ---- system under test --------------------------------------------------------
namespace sut {
namespace decl {
#include "generated/uc_sut_decl.inc"
}
typedef decltype(decl::otUnknownCounters) DeclType;
static_assert(std::is_array<DeclType>::value && std::rank<DeclType>::value == 1,
              "otUnknownCounters is no longer a one-dimensional array");
static_assert(std::is_same<std::remove_extent<DeclType>::type, uint8_t>::value,
              "otUnknownCounters element type is no longer uint8_t");
constexpr size_t kCounterBytes = std::extent<DeclType>::value;
static_assert(kCounterBytes + kGuardBytes > (255 >> 2),
              "guard too small to catch MsgID 255");
struct Arena { uint8_t counters[kCounterBytes]; uint8_t guard[kGuardBytes]; };
static_assert(offsetof(Arena, guard) == kCounterBytes, "guard must follow the counters directly");
static Arena arena;
#define otUnknownCounters (arena.counters)
#include "generated/uc_sut_funcs.inc"
#undef otUnknownCounters
static const CounterImpl impl = { UC_SUT_LABEL, arena.counters, kCounterBytes, arena.guard,
                                  getUnknownCount, incUnknownCount, clearUnknownCount };
}  // namespace sut

// ---- reference revision (optional) ---------------------------------------------
#ifdef UC_HAVE_REF
namespace ref {
namespace decl {
#include "generated/uc_ref_decl.inc"
}
typedef decltype(decl::otUnknownCounters) DeclType;
static_assert(std::is_array<DeclType>::value && std::rank<DeclType>::value == 1,
              "otUnknownCounters is no longer a one-dimensional array (REF)");
static_assert(std::is_same<std::remove_extent<DeclType>::type, uint8_t>::value,
              "otUnknownCounters element type is no longer uint8_t (REF)");
constexpr size_t kCounterBytes = std::extent<DeclType>::value;
static_assert(kCounterBytes + kGuardBytes > (255 >> 2),
              "guard too small to catch MsgID 255 (REF)");
struct Arena { uint8_t counters[kCounterBytes]; uint8_t guard[kGuardBytes]; };
static_assert(offsetof(Arena, guard) == kCounterBytes, "guard must follow the counters directly (REF)");
static Arena arena;
#define otUnknownCounters (arena.counters)
#include "generated/uc_ref_funcs.inc"
#undef otUnknownCounters
static const CounterImpl impl = { UC_REF_LABEL, arena.counters, kCounterBytes, arena.guard,
                                  getUnknownCount, incUnknownCount, clearUnknownCount };
}  // namespace ref
#endif

// ---- checks 1-6 ---------------------------------------------------------------
// One sample line. byteIdx is a guard byte for an access past the array and a
// counter byte for an alias; -1 when the byte is not known.
struct Hit { int id; int byteIdx; uint8_t before; uint8_t after; };

// The MsgIDs that showed one kind of finding on one path.
struct Finding {
  int count   = 0;
  int firstId = -1;
  int lastId  = -1;
  int mapped  = 0;   // past the array only: hits exactly at guard byte (id>>2)-N, bits (id&3)*2
  Hit sample[5];
  int nsample = 0;
};

struct PathReport {
  Finding outside;     // checks 1-3: touched memory past the array
  Finding alias;       // checks 4-6: a MsgID >= 4*N used a counter inside the array
  int     writes = 0;  // get only: calls that changed any counter or guard byte
};

static bool isSampleId(int id) {
  return id == 128 || id == 131 || id == 132 || id == 133 || id == 255;
}

static void note(Finding& f, int id) {
  f.count++;
  if (f.firstId < 0) f.firstId = id;
  f.lastId = id;
}

static void addSample(Finding& f, const Hit& h) {
  if (isSampleId(h.id) && f.nsample < 5) f.sample[f.nsample++] = h;
}

static bool allBytesAre(const uint8_t* p, size_t n, uint8_t v) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] != v) return false;
  }
  return true;
}

static int firstByteNot(const uint8_t* p, size_t n, uint8_t v) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] != v) return (int)i;
  }
  return -1;
}

// Write path: counters and guard at kFieldFill before every call. A changed
// guard byte is an access past the array (every MsgID). For a MsgID the array
// does not cover (>= 4*N), a changed counter byte is an alias. Below 4*N,
// changing its own counter is the helper's job and is left to checks 7 and 8.
static PathReport runWritePath(const CounterImpl& m, void (*op)(uint8_t)) {
  PathReport r;
  const int firstUncovered = (int)(4 * m.counterBytes);
  for (int id = 0; id <= 255; id++) {
    memset(m.counters, kFieldFill, m.counterBytes);
    memset(m.guard, kFieldFill, kGuardBytes);
    op((uint8_t)id);

    int nChanged = 0, firstChanged = -1;
    for (size_t g = 0; g < kGuardBytes; g++) {
      if (m.guard[g] != kFieldFill) {
        nChanged++;
        if (firstChanged < 0) firstChanged = (int)g;
      }
    }
    if (nChanged > 0) {
      note(r.outside, id);
      const int     expectedByte = (id >> 2) - (int)m.counterBytes;
      const uint8_t fieldMask    = (uint8_t)(0x03u << ((id & 3) * 2));
      const uint8_t diff         = (uint8_t)(m.guard[firstChanged] ^ kFieldFill);
      if (nChanged == 1 && firstChanged == expectedByte && (diff & (uint8_t)~fieldMask) == 0) r.outside.mapped++;
      addSample(r.outside, { id, firstChanged, kFieldFill, m.guard[firstChanged] });
    }

    if (id >= firstUncovered) {
      const int c = firstByteNot(m.counters, m.counterBytes, kFieldFill);
      if (c >= 0) {
        note(r.alias, id);
        addSample(r.alias, { id, c, kFieldFill, m.counters[c] });
      }
    }
  }
  return r;
}

// Read path, two passes per MsgID. get() must not change any byte in either.
//  A: counters 0, guard fields 3. A non-zero result was read past the array.
//  B: counters fields 3, guard 0. For a MsgID >= 4*N a non-zero result was
//     read from a counter inside the array (an alias).
static PathReport runReadPath(const CounterImpl& m) {
  PathReport r;
  const int firstUncovered = (int)(4 * m.counterBytes);
  for (int id = 0; id <= 255; id++) {
    memset(m.counters, 0x00, m.counterBytes);
    memset(m.guard, 0xFF, kGuardBytes);
    uint8_t v = m.get((uint8_t)id);
    if (!allBytesAre(m.counters, m.counterBytes, 0x00) || !allBytesAre(m.guard, kGuardBytes, 0xFF)) r.writes++;
    if (v != 0) {
      note(r.outside, id);
      const int expectedByte = (id >> 2) - (int)m.counterBytes;
      if (v == 3 && expectedByte >= 0 && expectedByte < (int)kGuardBytes) r.outside.mapped++;
      addSample(r.outside, { id, expectedByte, 0x00, v });
    }

    memset(m.counters, 0xFF, m.counterBytes);
    memset(m.guard, 0x00, kGuardBytes);
    v = m.get((uint8_t)id);
    if (!allBytesAre(m.counters, m.counterBytes, 0xFF) || !allBytesAre(m.guard, kGuardBytes, 0x00)) r.writes++;
    if (id >= firstUncovered && v != 0) {
      note(r.alias, id);
      addSample(r.alias, { id, -1, 0x00, v });
    }
  }
  return r;
}

// Checks 1-3.
static void printOutside(const char* who, const char* path, const PathReport& r,
                         const CounterImpl& m, bool asserted, bool isRead) {
  const Finding& f = r.outside;
  if (f.count == 0 && r.writes == 0) {
    printf("[%s] %-30s MsgIDs 0-255: no access past the %u-byte array%s\n",
           who, path, (unsigned)m.counterBytes, asserted ? "  PASS" : "  (info)");
    return;
  }
  if (f.count > 0) {
    printf("[%s] %-30s MsgIDs 0-255: %d MsgIDs %s past the array (MsgIDs %d-%d), "
           "%d of %d at the predicted guard byte and bits%s\n",
           who, path, f.count, isRead ? "read" : "wrote", f.firstId, f.lastId,
           f.mapped, f.count, asserted ? "  FAIL" : "  (info)");
  }
  for (int i = 0; i < f.nsample; i++) {
    const Hit& h = f.sample[i];
    const int lo = (h.id & 3) * 2;
    if (isRead) {
      printf("        MsgID %3d: get() returned %u, taken from guard byte %d, bits %d-%d\n",
             h.id, (unsigned)h.after, h.byteIdx, lo, lo + 1);
    } else {
      printf("        MsgID %3d: guard byte %d, bits %d-%d changed 0x%02X -> 0x%02X\n",
             h.id, h.byteIdx, lo, lo + 1, (unsigned)h.before, (unsigned)h.after);
    }
  }
  if (r.writes > 0) {
    printf("[%s] %-30s get() changed memory on %d calls%s\n",
           who, path, r.writes, asserted ? "  FAIL" : "  (info)");
  }
}

// Checks 4-6.
static void printAlias(const char* who, const char* path, const PathReport& r,
                       const CounterImpl& m, bool asserted, bool isRead) {
  const Finding& f = r.alias;
  const int firstUncovered = (int)(4 * m.counterBytes);
  const int lastCovered    = firstUncovered - 1;
  if (f.count == 0) {
    if (isRead) {
      printf("[%s] %-30s MsgIDs %d-255: get() returns 0 with every counter at 3%s\n",
             who, path, firstUncovered, asserted ? "  PASS" : "  (info)");
    } else {
      printf("[%s] %-30s MsgIDs %d-255: counters of MsgIDs 0-%d unchanged%s\n",
             who, path, firstUncovered, lastCovered, asserted ? "  PASS" : "  (info)");
    }
    return;
  }
  printf("[%s] %-30s MsgIDs %d-255: %d MsgIDs %s a counter inside the array (MsgIDs %d-%d)%s\n",
         who, path, firstUncovered, f.count, isRead ? "read" : "changed", f.firstId, f.lastId,
         asserted ? "  FAIL" : "  (info)");
  for (int i = 0; i < f.nsample; i++) {
    const Hit& h = f.sample[i];
    if (isRead) {
      printf("        MsgID %3d: get() returned %u with every counter at 3 and the guard at 0\n",
             h.id, (unsigned)h.after);
      continue;
    }
    const uint8_t d = (uint8_t)(h.before ^ h.after);
    int field = 0;
    while (field < 3 && ((d >> (field * 2)) & 0x03) == 0) field++;
    printf("        MsgID %3d: changed the counter of MsgID %d (counter byte %d, 0x%02X -> 0x%02X)\n",
           h.id, h.byteIdx * 4 + field, h.byteIdx, (unsigned)h.before, (unsigned)h.after);
  }
}

// ---- check 7: the in-range counter contract -------------------------------------
// For every MsgID the array covers, from zeroed counters: four inc calls
// saturate the 2-bit counter, so get() returns 3, and a clear sets it back to
// 0. handleMasterResponse() never passes MsgID 0, but the helpers do not
// special-case it, so 0 is tested too.
struct InRangeReport { int bad = 0; int firstId = -1; uint8_t afterInc = 0; uint8_t afterClear = 0; };

static InRangeReport runInRange(const CounterImpl& m) {
  InRangeReport r;
  const int covered = (int)(4 * m.counterBytes);
  for (int id = 0; id < covered; id++) {
    memset(m.counters, 0x00, m.counterBytes);
    memset(m.guard, kFieldFill, kGuardBytes);
    for (int n = 0; n < 4; n++) m.inc((uint8_t)id);
    const uint8_t afterInc = m.get((uint8_t)id);
    m.clear((uint8_t)id);
    const uint8_t afterClear = m.get((uint8_t)id);
    if (afterInc == 3 && afterClear == 0) continue;
    if (r.bad == 0) { r.firstId = id; r.afterInc = afterInc; r.afterClear = afterClear; }
    r.bad++;
  }
  return r;
}

static void printInRange(const char* who, const InRangeReport& r, const CounterImpl& m, bool asserted) {
  const int lastCovered = (int)(4 * m.counterBytes) - 1;
  if (r.bad == 0) {
    printf("[%s] %-30s MsgIDs 0-%d: 4 x inc gives 3, clear gives 0%s\n",
           who, "in-range counter contract", lastCovered, asserted ? "  PASS" : "  (info)");
    return;
  }
  printf("[%s] %-30s MsgIDs 0-%d: %d MsgIDs break it, first MsgID %d: "
         "get() is %u after 4 x inc, %u after clear%s\n",
         who, "in-range counter contract", lastCovered, r.bad, r.firstId,
         (unsigned)r.afterInc, (unsigned)r.afterClear, asserted ? "  FAIL" : "  (info)");
}

// Returns the number of failed checks (0-7).
static int checkImpl(const char* who, const CounterImpl& m, bool asserted) {
  const PathReport    inc = runWritePath(m, m.inc);
  const PathReport    clr = runWritePath(m, m.clear);
  const PathReport    get = runReadPath(m);
  const InRangeReport in  = runInRange(m);
  printOutside(who, "inc   (UNKNOWN_DATA_ID path)", inc, m, asserted, false);
  printAlias  (who, "inc   (UNKNOWN_DATA_ID path)", inc, m, asserted, false);
  printOutside(who, "clear (READ_ACK/WRITE_ACK)",   clr, m, asserted, false);
  printAlias  (who, "clear (READ_ACK/WRITE_ACK)",   clr, m, asserted, false);
  printOutside(who, "get   (3-strike test)",        get, m, asserted, true);
  printAlias  (who, "get   (3-strike test)",        get, m, asserted, true);
  printInRange(who, in, m, asserted);
  return (inc.outside.count != 0) + (clr.outside.count != 0)
       + (get.outside.count != 0 || get.writes != 0)
       + (inc.alias.count != 0) + (clr.alias.count != 0) + (get.alias.count != 0)
       + (in.bad != 0);
}

// ---- checks 8-9: differential SUT vs REF -----------------------------------------
#ifdef UC_HAVE_REF
static uint32_t rngState = 1;
static uint32_t rngNext() {            // xorshift32, fixed seed per run
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

static const char* const kOpName[3] = { "get", "inc", "clear" };

static bool sameState(const CounterImpl& a, const CounterImpl& b) {
  return memcmp(a.counters, b.counters, a.counterBytes) == 0 &&
         memcmp(a.guard, b.guard, kGuardBytes) == 0;
}

static void copyState(const CounterImpl& from, const CounterImpl& to) {
  memcpy(to.counters, from.counters, from.counterBytes);
  memcpy(to.guard, from.guard, kGuardBytes);
}

// Applies one operation to both implementations. True when the returned
// values and the full counters + guard state are identical afterwards.
static bool applyBoth(const CounterImpl& a, const CounterImpl& b, int op, uint8_t id) {
  uint8_t ra = 0, rb = 0;
  switch (op) {
    case 0:  ra = a.get(id); rb = b.get(id); break;
    case 1:  a.inc(id);      b.inc(id);      break;
    default: a.clear(id);    b.clear(id);    break;
  }
  return ra == rb && sameState(a, b);
}

struct DiffReport { long ops = 0; long diffs = 0; int firstId = -1; int firstOp = -1; };

static void noteDiff(DiffReport& d, int op, int id) {
  if (d.diffs == 0) { d.firstId = id; d.firstOp = op; }
  d.diffs++;
}

static DiffReport differential(const CounterImpl& a, const CounterImpl& b,
                               int lo, int hi, long randomOps, uint32_t seed) {
  DiffReport d;

  // 1. Every MsgID, every operation, from each fixed fill of counters and guard.
  static const uint8_t patterns[] = { 0x00, 0xFF, 0x55, 0xAA, 0xA5, 0x5A, 0x1B, 0xE4 };
  for (uint8_t p : patterns) {
    for (int id = lo; id <= hi; id++) {
      for (int op = 0; op < 3; op++) {
        memset(a.counters, p, a.counterBytes); memset(a.guard, p, kGuardBytes);
        copyState(a, b);
        d.ops++;
        if (!applyBoth(a, b, op, (uint8_t)id)) noteDiff(d, op, id);
      }
    }
  }

  // 2. A seeded random run on one evolving state. After a difference the
  //    reference is resynchronised, so each difference is counted once.
  rngState = seed;
  for (size_t i = 0; i < a.counterBytes; i++) a.counters[i] = (uint8_t)rngNext();
  for (size_t i = 0; i < kGuardBytes; i++)    a.guard[i]    = (uint8_t)rngNext();
  copyState(a, b);
  const uint32_t span = (uint32_t)(hi - lo + 1);
  for (long n = 0; n < randomOps; n++) {
    const int id = lo + (int)(rngNext() % span);
    const int op = (int)(rngNext() % 3u);
    d.ops++;
    if (!applyBoth(a, b, op, (uint8_t)id)) { noteDiff(d, op, id); copyState(a, b); }
  }
  return d;
}
#endif

int main() {
  printf("== otUnknownCounters bounds: host proof ==\n");
  printf("SUT: %s\n", UC_SUT_LABEL);
#ifdef UC_HAVE_REF
  printf("REF: %s\n", UC_REF_LABEL);
#endif
  printf("counters: %u bytes from the sliced declaration; guard: %u bytes directly after them\n\n",
         (unsigned)sut::kCounterBytes, (unsigned)kGuardBytes);

  int failures = checkImpl("SUT", sut::impl, true);

#ifdef UC_HAVE_REF
  printf("\n");
  checkImpl("REF", ref::impl, false);
  printf("\n");
  if (sut::kCounterBytes != ref::kCounterBytes) {
    printf("[DIFF] array sizes differ: SUT %u bytes, REF %u bytes  FAIL\n",
           (unsigned)sut::kCounterBytes, (unsigned)ref::kCounterBytes);
    failures++;
  } else {
    const DiffReport in = differential(sut::impl, ref::impl, 0, 127, 1000000L, 0x2545F491u);
    printf("[DIFF] MsgIDs 0-127:   %ld operations, %ld differences%s\n",
           in.ops, in.diffs, in.diffs ? "  FAIL" : "  PASS");
    if (in.diffs) {
      printf("       first difference: %s(%d)\n", kOpName[in.firstOp], in.firstId);
      failures++;
    }
    const DiffReport out = differential(sut::impl, ref::impl, 128, 255, 100000L, 0x9E3779B9u);
    printf("[DIFF] MsgIDs 128-255: %ld operations, %ld differences  "
           "(info: expected > 0 when REF reaches past the array)\n", out.ops, out.diffs);
    if (out.diffs) {
      printf("       first difference: %s(%d)\n", kOpName[out.firstOp], out.firstId);
    }
  }
#endif

  if (failures) printf("\n== %d check(s) failed ==\n", failures);
  else          printf("\n== all checks passed ==\n");
  return failures ? 1 : 0;
}

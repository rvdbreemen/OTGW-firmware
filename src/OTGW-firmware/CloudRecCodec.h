/*
***************************************************************************
**  Program  : CloudRecCodec.h
**  TASK-1963 PoC (branch research/cloud-recording only): the encoders of the cloud recorder's mix record.
**  Plain C++ with no Arduino dependency, so the host test (tests/test_cloudrec_codec.py) compiles this
**  same header and checks its output against the Python decoders in tools/cloud-recorder/otrec.py.
**
**  Formats (otrec.py is the reference):
**    mix record (fmt 4): sections, each `type u8, LEB128 length, data`
**      5 = OT, model v2: LEB128 offset (ms) of the first frame from t0, LEB128 frame count, range-coder bytes
**      2 = SAT: SATTICK ticks (field mask + fixed-point deltas, text fallback)
**      3 = debug: address tokens (heap, printf, println, raw, line raw)
***************************************************************************
*/
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace crec {

// ---------------------------------------------------------------- byte buffer with a hard cap

struct Buf {
  uint8_t *p;
  size_t cap;
  size_t n;
  bool full;
  void (*sink)(void *ctx, uint8_t b);   // TASK-1972: when set, bytes go to sink() instead of memory (and are counted)
  void *ctx;
  void init(uint8_t *mem, size_t size) { p = mem; cap = size; n = 0; full = false; sink = nullptr; ctx = nullptr; }
  void initSink(void (*f)(void *, uint8_t), void *c) { p = nullptr; cap = (size_t)-1; n = 0; full = false; sink = f; ctx = c; }
  void put(uint8_t b) { if (sink) { sink(ctx, b); n++; } else if (n < cap) p[n++] = b; else full = true; }
  void bytes(const void *d, size_t k) { const uint8_t *s = (const uint8_t *)d; for (size_t i = 0; i < k; i++) put(s[i]); }
  void leb(uint64_t v) { do { uint8_t b = v & 0x7F; v >>= 7; put(b | (v ? 0x80 : 0)); } while (v); }
  void u24(uint32_t v) { put(v & 0xFF); put((v >> 8) & 0xFF); put((v >> 16) & 0xFF); }
  size_t room() const { return cap > n ? cap - n : 0; }
};

inline uint64_t zz(int64_t v) { return v < 0 ? (((uint64_t)(-v)) << 1) - 1 : ((uint64_t)v) << 1; }
inline int lebLen(uint64_t v) { int k = 1; while (v >>= 7) k++; return k; }

// ---------------------------------------------------------------- binary range coder (LZMA style, 12-bit)

static const int RC_PB = 12;
static const uint16_t RC_INIT = 1 << (RC_PB - 1);
static const int RC_SHIFT = 4;

struct RcEnc {
  Buf *out;
  uint64_t low;
  uint32_t rng;
  uint8_t cache;
  uint64_t csize;
  bool first;
  void init(Buf *o) { out = o; low = 0; rng = 0xFFFFFFFFu; cache = 0; csize = 1; first = true; }
  void emit(uint8_t b) { if (first) { first = false; return; } out->put(b); }
  void shift() {
    if (low < 0xFF000000ull || low >= (1ull << 32)) {
      uint8_t carry = (uint8_t)(low >> 32);
      uint8_t temp = cache;
      do { emit((uint8_t)(temp + carry)); temp = 0xFF; } while (--csize);
      cache = (uint8_t)((low >> 24) & 0xFF);
    }
    csize++;
    low = (low << 8) & 0xFFFFFFFFull;
  }
  void norm() { while (rng < (1u << 24)) { rng <<= 8; shift(); } }
  void encodeBit(uint16_t *p, int b) {
    uint32_t bound = (rng >> RC_PB) * (*p);
    if (!b) { rng = bound; *p += ((1 << RC_PB) - *p) >> RC_SHIFT; }
    else    { low += bound; rng -= bound; *p -= *p >> RC_SHIFT; }
    norm();
  }
  void direct(uint64_t v, int nbits) {
    for (int k = nbits - 1; k >= 0; k--) { rng >>= 1; if ((v >> k) & 1) low += rng; norm(); }
  }
  void finish() { for (int i = 0; i < 5; i++) shift(); }
};

inline void treeEnc(RcEnc &e, uint16_t *probs, uint32_t v, int nbits) {
  uint32_t m = 1;
  for (int k = nbits - 1; k >= 0; k--) { int b = (v >> k) & 1; e.encodeBit(&probs[m], b); m = (m << 1) | b; }
}

inline void numEnc(RcEnc &e, uint16_t *lenProbs, uint64_t u) {
  int n = 0;
  for (uint64_t x = u; x; x >>= 1) n++;
  treeEnc(e, lenProbs, (uint32_t)n, 6);
  if (n > 1) e.direct(u - (1ull << (n - 1)), n - 1);
}

// ---------------------------------------------------------------- OT model v2 (fixed tables)

static const char OT_LETTERS[5] = {'T', 'B', 'R', 'A', 'E'};

struct OtModel {
  uint16_t let[5][8], raw[1], idhit[5], id[256], thit[5][128], type[5][8], vsame[5][128], dtlen[5][5][64], vlen[5][64];
  uint8_t prev;
  bool haveT;
  int64_t lastT;
  int32_t lastDt[5][5];
  uint8_t nextId[256];
  uint8_t lastTid;
  bool pendT, pendR;
  uint8_t ptId, ptMt, prId, prMt;
  uint16_t ptVal, prVal;
  uint16_t lastVal[5][128];
  uint8_t lastType[5][128];

  void reset() {
    uint16_t *tabs[] = {&let[0][0], raw, idhit, id, &thit[0][0], &type[0][0], &vsame[0][0], &dtlen[0][0][0], &vlen[0][0]};
    size_t sizes[] = {sizeof(let), sizeof(raw), sizeof(idhit), sizeof(id), sizeof(thit), sizeof(type), sizeof(vsame),
                      sizeof(dtlen), sizeof(vlen)};
    for (size_t t = 0; t < sizeof(sizes) / sizeof(sizes[0]); t++)
      for (size_t i = 0; i < sizes[t] / sizeof(uint16_t); i++) tabs[t][i] = RC_INIT;
    prev = 0; haveT = false; lastT = 0; lastTid = 0; pendT = pendR = false;
    ptId = ptMt = prId = prMt = 0; ptVal = prVal = 0;
    memset(lastDt, 0, sizeof(lastDt)); memset(nextId, 0, sizeof(nextId));
    memset(lastVal, 0, sizeof(lastVal)); memset(lastType, 0, sizeof(lastType));
  }
};

inline int otLetterIndex(char c) { for (int i = 0; i < 5; i++) if (OT_LETTERS[i] == c) return i; return -1; }

inline bool parityOk(uint32_t f) { int c = 0; for (uint32_t x = f; x; x &= x - 1) c++; return (c & 1) == 0; }

// One frame; t is ms relative to the first frame of the section. Mirrors otrec.ot2_encode_frame.
inline void otEncodeFrame(RcEnc &e, OtModel &m, int64_t t, int li, uint32_t f) {
  int pl = m.prev;
  treeEnc(e, m.let[pl], (uint32_t)li, 3);
  int64_t dt = m.haveT ? t - m.lastT : 0;
  m.haveT = true;
  m.lastT = t;
  numEnc(e, m.dtlen[pl][li], zz(dt - m.lastDt[pl][li]));
  uint8_t mt = (f >> 28) & 7, did = (f >> 16) & 0xFF;
  uint16_t val = f & 0xFFFF;
  bool ok = parityOk(f) && ((f >> 24) & 0xF) == 0;
  e.encodeBit(m.raw, ok ? 0 : 1);
  if (ok) {
    // prediction (otrec._predict2)
    int pid;
    int ptype = -1;
    int32_t pval = -1;
    char L = OT_LETTERS[li];
    if (L == 'T') pid = m.nextId[m.lastTid];
    else if (L == 'R') pid = m.pendT ? m.ptId : 0;
    else {
      bool useR = (L == 'B' && m.pendR);
      uint8_t rid = 0, rt = 0; uint16_t rv = 0;
      if (useR) { rid = m.prId; rt = m.prMt; rv = m.prVal; }
      else if (m.pendT) { rid = m.ptId; rt = m.ptMt; rv = m.ptVal; }
      pid = rid;
      ptype = rt == 0 ? 4 : rt == 1 ? 5 : 7;
      if (rt == 1) pval = rv;
    }
    e.encodeBit(&m.idhit[li], did == pid ? 0 : 1);
    if (did != pid) treeEnc(e, m.id, did, 8);
    int k = did & 0x7F;
    if (ptype < 0) ptype = m.lastType[li][k];
    e.encodeBit(&m.thit[li][k], mt == ptype ? 0 : 1);
    if (mt != ptype) treeEnc(e, m.type[li], mt, 3);
    uint16_t ref = pval >= 0 ? (uint16_t)pval : m.lastVal[li][k];
    e.encodeBit(&m.vsame[li][k], val == ref ? 0 : 1);
    if (val != ref) {
      int32_t delta = (int32_t)(((uint32_t)val - ref + 0x8000) & 0xFFFF) - 0x8000;
      numEnc(e, m.vlen[li], zz(delta) - 1);
    }
  } else {
    e.direct(f, 32);
  }
  // update (otrec._update2)
  m.lastDt[pl][li] = (int32_t)dt;
  char L = OT_LETTERS[li];
  if (L == 'T') {
    m.nextId[m.lastTid] = did; m.lastTid = did;
    m.pendT = true; m.ptId = did; m.ptMt = mt; m.ptVal = val; m.pendR = false;
  } else if (L == 'R') {
    m.pendR = true; m.prId = did; m.prMt = mt; m.prVal = val;
  }
  m.lastVal[li][did & 0x7F] = val;
  m.lastType[li][did & 0x7F] = mt;
  m.prev = (uint8_t)li;
}

// ---------------------------------------------------------------- SAT section (type 2): SATTICK ticks

static const char *const SAT_FORMATS[2] = {
  "SATTICK room=%.2f raw=%.2f rv=%d target=%.1f out=%.1f osrc=%c curve=%.2f p=%.2f i=%.2f d=%.2f praw=%.2f "
  "pid=%.2f cs=%.1f mm=%s ch=%u mode=%d pwm=%d duty=%.2f pon=%lu poff=%lu starts=%u flame=%d flow=%.1f "
  "ret=%.1f fc=%c fage=%lu rc=%c rage=%lu rsrc=%s dhw=0\r\n",
  "SATTICK target=%.1f mode=%d flame=%d flow=%.1f ret=%.1f fc=%c fage=%lu rc=%c rage=%lu rsrc=%s dhw=1\r\n",
};
static const int SAT_MAX_FIELDS = 32;
static const int SAT_FIELD_LEN = 24;

struct SatField { char conv; int prec; };

// Parses a SATTICK format into its conversions and literal parts. Returns the number of fields.
inline int satFields(const char *fmt, SatField *f, const char **lit, size_t *litLen) {
  int n = 0;
  const char *p = fmt, *start = fmt;
  while (*p) {
    if (*p != '%') { p++; continue; }
    lit[n] = start; litLen[n] = (size_t)(p - start);
    const char *s = p + 1;
    int prec = 0;
    if (*s == '.') { s++; while (*s >= '0' && *s <= '9') prec = prec * 10 + (*s++ - '0'); }
    while (*s == 'l') s++;
    f[n].conv = *s; f[n].prec = prec;
    n++;
    p = s + 1; start = p;
  }
  lit[n] = start; litLen[n] = strlen(start);
  return n;
}

// Exact fixed-point form of a printed number, as otrec._fixed. Returns false when the text is not canonical.
inline bool satFixed(const char *t, size_t len, char conv, int p, int64_t *out) {
  bool neg = len && t[0] == '-';
  size_t i = neg ? 1 : 0;
  size_t whole = 0;
  int64_t v = 0;
  while (i < len && t[i] >= '0' && t[i] <= '9') { v = v * 10 + (t[i] - '0'); i++; whole++; }
  if (whole == 0 || (whole > 1 && t[neg ? 1 : 0] == '0')) return false;
  if (conv == 'f' && p > 0) {
    if (i >= len || t[i] != '.') return false;
    i++;
    int frac = 0;
    while (i < len && t[i] >= '0' && t[i] <= '9') { v = v * 10 + (t[i] - '0'); i++; frac++; }
    if (frac != p) return false;
  }
  if (i != len) return false;
  if (neg && v == 0) return false;
  *out = neg ? -v : v;
  return true;
}

struct SatState {
  bool have[2];
  char val[2][SAT_MAX_FIELDS][SAT_FIELD_LEN];
  uint8_t len[2][SAT_MAX_FIELDS];
  int64_t prevT;
  void reset(int64_t t0) { have[0] = have[1] = false; prevT = t0; }
};

// Appends one tick (text exactly as printed). Returns false when the text does not match the format or when
// out ran full; in both cases st is unchanged.
inline bool satEncode(Buf &out, SatState &st, int64_t t, int fi, const char *text) {
  SatField f[SAT_MAX_FIELDS];
  const char *lit[SAT_MAX_FIELDS + 1];
  size_t litLen[SAT_MAX_FIELDS + 1];
  int nf = satFields(SAT_FORMATS[fi], f, lit, litLen);
  const char *vs[SAT_MAX_FIELDS];
  size_t vl[SAT_MAX_FIELDS];
  size_t tl = strlen(text);
  if (tl < litLen[0] || memcmp(text, lit[0], litLen[0])) return false;
  size_t pos = litLen[0];
  for (int k = 0; k < nf; k++) {
    const char *next = lit[k + 1];
    size_t nl = litLen[k + 1];
    size_t end;
    if (k == nf - 1) {                     // last literal is anchored at the end of the text
      if (tl < nl || tl - nl < pos || memcmp(text + tl - nl, next, nl)) return false;
      end = tl - nl;
    } else {
      const char *hit = nullptr;
      for (size_t q = pos; q + nl <= tl; q++) if (!memcmp(text + q, next, nl)) { hit = text + q; break; }
      if (!hit) return false;
      end = (size_t)(hit - text);
    }
    vs[k] = text + pos; vl[k] = end - pos;
    if (vl[k] >= SAT_FIELD_LEN) return false;
    pos = end + nl;
  }
  uint64_t mask = 0;
  for (int k = 0; k < nf; k++)
    if (!st.have[fi] || st.len[fi][k] != vl[k] || memcmp(st.val[fi][k], vs[k], vl[k])) mask |= 1ull << k;
  out.put((uint8_t)fi);
  out.leb((uint64_t)(t - st.prevT));
  out.leb(mask);
  for (int k = 0; k < nf; k++) {
    if (!((mask >> k) & 1)) continue;
    char c = f[k].conv;
    if (c == 'd' || c == 'i' || c == 'u' || c == 'f') {
      int64_t now, ref = 0;
      bool okNow = satFixed(vs[k], vl[k], c, f[k].prec, &now);
      bool okRef = !st.have[fi] || satFixed(st.val[fi][k], st.len[fi][k], c, f[k].prec, &ref);
      if (okNow && okRef) { out.leb(zz(now - (st.have[fi] ? ref : 0)) << 1); continue; }
      out.leb(((uint64_t)vl[k] << 1) | 1);
      out.bytes(vs[k], vl[k]);
    } else if (c == 'c') {
      out.put(vl[k] ? (uint8_t)vs[k][0] : 0);
    } else {
      out.put((uint8_t)vl[k]);
      out.bytes(vs[k], vl[k]);
    }
  }
  if (out.full) return false;               // the caller rolls the buffer back; the state is untouched
  for (int k = 0; k < nf; k++) { memcpy(st.val[fi][k], vs[k], vl[k]); st.len[fi][k] = (uint8_t)vl[k]; }
  st.have[fi] = true;
  st.prevT = t;
  return true;
}

// ---------------------------------------------------------------- debug section (type 3): address tokens

static const uint32_t DROM_BASE = 0x3C000000u;
enum : uint8_t { DBG_HEAP = 0, DBG_PRINTF = 1, DBG_PRINTLN = 2, DBG_RAW = 3, DBG_LINE_RAW = 4 };

struct DbgState {
  int64_t prevUs;
  uint32_t heap, block;
  bool heapSent;
  void reset(int64_t t0ms) { prevUs = t0ms * 1000; heapSent = false; heap = block = 0; }
};

inline void dbgHeap(Buf &out, DbgState &st, uint32_t heap, uint32_t block) {
  if (st.heapSent && heap == st.heap && block == st.block) return;
  out.put(DBG_HEAP); out.leb(heap); out.leb(block);
  st.heap = heap; st.block = block; st.heapSent = true;
}

inline void dbgLineHead(Buf &out, DbgState &st, uint8_t kind, int64_t tUs) {
  out.put(kind);
  out.leb((uint64_t)(tUs - st.prevUs));
  st.prevUs = tUs;
}

inline void dbgPrintln(Buf &out, DbgState &st, int64_t tUs, uint32_t strAddr, uint32_t fnAddr, uint32_t line) {
  dbgLineHead(out, st, DBG_PRINTLN, tUs);
  out.u24(strAddr - DROM_BASE); out.u24(fnAddr - DROM_BASE); out.leb(line);
}

inline void dbgLineRaw(Buf &out, DbgState &st, int64_t tUs, uint32_t fnAddr, uint32_t line, const char *raw, size_t n) {
  dbgLineHead(out, st, DBG_LINE_RAW, tUs);
  out.u24(fnAddr - DROM_BASE); out.leb(line); out.leb(n); out.bytes(raw, n);
}

inline void dbgRaw(Buf &out, const char *raw, size_t n) { out.put(DBG_RAW); out.leb(n); out.bytes(raw, n); }

// Argument encoder for a printf item. The caller walks the format with its va_list and feeds every
// conversion here; it returns false for anything the decoder could not render exactly, and the caller then
// rolls the buffer back and sends a line raw item instead.
struct DbgArgs {
  Buf *out;
  bool sint(int64_t v) { out->leb(zz(v)); return true; }
  bool uint(uint64_t v) { out->leb(v); return true; }
  bool flt(double v) {
    float f = (float)v;
    if (v == v && (double)f != v) return false;   // not exact as float32 (NaN passes)
    uint8_t b[4]; memcpy(b, &f, 4); out->bytes(b, 4); return true;
  }
  bool chr(int c) { out->put((uint8_t)c); return true; }
  bool str(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    if (n > 127) return false;
    out->put((uint8_t)n); out->bytes(s, n); return true;
  }
};

// One step of the format walk: parses the conversion at fmt (which points at '%'). Returns the conversion
// letter (d i u x X f c s S), '%' for "%%", or 0 for anything else. *len is the number of length modifier 'l's.
inline char dbgConv(const char *&fmt, int *lenMod) {
  const char *s = fmt + 1;
  if (*s == '%') { fmt = s + 1; return '%'; }
  while (*s == '-' || *s == '+' || *s == ' ' || *s == '#' || *s == '0') s++;
  while (*s >= '0' && *s <= '9') s++;
  if (*s == '.') { s++; while (*s >= '0' && *s <= '9') s++; }
  int l = 0;
  while (*s == 'h' || *s == 'l' || *s == 'z') { if (*s == 'l') l++; s++; }
  char c = *s;
  if (!c || !strchr("diuxXfcsS", c)) return 0;
  *lenMod = l;
  fmt = s + 1;
  return c;
}

// ---------------------------------------------------------------- mix record assembly

static const uint8_t SEC_SAT = 2, SEC_DBG = 3, SEC_OT2 = 5;
static const uint8_t SEC_LOST = 7;   // TASK-1972: LEB128 items, LEB128 records lost since the previous record

// Writes section `type` with `data` into out. Returns false when it does not fit.
inline bool mixSection(Buf &out, uint8_t type, const uint8_t *data, size_t n) {
  if (!n) return true;
  if (out.room() < 1 + (size_t)lebLen(n) + n) return false;
  out.put(type); out.leb(n); out.bytes(data, n);
  return true;
}

// The OT section: offset of the first frame from t0, frame count, the finished range-coder bytes.
inline bool mixOtSection(Buf &out, int64_t t0Ms, int64_t firstMs, uint32_t count, const uint8_t *rc, size_t rcLen) {
  if (!count) return true;
  size_t n = (size_t)lebLen((uint64_t)(firstMs - t0Ms)) + (size_t)lebLen(count) + rcLen;
  if (out.room() < 1 + (size_t)lebLen(n) + n) return false;
  out.put(SEC_OT2); out.leb(n); out.leb((uint64_t)(firstMs - t0Ms)); out.leb(count); out.bytes(rc, rcLen);
  return true;
}

// ---------------------------------------------------------------- the recorder: one open record

// Section buffers of one record; together they fit the 4 KB recording budget of a board without PSRAM
// (ADR-213). An append that does not fit returns false; the caller then closes the record (send it), opens
// a new one and appends again.
static const size_t REC_OT_BYTES = 768, REC_SAT_BYTES = 512, REC_DBG_BYTES = 2816;
static const size_t REC_MAX_BYTES = REC_OT_BYTES + REC_SAT_BYTES + REC_DBG_BYTES + 32;   // + section headers, SEC_LOST

struct Recorder {
  uint8_t otMem[REC_OT_BYTES], satMem[REC_SAT_BYTES], dbgMem[REC_DBG_BYTES];
  Buf ot, sat, dbg;
  RcEnc rc;
  OtModel model;
  SatState satSt;
  DbgState dbgSt;
  int64_t t0Ms, firstOtMs;
  uint32_t otCount;
  bool open;

  void begin(int64_t t0ms) {
    ot.init(otMem, sizeof(otMem)); sat.init(satMem, sizeof(satMem)); dbg.init(dbgMem, sizeof(dbgMem));
    rc.init(&ot); model.reset(); satSt.reset(t0ms); dbgSt.reset(t0ms);
    t0Ms = t0ms; firstOtMs = t0ms; otCount = 0; open = true;
  }
  bool empty() const { return !otCount && !sat.n && !dbg.n; }
  // Loss since the previous record (busy recorder, a record that could not be queued, a pause). Kept across
  // begin() and written as SEC_LOST at the head of the next closed record, so a gap is never silent.
  uint32_t lostItems = 0, lostRecords = 0;

  bool otFrame(int64_t tMs, char letter, uint32_t frame) {
    int li = otLetterIndex(letter);
    if (li < 0) return true;                         // not an OT frame line: nothing to record
    if (ot.room() < 24) return false;                // worst case of one frame plus the 5 finish bytes
    if (!otCount) firstOtMs = tMs;
    otEncodeFrame(rc, model, tMs - firstOtMs, li, frame);
    otCount++;
    return true;
  }
  bool satTick(int64_t tMs, int fi, const char *text) {
    size_t mark = sat.n;
    if (satEncode(sat, satSt, tMs, fi, text)) return true;
    bool full = sat.full;
    sat.n = mark; sat.full = false;
    return !full;                                    // a line that is not a SATTICK is simply not recorded
  }
  // Debug items: the caller writes the item into dbg and then calls dbgCommit(mark); a full buffer rolls back.
  size_t dbgMark() const { return dbg.n; }
  bool dbgCommit(size_t mark, const DbgState &before) {
    if (!dbg.full) return true;
    dbg.n = mark; dbg.full = false; dbgSt = before;
    return false;
  }

  // Finishes the record into out (cap REC_MAX_BYTES). Returns the record length.
  size_t close(uint8_t *out, size_t cap) {
    Buf b; b.init(out, cap);
    if (otCount) rc.finish();
    if (lostItems || lostRecords) {
      uint8_t g[12];
      Buf gb; gb.init(g, sizeof(g));
      gb.leb(lostItems); gb.leb(lostRecords);
      mixSection(b, SEC_LOST, g, gb.n);
      lostItems = lostRecords = 0;
    }
    mixOtSection(b, t0Ms, firstOtMs, otCount, ot.p, ot.n);
    mixSection(b, SEC_SAT, sat.p, sat.n);
    mixSection(b, SEC_DBG, dbg.p, dbg.n);
    open = false;
    return b.full ? 0 : b.n;
  }
};

// A printf line: walks fmt, takes each argument from src (src.sint(l) / src.uint(l) / src.dbl() / src.chr() /
// src.str(), l = number of 'l' modifiers) and encodes it. Returns TOK_OK, TOK_FULL (the record has no room:
// close it and try again) or TOK_NO (the line cannot be sent as tokens exactly: an unknown conversion, a
// string over 127 bytes, a double that is not a float32, or text the device cut at its buffer; send a line
// raw item with the printed text instead). Nothing is left in the buffer unless TOK_OK.
enum TokResult { TOK_OK = 0, TOK_FULL = 1, TOK_NO = 2 };

template <typename Src>
TokResult dbgPrintfTokens(Recorder &r, int64_t tUs, uint32_t fmtAddr, uint32_t fnAddr, uint32_t line, const char *fmt,
                     Src &src, bool truncated) {
  if (truncated) return TOK_NO;
  size_t mark = r.dbgMark();
  DbgState before = r.dbgSt;
  dbgLineHead(r.dbg, r.dbgSt, DBG_PRINTF, tUs);
  r.dbg.u24(fmtAddr - DROM_BASE); r.dbg.u24(fnAddr - DROM_BASE); r.dbg.leb(line);
  DbgArgs a{&r.dbg};
  bool ok = true;
  for (const char *p = fmt; *p && ok;) {
    if (*p != '%') { p++; continue; }
    int l = 0;
    char c = dbgConv(p, &l);
    switch (c) {
      case '%': break;
      case 'd': case 'i': ok = a.sint(src.sint(l)); break;
      case 'u': case 'x': case 'X': ok = a.uint(src.uint(l)); break;
      case 'f': ok = a.flt(src.dbl()); break;
      case 'c': ok = a.chr(src.chr()); break;
      case 's': case 'S': ok = a.str(src.str()); break;
      default: ok = false;
    }
  }
  bool full = r.dbg.full;
  if (ok && !full) return TOK_OK;
  r.dbg.n = mark; r.dbg.full = false; r.dbgSt = before;
  return full ? TOK_FULL : TOK_NO;
}

// ---------------------------------------------------------------- TASK-1972: LZ + range coder over a whole record
//
// A closed record (fmt 4) is sent as fmt 5 when this makes it smaller: LEB128 raw length, CRC-32 (IEEE, little
// endian) of the raw record, then the range coder's bytes. The decoder refuses a record whose output does not
// match the CRC, so a fault in either coder can never change data unnoticed. LZ77 whose window is the record itself, so no window buffer; the match finder keeps the last position per
// 3-byte hash. Every decision goes through the range coder: match flag (context: previous token was a match),
// literal (one 256-node bit tree), length 3..258 (4-bit tree, 15 = an 8-bit tree follows), distance 1..4096
// (6-bit tree of the high bits, 6 direct bits). otrec.lz_encode() / lz_decode() are the reference; the host test
// checks that both give the same bytes. RAM: LzWork, 3.2 KB.

static const int LZ_HASH_BITS = 10;
static const size_t LZ_MAX_MATCH = 258;
static const size_t LZ_MAX_DIST = 4096;

struct LzWork {
  uint16_t head[1 << LZ_HASH_BITS];     // position + 1 of the last 3-byte string with this hash, 0 = none
  uint16_t match[2], lit[256], len[16], len2[256], dist[64];
};

inline uint32_t lzHash(const uint8_t *d, size_t i) {
  uint32_t x = (uint32_t)d[i] << 16 | (uint32_t)d[i + 1] << 8 | d[i + 2];
  return (x * 2654435761u) >> (32 - LZ_HASH_BITS);
}

// CRC-32 as zlib.crc32 (reflected, polynomial 0xEDB88320), bitwise: no table.
inline uint32_t crc32(const uint8_t *d, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

inline void lzEncode(const uint8_t *in, size_t n, Buf &out, LzWork &w) {
  for (auto &h : w.head) h = 0;
  uint16_t *tables[] = {w.match, w.lit, w.len, w.len2, w.dist};
  const size_t sizes[] = {2, 256, 16, 256, 64};
  for (int t = 0; t < 5; t++) for (size_t k = 0; k < sizes[t]; k++) tables[t][k] = RC_INIT;
  out.leb(n);
  uint32_t crc = crc32(in, n);
  for (int k = 0; k < 4; k++) out.put((uint8_t)(crc >> (8 * k)));
  RcEnc e;
  e.init(&out);
  int prev = 0;
  size_t i = 0;
  while (i < n) {
    size_t best = 0, dist = 0;
    if (i + 3 <= n) {
      uint32_t h = lzHash(in, i);
      size_t j = w.head[h];
      w.head[h] = (uint16_t)(i + 1);
      if (j && i - (j - 1) <= LZ_MAX_DIST) {
        j--;
        while (best < LZ_MAX_MATCH && i + best < n && in[j + best] == in[i + best]) best++;
        dist = i - j;
      }
    }
    if (best >= 3) {
      e.encodeBit(&w.match[prev], 1);
      uint32_t L = (uint32_t)(best - 3);
      if (L < 15) treeEnc(e, w.len, L, 4);
      else { treeEnc(e, w.len, 15, 4); treeEnc(e, w.len2, L - 15, 8); }
      uint32_t d = (uint32_t)(dist - 1);
      treeEnc(e, w.dist, d >> 6, 6);
      e.direct(d & 63, 6);
      for (size_t k = i + 1; k < i + best; k++)
        if (k + 3 <= n) w.head[lzHash(in, k)] = (uint16_t)(k + 1);
      i += best;
      prev = 1;
    } else {
      e.encodeBit(&w.match[prev], 0);
      treeEnc(e, w.lit, in[i], 8);
      i++;
      prev = 0;
    }
  }
  e.finish();
}

}  // namespace crec

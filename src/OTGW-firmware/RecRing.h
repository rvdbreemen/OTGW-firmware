/*
***************************************************************************
**  Program  : RecRing.h
**  TASK-2907 (branch research/cloud-recording only): the local sink of the one recorder. A byte ring of closed
**  records, each stored as it would be sent (fmt 4, or fmt 5 when LZ-coded), so a local download and the cloud
**  carry the same records and one decoder reads both. No Arduino or platform include: tests/test_rec_ring.py
**  compiles this header as it is.
**
**  ENTRY (little endian): magic 0xA5 | fmt u8 | len u16 | seq u64 | t0 i64 | payload (len bytes). An entry may
**  wrap around the end of the ring. When a new entry does not fit, the oldest entries are dropped until it does;
**  the count of dropped entries is kept.
**
**  DOWNLOAD FILE: "OTGWMIX1" | version u16 (1) | header length u16 | u8-length texts: firmware version, githash,
**  uid | then the entries, oldest first, in the entry layout above. The caller holds the ring's lock around every
**  call; nothing here blocks or allocates.
***************************************************************************
*/
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace crec {

static const uint32_t RING_HDR = 20;
static const uint8_t RING_MAGIC = 0xA5;

struct RecRing {
  uint8_t *m = nullptr;
  uint32_t cap = 0;
  uint32_t head = 0;          // offset of the oldest entry
  uint32_t used = 0;          // bytes in use
  uint32_t entries = 0;
  uint32_t evicted = 0;       // entries dropped to make room
  uint32_t w = 0;             // write offset of the entry being written
  uint32_t wLen = 0;
  // A download that has sent part of an entry pins it, so it cannot be dropped in the middle (the file would get a
  // cut entry). The pin lapses after PIN_MS without a new read, so an abandoned download never blocks the ring.
  static const uint32_t PIN_MS = 10000;
  bool pinned = false;
  uint64_t pinSeq = 0;
  uint32_t pinMs = 0;

  void init(uint8_t *mem, uint32_t size) {
    m = mem; cap = size; head = used = entries = evicted = 0;
    pinned = false;
  }
  bool ready() const { return m != nullptr && cap > RING_HDR; }

  uint8_t get(uint32_t off) const { return m[off % cap]; }
  void put(uint32_t off, uint8_t b) { m[off % cap] = b; }
  uint64_t getLe(uint32_t off, int n) const {
    uint64_t v = 0;
    for (int k = 0; k < n; k++) v |= (uint64_t)get(off + (uint32_t)k) << (8 * k);
    return v;
  }
  void putLe(uint32_t off, uint64_t v, int n) {
    for (int k = 0; k < n; k++) put(off + (uint32_t)k, (uint8_t)(v >> (8 * k)));
  }

  uint32_t entryBytes(uint32_t off) const { return RING_HDR + (uint32_t)getLe(off + 2, 2); }
  uint64_t entrySeq(uint32_t off) const { return getLe(off + 4, 8); }

  // Starts an entry of len payload bytes; drops the oldest entries until it fits. False if it never can, or when
  // the oldest entry is pinned by a download in progress (the caller counts the record as lost).
  bool begin(uint8_t fmt, uint32_t len, uint64_t seq, int64_t t0, uint32_t nowMs) {
    if (!ready() || len > 0xFFFF || RING_HDR + len > cap) return false;
    while (cap - used < RING_HDR + len && entries) {
      if (pinned && entrySeq(head) == pinSeq) {
        if (nowMs - pinMs <= PIN_MS) return false;
        pinned = false;
      }
      uint32_t l = entryBytes(head);
      head = (head + l) % cap;
      used -= l;
      entries--;
      evicted++;
    }
    w = (head + used) % cap;
    put(w, RING_MAGIC);
    put(w + 1, fmt);
    putLe(w + 2, len, 2);
    putLe(w + 4, seq, 8);
    putLe(w + 12, (uint64_t)t0, 8);
    w = (w + RING_HDR) % cap;
    wLen = len;
    return true;
  }
  void byte(uint8_t b) { put(w, b); w = (w + 1) % cap; }
  void commit() { used += RING_HDR + wLen; entries++; }

  // Offset of the first entry whose seq is >= seq, walking from the oldest; false when there is none.
  bool find(uint64_t seq, uint32_t &off) const {
    uint32_t o = head;
    for (uint32_t i = 0; i < entries; i++) {
      if (entrySeq(o) >= seq) { off = o; return true; }
      o = (o + entryBytes(o)) % cap;
    }
    return false;
  }
  bool oldestSeq(uint64_t &seq) const {
    if (!entries) return false;
    seq = entrySeq(head);
    return true;
  }
  bool newestSeq(uint64_t &seq) const {
    if (!entries) return false;
    uint32_t o = head;
    for (uint32_t i = 0; i + 1 < entries; i++) o = (o + entryBytes(o)) % cap;
    seq = entrySeq(o);
    return true;
  }
};

// The download is read in chunks while the ring keeps changing: the cursor names the next entry by its seq and
// the bytes of it already sent. Fills buf from the ring and returns the bytes written (0 = done). An entry that
// was dropped before its turn is skipped: the gap shows in the seq numbers of the file.
struct RingCursor {
  uint64_t nextSeq = 0;
  uint32_t sentOfEntry = 0;
  bool started = false;
};

inline size_t ringFill(RecRing &r, RingCursor &c, uint8_t *buf, size_t cap, uint32_t nowMs) {
  size_t n = 0;
  // A stop/reset or expired pin can remove the remainder of a partial entry.
  // End the file if that happened so later entries cannot become its payload.
  if (c.sentOfEntry && (!r.pinned || r.pinSeq != c.nextSeq)) return 0;
  r.pinned = false;
  while (n < cap) {
    uint32_t off;
    if (!r.find(c.nextSeq, off)) break;
    uint64_t seq = r.entrySeq(off);
    uint32_t total = r.entryBytes(off);
    while (n < cap && c.sentOfEntry < total) buf[n++] = r.get(off + c.sentOfEntry++);
    if (c.sentOfEntry < total) {               // part of this entry is out: pin it until the rest follows
      c.nextSeq = seq;
      r.pinned = true; r.pinSeq = seq; r.pinMs = nowMs;
      break;
    }
    c.nextSeq = seq + 1;
    c.sentOfEntry = 0;
  }
  return n;
}

// The download is over (finished or the client left): release the pin.
inline void ringFillDone(RecRing &r) { r.pinned = false; }

}  // namespace crec

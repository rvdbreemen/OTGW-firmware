#!/usr/bin/env python3
"""Prototype codecs for the OpenTherm frame stream (research, TASK-1862 / TASK-1473).

Input: an otframes.log (tab separated: host_time, device_time HH:MM:SS.uuuuuu, letter, 8 hex)
or, with --synthetic HOURS, a generated stream. Output: bytes per frame and per day for

  A  ADR-213 layout B (type u8, LEB128 dt ms, 4-byte frame; repeat = type+dt+MsgID; 4 KiB blocks)
  A+z  layout B compressed with zlib per 4 KiB block (upper bound of what deflate gains)
  Z  layout B as one zlib stream with a sync flush every 10 s (streaming over one connection)
  M  model codec: OT-aware prediction + adaptive binary range coder, context reset every N s

Every codec is lossless on (letter, 32-bit frame, ms timestamp). M is decoded again and compared.
"""
import argparse, random, re, sys, zlib, math

# ---------------------------------------------------------------- input

LINE = re.compile(r"(\d\d):(\d\d):(\d\d)\.(\d{3,6})\s+([TBRAE])\s*([0-9A-Fa-f]{8})")

def read_log(path):
    out, day = [], 0
    last = None
    with open(path, errors="replace") as f:
        for ln in f:
            if ln.startswith("#"):
                continue
            m = LINE.search(ln)
            if not m:
                continue
            h, mi, s, frac, letter, hx = m.groups()
            us = int(frac.ljust(6, "0"))
            t = ((int(h) * 60 + int(mi)) * 60 + int(s)) * 1000 + us // 1000 + day
            if last is not None and t < last - 3_600_000:   # midnight wrap
                day += 86_400_000
                t += 86_400_000
            last = t
            out.append((t, letter, int(hx, 16)))
    return out

def parity_ok(f):
    return bin(f).count("1") % 2 == 0

def with_parity(f):
    f &= 0x7FFFFFFF
    return f | (0x80000000 if bin(f).count("1") % 2 else 0)

def mk(mtype, did, val):
    return with_parity((mtype << 28) | (did << 16) | (val & 0xFFFF))

def f88(x):
    return int(round(x * 256)) & 0xFFFF

def synthetic(hours, seed=1):
    """Thermostat poll cycle with boiler replies, gateway overrides and slow physics.
    Modelled on the field rate (2.46 frames/s). Values have sensor noise on the low bits."""
    rnd = random.Random(seed)
    cycle = [0, 1, 0, 25, 0, 17, 0, 28, 0, 24, 0, 16, 0, 1, 0, 25, 0, 26, 0, 56, 0, 14, 0, 18,
             0, 5, 0, 3, 0, 27, 0, 116, 0, 120]
    t = 0.0
    out = []
    flame, ch = False, True
    setp, room, roomsp, water, ret, outside = 45.0, 20.4, 20.5, 40.0, 35.0, 8.0
    mod = 0.0
    idx = 0
    end = hours * 3600_000
    over_ids = {1}           # SAT overrides the control setpoint: T, R, B, A
    while t < end:
        did = cycle[idx % len(cycle)]
        idx += 1
        # physics, once per frame pair
        if rnd.random() < 0.002:
            flame = not flame
        target = setp if flame else ret
        water += (target - water) * 0.01 + rnd.gauss(0, 0.03)
        ret += (water - 6 - ret) * 0.005 + rnd.gauss(0, 0.02)
        room += rnd.gauss(0, 0.0005) + (0.0003 if flame else -0.0002)
        outside += rnd.gauss(0, 0.001)
        mod = max(0.0, min(100.0, mod + (rnd.gauss(0, 1.0) if flame else -mod)))
        if rnd.random() < 0.003:
            setp = max(25.0, min(70.0, setp + rnd.choice([-1, 1]) * rnd.randint(1, 5)))
        status_hi = (1 if ch else 0)
        status_lo = (0x0A if flame else 0x02)
        if did == 0:
            treq = mk(0, 0, status_hi << 8)
            bresp = mk(4, 0, (status_hi << 8) | status_lo)
        elif did == 1:
            treq = mk(1, 1, f88(round(setp + 3, 0)))
            bresp = mk(5, 1, f88(setp))
        elif did == 16:
            treq = mk(1, 16, f88(roomsp))
            bresp = mk(5, 16, f88(roomsp))
        elif did == 24:
            treq = mk(1, 24, f88(round(room * 100) / 100))
            bresp = mk(5, 24, f88(round(room * 100) / 100))
        elif did == 17:
            treq = mk(0, 17, 0)
            bresp = mk(4, 17, f88(round(mod)))
        elif did == 25:
            treq = mk(0, 25, 0)
            bresp = mk(4, 25, f88(round(water * 16) / 16))
        elif did == 28:
            treq = mk(0, 28, 0)
            bresp = mk(4, 28, f88(round(ret * 16) / 16))
        elif did == 27:
            treq = mk(0, 27, 0)
            bresp = mk(4, 27, f88(round(outside * 10) / 10))
        elif did == 26:
            treq = mk(0, 26, 0)
            bresp = mk(6, 26, 0)       # data invalid
        elif did in (116, 120):
            treq = mk(0, did, 0)
            bresp = mk(4, did, 1234 + (idx // 3000))
        else:
            treq = mk(0, did, 0)
            bresp = mk(4, did, (did * 37) & 0xFFFF)
        jit = rnd.gauss(0, 3)
        out.append((int(t), "T", treq))
        if did in over_ids:
            r = mk(1, 1, f88(setp))
            out.append((int(t + 2 + rnd.random() * 2), "R", r))
            out.append((int(t + 40 + jit), "B", bresp))
            out.append((int(t + 42 + jit), "A", mk(5, 1, (treq & 0xFFFF))))
        else:
            out.append((int(t + 40 + jit), "B", bresp))
        t += 1000 * (0.81 + rnd.gauss(0, 0.01))
        if rnd.random() < 0.0005:
            t += rnd.randint(2000, 15000)      # a bus gap
    return out

# ---------------------------------------------------------------- codec A (ADR-213 layout B)

def leb(n):
    b = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n:
            b.append(x | 0x80)
        else:
            b.append(x)
            return bytes(b)

def layout_b(frames, block=4096):
    blocks, cur, cache, last_t = [], bytearray(), {}, None
    def flush():
        nonlocal cur, cache
        if cur:
            blocks.append(bytes(16) + bytes(cur))
        cur, cache = bytearray(), {}
    for t, L, f in frames:
        dt = 0 if last_t is None else max(0, t - last_t)
        last_t = t
        did = (f >> 16) & 0xFF
        key = (L, did)
        rec = (bytes([1]) + leb(dt) + bytes([did])) if cache.get(key) == f else \
              (bytes([0]) + leb(dt) + f.to_bytes(4, "big"))
        if 16 + len(cur) + len(rec) > block:
            flush()
            rec = bytes([0]) + leb(dt) + f.to_bytes(4, "big")
        cur += rec
        cache[key] = f
    flush()
    return blocks

# ---------------------------------------------------------------- binary range coder (LZMA style)

PB = 12
P_INIT = 1 << (PB - 1)
SHIFT = 4

class Enc:
    def __init__(self):
        self.low, self.rng, self.cache, self.csize, self.out = 0, 0xFFFFFFFF, 0, 1, bytearray()
    def bit(self, probs, i, b):
        p = probs[i]
        bound = (self.rng >> PB) * p
        if b == 0:
            self.rng = bound
            probs[i] = p + (((1 << PB) - p) >> SHIFT)
        else:
            self.low += bound
            self.rng -= bound
            probs[i] = p - (p >> SHIFT)
        while self.rng < (1 << 24):
            self.rng = (self.rng << 8) & 0xFFFFFFFF
            self._shift()
    def direct(self, v, n):
        for k in range(n - 1, -1, -1):
            self.rng >>= 1
            if (v >> k) & 1:
                self.low += self.rng
            while self.rng < (1 << 24):
                self.rng = (self.rng << 8) & 0xFFFFFFFF
                self._shift()
    def _shift(self):
        if self.low < 0xFF000000 or self.low >= (1 << 32):
            carry = self.low >> 32
            temp = self.cache
            while True:
                self.out.append((temp + carry) & 0xFF)
                temp = 0xFF
                self.csize -= 1
                if self.csize == 0:
                    break
            self.cache = (self.low >> 24) & 0xFF
        self.csize += 1
        self.low = (self.low << 8) & 0xFFFFFFFF
    def finish(self):
        for _ in range(5):
            self._shift()
        return bytes(self.out[1:])     # first byte is always the initial cache 0

class Dec:
    def __init__(self, data):
        self.d, self.pos = b"\x00" + data + b"\x00" * 8, 1
        self.code, self.rng = 0, 0xFFFFFFFF
        for _ in range(4):
            self.code = (self.code << 8) | self.d[self.pos]
            self.pos += 1
    def bit(self, probs, i):
        p = probs[i]
        bound = (self.rng >> PB) * p
        if self.code < bound:
            self.rng = bound
            probs[i] = p + (((1 << PB) - p) >> SHIFT)
            b = 0
        else:
            self.code -= bound
            self.rng -= bound
            probs[i] = p - (p >> SHIFT)
            b = 1
        while self.rng < (1 << 24):
            self.rng = (self.rng << 8) & 0xFFFFFFFF
            self.code = ((self.code << 8) | self.d[self.pos]) & 0xFFFFFFFF
            self.pos += 1
        return b
    def direct(self, n):
        v = 0
        for _ in range(n):
            self.rng >>= 1
            b = 1 if self.code >= self.rng else 0
            if b:
                self.code -= self.rng
            v = (v << 1) | b
            while self.rng < (1 << 24):
                self.rng = (self.rng << 8) & 0xFFFFFFFF
                self.code = ((self.code << 8) | self.d[self.pos]) & 0xFFFFFFFF
                self.pos += 1
        return v

# ---------------------------------------------------------------- model codec M

LETTERS = "TBRAE"

class Model:
    """All adaptive state. Encoder and decoder run the identical model."""
    def __init__(self):
        self.p = {}
        self.prev_letter = 0
        self.last_t = None
        self.last_dt = {}            # (prev_letter, letter) -> dt
        self.next_id = {}            # last T id -> following T id
        self.last_tid = 0
        self.pending = {}            # letter -> (id, mtype, value) of the request it answers
        self.last_val = {}           # (letter, id) -> value
        self.last_type = {}          # (letter, id) -> mtype
    def probs(self, name, size):
        a = self.p.get(name)
        if a is None:
            a = self.p[name] = [P_INIT] * size
        return a

def tree_enc(e, probs, v, nbits):
    m = 1
    for k in range(nbits - 1, -1, -1):
        b = (v >> k) & 1
        e.bit(probs, m, b)
        m = (m << 1) | b

def tree_dec(d, probs, nbits):
    m = 1
    for _ in range(nbits):
        m = (m << 1) | d.bit(probs, m)
    return m - (1 << nbits)

def zz(n):
    return (n << 1) ^ (n >> 63) if n < 0 else n << 1

def unzz(u):
    return (u >> 1) ^ -(u & 1)

def num_enc(e, model, ctx, u):
    """u >= 0: bit length (0..32) via an adaptive tree, then the bits under the top bit."""
    n = u.bit_length()
    tree_enc(e, model.probs(("len",) + ctx, 64), n, 6)
    if n > 1:
        low = u - (1 << (n - 1))
        top = min(n - 1, 4)           # first bits adaptive per length, rest direct
        hi = low >> (n - 1 - top)
        tree_enc(e, model.probs(("mant", n) + ctx, 1 << (top + 1)), hi, top)
        rest = n - 1 - top
        if rest:
            e.direct(low & ((1 << rest) - 1), rest)

def num_dec(d, model, ctx):
    n = tree_dec(d, model.probs(("len",) + ctx, 64), 6)
    if n == 0:
        return 0
    if n == 1:
        return 1
    top = min(n - 1, 4)
    hi = tree_dec(d, model.probs(("mant", n) + ctx, 1 << (top + 1)), top)
    rest = n - 1 - top
    low = (hi << rest) | (d.direct(rest) if rest else 0)
    return (1 << (n - 1)) + low

RESP = {"B": "T", "A": "T"}      # B answers the last R if there was one, else T; A answers T

def predict(model, li):
    L = LETTERS[li]
    if L == "T":
        pid = model.next_id.get(model.last_tid, 0)
        return pid, None, None
    if L == "R":
        tid, tt, tv = model.pending.get("T", (0, 0, 0))
        return tid, None, None
    src = "R" if (L == "B" and "R" in model.pending) else "T"
    rid, rt, rv = model.pending.get(src, (0, 0, 0))
    ptype = {0: 4, 1: 5}.get(rt, 7)
    pval = rv if rt == 1 else None
    return rid, ptype, pval

def update(model, li, did, mt, val, dt_key, dt):
    L = LETTERS[li]
    model.last_dt[dt_key] = dt
    if L == "T":
        model.next_id[model.last_tid] = did
        model.last_tid = did
        model.pending = {"T": (did, mt, val)}
    elif L == "R":
        model.pending["R"] = (did, mt, val)
    model.last_val[(li, did)] = val
    model.last_type[(li, did)] = mt
    model.prev_letter = li

def encode_frame(e, model, t, L, f):
    li = LETTERS.index(L)
    tree_enc(e, model.probs(("let", model.prev_letter), 8), li, 3)
    dt_key = (model.prev_letter, li)
    dt = 0 if model.last_t is None else t - model.last_t
    model.last_t = t
    pdt = model.last_dt.get(dt_key, 0)
    num_enc(e, model, ("dt",) + dt_key, zz(dt - pdt))
    ok = parity_ok(f) and ((f >> 24) & 0xF) == 0
    e.bit(model.probs(("raw",), 1), 0, 0 if ok else 1)
    if not ok:
        e.direct(f, 32)
        update(model, li, (f >> 16) & 0xFF, (f >> 28) & 7, f & 0xFFFF, dt_key, dt)
        return
    mt, did, val = (f >> 28) & 7, (f >> 16) & 0xFF, f & 0xFFFF
    pid, ptype, pval = predict(model, li)
    hit = did == pid
    e.bit(model.probs(("idhit", li), 1), 0, 0 if hit else 1)
    if not hit:
        tree_enc(e, model.probs(("id", li), 256), did, 8)
    if ptype is None:
        ptype = model.last_type.get((li, did), 0)
    e.bit(model.probs(("thit", li, did), 1), 0, 0 if mt == ptype else 1)
    if mt != ptype:
        tree_enc(e, model.probs(("type", li), 8), mt, 3)
    ref = pval if pval is not None else model.last_val.get((li, did), 0)
    same = val == ref
    e.bit(model.probs(("vsame", li, did), 1), 0, 0 if same else 1)
    if not same:
        delta = ((val - ref + 0x8000) & 0xFFFF) - 0x8000
        num_enc(e, model, ("v", li, did), zz(delta) - 1)   # delta != 0
    update(model, li, did, mt, val, dt_key, dt)

def decode_frame(d, model):
    li = tree_dec(d, model.probs(("let", model.prev_letter), 8), 3)
    dt_key = (model.prev_letter, li)
    pdt = model.last_dt.get(dt_key, 0)
    dt = unzz(num_dec(d, model, ("dt",) + dt_key)) + pdt
    t = dt if model.last_t is None else model.last_t + dt
    model.last_t = t
    if d.bit(model.probs(("raw",), 1), 0):
        f = d.direct(32)
        update(model, li, (f >> 16) & 0xFF, (f >> 28) & 7, f & 0xFFFF, dt_key, dt)
        return t, LETTERS[li], f
    pid, ptype, pval = predict(model, li)
    did = pid if d.bit(model.probs(("idhit", li), 1), 0) == 0 else tree_dec(d, model.probs(("id", li), 256), 8)
    if ptype is None:
        ptype = model.last_type.get((li, did), 0)
    mt = ptype if d.bit(model.probs(("thit", li, did), 1), 0) == 0 else tree_dec(d, model.probs(("type", li), 8), 3)
    ref = pval if pval is not None else model.last_val.get((li, did), 0)
    if d.bit(model.probs(("vsame", li, did), 1), 0) == 0:
        val = ref
    else:
        delta = unzz(num_dec(d, model, ("v", li, did)) + 1)
        val = (ref + delta) & 0xFFFF
    update(model, li, did, mt, val, dt_key, dt)
    return t, LETTERS[li], with_parity((mt << 28) | (did << 16) | val)

CHUNK_HDR = 12    # u32 seq, u32 epoch seconds of the first frame, u16 frame count, u16 ms offset

def model_codec(frames, chunk_s):
    """Chunks of chunk_s seconds; the model resets at every chunk, so each chunk decodes alone."""
    chunks, i = [], 0
    while i < len(frames):
        start = frames[i][0]
        j = i
        while j < len(frames) and frames[j][0] - start < chunk_s * 1000:
            j += 1
        e, m = Enc(), Model()
        for t, L, f in frames[i:j]:
            encode_frame(e, m, t - start, L, f)
        chunks.append((start, j - i, e.finish()))
        i = j
    return chunks

def model_decode(chunks):
    out = []
    for start, n, data in chunks:
        d, m = Dec(data), Model()
        for _ in range(n):
            t, L, f = decode_frame(d, m)
            out.append((t + start, L, f))
    return out

# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", nargs="?")
    ap.add_argument("--synthetic", type=float, default=0)
    ap.add_argument("--chunks", default="10,60,600,3600")
    a = ap.parse_args()
    frames = synthetic(a.synthetic) if a.synthetic else read_log(a.log)
    if not frames:
        sys.exit("no frames")
    span_h = (frames[-1][0] - frames[0][0]) / 3_600_000
    per_day = 24 / span_h
    n = len(frames)
    print(f"frames {n}, span {span_h:.2f} h, {n / span_h / 3600:.2f} frames/s")
    def row(name, nbytes):
        print(f"{name:52s} {nbytes / n:6.2f} B/frame  {nbytes * per_day / 1024:8.0f} KiB/day  {nbytes * per_day / 86400:6.2f} B/s")
    text = sum(len(f"00:00:00.000000\t{L}\t{f:08X}\n") for _, L, f in frames)
    row("text line (device time + letter + hex)", text)
    blocks = layout_b(frames)
    row("A   ADR-213 layout B, 4 KiB blocks", sum(len(b) for b in blocks))
    row("A+z layout B, zlib -9 per 4 KiB block", sum(len(zlib.compress(b, 9)) for b in blocks))
    # Z: one deflate stream, sync flush every 10 s, over layout-B records without block resets
    rec = layout_b(frames, block=1 << 30)[0][16:]
    # approximate 10 s flush points by splitting the record stream proportionally
    co = zlib.compressobj(9)
    step = max(1, len(rec) * 10 // int(span_h * 3600))
    z = 0
    for k in range(0, len(rec), step):
        z += len(co.compress(rec[k:k + step])) + len(co.flush(zlib.Z_SYNC_FLUSH))
    row("Z   layout B, one zlib stream, sync flush per 10 s", z)
    for cs in [int(x) for x in a.chunks.split(",")]:
        ch = model_codec(frames, cs)
        size = sum(len(c[2]) + CHUNK_HDR for c in ch)
        row(f"M   model + range coder, reset per {cs} s chunk", size)
        if cs == 600:
            dec = model_decode(ch)
            ok = dec == frames
            print(f"    lossless round trip ({cs} s chunks): {'OK' if ok else 'MISMATCH'}")
            if not ok:
                bad = next(k for k in range(n) if dec[k] != frames[k])
                print("    first mismatch", bad, frames[bad], dec[bad])

if __name__ == "__main__":
    main()

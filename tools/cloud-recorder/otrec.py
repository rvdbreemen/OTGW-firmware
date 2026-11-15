"""Payload formats of the OTGW collection point batches (TASK-1938).

fmt 1, stream "ot": LEB128 frame count, then the range-coder chunk from
scripts/ot_stream_codec_study.py. Frame times are ms relative to the batch's t0
(epoch ms of the first frame). The model resets per batch, so every batch
decodes on its own.

fmt 4, stream "mix": all streams of one interval in one record. A sequence of
sections, each `type u8, LEB128 length, data`:
  1 = OT: LEB128 offset (ms) of the first frame from the record's t0, then the fmt 1 payload
  2 = SAT (opaque until the SAT encoder exists)
  3 = debug, address tokens, uncompressed
  4 = debug, address tokens, zlib
"""
import os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import ot_stream_codec_study as codec  # noqa: E402

FMT_OT_V1 = 1
FMT_MIX_V1 = 4
SEC_OT, SEC_SAT, SEC_DBG, SEC_DBG_Z = 1, 2, 3, 4

# Batch sizes measured with batch_size_study.py (one hour of data per variant, local PocketBase):
#   ot  300 s: 12 requests/h, 20.6 KB sent, 16 KB database per hour; 60 s costs 2.4x the traffic,
#              900 s saves 28 % traffic but loses up to 15 min on a reboot. One record per frame
#              ("per line") costs 1.75 MB sent and 1.6 MB database per hour.
#   dbg  60 s: 1.73 MB sent per hour at full debug; 10 s costs 15 % more, 120 s saves 1.7 %.
# A batch is also sent early when its payload reaches MAX_PAYLOAD_BYTES (the 65,536-char field).
BATCH_SECONDS = {"ot": 300, "sat": 300, "dbg": 60}
MAX_PAYLOAD_BYTES = 48 * 1024
# One "mix" record per interval (mix_size_study in the README): 60 s on every board. A board without PSRAM
# has a 4 KB recording buffer (ADR-213) and sends the record early when the buffer is full; with PSRAM the
# early flush is at MAX_PAYLOAD_BYTES.
MIX_SECONDS = 60
MIX_BUFFER_NO_PSRAM = 4 * 1024
# The create reply carries only the new id (about 275 B with headers) instead of the whole record.
CREATE_PATH = "/api/collections/batches/records?fields=id"


def _leb(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def _unleb(data):
    n = shift = 0
    for i, b in enumerate(data):
        n |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return n, i + 1
    raise ValueError("truncated LEB128")


def pack_ot(frames):
    """frames: [(epoch_ms, letter, frame32)], one batch. Returns (t0, payload bytes)."""
    t0 = frames[0][0]
    e, m = codec.Enc(), codec.Model()
    for t, letter, f in frames:
        codec.encode_frame(e, m, t - t0, letter, f)
    return t0, _leb(len(frames)) + e.finish()


def unpack_ot(t0, payload):
    n, used = _unleb(payload)
    d, m = codec.Dec(payload[used:]), codec.Model()
    out = []
    for _ in range(n):
        t, letter, f = codec.decode_frame(d, m)
        out.append((t0 + t, letter, f))
    return out


def split_batches(frames, seconds):
    """Cuts a frame list into batches of at most `seconds`, as a gateway would send them."""
    out, cur = [], []
    for fr in frames:
        if cur and fr[0] - cur[0][0] >= seconds * 1000:
            out.append(cur)
            cur = []
        cur.append(fr)
    if cur:
        out.append(cur)
    return out


def pack_mix(t0, ot_frames=(), sat=b"", dbg=b"", dbg_compressed=False):
    """One mix record. ot_frames: [(epoch_ms, letter, frame32)], all >= t0. Returns payload bytes."""
    out = bytearray()
    if ot_frames:
        first, data = pack_ot(list(ot_frames))
        data = _leb(first - t0) + data
        out += bytes([SEC_OT]) + _leb(len(data)) + data
    if sat:
        out += bytes([SEC_SAT]) + _leb(len(sat)) + sat
    if dbg:
        out += bytes([SEC_DBG_Z if dbg_compressed else SEC_DBG]) + _leb(len(dbg)) + dbg
    return bytes(out)


def unpack_mix(t0, payload):
    """Returns {section type: data}; the OT section is decoded to frames."""
    out, i = {}, 0
    while i < len(payload):
        typ = payload[i]
        n, used = _unleb(payload[i + 1:])
        data = payload[i + 1 + used:i + 1 + used + n]
        i += 1 + used + n
        if typ == SEC_OT:
            off, used2 = _unleb(data)
            out[typ] = unpack_ot(t0 + off, data[used2:])
        else:
            out[typ] = data
    return out


# ---------------------------------------------------------------- shared helpers for the SAT and debug sections

import re as _re
import struct as _struct

SPEC = _re.compile(r"%([-+ #0]*)(\d*)(?:\.(\d+))?(hh|h|ll|l|z)?([diuxXfcsS%])")


def _zz(n):
    return (n << 1) ^ (n >> 63) if n < 0 else n << 1


def _unzz(u):
    return (u >> 1) ^ -(u & 1)


def py_printf(fmt, args):
    """C printf for the conversions the firmware uses; args in conversion order."""
    out, pos, k = [], 0, 0
    for m in SPEC.finditer(fmt):
        out.append(fmt[pos:m.start()])
        pos = m.end()
        flags, width, prec, _length, conv = m.groups()
        if conv == "%":
            out.append("%")
            continue
        spec = "%" + flags + width + ("." + prec if prec is not None else "") + ("s" if conv == "S" else conv)
        out.append(spec % args[k])
        k += 1
    out.append(fmt[pos:])
    return "".join(out)


def conversions(fmt):
    return [m.group(5) for m in SPEC.finditer(fmt) if m.group(5) != "%"]


# ---------------------------------------------------------------- SAT section (type 2), version 1
#
# A sequence of ticks. Each tick: format index u8 (into SAT_FORMATS), LEB128 ms since the previous tick (the
# first: since the record's t0), LEB128 mask of the fields that changed since the previous tick of the same
# format in this section (all fields for the first one), then per changed field, in order:
#   number field (d, u, f): LEB128 of (zigzag(delta of the fixed-point value) << 1); the fixed-point value is
#       the printed number times 10^precision. A text that does not round-trip that way (nan, inf, -0.0) is
#       sent as LEB128((len << 1) | 1) + the text.
#   char field (c): 1 byte.   string field (s): length byte + bytes.
# The decoder rebuilds the exact SATTICK text from the format string.

SAT_FORMATS = [
    "SATTICK room=%.2f raw=%.2f rv=%d target=%.1f out=%.1f osrc=%c curve=%.2f p=%.2f i=%.2f d=%.2f praw=%.2f "
    "pid=%.2f cs=%.1f mm=%s ch=%u mode=%d pwm=%d duty=%.2f pon=%lu poff=%lu starts=%u flame=%d flow=%.1f "
    "ret=%.1f fc=%c fage=%lu rc=%c rage=%lu rsrc=%s dhw=0\r\n",
    "SATTICK target=%.1f mode=%d flame=%d flow=%.1f ret=%.1f fc=%c fage=%lu rc=%c rage=%lu rsrc=%s dhw=1\r\n",
]


def _sat_fields(fmt):
    """[(conv, precision)] per conversion."""
    return [(m.group(5), int(m.group(3) or 0)) for m in SPEC.finditer(fmt) if m.group(5) != "%"]


def _literals(fmt):
    parts, pos = [], 0
    for m in SPEC.finditer(fmt):
        if m.group(5) == "%":
            continue
        parts.append(fmt[pos:m.start()])
        pos = m.end()
    parts.append(fmt[pos:])
    return parts


def sat_field_texts(fmt, text):
    lit = _literals(fmt)
    rx = "^" + "(.*?)".join(_re.escape(p) for p in lit) + "$"
    m = _re.match(rx, text, _re.S)
    if not m:
        raise ValueError("text does not match the SATTICK format")
    return list(m.groups())


def sat_encode(t0, ticks):
    """ticks: [(epoch_ms, format index, rendered text)]. Returns section bytes."""
    out, prev_t, prev = bytearray(), t0, {}
    for t, fi, text in ticks:
        fmt = SAT_FORMATS[fi]
        fields = _sat_fields(fmt)
        vals = sat_field_texts(fmt, text)
        old = prev.get(fi)
        mask = 0
        for i, v in enumerate(vals):
            if old is None or old[i] != v:
                mask |= 1 << i
        out += bytes([fi]) + _leb(t - prev_t) + _leb(mask)
        for i, ((conv, p), v) in enumerate(zip(fields, vals)):
            if not mask >> i & 1:
                continue
            if conv in "diuf":
                fixed = _fixed(v, p, conv)
                ref = _fixed(old[i], p, conv) if old is not None else None
                if fixed is not None and (old is None or ref is not None):
                    out += _leb(_zz(fixed - (ref or 0)) << 1)
                    continue
                raw = v.encode()
                out += _leb(len(raw) << 1 | 1) + raw
            elif conv == "c":
                out += v.encode("latin-1")[:1]
            else:
                raw = v.encode()[:255]
                out += bytes([len(raw)]) + raw
        prev[fi] = vals
        prev_t = t
    return bytes(out)


def _fixed(text, p, conv):
    """Fixed-point int of a printed number, or None when it would not print back to the same text."""
    try:
        if conv == "f":
            n = round(float(text) * 10 ** p)
            back = "%.*f" % (p, n / 10 ** p)
        else:
            n = int(text)
            back = str(n)
    except ValueError:
        return None
    return n if back == text else None


def sat_decode(t0, data):
    """Returns [(epoch_ms, format index, text)]."""
    out, i, prev_t, prev = [], 0, t0, {}
    while i < len(data):
        fi = data[i]
        i += 1
        dt, u = _unleb(data[i:]); i += u
        mask, u = _unleb(data[i:]); i += u
        fmt = SAT_FORMATS[fi]
        fields = _sat_fields(fmt)
        old = prev.get(fi)
        vals = []
        for k, (conv, p) in enumerate(fields):
            if not mask >> k & 1:
                vals.append(old[k])
                continue
            if conv in "diuf":
                code, u = _unleb(data[i:]); i += u
                if code & 1:
                    n = code >> 1
                    vals.append(data[i:i + n].decode()); i += n
                else:
                    ref = _fixed(old[k], p, conv) if old is not None else 0
                    n = (ref or 0) + _unzz(code >> 1)
                    vals.append("%.*f" % (p, n / 10 ** p) if conv == "f" else str(n))
            elif conv == "c":
                vals.append(data[i:i + 1].decode("latin-1")); i += 1
            else:
                n = data[i]; i += 1
                vals.append(data[i:i + n].decode()); i += n
        lit = _literals(fmt)
        text = lit[0] + "".join(v + lit[j + 1] for j, v in enumerate(vals))
        prev_t += dt
        out.append((prev_t, fi, text))
        prev[fi] = vals
    return out


# ---------------------------------------------------------------- debug section (type 3, or 4 = zlib), version 1
#
# A sequence of items. Offsets are from DROM_BASE (0x3C000000), 3 bytes little-endian, and point at the C
# strings in the firmware image; a decoder reads them from the firmware.bin (or .elf) of the same build.
#   0x00 heap:     LEB128 free heap, LEB128 largest block (the values _debugBOL prints, sent when they change)
#   0x01 printf:   LEB128 us since the previous line (the first: since t0), format offset, function offset,
#                  LEB128 line, then the arguments in conversion order: d/i zigzag LEB128, u/x/X LEB128,
#                  f float32 LE, c 1 byte, s length byte (up to 127) + bytes
#   0x02 println:  LEB128 us, string offset, function offset, LEB128 line (prints the string and \r\n)
#   0x03 raw:      LEB128 length + bytes, printed as they are (no line prefix)
# Rendering (as _debugBOL + _debugPrintf_P do): "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): " + body.

DROM_BASE = 0x3C000000
DBG_HEAP, DBG_PRINTF, DBG_PRINTLN, DBG_RAW = 0, 1, 2, 3


def _u24(n):
    return (n & 0xFFFFFF).to_bytes(3, "little")


def dbg_encode(t0_ms, items):
    """items: ("heap", free, block) | ("raw", bytes) | ("println", t_us, str_addr, fn_addr, line) |
    ("printf", t_us, fmt_addr, fn_addr, line, args, fmt_text). args are the values in conversion order; the
    device reads the conversions from the format string it was given, as fmt_text stands in for here."""
    out, prev_us = bytearray(), t0_ms * 1000
    for it in items:
        kind = it[0]
        if kind == "heap":
            out += bytes([DBG_HEAP]) + _leb(it[1]) + _leb(it[2])
        elif kind == "raw":
            out += bytes([DBG_RAW]) + _leb(len(it[1])) + it[1]
        else:
            t_us, fmt_addr, fn_addr, line = it[1:5]
            out += bytes([DBG_PRINTF if kind == "printf" else DBG_PRINTLN]) + _leb(t_us - prev_us)
            out += _u24(fmt_addr - DROM_BASE) + _u24(fn_addr - DROM_BASE) + _leb(line)
            prev_us = t_us
            if kind == "printf":
                out += _dbg_args(conversions(it[6]), it[5])
    return bytes(out)


def _dbg_args(convs, args):
    out = bytearray()
    for conv, v in zip(convs, args):
        if conv in "di":
            out += _leb(_zz(int(v)))
        elif conv in "uxX":
            out += _leb(int(v) & 0xFFFFFFFF)
        elif conv == "f":
            out += _struct.pack("<f", v)
        elif conv == "c":
            out += bytes([int(v) & 0xFF])
        else:
            s = str(v).encode()[:127]
            out += bytes([len(s)]) + s
    return out


def dbg_decode(t0_ms, data, image, tz):
    """Returns the telnet bytes the device would have printed. image: esp_image.EspImage, tz: ZoneInfo."""
    import datetime
    out, i, t_us, heap = bytearray(), 0, t0_ms * 1000, (0, 0)
    while i < len(data):
        kind = data[i]
        i += 1
        if kind == DBG_HEAP:
            free, u = _unleb(data[i:]); i += u
            block, u = _unleb(data[i:]); i += u
            heap = (free, block)
            continue
        if kind == DBG_RAW:
            n, u = _unleb(data[i:]); i += u
            out += data[i:i + n]; i += n
            continue
        dt, u = _unleb(data[i:]); i += u
        t_us += dt
        fmt = image.read_cstr(DROM_BASE + int.from_bytes(data[i:i + 3], "little"))
        fn = image.read_cstr(DROM_BASE + int.from_bytes(data[i + 3:i + 6], "little"))
        i += 6
        line, u = _unleb(data[i:]); i += u
        ts = datetime.datetime.fromtimestamp(t_us // 1000000, tz)
        prefix = "%02d:%02d:%02d.%06d (%7u|%6u) %-12.12s(%4d): " % (
            ts.hour, ts.minute, ts.second, t_us % 1000000, heap[0], heap[1], fn, line)
        if kind == DBG_PRINTLN:
            out += (prefix + fmt + "\r\n").encode()
            continue
        args = []
        for conv in conversions(fmt):
            if conv in "di":
                v, u = _unleb(data[i:]); i += u
                args.append(_unzz(v))
            elif conv in "uxX":
                v, u = _unleb(data[i:]); i += u
                args.append(v)
            elif conv == "f":
                args.append(_struct.unpack("<f", data[i:i + 4])[0]); i += 4
            elif conv == "c":
                args.append(data[i]); i += 1
            else:
                n = data[i] & 0x7F; i += 1
                args.append(data[i:i + n].decode(errors="replace")); i += n
        out += (prefix + py_printf(fmt, args)).encode()
    return bytes(out)

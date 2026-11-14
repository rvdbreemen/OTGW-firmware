"""Payload formats of the OTGW collection point batches (TASK-1938).

fmt 1, stream "ot": LEB128 frame count, then the range-coder chunk from
scripts/ot_stream_codec_study.py. Frame times are ms relative to the batch's t0
(epoch ms of the first frame). The model resets per batch, so every batch
decodes on its own.
"""
import os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import ot_stream_codec_study as codec  # noqa: E402

FMT_OT_V1 = 1

# Batch sizes measured with batch_size_study.py (one hour of data per variant, local PocketBase):
#   ot  300 s: 12 requests/h, 20.6 KB sent, 16 KB database per hour; 60 s costs 2.4x the traffic,
#              900 s saves 28 % traffic but loses up to 15 min on a reboot. One record per frame
#              ("per line") costs 1.75 MB sent and 1.6 MB database per hour.
#   dbg  60 s: 1.73 MB sent per hour at full debug; 10 s costs 15 % more, 120 s saves 1.7 %.
# A batch is also sent early when its payload reaches MAX_PAYLOAD_BYTES (the 65,536-char field).
BATCH_SECONDS = {"ot": 300, "sat": 300, "dbg": 60}
MAX_PAYLOAD_BYTES = 48 * 1024
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

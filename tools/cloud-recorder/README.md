# Cloud recorder tools

Host-side tools for the opt-in diagnostic recording (TASK-1862, TASK-1938). The firmware does not send
anything yet: device streaming is TASK-1473, which is on hold.

| Path | What it does |
|---|---|
| `pocketbase/` | The collection point: PocketBase on a Synology behind a Cloudflare Tunnel. Setup guide in its README |
| `otrec.py` | Batch payload formats. fmt 1 = OT stream: LEB128 frame count + the range-coder chunk of `scripts/ot_stream_codec_study.py`, times in ms from the batch's `t0` |
| `simulate_gateway.py` | Uploads a generated OT stream the way a gateway would (test tool) |
| `export_batches.py` | Fetches one gateway's OT batches (superuser, LAN only) and writes an `otframes.log` that the firmware replay reads (ADR-209); missing batches become `#` note lines |
| `dbg_size_study.py` | Sizes the debug stream (text, address-token, both with zlib) on lines built from the firmware's real format strings |

## Round trip (tested 2026-11-14, local PocketBase in Docker)

```bash
python3 simulate_gateway.py http://127.0.0.1:8090 gw-test@devices.invalid '<pw>' --minutes 30 --batch 300 --skip-seq 3 --frames-out ref.tsv
python3 export_batches.py http://127.0.0.1:8090 <admin-email> '<admin-pw>' gw-test@devices.invalid otframes.log
```

Result: 5 batches sent (batch 3 left out), 3,920 frames, 4,683 B payload (1.19 B per frame). The export
gave back the same 3,920 frames with identical ms times, letters and frames, and one `# gap: batches 3..3
missing` line. The first 400 lines of the export, fed through the real replay code compiled from
`OTGW-Core.ino` (the `tests/test_replay_timing.py` harness), were all dispatched as bare `<letter><frame>`
lines in `timed` mode, 0 ms from the recorded timing.

## Debug stream

`dbg_size_study.py` uses the 985 Debug calls with a literal format in `src/OTGW-firmware` (help menu left
out) and the real `_debugBOL` prefix. Call-site mix and argument values are generated, so the volume is an
estimate; the ratios are the result. At about 3 KB/s of telnet text (29 lines/s):

| Form | B per line | B/s | Per gateway per month |
|---|---|---|---|
| Text | 104 | 3,039 | 7.9 GB |
| Text + zlib | 36 | 1,036 | 2.7 GB |
| Address token | 16 | 463 | 1.2 GB |
| Address token + zlib | 12 | 354 | 0.9 GB |

The address token is the function name's and the format string's flash offsets (3 bytes each, because
identical strings can share one address under LTO), the line number, a time delta and the arguments in
binary. The host resolves both offsets from the `.elf` of that build. The largest 60 s batch in token+zlib
form was 21.7 KB, which fits the 65,536-character payload field (48 KiB binary).

## Batch size (measured 2026-11-14, `batch_size_study.py`)

One hour of data per variant, one device, one keep-alive connection, fresh local PocketBase; every create asks
for `?fields=id`, so the reply is only the id (about 275 B with headers; without it the reply carried the whole
payload back). Bytes are HTTP headers + body; TLS and TCP are not counted.

| Variant | Requests/h | Sent | Received | Database growth | ms/request |
|---|---|---|---|---|---|
| OT, one record per frame (50 per `/api/batch` request) | 189 | 1,750 KB | 491 KB | 1,634 KB | 24.7 |
| OT, 10 s | 342 | 193 KB | 94 KB | 98 KB | 1.5 |
| OT, 60 s | 60 | 49 KB | 17 KB | 29 KB | 1.3 |
| **OT, 300 s** | **12** | **20.6 KB** | **3.3 KB** | **16 KB** | 1.6 |
| OT, 900 s | 4 | 14.9 KB | 1.1 KB | 16 KB | 1.7 |
| Debug, 10 s | 360 | 1,977 KB | 99 KB | 1,933 KB | 1.6 |
| Debug, 30 s | 120 | 1,773 KB | 33 KB | 1,868 KB | 1.9 |
| **Debug, 60 s** | **60** | **1,725 KB** | **16.5 KB** | **1,745 KB** | 1.8 |
| Debug, 120 s | 30 | 1,696 KB | 8.3 KB | 1,720 KB | 2.4 |

Chosen (in `otrec.py`): **OT and SAT every 300 s, debug every 60 s, and any batch early at 48 KiB.**

- One record per line costs about 85x the traffic and 100x the database of 300 s batches for the OT stream
  (about 1.2 GB per gateway-month), and 15 times the server time per request.
- OT: from 60 s to 300 s the traffic drops 58 % and the database 43 %; from 300 s to 900 s the database stays
  the same while a reboot can lose 15 minutes instead of 5. A 300 s OT batch is about 1.3 KB in RAM.
- Debug: from 10 s to 60 s saves 13 % traffic and 10 % database; 60 s to 120 s saves under 2 % and doubles the
  loss window. A 60 s debug batch is up to 21.7 KB (PSRAM).
- Rate: one gateway makes 12 + 12 + 60 = 84 requests per hour (1.4 per minute) against the limit of 30 per
  minute. After an outage the gateway resends its backlog at no more than 25 per minute: two hours offline is
  168 batches, about 7 minutes to catch up.

## One stream, all three on a fixed interval (`mix`, fmt 4; measured 2026-11-14, `mix_study.py`)

One record per interval holds an OT section, a SAT section and a debug section (`otrec.pack_mix`). The gateway
keeps one record buffer and sends it at every interval boundary, or earlier when the next item would not fit.
The SAT section is a 16 B stand-in every 38 s until the SAT encoder exists. One hour of data per profile,
fresh local PocketBase, `?fields=id` replies:

| Profile | Records/h | Sent early | Largest record | Sent/h | Database/h | OT decoded identical |
|---|---|---|---|---|---|---|
| PSRAM, 60 s, full debug (~3 KB/s text), zlib | 60 | 0 | 22.1 KB | 1.75 MB | 1.78 MB | yes |
| No PSRAM, 60 s, 4 KB buffer, debug ~0.3 KB/s, no compression | 60 | 0 | 3.8 KB | 301 KB | 291 KB | yes |
| No PSRAM, 60 s, 4 KB buffer, full debug, no compression | 480 | 420 | 4.1 KB | 2.49 MB | 2.47 MB | yes |

- **Every board: one record every 60 s.** Compared with separate streams (OT 300 s + debug 60 s), the gateway
  keeps one buffer and makes 60 instead of 72 requests per hour for the same bytes.
- **Without PSRAM the fixed interval holds only with limited debug.** With the 4 KB buffer of ADR-213, debug up
  to about 0.3 KB/s of text (about 3 lines/s) fits; with full debug the buffer fills every ~8 s (420 early sends
  per hour, 8 requests per minute, still under the limit of 30), and nothing is lost.
- **Without PSRAM there is no TLS** (ADR-218: the two 16.7 KB mbedTLS record buffers do not fit the 31.7 KB
  largest block, 14 of 14 handshakes failed). Such a board sends plain HTTP to the collection point on its own
  LAN (`http://<nas>:8090`), not through the tunnel. Over the internet only PSRAM boards can send.
- No compression on a board without PSRAM: zlib's compressor state is too large for the heap there. A small
  compressor (heatshrink, about 1 KB) was not measured.
- Export: `export_batches.py ... --stream mix` reads the OT section. Round trip with 59 mix records (one left
  out): 9,253 frames identical, one gap line.

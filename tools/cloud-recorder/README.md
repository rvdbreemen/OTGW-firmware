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

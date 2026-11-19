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
| `compress_study.py` | Compares lossless record compressors (zlib, LZSS, order-0/1/2 range coding, LZ-RC) on realistic records (TASK-1972) |

## Round trip (tested 2026-11-14, local PocketBase in Docker)

```bash
python3 simulate_gateway.py http://127.0.0.1:8090 otgw-00000000beef '<pw>' --minutes 30 --batch 300 --skip-seq 3 --frames-out ref.tsv
python3 export_batches.py http://127.0.0.1:8090 <admin-email> '<admin-pw>' otgw-00000000beef otframes.log
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

## Decoders for all three streams, and the proof (2026-11-14)

| File | What it does |
|---|---|
| `otrec.py` | The section formats and their encoders/decoders: OT (fmt 1 inside the mix record), SAT (`sat_encode`/`sat_decode`: per tick a field mask and fixed-point deltas, exact SATTICK text back), debug (`dbg_encode`/`dbg_decode`: address tokens, rebuilt into the telnet bytes) |
| `esp_image.py` | Reads C strings from an ESP32 `firmware.bin` by their load address (DROM segment) |
| `decode_all.py` | Downloads a gateway's mix records and writes `otframes.log`, `sat.log` and `debug.log`; needs the `firmware.bin` of the build that recorded |
| `ref_printf.c` | Reference renderer: the C library's `snprintf` per conversion, as `vsnprintf_P` on the device |
| `proof_all_streams.py` | The end-to-end proof below |

The proof uses the `firmware.bin` of the CI build of this branch (esp32-combo, commit `48a0eb2`, artifact digest
`sha256:59d8f3d8…`). 791 of the 854 debug calls with a line prefix have their format string as a whole C string
in that image (the rest are mostly a UTF-8 artefact of the source scan); both SATTICK format strings are in it
byte for byte. 30 minutes per profile, uploaded to a fresh local PocketBase, downloaded and decoded:

| Profile | OT frames | SATTICK lines | Debug lines (text) | Mix records | Largest | Stored | Result |
|---|---|---|---|---|---|---|---|
| No PSRAM (4 KB buffer, ~0.3 KB/s debug, no compression) | 4,706 | 48 | 5,554 (581 KB) | 51 | 4,062 B | 129 KB | all three identical |
| PSRAM (60 s, full debug, zlib) | 4,706 | 48 | 53,857 (5.6 MB) | 30 | 19,528 B | 569 KB | all three identical |

"Identical" means: the OT frames with their ms times, every SATTICK line (both formats, with `nan` and `-0.00`
fields, which go as text), and the debug output byte for byte against the text that C's `snprintf` renders,
including the `_debugBOL` prefix with time, heap and function name. Negative control: one flipped bit in one
debug section makes the debug comparison fail.

Not covered: a real device. The encoders here stand in for the firmware's; the formats are what a device-side
encoder has to write (TASK-1473, on hold). The argument values are generated, so the sizes are estimates.

## Firmware proof of concept (TASK-1963, branch only, `2.0.0-poc.N`)

The gateway records and uploads by itself over HTTPS, also on a board without PSRAM. Built in the env
`esp32-combo-tlsdyn` (`./build.sh --target esp32-combo-tlsdyn`), never in a normal build.
The wrapper puts its PlatformIO SDK packages in a separate core directory with an `-otgw-tlsdyn` suffix.
On a machine that built earlier versions of this experiment using the normal core,
reinstall the normal Arduino ESP32 framework and SDK libraries before using that core for standard builds.

- **TLS without PSRAM:** the IDF libraries are rebuilt (pioarduino `custom_sdkconfig`) with
  `CONFIG_MBEDTLS_DYNAMIC_BUFFER` and its free options, and DTLS off (Kconfig drops the dynamic buffers while
  DTLS is on). Records are allocated when needed and freed after the handshake.
- **Firmware:** `CloudRecCodec.h` (the encoders, shared with `tests/test_cloudrec_codec.py`), `CloudRecDevice.h`
  (capture, record queue, upload task), `CloudRec.h` / `CloudRec.ino`. Capture: a tee behind `Debug()` /
  `Debugln()`, hooks in `_debugBOL` / `_debugPrintf_P`, and `processOT()` next to the ADR-213 recorder.
  OT section v2 (fixed tables, `otrec.unpack_ot2`), mix record every 60 s or when a section is full.
- **Upload:** own task, HTTPS POST over esp-tls with TLS session resumption (tickets; a failed resumption falls back
  to a full handshake; TASK-1974) to the collection point, certificate check against GTS Root R1/R4 and
  ISRG Root X1/X2, `?fields=id`. Off unless `/cloudrec.cfg` on LittleFS has `enabled=1` plus `host` and `port`
  (the opt-in).
- **One recorder, two sinks (TASK-1991):** the same closed records also go to a local ring (`RecRing.h`: 1.5 MB
  of PSRAM, or 8 KB of heap with the ADR-213 start rule), LZ-coded as sent. Local recording is the existing switch
  (web UI Advanced > Debug, `/api/v2/diag/recording/start|stop`, resumes at boot); in this env it drives this ring
  and `diagRec` stays off. Capture runs while local recording is on or the cloud logs, so a cloud outage no longer
  pauses a local recording. `GET /api/v2/diag/recording` gives an `OTGWMIX1` file:
  `python3 decode_all.py --file otgw-recording.bin out/ --image <the .ino.bin>` writes the same four logs.
  Phase 2 (after the bench): the normal build switches and `diagRec` retires.
- **Cloud export across firmware updates:** the collection point stores the firmware ELF SHA-256 with
  each new batch. Apply the PocketBase migrations before updating gateways. Pass one `--image` per
  firmware binary in the retained history; the exporter matches each tagged batch to its image.
  If the history contains both tagged and older untagged batches, supply the older image with `--legacy-image`.
- **Events (TASK-1473, ADR-243):** a fifth section (SEC_EVT, 1,408 B) records everything the firmware receives,
  sends or measures besides OT/SATTICK/debug: commands and responses, MQTT, REST (GET 200 counted per route per
  minute), sensors (Dallas, S0, BLE, weather), WiFi, settings, time sync, reboot, telnet keys, webhook, firmware
  events, health every minute, mode changes and the port 25238 bridge commands. `decode_all.py` writes them to
  `events.log` (one line per event: time, type, fields as JSON). Every call site is inside `#if HAS_CLOUD_REC`.
- **Telemetry mode (TASK-1979):** the device record's `mode` / `flags` / `mode_until` (set by the maintainer) switch
  the log flags of `debugFlags.h` from the login and the token refresh every 15 minutes (`crec::parseRemote`,
  `crec::remoteEffective`, `cloudRecApplyRemote`); flags the user had on stay on afterwards.
- **Send queue (TASK-1974):** one record slot; a second one in PSRAM on PSRAM boards, oldest sent first.
- **SATTICK (TASK-1974):** with SAT debug off the SATTICK lines go only to the recorder (`SATTickTf` in
  `SATcontrol.ino`); with SAT debug on they go to telnet and are recorded from there, once.
- **Compression (TASK-1972):** a closed record goes out LZ-coded (fmt 5: LEB128 raw length + range-coder bytes,
  `crec::lzEncode`, reference `otrec.lz_encode`/`lz_decode`) when that is smaller, else as it is (fmt 4). LZ77 over
  the record itself (no window buffer), every decision through the range coder; 3.2 KB of RAM. Two passes: the
  first counts (Content-Length, raw or LZ), the second streams the bytes as base64 to TLS, so there is no second
  record buffer. Measured with `compress_study.py` (random argument values, so on the high side): 72 to 84 % of the
  raw record with light debug, 65 to 81 % with full debug, within 0.5 point of zlib level 9 (whose compressor does
  not fit a board without PSRAM). `tests/test_cloudrec_lz.py`: same bytes as the reference, lossless.
- **Enrollment and blocking (TASK-1971):** the gateway's uid is `otgw-` + its factory MAC; its secret (128 bits)
  is made once and kept in NVS, so a filesystem flash keeps the enrollment. The first login of an unknown uid
  enrolls it. A 403 reply that holds `otgw-blocked` (TASK-1972, any request) makes the gateway write `enabled=0` to
  `/cloudrec.cfg` at once and end its upload task: no further connection, and the opt-in is off until the user
  turns it on again. A uid that is enrolled with another secret (after a full
  flash erase) is retried every 24 h until the maintainer deletes the old record.
- **Service down (TASK-1971):** capture runs only while the service answers. It starts paused and resumes after a
  login that is not blocked. A failed upload or login pauses capture, drops what was not sent, and waits
  5, 10, 20, 40, 80, 160, 320, 640, 1280 min and then 24 h per further failure (each + 0 to 20 % random), back to
  5 min after a success (`CloudRecPolicy.h`, `tests/test_cloudrec_policy.py`). The gateway's own network going
  down pauses capture without a backoff step.
- **Sizes against a normal esp32-combo build:** static RAM +22.0 KB (152,524 vs 130,524 B); app 98.9 % of the
  2240K slot (26 KB left; this env cannot use LTO).
- **Since poc.12 (2026-11-19, TASK-1963): stock libraries, no hybrid compile.** poc.10's dynamic mbedTLS buffers
  panicked on .63 when freeing the RX buffer after the HTTP response (heap poisoning assert; likely
  espressif/esp-idf#14444). The env now links the stock Arduino libraries (esp-tls on mbedTLS, static 16 KB record
  buffers, no session tickets), and `cloudRecInstallTlsAllocator()` (first line of `setup()`) routes mbedTLS
  blocks above 4 KB to PSRAM through `mbedtls_platform_set_calloc_free()`. The upload only runs on a board with
  PSRAM. The notes below describe poc.1 to poc.11.
- **Fixed 2026-11-15 (TASK-1974): poc.1 to poc.4 did not contain the dynamic mbedTLS buffers.** The hybrid compile
  rebuilt mbedTLS with the env's settings but copied only top-level archives, so the firmware linked the prebuilt
  mbedTLS of the Arduino release (static 16 KB record buffers, no session tickets). `scripts/tlsdyn_mbedtls_libs.py`
  copies the nested archives; from poc.5 the ELF has the dynamic-buffer code (`esp_mbedtls_free_buf`,
  `esp_mbedtls_reset_free_rx_buffer`) and the session tickets. The app shrank by 21 KB (2,250,611 B, 98.1 %).
- **Build caveats found:** PlatformIO 6.2.0 fails the hybrid compile (SCons 4.11; 6.1.18 works); `-flto` must
  not reach the IDF sources; ESP Insights must be removed; and the hybrid compile writes its libraries into the
  shared `framework-arduinoespressif32-libs` package, so later builds on the same machine use them until that
  package is reinstalled. A line added to `custom_sdkconfig` only takes effect after deleting the generated
  `sdkconfig.esp32-combo-tlsdyn` in the repo root (gitignored): an existing file wins over the new defaults.
- **Not tested yet:** everything on a device (TLS heap peak, low-water, soak). That is the bench run of TASK-1963.

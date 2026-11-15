# Encoding the OT stream for minimal bytes: study

Status: research for the maintainer (TASK-1862, input for TASK-1473 which is on hold), 2026-11-14.
Nothing is implemented in the firmware. Prototype: `scripts/ot_stream_codec_study.py`.

## The question

Robert, 2026-11-14: the stream carries little data. How do we encode it so that the fewest bytes go
over the line while the most information arrives, with minimal storage and the data streamed to an
online cloud service?

## Short answer

- **An OpenTherm-aware model with an adaptive range coder brings every frame, with its exact ms timing,
  to about 0.9 to 1.1 bytes**: about 200 to 240 KiB per day per gateway. That is 3.5x to 4x smaller than ADR-213
  layout B (3.8 B per frame) and about 2x smaller than layout B with zlib. It is lossless: the prototype
  decodes its own output back to the identical frames and times.
- **Most of the remaining bits are timing**: about 5 of the 7 bits per frame. A time unit of 4 ms instead of
  1 ms gives 0.68 B per frame (150 KiB per day), and 10 ms gives 0.56 B (123 KiB per day). On PIC boards the
  device time is only good to about ±2 ms (2 ms serial task tick, then queue latency), so 1 ms already
  carries some noise. 4 ms is the honest resolution there.
- **On the wire, the packet overhead decides, not the payload.** At about 2.4 B/s, a 10 s batch carries 24 B
  of data in about 80 B of WebSocket, TLS 1.3 and TCP/IP headers (77 % overhead). A 60 s batch has 37 %
  overhead; a 300 s batch has 10 %. Batch per 60 s on one persistent connection.
- **Storage barely matters any more.** At about 200 KiB per day:
  - The cloud keeps exactly what the device sends, with no decoding on the server. 10 GB of R2 then
    holds about 1,700 gateway-months.
  - On the device, a 16 KB RAM buffer covers about 2 h of connection loss.
  - The free LittleFS space (about 230 KiB, TASK-1862) holds about a day of every frame with timing. That
    changes the earlier conclusion that 24 h of every frame does not fit on LittleFS.

## What the model uses

Each frame is: source letter (T B R A E), time since the previous frame, and the 32-bit frame (parity,
message type, spare, DataID, 16-bit value). The model predicts each part from what came before. The range
coder (LZMA style, 12-bit adaptive probabilities) then spends close to nothing on a correct prediction.

| Part | Prediction | Why it works on OpenTherm |
|---|---|---|
| Source letter | From the previous letter (T is followed by B, or by R in an override) | The bus is strictly request, then reply |
| DataID of a reply (B, A) and of a gateway frame (R) | Same ID as the request it answers | A reply always carries the request's ID |
| DataID of a request (T) | The ID that followed this ID the last time | Thermostats poll a fixed cycle |
| Message type | Read becomes Read-Ack, Write becomes Write-Ack; else the last type for this source and ID | Spec v4.2 message types |
| Value of a Write-Ack | The value of the write request (echo) | The slave echoes the written value |
| Other values | The last value for this source and ID; otherwise a signed delta coded by bit length | Temperatures in f8.8 move slowly; status flags rarely change |
| Time | The last gap for the same letter pair (for example T to B), coded as a signed residual | The cadence and the boiler's reply delay are steady |
| Parity, spare bits | Recomputed; a frame with bad parity or spare bits set is stored raw (32 bits) | Nothing is lost, also not a corrupt frame |

The model resets at the start of each stored chunk, so every chunk decodes on its own (as in ADR-213).
The reset costs bits while the model learns again: per 10 s chunk 3.2 B per frame, per 600 s 1.1 B,
per 3600 s 0.95 B. So **send in small batches, but store in chunks of 10 to 60 minutes**: the model
keeps running across batches on one connection, and the server appends the batches of one chunk to one
R2 object.

## Measurements

Generated stream, 24 h, 224,518 frames (2.60 frames per second, a 34-ID poll cycle, SAT overriding
MsgID 1 with T R B A, sensor noise on the low bits, an occasional bus gap). The generator is calibrated
against the real 20 h tester capture of TASK-1862: layout B gives 831 KiB per day here against 848
measured, and layout B with zlib per 4 KiB block gives 425 against 479.

| Encoding (lossless: letter, frame, ms time) | B per frame | KiB per day | B/s |
|---|---|---|---|
| Text line (device time, letter, 8 hex) | 27.0 | 5,920 | 70.2 |
| ADR-213 layout B, 4 KiB blocks | 3.79 | 831 | 9.85 |
| Layout B, zlib -9 per 4 KiB block | 1.94 | 425 | 5.04 |
| Layout B, one zlib stream, sync flush every 10 s | 1.94 | 426 | 5.05 |
| Model + range coder, reset per 600 s | 1.10 | 241 | 2.86 |
| Model + range coder, reset per 3600 s | 0.95 | 209 | 2.48 |
| Model + range coder, no reset (upper bound) | 0.91 | 200 | 2.37 |
| Same, time unit 4 ms (no reset) | 0.68 | 150 | 1.78 |
| Same, time unit 10 ms (no reset) | 0.56 | 123 | 1.46 |

Bit budget per frame (6 h, 600 s chunks): frame structure 2.7 bits, time 5.2, values 0.8.

Model state on the device: 195 probability tables with 4,353 entries, about 8.7 KB as u16 for the
34 IDs of the generated stream. A real installation with more IDs needs more; that has to be measured
(estimate 10 to 15 KB, PSRAM). Speed is no issue at 2.6 frames per second.

**The generated stream decides nothing on its own.** Timing jitter and sensor noise set the result, and
both are guesses here. Run the script on the real capture before any decision:

```bash
python3 scripts/ot_stream_codec_study.py <capture>/otframes.log --chunks 60,600,3600
```

## Transport overhead per batch

At 2.37 B/s of payload, over one persistent WebSocket over TLS 1.3 (WebSocket client frame 6 to 8 B, TLS 1.3
record 22 B, TCP/IP 52 B; ACKs and keep-alives come on top):

| Batch interval | Payload | Headers | Overhead |
|---|---|---|---|
| 10 s | 24 B | 80 B | 77 % |
| 60 s | 142 B | 82 B | 37 % |
| 300 s | 711 B | 82 B | 10 % |

An HTTPS POST per batch adds about 150 to 300 B of HTTP headers per request and is worse at every interval.

## Cost per gateway per month (Cloudflare, 2026-11-14)

Prices from the [Durable Objects](https://developers.cloudflare.com/durable-objects/platform/pricing/) and
[R2](https://developers.cloudflare.com/r2/pricing/) pricing pages. Assumed design: one Durable Object per
gateway with a hibernatable WebSocket. Every batch goes into SQLite as one row, and an hourly alarm writes
that hour as one R2 object and deletes the rows. The OT stream is about 209 KiB per day.
The handler times are estimates and were not measured: 10 ms per message, 300 ms per flush.

| Per gateway, 30 days | 60 s batches | 300 s batches |
|---|---|---|
| DO requests (WebSocket messages 20:1, alarms, reconnects) | 2,910 | 1,182 |
| DO duration (hibernation) | 81 GB-s | 38 GB-s |
| SQLite rows written | 44,640 | 10,080 |
| R2 PutObject / storage | 720 / 6.4 MB | 720 / 6.4 MB |
| Cost at the paid rates beyond the included amounts | $0.050 | $0.014 |
| Free plan: gateways before the first limit (rows written, 100,000 per day) | about 67 | about 297 |
| Workers Paid ($5 per month minimum): gateways inside the included amounts (DO requests, 1M per month) | about 340 | about 840 |

- **The binding free limit is SQLite rows written, not requests.** An earlier version of this section
  said about 1,380 gateways fit the free tier from requests alone; that was wrong.
- Without hibernation a Durable Object bills its full wall-clock time: 324,000 GB-s, about $4.05 per gateway
  per month. Hibernation is therefore a requirement.
- Text debug output at about 3 KB/s would be 7.8 GB per gateway per month in R2: $0.12 per month and over
  the free 10 GB with one gateway. Tokenized debug output keeps this in the range of the OT stream.

## The rest of the stream

- **SAT events**: the SATTICK control tick (30 fields, 95 per hour) goes into the same coder. Each field
  has its own context and is predicted from its previous value, as in the "delta-coded" row of TASK-1862
  (38 KiB per day). Expect less than that; not measured.
- **Debug text** is the large stream (up to about 3 KB/s with every module on). Text does not compress
  well. The known technique is tokenized logging: at compile time each format string becomes a 32-bit
  hash, the device sends the hash and the binary arguments, and the host turns them back into text from
  a token database cut from the ELF. Candidate: Pigweed `pw_tokenizer` (Apache-2.0, 65599 hash)
  ([source](https://pigweed.googlesource.com/pigweed/pigweed/+/6dc23f7e09c19eded0065aab3ba7f65ab1a95036/pw_tokenizer/public/pw_tokenizer/internal/)).
  Second candidate to check: Trice (licence not verified). It fits our macros, because every
  `DebugTf(PSTR(...))` passes through `_debugPrintf_P` (`debugStuff.ino:34-41`). A typical 60 to 100 B
  line becomes about 6 to 12 B before the range coder.
- **Inputs instead of outputs**: maximum information per byte comes from recording what the firmware cannot
  compute: OT frames, external commands, sensor readings, weather, setting changes, reboots. MQTT publishes
  and most debug lines follow from those on the same build. ADR-213 rejected "inputs only" for the local
  recording, because a replay on an older build does not reproduce the field outputs. For the cloud stream
  it is still the cheapest base, with outputs added per module as needed.

## Open points

1. Run the script on the real 20 h capture (and on a second unit). Choose the time unit (1 or 4 ms) from
   the measured jitter.
2. Model RAM with a real ID set; whether a fixed starting table (priors) shipped in the firmware makes
   short chunks cheap.
3. Port the encoder to C for the device. The decoder and the chunk-to-replay converter (ADR-209 replay
   format) stay on the host.
4. Licence and size check of the tokenizer candidates under the open-source rule in CLAUDE.md.
5. Everything here assumes Robert's TASK-1473 decisions (continuous stream, Cloudflare Durable Object + R2
   EU, outbound TLS on PSRAM boards). TASK-1473 stays on hold.

## Debug stream: measured size and the tokenizer choice (2026-11-14)

Measured with `tools/cloud-recorder/dbg_size_study.py` on lines built from the 985 Debug calls with a literal
format in `src/OTGW-firmware` (help menu left out) and the real `_debugBOL` prefix. The call-site mix and the
argument values are generated, so the volume is an estimate; the ratios hold. At about 3 KB/s of telnet text:
text 104 B per line (7.9 GB per gateway-month), text + zlib 36 B (2.7 GB), address token 16 B (1.2 GB),
address token + zlib 12 B (0.9 GB). At 0.3 KB/s the token + zlib form is 15 % of the text.

The candidates, checked on their primary sources:

| | pw_tokenizer (Pigweed) | Trice | Address form (the principle of Zephyr's dictionary logging) |
|---|---|---|---|
| Licence | Apache-2.0 | MIT | own code (Zephyr itself is Zephyr-only) |
| Token | 32-bit hash of the format string at compile time (C++ constexpr) | ID per call, list in `til.json` | flash offset of the format string (and of the function name), resolved from the build's ELF |
| Arguments | ints zigzag varint, float 4 B, strings length byte + up to 127 B | binary; runtime strings through `triceS` | same encoding as pw_tokenizer (own) |
| Build change | linker section script (`pw_add_tokenizer_sections_to_default_script.ld` or an include in the custom ESP32 script) and `-fdata-sections`; Pigweed source files | library compiled as C; Go host tool | none |
| Call sites | the docs advise against `PW_TOKENIZE_TO_BUFFER` in widely expanded macros such as a logging macro (use `pw_log_tokenized`); our formats sit in `PSTR(...)`, untested | all 1,270 calls rewritten to Trice calls | unchanged: only `_debugPrintf_P` and `_debugBOL` change |
| Host side | `database.py` builds a token CSV from the ELF | `til.json` | a decoder that reads strings at the given offsets from the ELF of the githash in the stream header |

Sources: [pw_tokenizer tokenization](https://pigweed.dev/pw_tokenizer/tokenization.html),
[pw_tokenizer get started](https://pigweed.dev/pw_tokenizer/get_started.html),
[Trice](https://github.com/rokath/trice) and its user manual,
[Zephyr logging](https://docs.zephyrproject.org/latest/services/logging/index.html) (dictionary-based logging:
"references to the ELF file are encoded instead of the whole strings"; the database works only with the same build).

Recommendation: the address form, built here (estimate 150 to 200 lines on the device, a host decoder next to
`export_batches.py`). Reason: no change to any of the 1,270 call sites and no linker-script change on the
ESP32 Arduino build, while the result is as small as a hashed token. Cost: the decoder needs the exact `.elf`
of each build that recorded, which we already publish with every zip. Under LTO, identical format strings can
share one address, so the record also carries the function name's offset and the line (included in the
numbers above). Known Zephyr issue for comparison: its database generator missed strings with section
stripping under LTO (zephyr issue 115815); reading the string directly at its address in the final ELF avoids
a separate database.

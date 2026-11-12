# Minimal OT log on the device and a daily upload: research and design proposal

Status: proposal for the maintainer (TASK-1862), 2026-11-12. Nothing is implemented. Docs only.

## The question

Robert, 2026-11-12: keep a minimal OpenTherm log on LittleFS (a date and a high-resolution
timestamp, the OpenTherm frame, and the most essential SAT events). How big are 24 hours and
7 days? Is there a simple way to upload it once a day to a free service such as Cloudflare,
where the data is collected? Opt-in, switched on during onboarding or in the settings.

## Short answer

| What is logged (per frame: source letter, 4-byte frame, time delta) | 24 h | 7 days |
|---|---|---|
| Every OT frame, ms timestamps (ADR-213 layout B) | **848 KiB** | **5.8 MiB** |
| Every OT frame, µs timestamps | 1,111 KiB | 7.6 MiB |
| Only frames whose value changed, ms (repeats dropped) | 190 KiB | 1.3 MiB |
| Essential SAT events (the SATTICK control tick, delta-coded) | 38 KiB | 0.26 MiB |
| Same SAT tick, every field as int16 | 139 KiB | 0.95 MiB |
| For comparison: the same OT frames as a text log line per frame | 4,938 KiB | 33.8 MiB |

- **24 h of every frame does not fit on LittleFS** (about 230 KiB is unreserved, see below).
  **7 days fits in no encoding.** 7 days means the cloud (or a PSRAM ring, lost on a reboot).
- The "changed values only" profile with the SAT events (about 230 KiB per day) is the only one
  that comes near a day on LittleFS, and it costs the timing of every repeated frame (below).
- Cloudflare's free plan has room for this data. The upload itself is the hard part:
  Cloudflare and comparable services want HTTPS, and the firmware does not do HTTPS
  (ADR-003). The ways around that are below, with their consequences.

## How the numbers were measured

- Source: a 20 h tester capture (`otframes.log` from the capture tool, 178,607 frames,
  8,872 frames per hour, PIC gateway with SAT on), kept outside the repository. The size
  model encodes the real frames; time deltas come from the device's own clock column
  (µs resolution), not the receiving PC, so WebSocket jitter does not inflate them.
  Script: a scratch copy of the ADR-213 layout B encoder (type u8, delta LEB128, 4-byte
  frame; a repeat is type + delta + MsgID; 16-byte header per self-decoding 4 KiB block).
- Cross-check against a second, independent source: ADR-213 / `recording-budget-1470.md`
  measured **881 KiB per day** for the OT frames of another unit (12.8 h field run), 4 % from
  the 848 KiB here, and 307 KiB per day changes-only (190 KiB here: that boiler changes
  values more often).
- SAT: the 1,920 SATTICK lines of the same capture (95 per hour, 30 fields each). The
  "delta-coded" row stores a 32-bit field mask plus only the fields that changed since the
  previous tick. This is an estimate from the telnet text, not a firmware encoder.
- Compression: zlib on each 4 KiB block on the PC brings every-frame ms to 479 KiB per day and
  changes-only to 114 KiB per day. That is an optimistic bound: an on-device compressor
  (heatshrink, for example) is weaker and needs RAM, and was not measured.

### What "changed values only" costs

87 % of the OT frames repeat the previous value for their source and MsgID (ADR-213 field
run; same order here). Dropping them shrinks the log by a factor of 4.5, but:

- the log no longer shows when each frame came, so a timed replay (TASK-1508) of that day is
  not possible, and a bus gap is not visible (you cannot tell "no frame" from "same value");
- Robert chose in ADR-213 (open question 4) to keep the repeat records for exactly that reason.

A cheap middle way, not measured: keep changes-only plus one "alive" record per MsgID per
minute, so gaps stay visible. Estimate: under 50 KiB per day extra.

## Where the data can live on the device

| | LittleFS file | PSRAM ring (ADR-213 recorder) |
|---|---|---|
| Space | Partition 768 KiB. The beta.197 image uses about 110 of 192 blocks (counted as non-erased blocks, an estimate). `build.py` keeps 96 KiB for runtime files (`FS_RUNTIME_RESERVE_KIB`, build.py:823). About **230 KiB** is left, and the next web asset takes from the same space (ADR-194). | 1.5 MB, already decided in ADR-213. Holds about 40 h of every frame + SAT ticks. |
| Survives a reboot | Yes. This is the reason to use LittleFS. | No. |
| Survives a filesystem update | **No.** A filesystem write replaces the partition (TASK-1524), and the project ships those often. The log must be uploaded (or downloaded) first, or it is lost. | No. |
| RAM cost | A write buffer. On a board without PSRAM the TASK-1528 night run reached a free-heap low-water mark of 536 B, so the buffer must be small (1 KiB, appended per 256-byte page) or the feature off on those boards. | None beyond ADR-213. |
| Flash wear and stalls | About 60 block writes per day at 230 KiB; with wear levelling far below the flash endurance. Each 4 KiB erase stalls the flash cache (ADR-213 rejected a LittleFS ring at 2.79 MB per day for that reason; at 230 KiB it is 12 times less). | None. |
| Boards | All. | Only boards with PSRAM. The OT-Direct board has no PSRAM. |

## How the data can get to a collection point

The service side is not the problem. **Cloudflare free plan** (official pages, 2026-11-12):
Workers 100,000 requests per day and 1,000 per minute, 10 ms CPU per request; R2 10 GB-month
storage, 1 million write operations and 10 million reads per month, no egress fees. One upload
per hour per gateway of the 230 KiB-per-day profile, kept 30 days, fits about 1,400 gateways
(limited by the write operations; storage allows about 1,500). The every-frame profile fits
about 390 gateways (storage). Not verified: whether R2 asks for a payment method on the free
plan, and whether the EU jurisdiction is available on it (open since TASK-1473).

The transport is the problem. Options:

| | A. Plain HTTP POST to a Worker | B. HTTPS from the device (ADR-218) | C. LAN relay (HA or a PC uploads) |
|---|---|---|---|
| Firmware rules | Allowed (no TLS). | Needs ADR-218 (Proposed, parked 2026-11-08) and an amendment of ADR-003. | Allowed; the device only serves a download on the LAN. |
| Boards | All. | Only boards with PSRAM. The OT-Direct board cannot. | All. |
| Works with Cloudflare | Only on a custom domain with "Always Use HTTPS" off. `http://workers.dev` answers 301 to HTTPS (checked 2026-11-12). Not verified that a Worker route then accepts plain HTTP. | Yes. | Yes (the relay speaks HTTPS). |
| Privacy in transit | **Cleartext over the internet**, including the device's upload token. | Encrypted. | Encrypted from the relay on. |
| Cost | Small firmware change; a domain on Cloudflare. | About 100 KB app flash in a nearly full slot (ADR-218), heap for the handshake. | No firmware TLS; the user must run a relay (an HA automation or a script). |
| Fails quietly offline | Must be built (TASK-1471 rules). | Same. | The relay's problem. |

Already rejected in TASK-1473 (2026-10-30, not repeated here): Google Cloud Storage (free tier
US only, billing account), MQTT cloud brokers (TLS mandatory, transport not storage), a
Hetzner VPS (maintenance), WireGuard on the ESP32 (stale), payload encryption over plain HTTP.
Considered there: Grafana Cloud Free EU (Loki), Hetzner Storage Box.

### Daily or hourly

A daily upload needs a full day on the device, which only the PSRAM ring (or the
changes-only profile on LittleFS, at the edge of the budget) can hold. If one upload fails,
the day is lost when the next day overwrites it. An hourly upload of the last hour keeps the
device buffer to a few hours (about 10 to 40 KiB per hour), loses at most an hour, and spreads
the load. Recommended: hourly, with the "once a day" wish met by the collection side
grouping the hours per day.

## Conflicts with earlier decisions

1. **ADR-213 decision 1** (Accepted, Robert 2026-11-05): "The firmware never sends a recording
   anywhere; it only answers a download request from the LAN." Options A and B change that and
   need a new ADR that amends it. Option C does not.
2. **TASK-1473** (parked by Robert on 2026-10-30, 11-03, 11-04 and 11-05, milestone 2.1.0)
   covers continuous cloud recording to Cloudflare R2 EU over outbound TLS on PSRAM boards.
   This proposal overlaps with it. Robert decides whether it reopens 1473, replaces it, or
   stays separate.
3. **"No HTTPS or WSS"** (core firmware rule, ADR-003): any upload to Cloudflare or a
   comparable service needs HTTPS outbound, except option A on a custom domain. The only
   sanctioned route is ADR-218 (outbound TLS on PSRAM boards, Proposed, parked 2026-11-08),
   which boards without PSRAM cannot use.
4. **Privacy.** OT logs show when people are home and how they heat (room temperature,
   setpoints, hot-water use). Uploading them makes the maintainer the controller of personal
   data under the GDPR: a consent text at onboarding that says what is uploaded, a random
   device id (not the MAC), a fixed retention (for example 30 days as an R2 lifecycle rule),
   a way to delete a gateway's data, and storage in the EU. Opt-in, off by default.

## Proposed design (for discussion)

1. **Phase 1, no cloud, all boards (no conflict):** an opt-in "OT log" switch in onboarding
   and settings. It reuses the ADR-213 record format with a filter: OT frames (T, B, R, A, E)
   and SAT ticks only. PSRAM boards keep every frame in the existing ring; boards without
   PSRAM write the changes-only profile in hourly files on LittleFS, at most 12 to 24 hours
   (oldest file deleted), with a small write buffer. Download from the web UI, and
   automatically before a filesystem update.
2. **Phase 2, upload:** after Robert picks a transport (questions below) and a new ADR
   amends ADR-213 decision 1: an hourly upload task in the style of the webhook task
   (ADR-136/137), a Worker plus an R2 bucket with a 30-day lifecycle, one random device id
   and token per gateway, and a consent text.

Open questions for Robert, one decision each (sent through the coordinator): where the log
lives and which frame profile; which transport; daily or hourly; and how this relates to
TASK-1473.

## Sources

- ADR-213 and `docs/diagnostics/recording-budget-1470.md` (record format, field-run sizes, RAM).
- ADR-003, ADR-194 (LittleFS budget), ADR-218 (outbound TLS, Proposed), TASK-1473, TASK-1508, TASK-1524, TASK-1528.
- `build.py:823` (`FS_RUNTIME_RESERVE_KIB = 96`).
- Cloudflare: [Workers limits](https://developers.cloudflare.com/workers/platform/limits/), [Workers pricing](https://developers.cloudflare.com/workers/platform/pricing/), [R2 pricing](https://developers.cloudflare.com/r2/pricing/), [Always Use HTTPS](https://developers.cloudflare.com/ssl/edge-certificates/additional-options/always-use-https/).

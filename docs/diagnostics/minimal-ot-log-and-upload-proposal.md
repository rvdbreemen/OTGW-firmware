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
- Cloudflare's free plan has room for this data, and a Worker on `workers.dev` accepts plain HTTP
  (verified 2026-11-12), so no SSL is needed on the device. Google and the cloud MQTT brokers do need TLS.
  Without TLS the data is readable in transit unless the payload is encrypted. Details and ranking below.

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

Robert's brief, 2026-11-12 (via the coordinator): "Voor het feature om data op te slaan in van OT-logs,
onderzoek wat hier een goede optie is, bv via REST of MQTT naar cloudflare of google, in elk geval naar een
dienst die schaalbaar is, en gratis. bij voorkeur geen SSL nodig heeft, maar als het nodig is, dan wil ik dat
weten. Het doel is om datacaptures op te slaan van gebruikers die in beta mode meedoen en opt-in hebben op het
delen van data te verbetering van de firmware."

### Is SSL needed?

**No, not for Cloudflare Workers; yes for Google and for the cloud MQTT brokers.**

- Cloudflare: three public Workers on `*.workers.dev` answered a plain `http://` request with
  `HTTP/1.1 200 OK`, no redirect and no `Strict-Transport-Security` header (curl, 2026-11-12:
  `demo.turnstile.workers.dev`, `demo-endpoints.pydantic.workers.dev`,
  `remix-cloudflare-demo.jacob-ebey.workers.dev`). Only the bare `http://workers.dev` redirects (301 to
  `workers.cloudflare.com`). Cloudflare's own Workers pages say nothing either way (workers.dev page, last
  updated 2026-11-06; Custom Domains page, 2026-09-29); "Always Use HTTPS" is an opt-in zone setting that
  redirects every host of a zone when turned on (page updated 2026-08-14). So a Worker can take plain HTTP
  uploads today. Measured with GET; a POST takes the same path, but that was not tested.
- Google: `http://script.google.com/...` answers 301 to HTTPS (curl, 2026-11-12). Cloud Run and Firebase
  were not probed with a real service; they need a billing account anyway (below).
- Cloud MQTT brokers: TLS (port 8883) was found mandatory in the TASK-1473 research (2026-10-30); not
  re-verified today, the vendor pages fetched today do not state the ports.

**What plain HTTP costs.** Without TLS the upload is readable on the way (OT data shows presence and
heating habits) and a sniffed upload credential can be replayed. Two cheap measures restore most of it
without a TLS session: an HMAC-SHA256 signature per upload with a per-device secret that is never sent
(integrity and authenticity; SHA-256 has hardware support on the ESP32-S3), and optionally encrypting the
payload to the collector's public key (X25519 + AES-GCM from the mbedTLS primitives that are already in the
Arduino core; no 2 x 16.7 KB TLS record buffers). Payload encryption over plain HTTP was rejected in
TASK-1473 on 2026-10-30 without a recorded reason; it is listed here again because it is the only way to
encrypt on boards without PSRAM. Its flash and RAM cost were not measured.

**What TLS costs on the device** (ADR-218 proof of concept on both bench units, 2026-11-07): two record
buffers of about 16.7 KB each, each one contiguous block; the largest free internal block was 31,732 B, so
every handshake without PSRAM failed (14/14), with the allocator redirected to PSRAM all succeeded; about
96 KB of code plus 5.4 KB of root certificates in an app slot that is about 93 % full; 0.7 to 0.8 s per
connect. Boards without PSRAM (the OT-Direct board among them) cannot do TLS.

### Volume against the free limits

Per gateway: about 230 KiB per day (changed values + SAT) or about 890 KiB per day (every frame + SAT),
measured above. Retention 30 days.

| Beta gateways | Stored, 230 KiB/day | Stored, 890 KiB/day | Uploads per day, hourly | Write ops per month, hourly |
|---|---|---|---|---|
| 50 | 0.33 GiB | 1.3 GiB | 1,200 | 36,000 |
| 200 | 1.3 GiB | 5.1 GiB | 4,800 | 144,000 |
| 1,000 | 6.6 GiB | 25.5 GiB | 24,000 | 720,000 |

### Candidates

| | Cloudflare Worker + R2 | Google (Apps Script + Drive, Cloud Run + Cloud Storage, Firebase) | Cloud MQTT broker (EMQX Serverless; HiveMQ) | Robert's NAS (self-hosted) |
|---|---|---|---|---|
| Free limits | Workers: 100,000 requests/day, 10 ms CPU per request, 100 MB request body on a free domain plan ([limits](https://developers.cloudflare.com/workers/platform/limits/)). R2: 10 GB-month, 1 million Class A and 10 million Class B ops per month, free egress; Standard storage only ([pricing](https://developers.cloudflare.com/r2/pricing/), updated 2026-10-30). | Cloud Run: 2 million requests/month; Cloud Storage free tier: 5 GB-months, 5,000 Class A ops/month, **US regions only**; **a billing account is required** for the Free Tier ([free features](https://docs.cloud.google.com/free/docs/free-cloud-features)). Firebase Spark: no Cloud Functions, no Cloud Storage; Firestore 1 GiB and 20,000 writes/day ([pricing](https://firebase.google.com/pricing)). Apps Script (consumer): 6 min per run, 90 min trigger time/day, 20,000 URL fetches/day ([quotas](https://developers.google.com/apps-script/guides/services/quotas)). | EMQX Serverless: 1 million session minutes, **1 GB traffic** and 1 million rule actions per month ([pricing](https://www.emqx.com/en/pricing)). HiveMQ Cloud: **the Serverless plan is retired on 2026-12-31** ([docs](https://docs.hivemq.com/hivemq-cloud/introduction.html)); the pricing page lists no free cloud plan, only Starter (paid) and a 30-day self-hosted Lab licence ([pricing](https://www.hivemq.com/pricing/)). | No vendor limits; the home uplink and the NAS are the limit. |
| Fits 1,000 gateways | Yes for 230 KiB/day (6.6 GiB, 720,000 writes); every frame needs 25.5 GiB, above the 10 GB (paid: storage is billed per GB-month). | Cloud Storage 5 GB and 5,000 writes/month: no beyond about 7 gateways hourly. Firestore 20,000 writes/day: about 800 gateways hourly, 1 GiB storage: about 150. | 1 GB/month: about 150 gateways at 230 KiB/day. | Yes, if the uplink holds. |
| Storage, not only transport | Yes (R2). | Yes. | **No**: a broker forwards; a subscriber that stores must run somewhere else. | Yes. |
| TLS needed | No (verified above). | Yes. | Yes (TASK-1473 research). | No. |
| EU data location | R2 jurisdiction `eu` exists ([data location](https://developers.cloudflare.com/r2/reference/data-location/)); the page names no plan restriction, not verified on a free account. | Free Cloud Storage only in US regions. | Region choice not checked. | Yes (in Robert's home). |
| Billing / payment method | Not stated on the R2 pricing page; not verified. | Billing account required (Cloud); Firebase Spark has no Functions/Storage. | Not checked. | None. |
| Upload auth | Worker code: per-device token or HMAC signature, rate limit and size cap per device in the Worker. | Apps Script web app: anyone-with-link; own checks in the script. | Broker username/password per device. | Own reverse proxy. |
| Lock-in | Low: R2 speaks the S3 API, the Worker is about 100 lines. | Medium. | Low (MQTT), but needs a storing subscriber. | None, but Robert carries the operations. |
| Other risk | Plain HTTP: content readable in transit unless encrypted (above). | US storage on the free tier conflicts with storing in the EU. | HiveMQ free option disappears. | Opens the home network to the internet; uptime depends on the house; Robert is the operator. |

Terms of service and data-processing terms for storing third-party personal data were not checked for any
candidate; that is part of the GDPR step below.

### Abuse protection and the per-device secret

The firmware is public, so a shared upload key in the image leaks on the first download. Every gateway
needs its own secret, and where that secret comes from decides whether the HMAC means anything.

- **Enrolment.** At opt-in the tester enters a beta invite code (handed out on request, for example on
  Discord, revocable per code). The gateway generates an X25519 key pair itself and sends its public key
  with the invite code to the Worker's `/enrol`. The Worker's public key is compiled into the firmware. Both
  sides derive the same device secret by Diffie-Hellman, so **the secret never crosses the wire**. This
  matters on plain HTTP: if the Worker sent a secret back, anyone on the path at enrolment would have it.
  The Worker answers with a random device id (not the MAC) and stores id, public key, invite code and date.
- **Storage on the gateway.** The private key and device id live in the settings as secrets: masked in
  GET /api/v2/settings and in a settings backup (ADR-056), replaced before they enter a recording
  (ADR-213 decision 6), and erased by a factory reset or when the tester switches the feature off.
- **Each upload** carries the device id, a timestamp, a sequence number and an HMAC-SHA256 over all three
  and the body. The Worker checks the signature, refuses an old timestamp or a sequence number it has
  seen, caps size and rate per device, and writes only under that device's prefix. A device can never
  read or list data.
- **Revocation.** Three levels, all in the Worker's device table (Workers KV or D1): delete one device
  (the tester withdraws consent: a signed "forget me" request from the gateway or a request to Robert;
  the Worker also deletes the device's prefix in R2), revoke an invite code (every device enrolled with
  it stops), or rotate the Worker key pair (every gateway must enrol again; a firmware update ships the new
  public key).
- **What a stolen gateway secret allows:** uploading junk under that one device id until it is revoked.
  It never gives access to other testers' data, because nothing can be read through the Worker.

### A forced-HTTPS day

Cloudflare documents no guarantee that Workers on `workers.dev` keep answering plain HTTP; it only works
today. The firmware must survive the day it stops: a 301 or 308 to `https://`, or any TLS-only answer,
stops the upload (no retry loop, one log line per retry interval, TASK-1471 rules), the local log keeps
running, and the web UI shows "upload unavailable". Fallbacks then, in order: move the Worker to a custom
domain with "Always Use HTTPS" off (needs a domain on Cloudflare; the same zone setting could later be
forced too), HTTPS through ADR-218 on PSRAM boards, or a LAN relay on any board. The upload URL is a
setting with a firmware default, so a fallback needs no new firmware.

### Ranking

1. **Cloudflare Worker on `workers.dev` + R2 (EU jurisdiction), plain HTTP, HMAC-signed uploads.** Free for
   up to about 1,000 beta gateways at the changed-values profile, works on every board without TLS, low
   lock-in. Weak spot: the content is readable in transit; payload encryption closes that (cost not measured).
2. **The same backend over HTTPS (ADR-218)** on PSRAM boards, if Robert wants transport encryption and accepts
   that boards without PSRAM upload in plain HTTP or not at all.
3. **The same backend through a LAN relay** (Home Assistant or a script): encrypted, no firmware TLS, but the
   tester must run the relay.
4. **Robert's NAS:** no vendor limits or TLS, but it exposes the home network and makes Robert the operator.
5. **Google:** a billing account, free storage only in the US, HTTPS forced.
6. **Cloud MQTT brokers:** TLS required, no storage, 1 GB per month on EMQX, HiveMQ's free plan retires on
   2026-12-31.

### Daily or hourly

A daily upload needs a full day on the device, which only the PSRAM ring (or the
changes-only profile on LittleFS, at the edge of the budget) can hold. If one upload fails,
the day is lost when the next day overwrites it. An hourly upload of the last hour keeps the
device buffer to a few hours (about 10 to 40 KiB per hour), loses at most an hour, and spreads
the load. Recommended: hourly, with the "once a day" wish met by the collection side
grouping the hours per day.

## Conflicts with earlier decisions

1. **ADR-213 decision 1** (Accepted, Robert 2026-11-05): "The firmware never sends a recording
   anywhere; it only answers a download request from the LAN." Any upload by the device itself
   (ranking 1, 2 and 4) changes that and needs a new ADR that amends it. A LAN relay (ranking 3) does not.
2. **TASK-1473** (parked by Robert on 2026-10-30, 11-03, 11-04 and 11-05, milestone 2.1.0)
   covers continuous cloud recording to Cloudflare R2 EU over outbound TLS on PSRAM boards.
   This proposal overlaps with it. Robert decides whether it reopens 1473, replaces it, or
   stays separate.
3. **"No HTTPS or WSS"** (core firmware rule, ADR-003): Google and the cloud MQTT brokers need HTTPS or TLS
   outbound, and the only sanctioned route for that is ADR-218 (outbound TLS on PSRAM boards, Proposed,
   parked 2026-11-08), which boards without PSRAM cannot use. A Cloudflare Worker over plain HTTP stays
   within the rule.
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
   (ADR-136/137) that posts over plain HTTP to a Worker on `workers.dev`, an R2 bucket in the
   EU jurisdiction with a 30-day lifecycle, per-device enrolment with a beta invite code, HMAC-signed
   uploads, and a consent text. Payload encryption if Robert wants the content unreadable in transit.

Open questions for Robert, one decision each (sent through the coordinator): where the log
lives and which frame profile; which transport; daily or hourly; and how this relates to
TASK-1473.

## Sources

- ADR-213 and `docs/diagnostics/recording-budget-1470.md` (record format, field-run sizes, RAM).
- ADR-003, ADR-194 (LittleFS budget), ADR-218 (outbound TLS, Proposed), TASK-1473, TASK-1508, TASK-1524, TASK-1528.
- `build.py:823` (`FS_RUNTIME_RESERVE_KIB = 96`).
- Plain-HTTP probes (curl, 2026-11-12): three public `*.workers.dev` Workers 200 over HTTP; `http://workers.dev` 301; `http://script.google.com` 301 to HTTPS.
- Google: [Free features](https://docs.cloud.google.com/free/docs/free-cloud-features), [Firebase pricing](https://firebase.google.com/pricing), [Apps Script quotas](https://developers.google.com/apps-script/guides/services/quotas). MQTT: [EMQX pricing](https://www.emqx.com/en/pricing), [HiveMQ Cloud docs](https://docs.hivemq.com/hivemq-cloud/introduction.html), [HiveMQ pricing](https://www.hivemq.com/pricing/). Cloudflare R2 [data location](https://developers.cloudflare.com/r2/reference/data-location/).
- Cloudflare: [Workers limits](https://developers.cloudflare.com/workers/platform/limits/), [Workers pricing](https://developers.cloudflare.com/workers/platform/pricing/), [R2 pricing](https://developers.cloudflare.com/r2/pricing/), [Always Use HTTPS](https://developers.cloudflare.com/ssl/edge-certificates/additional-options/always-use-https/).

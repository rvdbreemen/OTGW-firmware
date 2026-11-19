# Minimal OT log on the device and a daily upload: research and design proposal

Status: proposal for the maintainer (TASK-1862), 2026-11-12. Nothing is implemented. Docs only.
Design and open decisions rewritten 2026-11-16 for EU hosting, the opt-in dialog, free-only and the encoding study (see "Proposed design").
Robert rejected the batch-upload transports of question 2 and asked for streaming per log line over HTTPS: see "Streaming per log line".

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

## Streaming per log line (Robert's follow-up, 2026-11-12)

Robert's answers to question 2, by DM via the coordinator (verbatim): "Okay, geen van de opties klinkt goed.
Wat als we volledig zouden streamen naar een cloud dienst ?" (msg 1570328825353609217) and "Ik wil het dan
versleuteld, maar wat mij betreft zijn dit geen opties. Onderzoek een optie die streaming is, dus per logregel
wegschrijven naar een clouddienst. Welke clouddiensten zijn dan beschikbaar. En HTTPS is wat mij betreft dan
een optie." (msg 1570329291030270033). So: a continuous stream, encrypted in transit, HTTPS allowed.

### The rate that decides everything

The 20 h capture has 8,872 OT frames per hour: **2.46 frames per second per gateway**, 213,000 per day,
6.4 million per month, plus 95 SAT ticks per hour. As text that is about 5.5 MiB per day (about 165 MiB per
month); in the binary record format about 0.9 MiB per day (about 26 MiB per month).

Every free tier below counts messages or requests. **One cloud message per frame exhausts each of them with
one to ten gateways.** What works is the way log services are built: the gateway writes every line with its
own timestamp into a buffer and sends what has gathered every few seconds over one connection that stays
open. Each line keeps its exact time; the delay to the cloud is the batch interval. With 10 s batches a
gateway sends 8,640 messages per day instead of 213,000.

### Candidates (all encrypted with TLS)

| | Protocol | Free tier (primary source) | Gateways free, 10 s batches | Retention | EU region | Device auth | Verdict |
|---|---|---|---|---|---|---|---|
| **Grafana Cloud Logs (Loki)** | HTTPS `POST /loki/api/v1/push`, JSON (gzip allowed) or protobuf; every line its own timestamp ([API](https://grafana.com/docs/loki/latest/reference/loki-http-api/)) | 50 GB ingested per month, 14 days retention ([pricing](https://grafana.com/pricing/); [logs invoice](https://grafana.com/docs/grafana-cloud/cost-management-and-billing/understand-your-invoice/logs-invoice/)); how bytes are counted and any rate limit not stated | about 300 (text lines, by volume) | 14 days (free) | EU regions exist (Germany, Ireland, Sweden, Belgium, Netherlands) ([regions](https://grafana.com/docs/grafana-cloud/account-management/regional-availability/)); not stated whether Free can choose them | Access-policy token per tester (write-only scope) | Best fit for "per log line": least server code, query and graphs included. Data must be exported within 14 days for replays. |
| **Cloudflare Worker + Durable Object + R2** | WebSocket over TLS (`wss://`) to a Durable Object that buffers and writes R2; or HTTPS POST to a Worker | DO on Free: 100,000 requests/day, incoming WebSocket messages billed 20:1, 13,000 GB-s duration/day, SQLite only ([DO pricing](https://developers.cloudflare.com/durable-objects/platform/pricing/), updated 2026-10-18); R2 10 GB-month, 1M writes/month ([R2](https://developers.cloudflare.com/r2/pricing/)); Workers 100,000 requests/day ([limits](https://developers.cloudflare.com/workers/platform/limits/)); Queues 10,000 ops/day, 24 h retention ([Workers pricing](https://developers.cloudflare.com/workers/platform/pricing/)) | about 230 over WebSocket (8,640 msgs / 20 = 432 request-equivalents per gateway per day); about 11 with an HTTPS POST every 10 s, about 69 with one per minute | Own choice (R2 lifecycle); 10 GB holds about 380 gateways x 30 days of the binary format | R2 jurisdiction `eu` ([data location](https://developers.cloudflare.com/r2/reference/data-location/)) | Own: per-device enrolment (section above) | Full control, binary format (6x smaller than text), replay-ready raw data kept as long as wanted. More code: Worker + DO (estimate 200 lines) and a WebSocket client on the device. DO duration under hibernation not measured. |
| **InfluxDB Cloud (free)** | HTTPS line protocol | 5 MB writes per 5 minutes, 30 days retention, 2 buckets ([pricing](https://www.influxdata.com/influxdb-cloud-pricing/)) | about 135 (write rate, per organisation) | 30 days | Not stated on the pricing page | One token per bucket or per tester | Time-series, not logs; frames fit as points. Write cap is per organisation. |
| **AWS IoT Core** | MQTT over TLS with an X.509 certificate per device, or HTTPS | 500,000 messages and 2.25 million connection-minutes per month, **for 12 months only**; messages metered per 5 KB; then $1 per million messages in eu-west-1; Basic Ingest topics have no messaging cost ([pricing](https://aws.amazon.com/iot-core/pricing/)) | about 1 (10 s) or 11 (1 min batches) inside the free months; 1,000 gateways at 1 min batches cost about $43 per month in messages, plus storage | Rule into S3 (own choice) | Yes (eu-west-1, eu-central-1) | Best of all: a certificate per device | Strongest device identity, but not free after 12 months. |
| Azure IoT Hub (Free) | MQTT/AMQP/HTTPS | 8,000 messages per day per hub, 0.5 KB blocks, 500 devices ([pricing](https://azure.microsoft.com/en-us/pricing/details/iot-hub/)) | about 0 to 2 | Not a store | Yes | Per device | Too small. |
| Better Stack Logs (Free) | HTTPS | 3 GB per month, 3 days retention ([pricing](https://betterstack.com/pricing)) | about 18 | 3 days | Not stated for Free | Source token | Too small. |
| EMQX Serverless (+ rule to storage) | MQTT over TLS | 1 GB traffic per month, 1M session minutes, 1M rule actions ([pricing](https://www.emqx.com/en/pricing)) | about 6 (text) to 38 (binary) | Needs a storage sink | Not checked | Username/password per device | Too small; HiveMQ's free Serverless plan retires on 2026-12-31 ([docs](https://docs.hivemq.com/hivemq-cloud/introduction.html)). |
| Google Pub/Sub | HTTPS/gRPC | Pricing page could not be read today; the Google Cloud Free Tier needs a billing account ([free features](https://docs.cloud.google.com/free/docs/free-cloud-features)) | not established | Transport, not a store | Yes (paid regions) | Service account per device | Billing account; transport only. |

"Gateways free" is the number of beta gateways one free account carries at 10 s batches, from the limit that
binds first, rounded down; derived from the numbers above, not measured on the services.

### What the gateway must carry

- **TLS only on boards with PSRAM.** The ADR-218 proof of concept (2026-11-07, both bench units) needs two
  record buffers of about 16.7 KB each; without PSRAM 14 of 14 handshakes failed, with the allocator
  redirected to PSRAM all succeeded. A stream keeps that connection open all the time, so those 33 KB stay
  allocated in PSRAM for as long as the stream runs. **The OT-Direct board and other boards without PSRAM
  cannot stream**; they keep the local log (phase 1) only.
- **Flash.** The TLS client adds about 96 KB plus 5.4 KB of root certificates to an app slot that is about
  93 % full (ADR-218), and a WebSocket client (for the Cloudflare option) adds more. Whether both fit was not
  measured; it is the first thing to measure, before any choice is final.
- **Rules.** Outbound HTTPS or WSS needs ADR-218 to be accepted (it waits for "a concrete feature", and this
  is one) and amends ADR-003, which forbids WSS. The upload runs in its own task with its own stack, never on
  async_tcp (ADR-137, ADR-218).
- **Offline.** The stream reads from the same buffer as the local log: on a PSRAM board the ADR-213 ring
  (about 40 h). When the connection drops, the gateway keeps recording and resends from the last acknowledged
  record when it is back, with the original timestamps. Loki can refuse entries that are too old; the limit
  on Grafana Cloud Free was not checked.
- **Encryption.** TLS encrypts the transport; the service then sees the data in the clear. The GDPR points
  from the conflicts section stay: consent at opt-in, per-device id, retention, deletion, EU storage, and a
  data-processing agreement with the chosen service (not checked for any of them).

### Ranking for streaming

1. **Cloudflare Worker + Durable Object (WebSocket over TLS) into R2, EU jurisdiction.** About 230 beta
   gateways free at 10 s batches, raw binary data kept as long as Robert wants and ready for replays, own
   per-device enrolment. Cost: most code of the four, a WebSocket client on the device, DO duration not yet
   measured.
2. **Grafana Cloud Logs (Loki), EU region.** About 300 gateways free, the simplest device side (HTTPS POST of
   batched lines) and analysis tools included. Cost: 14 days retention on Free (export for replays), text
   format, one token per tester to manage, free-plan EU choice and byte counting not verified.
3. **InfluxDB Cloud free.** About 135 gateways, 30 days, time-series model.
4. **AWS IoT Core.** Best device identity, not free after 12 months.

Too small or not a store: Azure IoT Hub Free, Better Stack Free, EMQX Serverless, Google Pub/Sub.

## Conflicts with earlier decisions

1. **ADR-213 decision 1** (Accepted, Robert 2026-11-05): "The firmware never sends a recording
   anywhere; it only answers a download request from the LAN." Any upload by the device itself
   (ranking 1, 2 and 4) changes that and needs a new ADR that amends it. A LAN relay (ranking 3) does not.
2. **TASK-1473** (parked by Robert on 2026-10-30, 11-03, 11-04 and 11-05, milestone 2.1.0)
   covers continuous remote telemetry to Cloudflare R2 EU over outbound TLS on PSRAM boards.
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

Status of this section: rewritten 2026-11-16 (TASK-1862). It replaces the earlier plain-HTTP, R2-based design
and follows Robert's later requirements: **R1** EU hosting (#dev-sat-mqtt, msg 1570518857121271941), **R2** an
explicit opt-in dialog, off by default (msgs 1570519191797112963 and 1570520075755323554), streaming over TLS
allowed (DMs 1570328825353609217 and 1570329291030270033), minimal encoding (2026-11-14), and **free only,
never pay** (2026-11-14). Sources are the task notes of TASK-1862 and the commits named per claim.

### Rules the design must meet

1. **Free, no payment method on the account.** Then every limit is a hard error and nothing can be billed.
   The firmware drops data quietly on errors (TASK-1471). If a signup asks for a card, that route is out.
2. **Data stored in the EU (R1).** A service whose free plan cannot be pinned to an EU region does not
   qualify until that is verified.
3. **Opt-in dialog (R2).** Off by default. It sits in onboarding and in the settings, names what leaves the
   gateway (OT frames and SAT ticks with timestamps, a random device id), where it is stored, how long it is
   kept, and how to have it deleted. Nothing is sent before the user says yes.
4. **Encrypted in transit.** TLS, so only boards with PSRAM can stream (ADR-218 proof of concept, 2026-11-07:
   14 of 14 handshakes failed without PSRAM, all succeeded with the allocator redirected to PSRAM). The
   OT-Direct board and other boards without PSRAM keep the local log only.
5. **Rule changes needed first.** Outbound TLS needs ADR-218 accepted (it amends ADR-003, which forbids WSS),
   and an upload by the device itself needs a new ADR that amends ADR-213 decision 1. Ask the coordinator for
   the next ADR number first (the notes record that ADR-231 is reserved and the next is ADR-232).

### What goes over the line

- **Encoding.** The OT-aware model with an adaptive range coder (`scripts/ot_stream_codec_study.py`, study
  `docs/diagnostics/ot-stream-encoding-study.md`, commit 3262a496c) needs 0.95 B per frame, 209 KiB per day,
  with 3,600 s chunks, and 1.10 B per frame with 600 s chunks. Layout B is 3.79 B, layout B with zlib 1.94 B.
  It is lossless (round trip verified). The result is from a generated 24 h stream calibrated against the
  real capture (layout B 831 against 848 KiB measured). **Not yet run on the real capture, and not ported to
  C.** Model state is about 8.7 KB for 34 IDs; 10 to 15 KB for a real ID set is an estimate.
- **Time resolution.** About 5 of the 7 bits per frame are timing. 4 ms gives 0.68 B per frame and 10 ms
  0.56 B (study). On PIC boards device time is only good to about 2 ms, so 4 ms is the honest unit there.
  The choice is made on the measured jitter of the real capture; it is an engineering step, not a decision
  for Robert.
- **Batch interval.** Headers decide the wire cost: 77 % overhead at 10 s, 37 % at 60 s, 10 % at 300 s
  (study, for WebSocket over TLS 1.3). An HTTPS POST per batch adds 150 to 300 B of HTTP headers and is worse
  at every interval (study). Store in chunks of 10 to 60 minutes, send in batches of 60 to 300 s.
- **Stream content.** OT frames and the SATTICK control tick (95 per hour, 30 fields, about 38 KiB per day
  delta-coded, from the task notes; expected smaller in the coder, not measured). Debug text is left out of
  this phase: it is about 3 KB/s with every module on and would end any free tier (study). Tokenized logging
  (Pigweed `pw_tokenizer`, Apache-2.0; Trice licence unverified) is a later step.

### Phases

1. **Phase 1, no cloud, all boards, no rule conflict.** An opt-in OT log switch in onboarding and settings.
   PSRAM boards keep every frame in the ADR-213 ring (about 40 h of every frame plus SAT, task notes). The
   study changes the earlier LittleFS conclusion: at about 200 KiB per day the roughly 230 KiB of unreserved
   LittleFS holds about a day of every frame with timing, so boards without PSRAM no longer need the
   changes-only profile. This assumes the encoder fits those boards; it was not measured. A filesystem update
   wipes the log (TASK-1524), so the web UI offers a download first.
2. **Phase 2, stream.** After the decisions below and the two ADRs. A separate task with its own stack, never
   on async_tcp (ADR-137, ADR-218). On a lost connection the gateway keeps recording and resends from the
   last acknowledged record with the original timestamps. Per-device enrolment with a beta invite code, a
   random device id (not the MAC), retention enforced by the backend, a delete path per device.

### Free backends, ranked

All figures are from the task notes (primary pricing pages read 2026-11-12 and 2026-11-14) unless marked
as derived or estimate. Gateway counts are for beta gateways per free account.

| Rank | Backend | Free and no card | EU (R1) | Capacity | Main con |
|---|---|---|---|---|---|
| 1 | **PocketBase on Robert's Synology behind a Cloudflare Tunnel** | Yes: the NAS and the domain exist; Tunnel is on the Cloudflare free plan | By construction, if the NAS is in the EU (his location is not stated in the sources) | Bounded by the NAS disk and line, not by a plan limit; not computed | Pre-1.0; runs on one home NAS |
| 2 | **Cloudflare Workers Free + Durable Objects (SQLite), no R2** | Yes, no card: "further operations of that type will fail with an error" (DO pricing page) | DO EU jurisdiction on Free **unverified** | About 67 gateways at 60 s batches, about 297 at 300 s (rows written, 100,000 per day; commit 354278bef) | EU unverified; WebSocket client may not fit the app slot |
| 3 | **Grafana Cloud Free (Loki)** | Yes, no card, 50 GB per month | EU regions exist; choice on Free **unverified** | About 300 gateways (text lines, 10 s batches, by volume) | 14 days retention; text format wastes the encoding |
| 4 | **Firebase Realtime Database, Spark** | Yes: "No payment method needed" | europe-west1 exists; Spark eligibility not stated | About 117 gateways at 300 s batches with 30-day retention (storage bound, 1 GB); about 100 at 60 s (commit a46708a26) | No Cloud Functions on Spark: cleanup from a host script; behaviour at the limit unverified |
| 5 | **Firestore, Spark** | Yes | Not stated for Spark | 20,000 writes per day; at one document per 300 s batch (288 per gateway per day) about 69 gateways (derived) | Smallest of the five; Blaze has no hard cap (commit 786a02f20) |

**1. PocketBase on the Synology** (commits 1869fb0bf and 3c30e3926, TASK-1938, `tools/remote-telemetry/pocketbase/`).
- Pros: no plan limits, retention under Robert's control (nightly cron, default 30 days), per-device accounts
  with create-only rules and a unique device, stream and sequence number so a resend after a lost reply is
  safe, admin UI stays on the LAN, only the login and batch paths pass the tunnel, Hyper Backup covers the
  SQLite file. MIT licence, one Go binary. The collector is already written.
- Cons: PocketBase is pre-1.0 with no backward-compatibility guarantee, and its GitHub repo was archived on
  2026-10-26 (development moved to ganigeorgiev.com). It has not run yet: TASK-1938 AC 2 and 3 need a
  Synology run. Availability follows the NAS and the home line, so a gap means lost data unless the gateway
  buffer bridges it (about 40 h in the PSRAM ring). The Cloudflare Zero Trust signup must not ask for a
  payment method (one source says it might; unverified): if it does, this route stops.
- Consequence for the user: nothing to pay and no third-party analytics company holds the data. Robert hosts
  testers' heating data himself and is the GDPR controller on his own hardware.
- The device side posts over HTTPS (TASK-1938 README), one POST per batch, so the cost per batch is higher
  than on a WebSocket (study).

**2. Cloudflare Free, no R2** (study, commits 354278bef and aa6d9db36). Pros: no card and a hard stop, no
server at home, scales to about 297 gateways at 300 s. Cons: EU jurisdiction on Free is unverified, which
could fail R1; R2 reportedly asks for a payment method, so storage stays in the DO's 5 GB SQLite; handler
times are estimates; a WebSocket client on the device adds flash to an app slot that is about 93 % full
(ADR-218), not measured. Consequence for the user: if the account hits a limit, uploads fail and nothing is
billed.

**3. Grafana Cloud Free** (commit a6ac7e775). Pros: simplest device side (HTTPS POST of lines), analysis
tools included, about 300 gateways. Cons: only 14 days of retention, text lines instead of the 0.95 B
encoding, one token per tester, EU choice and byte counting unverified. Consequence: a replay needs an
export within 14 days.

**4 and 5. Firebase RTDB and Firestore on Spark** (commits a46708a26, 786a02f20). Workable for a small beta,
but the EU region on Spark and the behaviour at the limit are unverified, and RTDB has no cleanup function.
If a card is ever added, Blaze has no hard cap: budget alerts do not cap spending (notes).

**Dropped** (reason in the notes): anything on Cloudflare R2 (payment method), InfluxDB Cloud (EU not stated),
AWS IoT Core (free for 12 months only), Azure IoT Hub Free (8,000 messages per day), Better Stack Free (3
days, too small), EMQX Serverless (too small; HiveMQ Serverless retires 2026-12-31), Google Pub/Sub (billing
account), Supabase Free (paused after a week of inactivity, EU not stated), self-hosted Supabase (4 GB RAM
minimum, too heavy for most Synology models). Appwrite was not checked.

**Recommendation:** rank 1 for the beta, with rank 2 as the scalable fallback once its EU jurisdiction on the
Free plan is verified. This is an estimate, not a measurement: the beta is small enough for one NAS.

## Open questions for Robert

One decision each, one DM each (sent by the coordinator). Each lists its options and what follows.

1. **Which free backend carries the stream?**
   - A: PocketBase on your Synology behind a Cloudflare Tunnel. Nothing to pay, EU by construction, no plan
     limit. You host the data and are the controller; gaps when the NAS or line is down.
   - B: Cloudflare Workers Free with Durable Objects, no R2. About 67 to 297 gateways, no server at home.
     EU jurisdiction on Free is unverified: if it is not available, this fails your EU rule.
   - C: Grafana Cloud Free. About 300 gateways, least code. 14 days retention, text only, EU choice on Free
     unverified.
   - Recommendation: A. Main risk: the NAS is a single point of failure, and PocketBase is pre-1.0 and its
     GitHub repo is archived.
2. **How does this relate to TASK-1473 (remote telemetry, parked)?**
   - A: One feature. TASK-1473 reopens, and the minimal OT stream is its first phase, under one ADR.
   - B: Keep them separate. TASK-1862 stays the opt-in minimal OT stream, sharing the collector
     (TASK-1938) and the codec. TASK-1473 stays parked.
   - Recommendation: B. Main risk: two features drift apart. You parked 1473 four times, and B keeps its
     larger scope out of the beta.
3. **Do we accept ADR-218 (outbound TLS on PSRAM boards) and amend ADR-213 decision 1 (the firmware never
   sends a recording)?**
   - A: Accept both now, so work can start. If the TLS client does not fit the app slot, the decision was
     premature.
   - B: Measure first whether TLS plus the HTTP or WebSocket client fits the app slot (about 93 % full),
     then accept. Costs a short delay, avoids a rule change for something that cannot ship.
   - C: Neither. No device upload at all; only the local log, and your own gateways on the LAN posting over
     plain HTTP to the collector.
   - Recommendation: B. Main risk: the measurement says it does not fit, and then rank 1 is only reachable
     through C.
4. **How often does a gateway send a batch?**
   - A: Every 60 s. 37 % header overhead, data at most 1 minute late, about 67 gateways on Cloudflare Free.
   - B: Every 300 s. 10 % overhead, data at most 5 minutes late, about 297 gateways on Cloudflare Free.
   - Recommendation: B for HTTPS POST to the collector. Main risk: a crash or power cut loses up to 5
     minutes of data that was still only in RAM.
5. **How long is the data kept, and what do you promise testers about deletion?**
   - A: 30 days, deletion on request by device id (the figure the earlier proposal used as an example, not a
     measured need).
   - B: 14 days, deletion on request. Smaller privacy surface, less history for replays.
   - C: 90 days. Better for slow problems, larger surface and more disk.
   - Recommendation: A. Main risk: a field problem that needs more than 30 days of history; then the tester
     sends a local download (phase 1).

## Decision status (2026-11-16)

Under Robert's rule of 2026-11-16 09:30 (decide technical points yourself with an advisor check; put to him only what is his or a hard stop), the coordinator decided three of the five questions above and put two to Robert:

- **Question 2 (relation to TASK-1473): B, decided by the coordinator.** Separate features: TASK-1862 stays the opt-in minimal OT stream and shares the collector (TASK-1938) and the codec; TASK-1473 stays parked.
- **Question 3 (ADR-218 and ADR-213 decision 1): B, decided by the coordinator.** Measure first. The remote telemetry proof of concept (poc.8, bench on .61) measures the TLS fit; poc.8 is at 98.4 % of the app slot. Accepting ADR-218 is a hard stop and goes to Robert after that measurement.
- **Question 4 (batch interval): B, decided by the coordinator.** Every 300 s: 10 % header overhead; up to 5 minutes of data lost on a crash is acceptable for diagnostic data.
- **Question 1 (backend): A, decided by Robert** (DM 1571804403793530952, asked in DM 1571804094593765398): PocketBase on his Synology behind a Cloudflare Tunnel. He hosts the data and is its controller. The collector is TASK-1938.
- **Question 5 (retention and deletion promise): A, decided by Robert** (DM 1571804460928336025, asked in DM 1571804100897669281): records are kept 30 days and deleted on request by device id. TASK-1938's retention cron uses 30 days.

## Sources

- ADR-213 and `docs/diagnostics/recording-budget-1470.md` (record format, field-run sizes, RAM).
- ADR-003, ADR-194 (LittleFS budget), ADR-218 (outbound TLS, Proposed), TASK-1473, TASK-1508, TASK-1524, TASK-1528.
- `build.py:823` (`FS_RUNTIME_RESERVE_KIB = 96`).
- Plain-HTTP probes (curl, 2026-11-12): three public `*.workers.dev` Workers 200 over HTTP; `http://workers.dev` 301; `http://script.google.com` 301 to HTTPS.
- Google: [Free features](https://docs.cloud.google.com/free/docs/free-cloud-features), [Firebase pricing](https://firebase.google.com/pricing), [Apps Script quotas](https://developers.google.com/apps-script/guides/services/quotas). MQTT: [EMQX pricing](https://www.emqx.com/en/pricing), [HiveMQ Cloud docs](https://docs.hivemq.com/hivemq-cloud/introduction.html), [HiveMQ pricing](https://www.hivemq.com/pricing/). Cloudflare R2 [data location](https://developers.cloudflare.com/r2/reference/data-location/).
- Cloudflare: [Workers limits](https://developers.cloudflare.com/workers/platform/limits/), [Workers pricing](https://developers.cloudflare.com/workers/platform/pricing/), [R2 pricing](https://developers.cloudflare.com/r2/pricing/), [Always Use HTTPS](https://developers.cloudflare.com/ssl/edge-certificates/additional-options/always-use-https/).
- Encoding: `docs/diagnostics/ot-stream-encoding-study.md`, `scripts/ot_stream_codec_study.py` (commits 3262a496c, 354278bef).
- Free-only backend research in the TASK-1862 notes (commits aa6d9db36, a46708a26, 786a02f20, 1869fb0bf); PocketBase collector `tools/remote-telemetry/pocketbase/` (TASK-1938, commit 3c30e3926).

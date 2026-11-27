---
id: "ADR-302"
title: "Remote telemetry in the normal build: what is collected and never collected, consent with a version, 30-day retention, deletion on request, the collection point and the PSRAM gate"
status: "Proposed"
date: "2026-11-27"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "remote telemetry"
  - "privacy"
  - "consent"
  - "data retention"
aliases:
  - "telemetry data contract"
  - "cloud telemetry"
components:
  - "RemoteTelemetryDevice"
  - "RemoteTelemetryPolicy"
  - "telemetry field list"
symbols:
  - "REMOTE_TELEMETRY_RING_PSRAM"
  - "REMOTE_TELEMETRY_INTERVAL_MS"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-302 Remote telemetry in the normal build: what is collected and never collected, consent with a version, 30-day retention, deletion on request, the collection point and the PSRAM gate

## Status

Proposed, 2026-11-27.

## Status History

```yaml
status_history:
  - date: 2026-11-27
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

Robert ordered remote telemetry into the next public beta: opt-in, off by default, on boards with PSRAM only, and
labelled experimental (2026-11-27, relayed by the coordinator; not seen by otgw-architect). Until now the recorder
was built only in a research environment (`esp32-combo-tlsdyn`, `HAS_CLOUD_REC`).

These decisions already hold on dev:

- ADR-218 (Accepted): outbound TLS (Transport Layer Security) client requests only on boards with PSRAM (pseudo-static
  RAM), with certificate verification always on.
- ADR-263 (Accepted): the BLE (Bluetooth Low Energy) and remote telemetry buffers may be allocated once from PSRAM at init.
- ADR-268 (Accepted): the recorder's local ring becomes the one local diagnostic recorder, and diagRec is removed.
- ADR-299 (Accepted): when the device asks, what a "No" means, and the NVS (non-volatile storage) fields `optin`,
  `offsrc` and `offrel`.

The contract on **what is sent, how long it is kept and how it is removed** was written in ADR-290 for the research
build. ADR-290 is Superseded (by ADR-218, on the TLS point), so the data contract has no Accepted home on dev. This
ADR gives it one, for the normal build.

Facts from the research lane (lane-poc-merge). otgw-architect read them; he did not build or run them.

- One record a minute (`REMOTE_TELEMETRY_INTERVAL_MS` 60000, `RemoteTelemetryDevice.h:220`), at most one upload per
  2 s (`REMOTE_TELEMETRY_UPLOAD_GAP_MS`, `:1719`).
- A ring of 1.5 MB in PSRAM (`REMOTE_TELEMETRY_RING_PSRAM`, `:1204`).
- The collection point is a self-hosted PocketBase on Robert's NAS (network-attached storage) behind a Cloudflare
  Tunnel (TASK-1938). The host name is in `RemoteTelemetryDevice.h:1584`. Each gateway authenticates on its own
  (`/api/collections/devices/auth-with-password`, `:985`).
- Robert's privacy rule (TASK-3010, 2026-11-21, chat, verbatim start): "Ik wil echt strak aan geen persoonlijke data
  in pocketbase, dus IP, Mac, SSID, BSSID, Name, Email. Niks van dat alles. Alleen locatie zou ik graag wel krijgen."
- ADR-290's estimate: about 2 to 3 MB of payload per gateway per day without debug text.

## Decision

Remote telemetry ships in the normal esp32-combo build as an **experimental, opt-in feature on boards with PSRAM
only**. It sends only the fields on one reviewed field list, to one self-hosted collection point, under a consent text
with a version number. Every record is deleted after 30 days, and a gateway's data is deleted on request.

1. **Consent.**
   - Telemetry is off until the user turns it on. ADR-299 decides when the device asks.
   - The dialog shows one consent text with a version number (`consent_ver`, an integer, starting at 1). The device
     stores the version the user agreed to in NVS.
   - A new text version asks again, once, also when the user said yes before. Until the user answers, no record is
     sent.
   - A "No" stays final under ADR-299, also for a new text version.
2. **What is collected.** Only the fields and events in the field list file (`docs/telemetry/field-list.md`, written by
   otgw-poc-dev and reviewed by otgw-architect). The categories:
   - OpenTherm frames;
   - SAT ticks;
   - firmware events: PIC and OT-Direct commands and replies, MQTT in and out with values, REST route counts, sensors,
     WiFi signal and state, reboots, OTA and file events, health once a minute;
   - settings as numbers and booleans only;
   - lost-record counts.

   A field that is not on the list is not sent. The list is versioned together with the consent text: a new category
   raises `consent_ver`.
3. **What is never collected.**
   - IP (Internet Protocol) addresses, MAC (media access control) addresses in any form, the WiFi network name
     (SSID) and access point address (BSSID), host or gateway names, user names, e-mail addresses.
   - Passwords, tokens, service keys, MQTT credentials.
   - Free-text setting values. They are sent only as set or empty.
   - A precise location. The weather location is rounded to 0.1 degree, so it is off by at most about 11 km.

   The gateway's only identity in the collection point is a short pseudonymous telemetry ID. It is derived from the MAC
   by a fixed one-way function, stored in NVS and shown in the web UI.
4. **Retention.** The collection point deletes every record older than 30 days. The period is fixed, not configurable
   per gateway, and verified on the server, not only configured.
5. **Deletion on request.**
   - A user asks for the deletion of all data under a telemetry ID by giving that ID to the maintainer, in a GitHub
     issue or in Discord support. The web UI hint says how.
   - The deletion covers every record and the device record, within 7 days.
   - Turning telemetry off stops sending at once. It does not delete data already sent, and the web UI says so.
6. **Collection point.** One self-hosted PocketBase in the EU: Robert's NAS behind a Cloudflare Tunnel.
   - The upload uses TLS to the Cloudflare edge and verifies the certificates against the pinned roots (ADR-218).
   - Cloudflare can read the payload at its edge. That exposure is accepted (ADR-290 point 9, Robert 2026-11-15)
     and named in the user text.
   - A blocked gateway pauses with back-off and stays opted in (ADR-299).
7. **PSRAM gate and size.**
   - A board without PSRAM gets no TLS handshake, no ring and no upload, and ignores an "on" write (ADR-218,
     ADR-299).
   - The ring is allocated once from PSRAM at init (ADR-263).
   - A capture point never waits more than 10 ms. A busy recorder costs a counted loss, never a stall.
   - The app image fits the slot it ships in, under the size gate in force for that release.
8. **Port 25238 bridge.** Bridge traffic is not recorded in the first experimental beta, in either direction. ADR-290
   point 6 (inbound bridge commands recorded as text) is not carried over for now: the command text can carry
   whatever a tool on the user's network sends. The field list and the consent text say so. The outbound PIC stream
   stays untouched (ADR-177). Interim choice by the coordinator while Robert is away, 2026-11-27, privacy-conservative
   and reversible; Robert confirms it (Open Questions).
9. **Consent text languages.** The approved consent text is the English one (Robert, DM 1575376575451504731,
   recorded in the TASK-3092 notes on lane-poc-merge). ADR-299 mentions "its NL, DE and EL translations", but no
   approved translation exists: TASK-3092 AC #4 is still open and no translated text was found (otgw-architect,
   git search on the lane branches; otgw-docs and otgw-webui found the same). Until Robert approves translations, the
   NL, DE and EL packs show the English consent text.

## Decision Contract

### Must

- Send nothing until the user has turned telemetry on and agreed to the current `consent_ver`.
- Send only fields on the reviewed field list, and every free-text value as set or empty.
- Use the pseudonymous telemetry ID as the only central identity.
- Round the location to 0.1 degree.
- Delete every record older than 30 days on the collection point, and a telemetry ID's data within 7 days of a
  request.
- Gate everything on PSRAM at runtime (ADR-218), and allocate the ring from PSRAM once (ADR-263).
- Keep every capture point under 10 ms, and count a loss instead of waiting.
- Label the feature experimental in the dialog, in the Settings row and in the release notes.

### Must Not

- Send any item of the never-collected list (decision 3).
- Turn telemetry on by default, by an update or by a settings.ini restore (ADR-299).
- Attempt a TLS handshake or allocate the ring on a board without PSRAM.
- Disable certificate verification.
- Let the collection point change the opt-in.

### Exceptions

- None.

### Verification

- A host test on a recorded sample from the real encoder finds zero hits for an IPv4 address, a MAC in any form, the
  bench SSID, the host name and an `@` e-mail pattern. It fails on a mutant that leaks one of them (TASK-3010 AC5).
- A host test of the policy shows:
  - no send before consent;
  - a new `consent_ver` asks again;
  - the PSRAM gate refuses an "on" write.

  It is red on code without the version check.
- A server check shows that a record older than 30 days is gone after the retention hook runs, and that a deletion
  request removes every record of a test ID.
- otgw-bench-tester runs a bench on .63 (PSRAM) and .76 (no PSRAM):
  - on .63: off after the update; on through the dialog; records arrive; a switch-off stops at once; a 60 min soak
    shows a flat heap;
  - on .76: no dialog; the row is disabled; an "on" write is ignored; no TLS attempt shows in telnet or the counters;
  - a decoded download is checked against the never-collected list.
- otgw-architect reviews the field list before the beta, and the review findings are resolved in the list.

## Alternatives Considered

- **Keep the data contract in ADR-290 only.** Rejected: ADR-290 is Superseded and was written for a research build. An
  Accepted feature needs an Accepted contract.
- **No consent version, one yes for ever.** Rejected: a new category of data would then go out under an old yes.
- **A retention period per gateway.** Rejected: one fixed 30 days is simpler to verify and to explain. It keeps
  ADR-290's constraint.
- **Encryption on the device, so that Cloudflare cannot read the payload.** Rejected for now: Robert accepted the edge
  exposure (2026-11-15). It is named in the user text, and a later ADR can add encryption.
- **Every board, with dynamic TLS buffers on boards without PSRAM.** Rejected: ADR-218 keeps TLS on boards with PSRAM,
  and the board without PSRAM already reached critical heap under light load (B2 bench on .76, 2026-11-27).

## Consequences

**Positive:**

- One Accepted record says what users agree to. The dialog, the docs and the tests can cite it.
- The field list and `consent_ver` make every change to what is sent a visible, versioned step.

**Negative:**

- Each new data category costs a new consent round, so users who said yes are asked again. *Mitigation:* one version
  per release at most.
- Deletion on request is manual work for the maintainer at first. *Mitigation:* the 7-day bound; a self-service button
  can follow.
- About 2 to 3 MB per gateway per day on the NAS (ADR-290 estimate, not measured here). The 30 days bound the storage.
- Code size on a nearly full app slot. *Risk:* the telemetry beta comes before the ADR-298 table change, so it lands
  on the old 2240K app0. *Mitigation:* the build size is measured before the beta slot is promised. If it does not
  fit, Robert chooses between a trim, shipping it with the new table, or a later beta.

## Open Questions

- [ ] Robert: accept this data contract as written. It covers consent with `consent_ver`, the never-collected list,
  30-day retention, deletion on request within 7 days, the Cloudflare edge exposure named in the user text, and the
  experimental label.
- [ ] Robert: deletion on request. Option A: through the maintainer by telemetry ID, with no new code. Option B: also a
  "delete my data" button in the web UI that sends the request, which means more code before the beta.
  Recommendation: A for this beta. The coordinator chose A in the interim (2026-11-27, reversible).
- [ ] Robert: the bridge recording (decision 8). It is left out of the first beta (coordinator, interim, 2026-11-27).
  Confirm that, or bring back ADR-290 point 6 in a later release.
- [ ] Robert: approve the NL, DE and EL translations of the consent text (decision 9). Until then the packs keep the
  English text.

## Related Decisions

- **ADR-218**: outbound TLS on boards with PSRAM. This ADR uses it for the upload.
- **ADR-263**: PSRAM buffers at init. The ring follows it.
- **ADR-268**: one recorder with a local ring. The same records feed this upload.
- **ADR-299**: when the device asks and what a "No" means. This ADR adds the consent version.
- **ADR-290**: the research-build design this contract comes from (Superseded).
- **ADR-217** and **ADR-177**: the bridge filter and the untouched PIC stream (decision 8).

## References

- `RemoteTelemetryDevice.h:220`, `:985`, `:1204`, `:1584` and `:1719` on lane-poc-merge (read 2026-11-27).
- TASK-3010 (strict privacy, Robert 2026-11-21), TASK-2937 (product feature) and TASK-1938 (collection server), on
  lane-poc-merge.
- `docs/adr/ADR-290-opt-in-cloud-telemetry-from-the-gateway.md`, decisions 2, 6 and 9.
- PocketBase collection rules and hooks: https://pocketbase.io/docs/
- Cloudflare Tunnel, where TLS ends at the edge: https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/

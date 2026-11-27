---
id: "ADR-304"
title: "One beta carries the ADR-298 guards, the new partition table and remote telemetry together (partly supersedes ADR-298 release order)"
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
  - "partition table migration"
  - "release order"
  - "beta release"
aliases:
  - "combined telemetry and partition beta"
  - "no bridge beta"
components:
  - "partition table release order"
symbols:
  - "partition_layout"
  - "otgw-layout"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-304 One beta carries the ADR-298 guards, the new partition table and remote telemetry together (partly supersedes ADR-298 release order)

## Status

Proposed, 2026-11-27. Waiting for Robert's explicit choice between this ADR (one combined beta) and the bridge release first
(ADR-298 as written). If he chooses the bridge first, this ADR is Rejected and ADR-298 stays unchanged.

## Status History

```yaml
status_history:
  - date: 2026-11-27
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: "Initial proposal, drafted on the coordinator's request after Robert's DMs 1575990956073951535 and 1575991202275393567"
    changed_via: adr-kit
```

## Context

Terms: OTA (over the air, an update over WiFi), USB (Universal Serial Bus), NVS (non-volatile storage, the key-value
partition that holds WiFi credentials, the admin password and the remote telemetry state), LittleFS (the file system
partition with the web files and settings.ini), REST (the HTTP API of the gateway), MAC (the network hardware address).

ADR-298 (Accepted) grows the 4MB app slot to 2304K and shrinks LittleFS to 704K. Every existing 4MB unit moves once over
USB. Its release-order Must reads: "first a bridge release on the old table that carries the guards (layout report,
image marker check, file-system size and `size=` checks) and marks its own image `otgw-layout=safeboot`; only a later
release changes the table." That follows Robert's answer to its Open Question 3 (DM 1575397374891786251, "B met terugval
naar A", 2026-11-26).

On 2026-11-27 Robert wrote two DMs, both read by otgw-architect through the Discord API:

- 1575990956073951535, 21:45:24Z, about the partition change: "Dat MOET mee als je de remote telemetry in dev brengt."
- 1575991202275393567, 21:46:23Z: "Ik wil dat je meer tegelijk combineert en releaset"

The first DM ties the table to remote telemetry. It does not say whether the bridge step stays; the coordinator reads both
DMs together as "one combined beta". ADR-302's Risk line already named this choice: "Robert chooses between a trim,
shipping it with the new table, or a later beta."

Facts, read by otgw-architect on origin/dev 44a50c43a unless marked:

- There is no OTA migration. `flash_otgw.bat --migrate --host <unit>` saves settings.ini over HTTP and then writes the
  two migrate images over USB: `write_flash -z 0x0 <migrate-0x0> 0xE000 <migrate-rest>` (`flash_otgw.bat:44-47`,
  `:246`; the same on keep/adr298-task2980 `:249`). ADR-298 Must Not forbids rewriting the table from the firmware or
  from safeboot.
- No published release has the guards yet. The `/update` page of a published beta accepts an app image of at most
  `APP_MAX = 0x230000` (2,293,760 B) and refuses a file-system image whose size is not `FS_SIZE = 0xC0000` as `fssize`
  (`update.html:159`, `:192`, `:205`). The combined app is about 99.2 % of 2,293,760 B, about 2,275,000 B (*estimate*
  from the measured percentage, otgw-poc-dev), so the app is accepted and the 704K file-system image is refused.
- Such a unit runs the new firmware on its old table with the old web files. ADR-298 describes that state (*read*,
  otgw-dev, f384db4cb): it mounts the old 768K file system with settings intact, warns at boot that a USB migration is
  needed, reports `partition_layout` different from `partition_layout_expected`, and refuses further images for the
  other table.
- The migration keeps settings.ini and dallas_labels.ini (saved and restored) and does not touch NVS. So the remote
  telemetry opt-in and salt stay, and the telemetry ID, derived from the MAC and the NVS salt (ADR-302), stays the same.
  Other LittleFS files are lost (`flash_otgw.bat:236-261`).

## Decision

The first public beta with remote telemetry also carries the ADR-298 guards and the new partition table, in one
release, without a bridge release before it.

This replaces only the release-order Must of ADR-298 and the outcome of its Open Question 3. Every other part of ADR-298
stays: the table, the guards, the image marker, USB-only migration, the notice and the release-notes Musts.

A unit that updates from a published beta through the browser gets what ADR-298 calls the fallback for a unit that skips
the bridge: new firmware on the old table, old web files, a boot warning, settings intact. With this ADR that fallback is
the normal path for every tester until they migrate.

## Decision Contract

### Must

- Put the migration steps at the top of the notes of that beta, above every other change, and in the #beta-testing
  announcement (ADR-298's notice Must, Robert's wording requirement): a breaking change; one `flash_otgw --migrate` over
  USB per 4MB unit, with a laptop and a cable; an OTA update alone is not enough; what is kept (settings.ini,
  dallas_labels.ini, WiFi, the admin password, the remote telemetry state); what is lost (other LittleFS files);
  rollback only with the old release's full merged image over USB.
- Say in the same notes what a tester sees after a browser update without migration: the firmware installs, the web
  files do not, the unit warns, settings stay, and the remote telemetry rows under Settings > System cannot be switched
  on ("save failed") until the migration.
- Pass the migration bench before the beta is published (bench-tester, unit .63): the fallback path (a published beta,
  `/update` with the combined app), then `--migrate`, then the checks of ADR-298 Verification plus BLE, MQTT and the
  telemetry ID and state.
- Keep the guards in this release, so the next release can rely on them.

### Must Not

- Publish this beta before the migration bench has passed.
- Describe an OTA migration route in any text for testers.

### Exceptions

- None.

### Verification

- Bench on .63 by otgw-bench-tester: fallback, migration, the ADR-298 checks, settings 0 differences, telemetry ID equal
  before and after.
- otgw-webui's code read (see Consequences) is checked on .63 in the fallback state: three telemetry rows under
  Settings > System, "save failed" when one is switched on, the layout warning on `/update`.
- The beta notes are checked against the Must list before publishing (otgw-docs).

## Alternatives Considered

- **ADR-298 as written: the bridge release first.** The bridge beta on the old table carries only the guards, with no
  USB step. The next beta carries telemetry and the table. A tester who took the bridge gets a clear refusal over WiFi
  and a pointer to `--migrate`, instead of a half-updated unit. Cost: one more build and bench item, and telemetry
  reaches testers about 12 hours later. The stable timeline does not change (freeze Monday 08:00 in both). This is
  otgw-architect's recommendation in `_comms/stable_replan_v3_combined_telemetry_table_2026-11-27.md`; the coordinator
  advised the combined beta, following Robert's DM 1575991202275393567.
- **Telemetry first on the old table, the table later.** Ruled out by Robert's DM 1575990956073951535.
- **An OTA migration.** Ruled out by ADR-298 Must Not (no table rewrite from the firmware or safeboot; power-loss
  hazard).

## Consequences

**Positive:**

- One beta and one bench cycle less; telemetry and the table reach testers together, as Robert asked.
- The size problem of ADR-302's Risk line is gone in the same release (about 96 % of the new slot, *estimate*).

**Negative:**

- Every tester who updates through the browser runs new firmware with old web files until they migrate. *Mitigation:*
  the boot warning and the `/update` warning of ADR-298, and the migration steps at the top of the notes.
- The consent dialog of ADR-299 is missing in that state, so no tester can opt in to remote telemetry before migrating.
  Remote telemetry stays off, which is the safe direction.
- What the old web files do against the new firmware (*read* by otgw-webui from the code, not run; old UI = origin/dev
  4c2e2d082 `data/v2.js`, new firmware = lane/telemetry-int de73a3ea1 and keep/adr298-task2980):
  - The `/update` page is served from the firmware, not from LittleFS (`OTGW-ModUpdateServer-esp32.h:486`, `:512`), so
    after the browser update the new page with the layout warning is shown.
  - The old UI does not know the setting `remotetelemetry`; its settings renderer puts it, `remotetelemetryasked` and
    `remotetelemetrypsram` as three generic rows under System (old `v2.js:4625-4636`; new `restAPI.ino:4339-4341`).
  - Switching the generic row on posts no consent version (old `v2.js:5290-5293`); the firmware answers 400 and the old
    UI shows "save failed" (old `v2.js:5311`). Consent cannot be bypassed (ADR-302 decision 1 holds).
  - The new fields in `/api/v2/health` and `/api/v2/device/info` are additive; the old UI does not read them.
  - Removed or renamed endpoints between a published beta and the candidate are *not checked*.
  *Mitigation:* the notes say what testers see, and the bench checks it on .63.
- The consent dialog and chip (lane/ba-rt2 b96e2dd52) are in the candidate: `git merge-base --is-ancestor` confirms
  b96e2dd52 in b579acc and de73a3ea1. A migrated unit gets the new web files and the dialog; an unmigrated unit keeps
  the old web files and has no dialog.
- More changes in one release make a fault harder to trace. *Mitigation:* each part keeps its own host test, and the
  bench covers the fallback and the migration.

## Open Questions

- [ ] Robert: one combined beta (this ADR), or the bridge release first (ADR-298 as written)? His DM
  1575991202275393567 asks for more combining; his DM 1575990956073951535 ties the table to telemetry but does not
  mention the bridge.

## Related Decisions

- **ADR-298** (partition table, Accepted): this ADR partly supersedes its release-order Must and the outcome of its Open
  Question 3. Everything else in ADR-298 stays.
- **ADR-191** (safeboot updates on 4MB boards): unchanged.
- **ADR-302** (remote telemetry data contract): its Risk line about code size is answered by shipping with the new table.
- **ADR-299** (remote telemetry opt-in): its dialog is missing on unmigrated units; telemetry stays off there.

## References

- Robert's DMs 1575990956073951535, 1575991202275393567 (2026-11-27), 1575397374891786251 (2026-11-26).
- `flash_otgw.bat:44-47`, `:236-261`, `:246`; `src/OTGW-firmware/webui/update.html:159`, `:192`, `:205` (origin/dev
  44a50c43a).
- keep/adr298-task2980 (TASK-2980, b86614dec..f384db4cb; 76ad1afaf is the table commit).
- `OTGW-field-data/_comms/stable_replan_v3_combined_telemetry_table_2026-11-27.md`.

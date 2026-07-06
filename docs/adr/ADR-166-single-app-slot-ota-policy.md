---
id: ADR-166
title: "Single-app-slot boards: firmware/app OTA is USB-only, filesystem OTA stays on WiFi"
status: Proposed
date: 2026-07-06
tags: [esp32s3, ota, partitions, safety, task959]
supersedes: []
superseded_by: []
related: [ADR-126]
deciders: [Robert van den Breemen]
---

# ADR-166: Single-app-slot boards: firmware/app OTA is USB-only, filesystem OTA stays on WiFi

## Status

Proposed. Date: 2026-07-06.

This records an already-shipped, empirically-forced safety decision (the
backend has rejected single-slot app OTA since alpha.311/TASK-959 AC#1; the
frontend hide added alongside this ADR is new). Left `Proposed` rather than
self-accepted per the adr-kit rule ("never self-approve") — the underlying
behavior is live and field-verified, but the maintainer has not reviewed this
specific record yet.

## Status History

status_history:
  - date: 2026-07-06
    status: Proposed
    changed_by: Agent (TASK-959)
    reason: Initial decision record documenting the single-app-slot app-OTA rejection (backend, alpha.311) and the new frontend hide added in this pass. Left Proposed pending maintainer review.
    changed_via: adr-kit
    changed_via: adr-kit

## Context

The 4MB ESP32-S3 partition table used by `esp32-classic` and `esp32-combo`
(`partitions_otgw_esp32.csv`) has a single app slot: `app0`/`ota_0` at
`0x260000`, sized for one ~1.9MB application image, plus a LittleFS region for
the web UI assets. There is no second `ota_1` slot — the 4MB budget does not
have room for two ~1.9MB app copies plus a usable filesystem.

`Update.begin(U_FLASH)` (the Arduino OTA API, `Update.h`) targets whatever
`esp_ota_get_next_update_partition()` returns. On a normal dual-slot ESP32
layout that is the INACTIVE slot; on this single-slot table it is the
partition currently marked bootable — which, with only one app slot, is
always the RUNNING partition. `Update.write()` then streams the incoming
image directly into the executing app's own `.text`.

This was empirically proven to brick a bench device (OTGW32 `.39`,
alpha.297/298, TASK-959): the upload progressed a few KB before the running
code faulted mid-write, the HTTP connection dropped with no "End: success",
`fwversion` never changed, and the partially-overwritten slot could not
reliably boot on the next reset — recoverable only via USB (`esptool`/
`flash_otgw.bat`). The Flash Utility (`/update`) offered firmware upload on
every board unconditionally, so this was reachable by any user, not just a
developer.

Filesystem OTA (`Update.begin(U_SPIFFS)`, LittleFS target) does not have this
problem: it targets a fixed, non-executing filesystem partition regardless of
app-slot count, and was already confirmed working over WiFi on the same
single-slot boards.

## Decision

**Firmware/app OTA is rejected outright on any board whose partition table has
no spare app slot (single `ota_X` == the running partition). Filesystem
(LittleFS) OTA remains available over WiFi on all boards, unaffected.**

The check is dynamic, not board-name-based:

```cpp
// OTGW-ModUpdateServer-esp32.h
inline bool hasSpareAppOtaSlot() {
  const esp_partition_t *running  = esp_ota_get_running_partition();
  const esp_partition_t *nextSlot = esp_ota_get_next_update_partition(nullptr);
  return nextSlot != nullptr && nextSlot != running;
}
```

Two independent layers apply it:

1. **Backend, before any write (the actual safety boundary).**
   `_handleUploadStart` calls `hasSpareAppOtaSlot()` and, if false, sets
   `_updaterError` and returns BEFORE `Update.begin()` is ever called. Nothing
   reaches flash. The HTTP response carries a clear message: `"app update is
   USB-only on this board (single app slot); use flash_otgw.bat over USB"`.
   This landed first (alpha.311) and is the layer that actually prevents the
   brick — everything else is a courtesy on top of it.
2. **Frontend, before the user ever tries (the courtesy).** `/api/v2/device/info`
   exposes `app_ota_available` (== `hasSpareAppOtaSlot()`). The `/update` page
   (`updateServerHtml.h`) fetches this on load and, when false, replaces the
   firmware-upload form with an explanatory note; the filesystem-upload form is
   untouched. A user on a single-slot board never sees a firmware-upload
   control that would only fail.

Both checks read live partition state, not a board-type flag: an 8MB
dual-slot table (`ota_0` + `ota_1`) would make `nextSlot != running` true and
silently re-enable app OTA on both layers with no code change.

**App updates therefore go over USB only** (`flash_otgw.bat` / `flash_otgw.sh`,
app-only `--update` mode preserves WiFi credentials and settings; factory mode
does a full erase). **Filesystem updates continue to work over OTA** via the
same `/update` page, unaffected by this decision.

## Alternatives Considered

### Alternative A: Shrink the app image or the filesystem to fit a second app slot on 4MB

Rejected. The web UI assets alone need a LittleFS partition large enough for
the v2 asset set (`v2.html`/`v2.css`/`v2.js`/`ds-tokens.css` plus the classic
assets); a second ~1.9MB app slot leaves too little room for that filesystem
to be useful. A minimal LittleFS-only 4MB dual-slot scheme exists upstream
but ships too small a filesystem for this project's web UI.

### Alternative B: Allow the write but warn the user first

Rejected outright as unsafe. Once `Update.write()` starts streaming into the
running partition, the device WILL fault mid-write on any interruption
(browser tab closed, WiFi hiccup, upload taking too long) — a warning dialog
does not change the outcome, it only delays when the user finds out. The
brick already happened in testing with no warning; adding one does not fix
the underlying corruption.

### Alternative C: Do nothing (keep offering app OTA on single-slot boards)

Rejected as the status quo that caused the original bug report — a normal
user action (uploading the wrong file, or the right file on the wrong board)
bricks the device with no fallback slot to recover from. Pre-ADR-166 default
in the codebase before TASK-959; not acceptable to keep now that the failure
mode is understood and easily prevented at zero cost to legitimate use
(filesystem OTA + USB app flashing already cover every real workflow).

## Consequences

**Benefits**

- App/firmware updates can never again write into the running partition and
  brick a single-slot board — the write is refused before it starts.
- Filesystem (web UI) updates keep working over WiFi with no behavior change,
  so most day-to-day update workflows (asset/UI updates between firmware
  releases) are unaffected.
- The `/update` page no longer offers a control that would only produce a
  confusing failure — users see the real reason and the correct alternative
  (`flash_otgw.bat`) immediately.
- Self-healing: an 8MB dual-slot board re-enables app OTA automatically with
  no code or config change, since the check reads live partition state.

**Trade-offs**

- App/firmware updates on 4MB boards require physical USB access; there is no
  remote (WiFi-only) path to update firmware on a single-slot board. This is
  a real operational constraint for field-deployed 4MB units, not just a
  bench limitation.
- True dual-slot app OTA with automatic rollback needs an 8MB S3-WROOM-1-N8
  module (pin-compatible BOM swap) on a future PCB revision — not a firmware
  fix, a hardware one.

**Risks and mitigations**

- *Risk*: a future refactor of `OTGWUpdateServer` accidentally calls
  `Update.begin(U_FLASH)` from a new code path that does not go through
  `_handleUploadStart`'s guard.
  *Mitigation*: `hasSpareAppOtaSlot()` is a small, named, reusable helper
  specifically so any new app-OTA entry point has an obvious one-line guard
  to call — the fix is not duplicated ad hoc per call site.
- *Risk*: the frontend hide (`/update` page) silently breaks if
  `/api/v2/device/info` ever drops the `app_ota_available` field.
  *Mitigation*: the frontend check fails open on a fetch error (shows the
  form as before) rather than failing closed — the backend guard is the real
  safety boundary regardless of what the UI shows, so a UI regression cannot
  reintroduce the brick, only reintroduce showing a form that will then be
  correctly rejected server-side.

## Related Decisions

- **ADR-126 (Fixed build targets, no runtime hardware detection)**: this ADR's
  partition-table constraint is a consequence of ADR-126's 4MB single-image
  targets (`esp32-classic`, `esp32-combo`); the `esp32` (OTGW32) target has
  its own partition table and is unaffected by this specific single-app-slot
  constraint in the same way (verify per-target before assuming coverage).

## References

- Epic/implementation: TASK-959 ("Guard firmware OTA on single-app-slot
  OTGW32 — it corrupts the running partition; app OTA is USB-only on 4MB")
- Code: `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h` (`hasSpareAppOtaSlot()`,
  `_handleUploadStart`), `src/OTGW-firmware/updateServerHtml.h` (frontend hide),
  `src/OTGW-firmware/restAPI.ino` (`app_ota_available` field)
- Partition table: `partitions_otgw_esp32.csv`
- Flash tooling: `flash_otgw.bat` / `flash_otgw.sh` (USB app-only `--update`
  mode preserves WiFi credentials)

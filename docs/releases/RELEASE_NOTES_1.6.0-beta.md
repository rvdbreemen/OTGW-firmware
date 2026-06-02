# 1.6.0-beta release notes

These are the release notes for the `1.6.0-beta.N` line on `dev`. The line builds on `1.5.0-fix2` (the current latest public stable) and tracks the next stable release, `1.6.0`. Field testers can flash any `v1.6.0-beta.*` prerelease from the [Releases page](https://github.com/rvdbreemen/OTGW-firmware/releases) and should report findings in Discord `#beta-testing`.

The bullets below summarise the user-visible changes that have landed on `dev` since the last public stable, `v1.5.0-fix2`.

**MQTT and Home Assistant discovery**
- **HA availability now reflects the MQTT link, not the OpenTherm bus** (ADR-074, regression fix). Entities like `DHW Control` and `Thermostat` no longer flap `unavailable` when the boiler stops talking. OT-bus liveness lives on the dedicated `otgw_connected` sensor. **Contract change:** consumers reading the base `<toptopic>/value/<nodeid>` topic as OT-bus liveness must migrate to `otgw_connected`.
- **Pure JIT MQTT discovery** (ADR-073, supersedes ADR-041): only non-OT pseudo-IDs queue at boot; OT MsgID discovery configs publish on first MsgID reception. Stalled-discovery edge case fixed by aligning the JIT trigger with the force-path `hasConfig` filter.
- **Proxy-answer (no-B) routing fix** (ADR-075): MsgIDs without a boiler response now route to the correct worldview topic instead of going silent.
- **MsgID 0 Status canonical publish** gated on boiler-side worldview so the canonical topic no longer flaps on thermostat-only frames.
- **HA PIC-control entities**: new `button` and `select` discovery configs under pseudo-ID 251 expose the PIC reset and mode controls as proper HA entities.
- **Standalone HA discovery topic wiper**: one-shot helper for cleaning stale retained discovery topics out of the broker.
- **HA capability-flag binary sensors (bits 2-5) no longer stuck at `unknown`** (ADR-076, PR #614): the global MQTT status fanout rate gate suppressed per-bit publishes on subsequent MsgID 5 frames. The rate gate is dropped and the per-bit publish is scoped to all three pending types so cooling, OTC active, CH2 active, and summer/winter reach their retained topics on every status change.

**Performance**
- **Mainloop responsiveness audit** (TASK-651, TASK-652, PR #617): all blocking `delay()` / `delayMs()` calls on the cooperative path replaced with non-blocking timer checks so `doBackgroundTasks()` keeps running at full cadence under load. This fixes occasional missed MQTT publish ticks and UI responsiveness drops under heavy OpenTherm traffic.
- **Mainloop Tier-1 follow-up** (TASK-671, PR #626): `handleOTGW()` PIC drain loops now bounded at four lines per call, so a noisy PIC cannot starve the rest of the loop; the dead `executeCommand` path is removed; a stray `delay(1)` on the cooperative path is replaced with `yield()`.
- **Mainloop Tier-1 follow-up #2** (TASK-673, PR #633): `String` usage removed from hot paths in `helperStuff.ino` / `webhook.ino`; `emergencyHeapRecovery()` is now real recovery (drops the OTGWstream client and pauses the discovery drip for one tick when heap is critical, see ADR-079); always-on `BGTRACE` instrumentation dropped from production builds.
- **Mainloop Tier-2 dispositions** (TASK-674, PR #635): webhook HTTP timeout tightened from 1000 ms to 500 ms (the existing webhook retry state machine absorbs the slack); the per-sensor OneWire read in `pollSensors()` is bus-physics-bound and not firmware-tunable; the 15 s `MQTTclient.setSocketTimeout()` is documented and accepted in ADR-080 as a known synchronous blocker bounded by the 42 s retry gate.

**Settings and networking**
- **Static IP address support with improved UI** (TASK-548, TASK-709): configure a fixed IP, subnet, gateway, and up to two DNS servers in the firmware settings. Persisted across reboots and applied before WiFiManager connects, so the device lands on a predictable address every time. The settings page now shows a "Use DHCP" toggle (on by default); the IP fields only appear when DHCP is disabled and are auto-filled from the current DHCP lease so switching to a fixed IP is a one-step operation. Each IP address uses four segmented inputs (0-255 per octet) with auto-advance and paste support. Octet inputs now use `inputMode="numeric"` for a better mobile keyboard, carry ARIA labels for screen readers, validate each field before saving, and keep the save button visible when invalid input is detected. Dark-theme support added for the fixed-IP section.

**Web UI and diagnostics**
- **Statistics table drag-to-resize columns** (TASK-703): grab any column header edge in the Statistics tab to resize it. Width preferences are saved in localStorage and survive page reloads.
- **LittleFS size display fixed** (TASK-701): the device-info API and Web UI were reporting 1 MB filesystem instead of the correct 2 MB.
- **Device-info low-heap precheck corrected** (TASK-723): `/api/v2/device/info` no longer returns a premature `503` solely because its largest contiguous heap block drops below the former 8 KB guard; the endpoint still preserves its lower allocation safety guard.
- **OT log scroll position preserved** (TASK-701): switching tabs or navigating back to the main page no longer resets the scroll position in the OT log.
- **Statistics column proportions and badge styling refined** (TASK-705, TASK-706): column widths are better balanced; the "boiler unsupported" badge is visually consistent.
- **MQTT discovery verify hourly trigger** (TASK-704): entities missed by the JIT pass are now recovered automatically every hour without user intervention.
- **Bilateral OT-bus support map** (TASK-683, 684, 686, 688, PR #640): the gateway now tracks which OpenTherm MsgIDs have actually been seen from the thermostat side, the boiler side, or both. The telnet diagnostic view labels each data point with a direction-aware "T / B / T+B" indicator. A new `GET /api/v2/otgw/support-map` REST endpoint exposes the bitmaps; the Web UI shows the map so you can see at a glance which data points your system is actually exchanging.
- **Heap drop counters in the per-minute log are now correct** (TASK-697, PR #642): `logHeapStats` was printing the window-scoped drop counters (which reset to 0 after each throttle warning) instead of the lifetime totals. The per-minute heap line now shows monotonically increasing lifetime values, consistent with MQTT stats topics, `/api/v2/heap`, and the debug dump.
- **Telnet diagnostic noise reduced** (TASK-683, PR #640): `onNotFound` now emits accurate `200 (file)` / `404` lines; the firmware-file-list endpoint no longer mirrors its JSON output to the telnet log; `checklittlefshash` is silent on a matching hash (only logs on mismatch).
- **FSexplorer "Update Firmware" button** is visible again on touch-capable desktops; the touch-class media query no longer hides the upload control.
- **Set-command debug surfacing**: silently-dropped set-commands now appear in the default debug stream instead of being swallowed.

**Tooling and build**
- **Flash scripts hardened**: `flash_otgw.sh` / `flash_otgw.bat` now mirror spec parity, verify SHA256 integrity, and pick the binary that matches the requested version. The `.bat` variant detects COM ports through the Windows registry and auto-downloads binaries when not found locally.
- **`build.py` auto-initialises missing git submodules** so a fresh clone or stale checkout builds without manual `git submodule update`.
- **`evaluate.py`** false-positive and stale checks fixed; the gate is now meaningful again.
- **`/beta-prerelease` skill + GitHub Action** for tag-driven (and, after the May 2026 workflow refresh, workflow-dispatch-driven) beta publishing, with draft-first asset attachment to satisfy GitHub's immutable-releases policy. The release body now inlines a "What's new since the last public release" digest sourced from this file's content above the `<!-- digest:end -->` sentinel, and the skill gates the README + CHANGELOG staleness check before the version bump so a stale narrative cannot ride out under a fresh tag.

**Code hygiene**
- **Dead and orphaned code paths cleaned out of `dev`**: inactive subsystem code and its matching scaffolding in `OTGW-firmware.h` removed, since neither is reachable on the 1.5.x / 1.6.x line.

For per-commit detail see [`CHANGELOG.md`](../../CHANGELOG.md) under `## [Unreleased]`. For architectural rationale see the linked ADRs under [`docs/adr/`](../adr/).

<!-- digest:end -->

## Full per-line detail (since v1.5.0-fix2)

Below is the long-form version of the digest above, with one section per area and explicit cross-references. The CI workflow inlines only the digest (everything above the `<!-- digest:end -->` marker) into the release body; this section is here for testers who follow the "Read the full release notes" link.

### MQTT contract changes (worth a second read)

- **HA availability decoupling (ADR-074, TASK-607).** Before this release, `<toptopic>/value/<nodeid>` doubled as the HA availability topic AND the OT-bus liveness indicator. The two are different things: when the boiler stops talking, MQTT is still healthy, but every entity went `unavailable`. As of `1.6.0-beta.N`, the HA availability topic reflects only the ESP↔MQTT link (birth/LWT). OT-bus liveness is on the dedicated `otgw_connected` sensor.
  - **Action for tester:** if any of your automations or dashboards key off the base value topic to detect that the boiler is talking, switch them to the `otgw_connected` sensor instead. The base value topic is no longer reliable for that purpose.
- **JIT (just-in-time) discovery (ADR-073, supersedes ADR-041).** Discovery configs for OpenTherm MsgIDs are no longer published at boot. They publish the first time the gateway sees a value for that MsgID, which trims a large startup burst and means HA only sees entities that are actually relevant to the connected boiler+thermostat. Boot-time discovery still applies to the small set of non-OT pseudo-IDs (climate, number, Dallas, heap stats, firmware/PIC). The `F` force-republish action remains available for one-shot recovery.
- **MsgID 0 Status gating (TASK-633).** The canonical `Status` topic now gates on the boiler-side worldview so it stops flapping on thermostat-only frames. This is a quality-of-life fix; no automation contract change.
- **Capability-flag bits 2-5 unknown in HA (ADR-076, TASK-649, PR #614).** The MQTT status fanout had a global rate gate that suppressed per-bit publishes on subsequent MsgID 5 frames. Bits 2-5 (cooling, OTC active, CH2 active, summer/winter) therefore appeared `unknown` in Home Assistant unless the boiler happened to send the first MsgID 5 after a discovery republish. The fix drops the rate gate and scopes the per-bit publish across all three pending types so every status change reaches every retained capability-flag topic.
  - **Action for tester:** if you previously saw `unknown` on `cooling_active`, `otc_active`, `ch2_active`, or `summer_winter_mode`, those entities should now populate within one MsgID 5 cycle after boot. No HA config change is required.

### Mainloop responsiveness

- **`delay()` / `delayMs()` audit (TASK-651, TASK-652, PR #617).** A handful of blocking `delay()` / `delayMs()` calls on the cooperative path were stalling `doBackgroundTasks()` for tens of milliseconds at a time, which manifested as occasional missed MQTT publish ticks and a sluggish Web UI under heavy OpenTherm traffic. Each site is now a non-blocking timer check (`DECLARE_TIMER_MS` / `DUE()`), so the loop yields back to the scheduler at full cadence.
  - **Action for tester:** if you previously noticed the Web UI lagging during a thermostat write or the live OT log stuttering, please confirm it is smooth on this beta. Heap and uptime should also report on schedule via MQTT.
- **Tier-1 follow-up (TASK-671, PR #626).** The PIC drain loops inside `handleOTGW()` were unbounded: a noisy PIC could starve the rest of `doBackgroundTasks()` for hundreds of milliseconds. Each drain loop is now capped at four lines per call, so PIC traffic spikes no longer steal the entire mainloop slot. The dead `executeCommand` path was removed and a stray `delay(1)` on the cooperative path is now `yield()`.
- **Tier-1 follow-up #2 (TASK-673, PR #633).** Three findings from the second mainloop review:
  - `String` usage removed from hot paths in `helperStuff.ino` / `webhook.ino` (heap-fragmentation per ADR-004).
  - `emergencyHeapRecovery()` reworked from a yield-and-log no-op into a real recovery routine (ADR-079): when `getHeapHealth() == HEAP_CRITICAL`, the OTGWstream client is dropped (operator can reconnect via OTmonitor) and the discovery drip is paused for one tick to stop adding pressure. MQTT and telnet are intentionally NOT dropped (MQTT reconnect is itself an expensive synchronous operation; telnet is the operator's live-diagnostics channel).
  - Always-on `BGTRACE` instrumentation removed from production builds.
- **Tier-2 dispositions (TASK-674, PR #635).** Three remaining synchronous blockers in the mainloop were inventoried:
  - **Webhook HTTP** (Item 5): timeout reduced from 1000 ms to 500 ms. The existing per-webhook retry state machine absorbs the slack.
  - **MQTT connect** (Item 6): `MQTTclient.setSocketTimeout(15)` accepted as a known sync-blocker (ADR-080). Worst-case 15 s stall per connect attempt, gated to one attempt every 42 s by `timerMQTTwaitforconnect`. The PIC serial path is hardware-buffered and the `handleOTGW()` drain is bounded (TASK-671), so the stall does not cause UART overrun. Replacing PubSubClient with an async client is out of scope for the 1.6.0 line.
  - **OneWire / Dallas read** (Item 7): bus-physics-bound (parasitic-power conversion time) and not firmware-tunable; left as-is.

### Architecture decisions worth noting

- **ADR-076 (Accepted).** Drops the global MQTT status fanout rate gate so all 13 capability-flag bits reach their retained topics on every status change.
- **ADR-077 (Superseded by ADR-078).** HA-core-style capability-flag aliases (37 opt-in topics) were drafted in detail and implemented behind a feature flag during the beta.8-beta.12 cycle. Field testing surfaced enough scope creep and complexity for the 1.6.0 stabilisation window that the change was reverted from `dev` and deferred to the 2.0.0 line. The decision is captured in ADR-078; no user-visible behaviour change in the 1.6.0 line.
- **ADR-079 (Accepted).** Defines what `emergencyHeapRecovery()` actually does when `getHeapHealth() == HEAP_CRITICAL`: drop the OTGWstream client (HA reconnects, operator reconnects via OTmonitor) and pause the discovery drip for one tick. MQTT and telnet are intentionally NOT dropped. Replaces the prior "yield + log" no-op behaviour.
- **ADR-080 (Accepted).** Accepts the existing 15 s `MQTTclient.setSocketTimeout()` as a known synchronous main-loop blocker, bounded by 15 s worst-case stall per connect attempt and the 42 s `timerMQTTwaitforconnect` retry gate. Replacing PubSubClient with an async MQTT client is recorded as out of scope for the 1.6.0 stabilisation window.

### Web UI fixes worth field-validating

- **FSexplorer Update Firmware button on touch desktops (GitHub #575, #598).** The touch-class CSS media query no longer hides the upload control. If you are running a touchscreen all-in-one PC and previously could not see the Update Firmware button, please confirm it is back.

### Tooling that may affect your normal workflow

- **`flash_otgw.sh` / `flash_otgw.bat` (PR #570).** Both scripts now share a spec-defined argument surface, verify the binary against `SHA256SUMS` before flashing, and pick the right `.ino.bin` / `.littlefs.bin` pair for the requested version. The Windows variant detects the OTGW's COM port through the registry and downloads the binaries from the GitHub release page when not present locally.
- **`build.py` (PR #594).** The build script now runs `git submodule update --init --recursive` for any submodule that is missing or empty. A fresh clone no longer requires you to remember the manual step.
- **`evaluate.py` (PR #592).** A handful of false-positives and one stale-check bug are fixed. CI's evaluator gate is meaningful again.
- **`/beta-prerelease` skill and `.github/workflows/beta-prerelease.yml`.** Beta cuts are now tag-driven (or, after the May 2026 refresh, workflow-dispatch-driven from the Actions UI). The Action publishes the release with all assets attached in one atomic draft-first call so the GitHub "immutable releases" policy does not trap stuck drafts.

### Documentation

- New integration guides for **openHAB** and **Domoticz**.
- New Dutch beginner guide for cleaning up stale MQTT topics in MQTT Explorer (`docs/guides/MQTT_STALE_TOPICS_CLEANUP*.md`).
- PIC and ESP firmware guides split into EN/NL language variants, with PIC guide scope restored and ESP-flash docs routed to `FLASH_GUIDE.md`.
- API and ADR docs refreshed mid-cycle: `docs/api/MQTT.md` documents the boot vs. JIT discovery split per ADR-073; `docs/api/README.md` corrects the `/discovery/verify` REST endpoint description; `docs/adr/README.md` gains ADR-041 (Superseded) and ADR-073 (Accepted) index entries.
- Release-notes housekeeping: `RELEASE_NOTES_1.5.0.md` and `RELEASE_GITHUB_1.5.0.md` moved out of the repo root into `docs/releases/`; older `1.3.3` and `1.3.4` notes archived under `docs/releases/archive/`.
- Documentation link paths normalised; markdown link-validation guardrail introduced (PR #573) and CI scope extended to `docs/guides/` and `docs/process/` (PR #581) to keep documentation cross-links honest.

### Removed

- Dead and orphaned subsystem code paths cleaned out of `dev` (PR #586, PR #589). Neither was reachable on the 1.5.x / 1.6.x line.
- Accidentally committed root files removed; `.gitignore` tightened so they cannot return (TASK-635, PR #606).

## How to test

1. **Flash** a `v1.6.0-beta.*` release using the Web UI (`Update Firmware`), the OTA route, or the `flash_otgw.sh` / `flash_otgw.bat` helper from a clone of the repo.
2. **Watch HA discovery.** Entities should NOT flap `unavailable` if the boiler is quiet but MQTT is up. The `otgw_connected` sensor should track OT-bus liveness instead.
3. **Force a discovery re-publish** if you previously saw a stalled-discovery state, by sending `F` over MQTT or via the Web UI (`Republish`). Confirm all expected entities appear.
4. **Try the touch-desktop scenario** if you run a touchscreen workstation: open FSexplorer, confirm the Update Firmware button is visible.
5. **Report findings** in Discord `#beta-testing`. Both positive ("looks good on my boiler model X") and negative reports help.

## Compatibility and breaking changes

- **HA availability source change (see ADR-074 above).** If your dashboards or automations depend on the base value topic being absent to detect that the boiler is silent, they need to migrate to `otgw_connected`. No firmware-side change can preserve the old behaviour while fixing the regression that motivated this change; field testers should review their HA configs once and adapt.
- No partition-layout changes. The `4M2M` layout introduced in v1.5.0 is preserved. Upgrading from v1.5.0-fix2 to a `1.6.0-beta.N` does not require a filesystem reformat.

## Where to read more

- [`CHANGELOG.md`](../../CHANGELOG.md): per-commit running log under `## [Unreleased]`.
- [`README.md`](README.md): landing page, "What's new on dev" section.
- [`docs/adr/`](../adr/): architectural rationale, in particular ADR-073 (JIT discovery), ADR-074 (HA availability), ADR-075 (proxy-answer routing).
- [`docs/guides/`](../guides/): flash, build, MQTT, and integration guides.

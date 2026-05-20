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

**Web UI and diagnostics**
- **FSexplorer "Update Firmware" button** is visible again on touch-capable desktops; the touch-class media query no longer hides the upload control.
- **Set-command debug surfacing**: silently-dropped set-commands now appear in the default debug stream instead of being swallowed.

**Tooling and build**
- **Flash scripts hardened**: `flash_otgw.sh` / `flash_otgw.bat` now mirror spec parity, verify SHA256 integrity, and pick the binary that matches the requested version. The `.bat` variant detects COM ports through the Windows registry and auto-downloads binaries when not found locally.
- **`build.py` auto-initialises missing git submodules** so a fresh clone or stale checkout builds without manual `git submodule update`.
- **`evaluate.py`** false-positive and stale checks fixed; the gate is now meaningful again.
- **`/beta-prerelease` skill + GitHub Action** for tag-driven (and, after the May 2026 workflow refresh, workflow-dispatch-driven) beta publishing, with draft-first asset attachment to satisfy GitHub's immutable-releases policy. The release body now inlines a "What's new since the last public release" digest sourced from this file's content above the `<!-- digest:end -->` sentinel, and the skill gates the README + CHANGELOG staleness check before the version bump so a stale narrative cannot ride out under a fresh tag.

**Code hygiene**
- **Dead and orphaned code paths cleaned out of `dev`**: inactive subsystem code and its matching scaffolding in `OTGW-firmware.h` removed, since neither is reachable on the 1.5.x / 1.6.x line.

For per-commit detail see [`CHANGELOG.md`](CHANGELOG.md) under `## [Unreleased]`. For architectural rationale see the linked ADRs under [`docs/adr/`](docs/adr/).

<!-- digest:end -->

## Full per-line detail (since v1.5.0-fix2)

Below is the long-form version of the digest above, with one section per area and explicit cross-references. The CI workflow inlines only the digest (everything above the `<!-- digest:end -->` marker) into the release body; this section is here for testers who follow the "Read the full release notes" link.

### MQTT contract changes (worth a second read)

- **HA availability decoupling (ADR-074, TASK-607).** Before this release, `<toptopic>/value/<nodeid>` doubled as the HA availability topic AND the OT-bus liveness indicator. The two are different things: when the boiler stops talking, MQTT is still healthy, but every entity went `unavailable`. As of `1.6.0-beta.N`, the HA availability topic reflects only the ESP↔MQTT link (birth/LWT). OT-bus liveness is on the dedicated `otgw_connected` sensor.
  - **Action for tester:** if any of your automations or dashboards key off the base value topic to detect that the boiler is talking, switch them to the `otgw_connected` sensor instead. The base value topic is no longer reliable for that purpose.
- **JIT (just-in-time) discovery (ADR-073, supersedes ADR-041).** Discovery configs for OpenTherm MsgIDs are no longer published at boot. They publish the first time the gateway sees a value for that MsgID, which trims a large startup burst and means HA only sees entities that are actually relevant to the connected boiler+thermostat. Boot-time discovery still applies to the small set of non-OT pseudo-IDs (climate, number, Dallas, heap stats, firmware/PIC). The `F` force-republish action remains available for one-shot recovery.
- **MsgID 0 Status gating (TASK-633).** The canonical `Status` topic now gates on the boiler-side worldview so it stops flapping on thermostat-only frames. This is a quality-of-life fix; no automation contract change.

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

- [`CHANGELOG.md`](CHANGELOG.md): per-commit running log under `## [Unreleased]`.
- [`README.md`](README.md): landing page, "What's new on dev" section.
- [`docs/adr/`](docs/adr/): architectural rationale, in particular ADR-073 (JIT discovery), ADR-074 (HA availability), ADR-075 (proxy-answer routing).
- [`docs/guides/`](docs/guides/): flash, build, MQTT, and integration guides.

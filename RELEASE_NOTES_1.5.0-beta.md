# OTGW-firmware v1.5.0-beta Release Notes

**Status:** Beta, in active development on `dev`.
**Branch:** dev (LTS line on Arduino Core 2.7.4)
**Branch baseline:** 2026-04-26
**Previous release:** [v1.4.1](docs/releases/RELEASE_NOTES_1.4.1.md)

> **This is a development release.** It is not recommended for production deployments. Use it on a test device or a spare gateway. A stable `v1.5.0` will follow when the line has soaked in the field.

## Overview

The `1.5.x` line is the long-term-support track of OTGW-firmware on the ESP8266. It carries forward the `v1.4.x` feature set on **Arduino Core 2.7.4**, the Core version the project shipped on reliably for years before the move to Core 3.1.2 in `v1.4.x`.

Why a dedicated LTS line on Core 2.7.4? Field experience on Arduino Core 3.1.2 surfaced behaviour that does not appear on Core 2.7.4: less reliable reboot after OTA, and stricter PROGMEM alignment that punishes legacy code paths with `Exception (3)` instead of tolerating them. Core 2.7.4 is the last Core version that ran this firmware without those classes of issue. Users who prefer the proven, conservative platform have a place to land here.

The separate ESP32 / SAT `v2.0.0` exploration on `feature-dev-2.0.0-otgw32-esp32-sat-support` continues independently. It targets a different platform and a different architecture, and is not affected by the Core selection for this LTS line.

## What's different from v1.4.1

### Arduino Core 2.7.4 baseline

The build is aligned with Arduino Core 2.7.4 while keeping the partition layout that `v1.4.x` introduced. Practical consequences:

- **LittleFS partition layout retained at `eesz=4M2M`** (4 MB flash, 2 MB LittleFS). This is a deliberate decoupling: the Core version and the partition layout are independent in this project (the FQBN `eesz=` parameter governs the partition, not the Core). Keeping the layout means **upgrading from `v1.4.1` to `1.5.x` does not require a filesystem partition reformat**; the v1.4.x `WARNING: flash filesystem FIRST` mitigation does not apply here.
- **`mqttha.cfg` archive removed from the filesystem image.** The static HA discovery template that consumed most of the v1.4.x filesystem footprint is no longer needed because HA discovery has been fully streamed since the `v1.4.0` rewrite.
- **lwIP returns to the version shipped with Core 2.7.4** (the 2.2.0 update was a Core 3.1.2 feature).
- **PROGMEM byte-safe helpers stay in place.** `pgm_strncmp_PP`, `pgm_read_char` and the `memcmp_P`-on-binary-data rules introduced for Core 3.1.2 are correct on both Core versions; the codebase keeps them defensively.

### Reboot reliability hardening

A series of fixes addresses the post-OTA reboot reliability that motivated the LTS effort. These changes harden the reboot path on both Core versions, but they were specifically driven by Core 3.1.x failure modes:

- **Deferred reboot with lifecycle heap snapshots** captures heap and flash state at four points around an OTA-triggered reboot, so failure modes can be diagnosed from telemetry instead of reproduction.
- **Explicit service cleanup before `ESP.restart()`** ensures WebSocket, telnet, HTTP and MQTT are torn down in a defined order rather than left to the SDK's restart cleanup.
- **`ESP.reset()` fallback path** for cases where `ESP.restart()` returns to the caller instead of resetting (a Core 3.1.x failure mode).
- **`WiFi.disconnect()` removed from the reboot path** because it wipes NVRAM credentials on Core 3.1.x; the device now reboots cleanly without losing stored WiFi.
- **Nightly restart routes through the unified `doRestart()` path** so it benefits from the same cleanup and snapshot machinery.
- **Safety-tail delay restored after `ESP.restart()`** so the auto-reset window fires reliably on devices that need it.

A research report capturing the full investigation is in `docs/research/`.

### MQTT publish gating tightening

The Status-frame fan-out and gated msgId publishing logic from `v1.4.x` is tightened further to reduce broker pressure and heap competition during burst windows:

- **msgId 0 Status fan-out gate decoupled from `iInterval`** with an independent 60-second heartbeat so Status republishes do not piggyback on the user-configured publish interval.
- **msgId 5/6/100 bit-and-byte fan-out gating** with a 60-second heartbeat (Scope C-min): the per-bit and per-byte child topics for these msgIds publish on change and at most once per minute as a heartbeat.
- **Minimum spacing of 1 second between gated fan-out publishes** so a burst of changes is serialised onto the MQTT outbound buffer rather than queued back-to-back.
- **Tightened spacing to 250 ms between gated publishes** in the latest beta build, with `BGTRACE`/`OTTRACE` instrumentation disabled by default for stability builds.

### Home Assistant discovery for stats topics

The hourly heap-stats topic introduced in `v1.4.1` now ships with full HA auto-discovery so the stats fields appear as proper HA sensors instead of raw JSON in MQTT Explorer:

- Discovery configs for `otgw-firmware/stats/*` metrics (free heap, fragmentation, tier counters, dropped publishes, discovery state).
- Pseudo-ID 247 stats discovery repaired and related publish gates hardened.
- `IS_PIC_ENTRY` flag honoured in HA discovery `stat_t` generators.
- The legacy `mqttha.cfg` archive pipeline is removed; discovery is streaming-only.
- New `sendMQTTDataPic()` helper centralises the `otgw-pic/` publish sites so future stats sensors are simpler to add.

### WebUI design system and dark/light hardening

A small but visible Web UI refresh:

- **Self-hosted Inter and JetBrains Mono fonts** so the UI renders with consistent typography offline, without external font CDNs.
- **Design system tokens** centralise colours, spacing, and typography so dark and light themes stay in sync as the UI evolves.
- **Dark theme fixes**: scrollbar styling, placeholder colour, `.input-changed` contrast (was unreadable black-on-dark-grey), `color-scheme` declaration.
- **Light theme fixes**: input contrast, mobile header toggle overlap.
- **Cross-browser hardening** for theme toggling on Chrome, Firefox, and Safari.
- **Log render hotpath fix** caps the WebUI restore buffer at 10k entries so a long-uptime session does not stall on page load. Per-message console logs are silenced behind the `otgwDebug.verbose` gate.

### Boot and loop diagnostics

- **`logBootSignature()` boot telemetry** captures reset reason, SDK version, sketch size, and free heap at the earliest moment of `setup()` so post-mortem diagnosis has consistent data.
- **`BGTRACE` instrumentation** in `doBackgroundTasks` and the main loop gives per-phase timing breakdowns when enabled.
- **`processOT` sub-trace** breaks the OpenTherm message processing into named phases with per-phase heap and time deltas.

These remain compiled in but are off by default in stability builds; enable via the documented telnet keys when diagnosing field issues.

### Library bumps

- **SimpleTelnet submodule** updated to `25a0250` (printf stack raised to 256 bytes for safer formatted debug output).

## Behavioural notes for users

- **No new MQTT topic, REST API, or settings format changes vs `v1.4.1`.** Existing automations and HA configurations carry over unchanged.
- **First-boot HA discovery cadence** is unchanged from `v1.4.1` (2 s normal / 10 s slow-mode). The discovery rewrite is reused as-is.
- **`mqttha.cfg` is no longer in the filesystem image.** This is intentional and safe; the streaming discovery rewrite supersedes it. Users on a fresh flash see no difference.

## Upgrade considerations

`v1.4.1` and `1.5.x` share the same `eesz=4M2M` flash and partition layout, so the upgrade does not require a filesystem partition reformat. The `WARNING: flash filesystem FIRST` rule from the `v1.4.x` notes does not apply here.

Standard upgrade procedure (subject to confirmation with `v1.5.0` stable):

- A **firmware-only OTA** (`*.ino.bin` via the Web UI Update page) is the lightest path: existing settings stay untouched. The new firmware runs against the existing filesystem from `v1.4.1`, which means you miss the `1.5.x` filesystem-side updates (font self-hosting, `mqttha.cfg` archive removal) but everything functional still works.
- A **full OTA** (firmware binary first, then the new filesystem image) brings the WebUI design system updates as well. Standard practice still applies: export your settings via the Web UI before flashing the filesystem image, since the new image is a fresh content bundle.
- Testing of `v1.4.1 → 1.5.x` upgrades across real devices is ongoing. The exact recommended procedure will be confirmed with `v1.5.0` stable.

For users staying on `v1.4.1`, no action is required. `v1.4.1` continues to be the latest stable release.

## Known limits and TBD items

- **Upgrade procedure from v1.4.1**: documented above; under field testing.
- **Soak time**: the LTS line needs multi-week field uptime before stable. Volunteers welcome on Discord.
- **Documentation coverage**: ADRs that captured Core 3.1.2-era decisions are still accurate as historical record but may need superseding ADRs once the Core 2.7.4 LTS choice is documented at the architecture level.

## How to test

If you want to help shake out the LTS line on Core 2.7.4:

1. Flash a non-production device.
2. Run it alongside your `v1.4.1` production gateway on the same broker (use a different `MQTT Unique ID`).
3. Report observations on Discord `#beta-testing`. Field data is what gets the line to stable.

Build artefacts are produced from `dev` on commit. There is no published GitHub release for `v1.5.0-beta` at this time; build locally with `python build.py` or pull a build from CI when one is offered.

## Architecture Decision Records

No new ADRs specifically for the Core 2.7.4 LTS choice yet. The existing ADRs from `v1.4.x` describe the architecture of features carried into `1.5.x`. An LTS-strategy ADR may be added before stable.

## Thank you

Thanks to everyone on Discord who flagged reboot and stability issues on Core 3.1.2 in field deployments. The decision to maintain a Core 2.7.4 LTS line is a direct response to that feedback.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.4.1](docs/releases/RELEASE_NOTES_1.4.1.md)

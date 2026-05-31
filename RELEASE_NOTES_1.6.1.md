# Release Notes: v1.6.1

v1.6.1 is a focused follow-up to v1.6.0. It makes MQTT on-change publishing the default, redesigns the boiler-unsupported diagnostics panel, and lands a set of MQTT and Web UI reliability fixes. Shipped after a short beta cycle on top of the v1.6.0 stable base.

Partition layout is unchanged from v1.6.0 (`eesz=4M2M`); no filesystem repartition is needed.

## New Features and Changes

### MQTT on-change publishing is now the default (ADR-081)

A new persisted setting, `MQTTonChangePublishing`, defaults to `true`, and the default publish interval is now `60` seconds.

- Changed OpenTherm values are published immediately.
- Unchanged values are refreshed once per `MQTTinterval` (default 60s) as a heartbeat, so Home Assistant does not mark entities unavailable.
- This replaces the previous default of publishing every repeated OpenTherm frame, cutting broker traffic substantially on a busy bus.
- A **Publish on change** checkbox and clearer tooltips were added to Settings > MQTT. Ticking it reveals the interval (and sets a 60s default); unticking it returns to legacy publish-every-message behaviour.

On upgrade, an existing `MQTTinterval=0` is migrated once to `60` and persisted (see Upgrade Notes).

### Redesigned boiler-unsupported diagnostics panel

The "Boiler does not implement these OpenTherm messages" notice on the Statistics tab is now a clean, readable table instead of a run-together list:

- Columns: **MsgID**, **Description** (human-readable name), **OpenTherm Name**, **Direction**.
- Styled as a proper amber notice card with a warning marker, monospace technical fields, and a Direction pill, in both light and dark themes.
- The `/api/v2/otgw/boiler-support` endpoint now also returns the human-readable message name (`friendly`).

### MQTT publish timing jitter

Periodic publish timers are now spread with a small random offset so the 5-minute and 60-second timers no longer align and fire together, which previously caused a burst-publish heap spike on some systems.

## Bug Fixes

- **MQTT truncated-publish protection**: on a partial or failed chunk write, the gateway now drops the TCP connection instead of finalising a truncated discovery config or value publish.
- **Web UI updates reliably after a firmware flash**: CSS assets are now served with `Cache-Control: no-cache` + ETag revalidation instead of a long-lived browser cache, so styling changes appear after an update. (One hard refresh may be needed the first time after upgrading to v1.6.1 to clear the old cached copy.)
- **Mobile settings layout**: settings field labels no longer wrap awkwardly on narrow screens.
- **Statistics table column resizing** corrected.
- **Boiler diagnostics tooltip** exposed correctly.

## Tooling

- **`scripts/capture-mqtt-debug.bat`**: a small Windows helper to capture MQTT debug output, making it easier to collect logs when reporting MQTT issues.

## Upgrade Notes

1. **MQTT publishing default changed.** Existing installs with `MQTTinterval=0` (publish every frame) are migrated once on first boot to `60` seconds with on-change publishing enabled. Changed values still publish immediately; unchanged values now refresh once per minute. To keep the old publish-every-frame behaviour, untick **Publish on change** in Settings > MQTT (or set `MQTTonChangePublishing=false`). Full detail in [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md).
2. **No filesystem repartition required.** LittleFS partition layout is unchanged from v1.6.0.

## Thank You

Special thanks to **GeorgeZ83** for extensive beta testing and detailed issue reports throughout the v1.6.1 beta cycle. The MQTT publishing behaviour and the diagnostics panel improvements in this release were driven directly by that feedback.

Thanks to everyone on [Discord](https://discord.gg/zjW3ju7vGQ) who helped test and diagnose across the beta builds.

Full per-commit detail: [`CHANGELOG.md`](CHANGELOG.md). Architectural rationale in [`docs/adr/ADR-081-mqtt-on-change-publishing-default.md`](docs/adr/ADR-081-mqtt-on-change-publishing-default.md).

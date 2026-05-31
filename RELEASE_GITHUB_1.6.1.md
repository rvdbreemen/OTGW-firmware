MQTT on-change publishing is now the default, a redesigned boiler-unsupported diagnostics panel, and a set of MQTT and Web UI reliability fixes on top of v1.6.0.

Full release notes: [RELEASE_NOTES_1.6.1.md](RELEASE_NOTES_1.6.1.md)
README and setup: [README.md](README.md)
Breaking changes: [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md)
API documentation: [docs/api/](docs/api/)

## New Features

- **MQTT on-change publishing is now the default** (ADR-081): the new `MQTTonChangePublishing` setting defaults to `true` with `MQTTinterval=60`. Changed OpenTherm values still publish immediately; unchanged values refresh once per minute as a heartbeat instead of on every repeated frame. This cuts broker traffic substantially while keeping Home Assistant entities fresh. A "Publish on change" checkbox and clearer tooltips were added to Settings > MQTT.
- **Redesigned boiler-unsupported diagnostics panel**: the "Boiler does not implement these OpenTherm messages" notice on the Statistics tab is now a clean table (MsgID, Description, OpenTherm Name, Direction) with human-readable message names, styled as a proper notice card in both light and dark themes.
- **MQTT publish timing jitter**: periodic publishes are now spread with a small random offset so the 5-minute and 60-second timers no longer fire together, avoiding a burst-publish heap spike.

## Bug Fixes

- **MQTT truncated-publish protection**: on a partial or failed chunk write the gateway now drops the TCP connection instead of finalising a truncated discovery or value publish.
- **Web UI updates reliably after a firmware flash**: CSS is now served with `no-cache` + ETag revalidation instead of a long-lived browser cache, so styling changes appear after an update (one hard refresh may be needed the first time after upgrading to v1.6.1).
- **Mobile settings layout**: settings field labels no longer wrap awkwardly on narrow screens.
- **Statistics table column resizing** corrected.
- **Boiler diagnostics tooltip** exposed correctly.

## Tooling

- **`scripts/capture-mqtt-debug.bat`**: a small Windows helper to capture MQTT debug output, making it easier to collect logs when reporting MQTT issues.

## Upgrade Notes

1. **MQTT publishing default changed**: existing installs with `MQTTinterval=0` (publish every frame) are migrated once to `60` seconds with on-change publishing enabled. To keep the old publish-every-frame behaviour, untick **Publish on change** in Settings > MQTT (or set `MQTTonChangePublishing=false`). See [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md).
2. No filesystem repartition required. LittleFS partition layout unchanged from v1.6.0.

## Thank You

Special thanks to **GeorgeZ83** for extensive beta testing and detailed issue reports throughout the v1.6.1 beta cycle, which drove the MQTT publishing and diagnostics work in this release.

Thanks to everyone on [Discord](https://discord.gg/zjW3ju7vGQ) who helped test and diagnose across the beta builds.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support, discussion, and beta testing.

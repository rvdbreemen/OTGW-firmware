v1.5.0 is the first stable release of the `1.5.x` long-term-support line on **Arduino Core 2.7.4**. It ships 29 beta builds worth of fixes, MQTT improvements, and Home Assistant discovery refinements on the proven, conservative Core version.

Full release notes: [RELEASE_NOTES_1.5.0.md](RELEASE_NOTES_1.5.0.md)
Breaking changes: [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md)

---

## Bug fixes

- **Master MQTT topic flapping** for `Tr`, `TrSet`, `MaxRelModLevelSetting`, and related write-only MsgIDs (ADR-066, TASK-478, TASK-561): base topic now uses Read-Ack and Write-Data only; boiler echo gated by per-MsgID `bSlaveEchoesValue` flag
- **TSet flapping on heat-pump boilers** (TASK-571): `bSlaveEchoesValue=false` for MsgID 1 stops the base topic from oscillating on non-echoing boilers
- **WiFi: no DHCP lease after first reboot post-flash** (TASK-432): `wifi_station_dhcpc_start()` removed; SDK manages DHCP autonomously again
- **GW=R infinite PIC reset loop** (TASK-538): `GW=R` is now fire-and-forget; queue entry cleared immediately after send
- **WiFi TCP listeners re-bound on reconnect**: socket state checked before re-binding to avoid port-already-in-use errors
- **WebSocket reload-storm**: 250 ms reconnect debounce and `pagehide` shutdown handler prevent connection table exhaustion

## Improvements

- **Sibling-suffix MQTT source topics** (ADR-070/071): `TSet_thermostat` / `TSet_boiler` instead of `TSet/thermostat` / `TSet/boiler` — source-variant entities now actually register in HA for the first time
- **Worldview semantics** (ADR-069): `/thermostat` carries thermostat intent, `/boiler` carries boiler echo — consistently across all MsgID families
- **Human-readable HA entity names** (ADR-072): `DHW Setpoint` instead of `OTGW_TdhwSet`; `unique_id` unchanged so automations are unaffected
- **MDI icons** added to all HA discovery sensor entities
- **HA discovery for diagnostic topics** (TASK-540): PIC and firmware metrics appear as proper HA sensors
- **GET /api/v2/debug** endpoint (TASK-536): one-call diagnostic dump for troubleshooting
- **Compact telnet banner** (TASK-545): reset reason, heap state, MQTT status, and all debug toggles in one screen
- **No-Python flash scripts**: `flash_otgw.sh` / `flash_otgw.bat` and `build.sh` / `build.bat` for environment-free builds and flashes
- Reboot reliability hardening: deferred reboot with heap snapshots, explicit service cleanup, `ESP.reset()` fallback, `WiFi.disconnect()` removed from reboot path
- MQTT publish gating tightened: 60 s heartbeats for msgId 0/5/6/100, 250 ms minimum spacing, smart 5-minute republish threshold
- WebUI: self-hosted Inter and JetBrains Mono fonts, design tokens, dark/light theme hardening

## Breaking changes

Three breaking changes vs v1.4.1. See [docs/BREAKING_CHANGES.md](docs/BREAKING_CHANGES.md) for migration commands.

1. **MQTT source topics: sibling-suffix shape** — `<msgid>/thermostat` and `<msgid>/boiler` become `<msgid>_thermostat` and `<msgid>_boiler`
2. **`/gateway` sub-topic removed** — canonical base topic replaces it
3. **HA discovery entity display names changed to Title Case** — cosmetic only; `unique_id` and entity IDs unchanged

## Upgrade notes

`v1.4.1` to `v1.5.0` shares the same `eesz=4M2M` partition layout. No filesystem partition reformat required.

1. Flash firmware (`*.ino.bin`) via the Web UI update page
2. Flash filesystem (`*.littlefs.bin`) via the same page — export settings first, the image is a fresh content bundle
3. Trigger a discovery republish from Settings to push updated entity names to HA
4. If `bSeparateSources` is enabled, clear old nested retained topics (see BREAKING_CHANGES.md)

---

## Thank you

Special shoutout to **andrebrait** for being the standout tester throughout the entire 1.5.0 beta cycle: reporting the DHCP regression that became a targeted fix, driving the entity friendly name improvements all the way to Title Case consistency, sharing HA integration screenshots at every iteration, and giving the final "It's looking perfect" that confirmed the release was ready.

Thanks to everyone who contributed through testing, logs, and feedback:

- **crashevans** (Discord) — systematic stress testing across beta.2, beta.5, beta.11, and beta.20 with detailed long-term monitoring logs
- **_reuzenpanda_** (Discord) — reported source-topic flapping and provided the telnet logs that pinpointed the ADR-071 fix; confirmed sibling-suffix topics in HA
- **fuzzyduck** (Discord) — confirmed the Title Case transition works without breaking existing HA automations
- **simontemplar** (Discord) — spotted the partition size documentation contradiction in the initial build
- **@andrebrait** (GitHub) — filed the WiFi/DHCP reconnect issue with full reproduction steps

Community members on [Discord](https://discord.gg/zjW3ju7vGQ) who helped diagnose, test, and verify.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

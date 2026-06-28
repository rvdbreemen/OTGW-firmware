# OTGW-firmware 1.7.1-beta.3

Pre-release on the 1.x maintenance line. Single follow-up to the 1.7.1-beta.2 cooling-climate fix.

## What's new

**hvac_mode and hvac_action are now standalone discoverable Home Assistant sensors** (GH #665 follow-up)

1.7.1-beta.2 added the unified `off`/`heat`/`cool` climate entity, which publishes `<pub>/hvac_mode` and `<pub>/hvac_action`. Those topics were only wired into the climate card, so the values were not visible as plain sensors — the #665 reporter could not find `hvac_action` anywhere.

This release adds two standalone discoverable sensor entities pointing at the existing topics (no new publish logic, climate entity unchanged):

- **HVAC Mode** — `sensor.<device>_hvac_mode` — values `off` / `heat` / `cool`
- **HVAC Action** — `sensor.<device>_hvac_action` — values `off` / `idle` / `heating` / `cooling`

After flashing, let Home Assistant re-run MQTT discovery (or restart HA); the two new sensors appear under the existing OTGW device.

## Upgrade

Flash both the firmware and the filesystem — over the air via the web UI, or over USB with the bundled flash script / the merged binary attached here.

## Changes since 1.7.1-beta.2

- feat(mqtt): expose hvac_mode + hvac_action as discoverable HA sensors (TASK-941, GH #665)
- docs(adr): ADR-085 unified heat/cool/off HA climate from OT status bits

The 2.0.0 (ESP32-S3) line carries the same feature as alpha.282.

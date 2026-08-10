# OTGW-firmware v1.7.3 Release Notes

**Release date:** 2026-08-10
**Branch:** main (from otgw-1.x.x)
**Compare:** [v1.7.2...v1.7.3](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.2...v1.7.3)

## Overview

v1.7.3 is a Home Assistant integration and OpenTherm decoding release for the 1.x (ESP8266) line. Entities no longer go to "unknown" after a Home Assistant Core restart, three Remeha vendor message IDs decode for the first time, and four ventilation topics that only ever published once at startup now honour their heartbeat. No breaking changes versus v1.7.2.

## Bug fixes

### Entities stayed "unknown" after every Home Assistant Core restart

Discovery configs are retained on the broker, so Home Assistant rebuilt the entities correctly after a restart. State topics are not retained, and most values publish only when they change. A freshly restarted Home Assistant therefore subscribed and then waited for a value the firmware had no reason to send. Temperatures looked correct while the thermostat card sat on "unknown", and only rebooting the ESP cleared it. A gateway reset did not, because that resets the PIC and not the ESP.

The `homeassistant/status` transition from `offline` to `online` now resets the publish gates, so every tracked value re-publishes as first-seen. That covers the 128 OpenTherm message-id slots, the status and ventilation-status bit and byte fan-outs, the ASF, RBP and Remote Override fan-outs, and `hvac_mode` and `hvac_action`. The republish is paced by OpenTherm bus arrival rather than emitted in one burst, and discovery itself is untouched, so the just-in-time discovery behaviour is unchanged. (TASK-1058, ADR-088)

### `hvac_mode` and `hvac_action` could latch a value Home Assistant never received

Both publishers discarded the result of the MQTT send and updated their in-memory cache regardless, while the force flag that triggered the send was already cleared. A single dropped publish therefore stranded the topic until the thermostat mode genuinely changed. They now update the cache only after a confirmed send, and fall back to the unset state otherwise, so the next OpenTherm frame retries. (TASK-1058)

### `hvac_mode` and `hvac_action` had no heartbeat

Both were the only on-change gated topics without an interval republish. Every neighbouring status topic already republishes once a minute, but these two were sent only on a real change or a Home Assistant restart, so a consumer that missed the last publish stayed stale indefinitely. Measured on a bench gateway before the fix, `hvac_mode` published exactly once across a ten minute capture.

They now republish at least every 5 minutes even when the value does not change. A genuine value change still publishes immediately, and the heartbeat timestamp only advances on a confirmed send, so a dropped publish retries on the next OpenTherm frame instead of restarting the five minute window. (TASK-1060)

### Remeha message IDs 131, 132 and 133 were never decoded

The Remeha vendor messages (dF-/dU-codes, service message, and detection of connected SCUs) arrived and were logged as `Unknown message`, producing no labelled value and no Home Assistant entity. The internal message-id list placed those three entries three positions too low, at 128 to 130, which are unassigned OEM ids; the message table had them correctly at 131 to 133. Ids 128 to 130 meanwhile produced label-less output because they were being decoded as if they were the Remeha messages.

Numbering now follows the OpenTherm data-id reference shipped in `docs/opentherm specification/`. Owners of a Remeha qSense or Tzerra gain three working sensors. (TASK-1064)

### Four ventilation and heat-recovery topics only ever published once

`vh_fault`, `vh_ventilation_mode`, `vh_bypass_status` and `vh_bypass_automatic_status` published at startup and never refreshed on their one minute heartbeat, so if Home Assistant missed that first message the entity stayed empty until the gateway was rebooted. The slave-side status bits shared their publish timers with the master-side bits, and the master fan-out reset those timers microseconds before the slave fan-out read them, so the interval never elapsed. The two remaining bits were unaffected because their timers had no master counterpart, which is what made the pattern identifiable. (TASK-1066)

## New features

None. This is a fix release.

## Internal improvements

- An OpenTherm decode-coverage simulation fixture, plus a golden-baseline regression gate and a one-command run that exercises the fixture on a device and compares against that baseline. This is what independently surfaced the ventilation heartbeat starvation. (TASK-1062, TASK-1063, TASK-1065)
- Release assets: `SHA256SUMS` and the source bundle are attached to stable releases again. (TASK-936)

## Behaviour changes

**"MQTT Home Assistant Reboot Detection" is deprecated and no longer shown in the web interface.** Detecting a Home Assistant restart now always requires observing it go offline first, which is what that setting used to switch off. Requiring the full offline-to-online transition also means a retained Home Assistant birth message, which some setups publish and the broker then replays on every reconnect, cannot trigger a republish on an ordinary reconnect. The setting is still read from and written to `settings.ini`, so existing configurations load unchanged; it simply no longer affects behaviour. (TASK-1058, ADR-088)

## Breaking changes

**No breaking changes versus v1.7.2.** No MQTT topic renames, no REST API removals, no settings-format changes, and no migration on upgrade.

## Upgrade notes

Flash **both** firmware and filesystem. Settings are preserved.

After upgrading, restart Home Assistant once and confirm the OTGW entities show real values within a few seconds rather than sitting on "unknown". Do not reboot the gateway to test this; rebooting is what used to mask the problem.

Remeha qSense and Tzerra owners gain three new entities (dF-/dU-codes, service message, connected SCUs) once the corresponding messages appear on the bus.

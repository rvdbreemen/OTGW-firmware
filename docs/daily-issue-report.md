# Daily Issue Report — 2026-06-27

## GitHub Issues (updated in last 24h)

### Issue #666 — winget failed to install EclipseMosquitto.Mosquitto
- **Reporter:** Pistoletjes
- **Created:** 2026-06-27T06:48:41Z
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/666
- **Classification:** Bug report (scripts)
- **Summary:** The winget package ID for Eclipse Mosquitto has changed. `scripts/capture-mqtt-debug.bat` still references the old ID `EclipseMosquitto.Mosquitto` (exit code -1978335212), but the correct current ID is `EclipseFoundation.Mosquitto`. The install step in the capture script fails as a result.

---

### Issue #665 — Switching heating and cooling with Honeywell Round Modulation Heat/Cool
- **Reporter:** jelvank
- **Created:** 2026-06-26T21:08:11Z
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/665
- **Classification:** Bug report (firmware / MQTT / HA integration)
- **Summary:** User has a Honeywell Round Modulation Heat/Cool thermostat connected to a TripleSolar PVT heat pump (capable of both heating and cooling), with an OTGW in between. Heating-only mode works fine, but heat/cool Auto mode is not represented correctly in Home Assistant via either the HA integration or MQTT:
  - Via HA integration: after switching thermostat to Auto/cooling, HA shows thermostat as a cooling-only device with no way to switch back to heating and no "off" mode.
  - Via MQTT: thermostat shows as a heating thermostat (with an "off" mode), but the setpoint displayed is actually the cooling (high) setpoint, not the heating setpoint.
  - 15 minutes of diagnostic logs attached to the issue (firmware 1.7.0+8745315).

---

## Tweakers Forum Posts (last 24h, excluding number3/rvdbreemen)

### Post by jelvank — 2026-06-26 20:23 GMT
- **URL:** https://gathering.tweakers.net/forum/list_message/85579056#85579056
- **Classification:** Related to bug report (heat/cool issue)
- **Summary:** jelvank acknowledges number3's suggestion to run the capture script and states they will file a GitHub issue rather than dump the full log in the forum thread. This post is the precursor to GitHub issue #665 above.

---

## Summary

Two new bug reports found in the last 24 hours:

1. **#666 (script bug):** `capture-mqtt-debug.bat` uses a stale winget package ID for Mosquitto — needs a one-line fix to update `EclipseMosquitto.Mosquitto` → `EclipseFoundation.Mosquitto`.

2. **#665 (firmware bug):** Heat/Cool Auto mode (dual setpoints — heating floor + cooling ceiling) is not handled correctly in OTGW firmware's MQTT / HA discovery. The thermostat mode and active setpoint are misreported in HA, and there is no way to switch back from cooling mode to heating mode via the HA interface. Logs attached to the issue.

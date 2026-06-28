# Daily Issue Report — 2026-06-28

## GitHub Issues (updated in last 24h)

### Issue #665 — Switching heating and cooling with Honeywell Round Modulation Heat/Cool
- **Reporter:** jelvank
- **Created:** 2026-06-26T21:08:11Z | **Last updated:** 2026-06-27T21:14:40Z
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/665
- **Labels:** bug, good first issue
- **Classification:** Bug report (firmware / MQTT / HA integration)
- **Summary:** User has a Honeywell Round Modulation Heat/Cool thermostat on a TripleSolar PVT heat pump (heating + cooling) connected via OTGW. Heat/Cool Auto mode (dual setpoints) is not correctly represented in Home Assistant — mode switching between heating/cooling is broken and the wrong setpoint is shown. A beta 1.7.1 fix was released by number3 and jelvank is now validating it. First test results were posted in the issue on 2026-06-27. Issue still open pending beta validation.

---

### Issue #666 — winget failed to install EclipseMosquitto.Mosquitto
- **Reporter:** Pistoletjes
- **Created:** 2026-06-27T06:48:41Z
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/666
- **Classification:** Bug report (scripts)
- **Summary:** The winget package ID for Eclipse Mosquitto has changed. `scripts/capture-mqtt-debug.bat` still references the old ID `EclipseMosquitto.Mosquitto` (exit code -1978335212); the current correct ID is `EclipseFoundation.Mosquitto`. One-line fix needed in the capture script. No comments yet; issue newly opened.

---

## Tweakers Forum Posts (last 24h, excluding number3/rvdbreemen)

### Post by jelvank — 2026-06-27 21:24 GMT
- **URL:** https://gathering.tweakers.net/forum/list_message/85585876#85585876
- **Classification:** Follow-up on bug report (heat/cool issue, related to GitHub #665)
- **Summary:** jelvank posted initial test results from the 1.7.1 beta in the GitHub issue. They are reconsidering the architecture given the OpenTherm limitations: the heat pump does not modulate — it only turns on/off based on thermostat demand, so OpenTherm's dual-setpoint auto mode adds complexity with little benefit. Two alternative paths mentioned: (1) run OTGW as the thermostat controller to get full boiler data back; (2) use the heat pump's simple on/off contacts for heat/cool switching (no data return). No new standalone bug; this is part of the ongoing #665 validation.

---

## Summary

Two open bug reports remain active this period:

1. **#665 (firmware / HA integration):** Heat/Cool Auto mode with dual setpoints (heating floor + cooling ceiling) is misreported in MQTT / HA discovery — wrong setpoint shown, mode switching broken. A 1.7.1 beta fix is in active validation by jelvank. **Needs: beta sign-off to close or further fix iteration.**

2. **#666 (script bug):** `scripts/capture-mqtt-debug.bat` uses stale winget package ID `EclipseMosquitto.Mosquitto`; correct ID is `EclipseFoundation.Mosquitto`. Simple one-line fix. **Needs: fix in capture script.**

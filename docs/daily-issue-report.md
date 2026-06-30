# Daily Issue Report — 2026-06-30

## GitHub Issues (updated in last 24h)

No GitHub issues were updated in the last 24 hours.

---

## Tweakers Forum Posts (last 24h, excluding number3/rvdbreemen)

### Post by tjanssen — 2026-06-29 09:10 GMT
- **URL:** https://gathering.tweakers.net/forum/list_message/85594732#85594732
- **Classification:** Bug indication + Feature request
- **Summary:** tjanssen reports experiencing another disconnect over the weekend (recurring issue, previously mentioned in earlier posts). Requests an uptime sensor be added to the firmware so that reboots/resets become visible without having to watch logs. Directed at number3.

### Post by hvxl — 2026-06-29 08:10 GMT
- **URL:** https://gathering.tweakers.net/forum/list_message/85594186#85594186
- **Classification:** Discussion / Not relevant (no code action needed)
- **Summary:** hvxl responds to jelvank's heat/cool discussion (#665 thread). States no PIC firmware changes are needed for the described use case. Suggests running heating via the thermostat as normal and controlling cooling directly via OTGW using the CE/CL commands. Notes that cooling control is less precision-sensitive than heating so a simpler algorithm suffices.

---

## Open Items Carried Over

1. **#665 — Switching heating and cooling with Honeywell Round Modulation Heat/Cool**
   - **Reporter:** jelvank
   - **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/665
   - **Status:** Open — 1.7.1 beta validation in progress by jelvank as of 2026-06-27.
   - **Classification:** Bug report (firmware / MQTT / HA integration)
   - **Summary:** Heat/Cool Auto mode (dual setpoints) not correctly represented in HA; mode switching broken. Beta 1.7.1 fix released; awaiting field validation sign-off.

2. **#666 — winget failed to install EclipseMosquitto.Mosquitto**
   - **Reporter:** Pistoletjes
   - **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/666
   - **Status:** Open — no comments or fix yet.
   - **Classification:** Bug report (scripts)
   - **Summary:** scripts/capture-mqtt-debug.bat uses stale winget package ID EclipseMosquitto.Mosquitto; correct ID is EclipseFoundation.Mosquitto. One-line fix needed.

3. **Recurring disconnect (Tweakers, tjanssen)** — NEW this report
   - **Reporter:** tjanssen (Tweakers)
   - **Source:** https://gathering.tweakers.net/forum/list_message/85594732#85594732
   - **Status:** Informal report; no GitHub issue filed yet.
   - **Classification:** Bug indication (connection stability)
   - **Summary:** User is experiencing recurring OTGW disconnects. Feature request for an uptime sensor to make resets visible at a glance. No GitHub issue open yet; may warrant one if the root cause is firmware-side.

---

## Summary

No new GitHub issues in the last 24 hours. Two Tweakers posts from non-excluded users:

- **tjanssen** reported a recurring disconnect and requested an uptime sensor — potentially a firmware stability signal worth tracking.
- **hvxl** provided guidance on cooling-mode control (CE/CL commands, no PIC changes needed) — discussion only, no action required.

---

# Daily Issue Report — 2026-06-29

## GitHub Issues (updated in last 24h)

No new issues in the last 24 hours.

---

## Tweakers Forum Posts (last 24h, excluding number3/rvdbreemen)

No new posts in the last 24 hours.

Most recent non-excluded activity was from **jelvank** on 2026-06-27 21:24 GMT (covered in the 2026-06-28 report):
ongoing validation of the 1.7.1 beta fix for heat/cool mode switching (GitHub #665).

---

## Open Items Carried Over

The following issues from previous reports remain open and unresolved:

1. **#665 — Switching heating and cooling with Honeywell Round Modulation Heat/Cool**
   - **Reporter:** jelvank
   - **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/665
   - **Status:** Open — 1.7.1 beta validation in progress by jelvank as of 2026-06-27.
   - **Classification:** Bug report (firmware / MQTT / HA integration)
   - **Summary:** Heat/Cool Auto mode (dual setpoints) not correctly represented in HA; mode switching broken. Beta 1.7.1 fix released; awaiting field validation sign-off.

2. **#666 — winget failed to install EclipseMosquitto.Mosquitto**
   - **Reporter:** Pistoletjes
   - **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/666
   - **Status:** Open — no comments or fix yet.
   - **Classification:** Bug report (scripts)
   - **Summary:** scripts/capture-mqtt-debug.bat uses stale winget package ID EclipseMosquitto.Mosquitto; correct ID is EclipseFoundation.Mosquitto. One-line fix needed.

---

## Summary

No new issues in the last 24 hours.

---

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

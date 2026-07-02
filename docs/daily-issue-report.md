# Daily Issue Report — 2026-07-02

**Generated:** 2026-07-02 07:09 UTC  
**Window:** Last 24 hours (since 2026-07-01 07:09 UTC)

---

## GitHub Issues

**Repository:** rvdbreemen/OTGW-firmware  
**Result:** No open issues updated in the last 24 hours.

---

## Tweakers Forum (GoT thread #1653967)

**Feed fetched:** 2026-07-02 07:09 UTC  
**Exclusions:** Posts by `number3` (maintainer) and `rvdbreemen` are excluded.

**Result:** No new user posts in the last 24 hours.

### Recent context (for reference — outside 24h window)

| Date (UTC) | Author | Summary |
|---|---|---|
| 2026-07-01 05:04 | number3 *(excluded)* | Confirms uptime is already published on MQTT; says HA auto-discovery sensor will be added in the next beta. |
| 2026-06-29 09:10 | tjanssen | **Feature request** — Asks for an uptime sensor so a reset/disconnect is easy to spot. Reports another weekend disconnect. |
| 2026-06-29 08:10 | hvxl | Replies on cooling/heating control: no PIC firmware changes needed; suggests using CE/CL commands via OTGW for cooling. |

---

## Summary

**No new bug reports, feature requests, or questions in the last 24 hours.**

The most recent user activity (tjanssen, Jun 29) requesting an uptime HA auto-discovery sensor has already been acknowledged by `number3` on Jul 1 — it will be added in the next beta.

---

# Daily Issue Report — 2026-07-01

## GitHub Issues (updated in last 24h)

No GitHub issues were updated in the last 24 hours.

---

## Tweakers Forum Posts (last 24h, excluding number3/rvdbreemen)

No new non-excluded posts in the last 24 hours.

**Context (excluded post):** `number3` posted at 2026-07-01 05:04 UTC in reply to tjanssen's uptime-sensor request from 2026-06-29:
> "There is an uptime published on MQTT. However, no auto Discovery sensor yet. Will add it in the next beta."

---

## No new issues in the last 24 hours

---

## Open Items Carried Over

### GitHub — Issue #666 (OPEN, no response yet)
- **Title:** winget failed to install EclipseMosquitto.Mosquitto
- **Reporter:** Pistoletjes
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/666
- **Created:** 2026-06-27
- **Classification:** Bug report (tooling — `scripts/capture-mqtt-debug.bat`)
- **Summary:** The winget package ID for Mosquitto has changed from `EclipseMosquitto.Mosquitto` to `EclipseFoundation.Mosquitto`, causing the capture script to fail installation with exit code -1978335212. No comments or fix yet.

### GitHub — Issue #665 (CLOSED 2026-06-28)
- **Title:** Switching heating and cooling with Honeywell Round Modulation Heat/Cool
- **Reporter:** jelvank
- **URL:** https://github.com/rvdbreemen/OTGW-firmware/issues/665
- **Classification:** Bug report (firmware / MQTT / HA integration)
- **Summary:** Heat/Cool Auto mode (dual setpoints) not correctly represented in HA. Closed as completed under milestone 1.7.1. jelvank is continuing field validation; follow-up discussion ongoing in Tweakers forum.

### Tweakers — Feature request (tjanssen, 2026-06-29)
- **URL:** https://gathering.tweakers.net/forum/list_message/85594732#85594732
- **Classification:** Feature request
- **Summary:** tjanssen requested an uptime/reset sensor for HA auto-discovery after experiencing recurring disconnects. `number3` acknowledged: uptime is on MQTT but not yet in HA auto-discovery — planned for next beta.

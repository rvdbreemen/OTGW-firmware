# Daily Issue Report — 2026-05-08

## Sources checked

| Source | Status |
|---|---|
| GitHub Issues (updated last 24h) | ✅ Fetched |
| Tweakers forum RSS (last 24h) | ❌ Blocked (network policy prevents access to gathering.tweakers.net) |

---

## GitHub Issues — updated since 2026-05-07

### Issue #543 — Max CH water setpoint shows 0°C in HA Boiler entity

- **URL**: https://github.com/rvdbreemen/OTGW-firmware/issues/543
- **Type**: Bug report
- **Reporter**: rvdbreemen (on behalf of Discord user `andrebrait`)
- **Updated**: 2026-05-07T06:08:34Z
- **Status**: Open

**Summary**: The maximum CH water setpoint (e.g. 90°C) is displayed correctly in the OTGW web UI and in the Home Assistant Thermostat entity, but always shows 0°C in the Home Assistant Boiler entity. Reported by `andrebrait` in Discord `#beta-testing` and present since at least v1.0.0.

**Suspected cause**: MQTT topic mapping issue — the value reaches the firmware correctly but may be published to the wrong MQTT source topic, or the HA Boiler entity's auto-discovery config references the wrong topic.

**Pending**: Telnet debug logs from `andrebrait` while the issue is present (requested 2026-04-08).

---

## Tweakers Forum

Could not fetch RSS feed — `gathering.tweakers.net` is not reachable from this environment (network allowlist blocks it). No Tweakers posts could be checked today.

---

## Summary

| Source | New bugs | Feature requests | Questions | Not relevant |
|---|---|---|---|---|
| GitHub | 1 | 0 | 0 | 0 |
| Tweakers | N/A | N/A | N/A | N/A |

**1 open bug report** found: Issue #543 (max CH water setpoint showing 0°C in HA Boiler entity).

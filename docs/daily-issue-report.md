# Daily Issue Report — 2026-06-24

## GitHub Issues (updated in last 24h)

**No open GitHub issues updated in the last 24 hours.**

---

## Tweakers Forum (last 24h, excluding number3/rvdbreemen)

Thread: [OTGW] OpenTherm gateway — Duurzame energie en installaties  
URL: https://gathering.tweakers.net/forum/list_messages/1653967

### Post 1
- **Reporter:** Ittie
- **Date:** Tue, 23 Jun 2026 20:29 UTC
- **URL:** https://gathering.tweakers.net/forum/list_message/85552232#85552232
- **Classification:** Bug report (confirmation)
- **Summary:** User confirms experiencing the same WiFi disconnect issue others reported. Notes that sometimes a reboot of the access point temporarily resolves it. Asks for access to the beta firmware to help validate the fix.

**Background context (from thread, outside 24h window):**
Multiple users (jaronbor, tjanssen, Ittie) report WiFi instability and connectivity drops starting with firmware 1.6.0/1.6.1, not present in 1.3.5 or 1.5.0. Symptoms include: device repeatedly dropping WiFi, high packet loss during pings, HA entities (e.g. `otgw_boiler_connected`) flipping to unavailable every few minutes, and "Access to invalid address (29)" crashes. Root cause identified by number3 as the removal of `delay(1)` from the main loop. A fix is in beta 1.7.0-beta.34 (released on GitHub) and testers are being sought via the Discord beta channel. number3 responded to Ittie pointing to the GitHub release.

---

## Summary

| Source | Count | Classification |
|--------|-------|----------------|
| GitHub Issues | 0 | — |
| Tweakers Forum | 1 | Bug report (confirmation of known WiFi regression in 1.6.x) |

**Known active bug:** WiFi disconnection regression introduced in 1.6.0, root cause identified (removal of `delay(1)` in mainloop), fix candidate in beta 1.7.0-beta.34. Field validation in progress.

---

## Open Issues Reference (all 4 open, for context)

| # | Title | Reporter | Labels | Last Updated |
|---|-------|----------|--------|--------------|
| [#656](https://github.com/rvdbreemen/OTGW-firmware/issues/656) | Proper way to update | sanderlv | — | 2026-06-05 |
| [#557](https://github.com/rvdbreemen/OTGW-firmware/issues/557) | Settings / Run Boot Command and PIC missing | dwd1 | bug, question | 2026-05-06 |
| [#154](https://github.com/rvdbreemen/OTGW-firmware/issues/154) | [Request] Suggestions to improve security | 0crap | enhancement | 2026-05-09 |
| [#10](https://github.com/rvdbreemen/OTGW-firmware/issues/10) | Human readable ASF, RBP-flags, TSP, FHB-values … | STemplar | enhancement, good first issue | 2022-10-09 |

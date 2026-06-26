# Daily Issue Report — 2026-06-26

## GitHub Issues (updated in last 24h)

### Issue #557 — Settings / Run Boot Command and PIC missing
- **URL**: https://github.com/rvdbreemen/OTGW-firmware/issues/557
- **Reporter**: dwd1
- **Labels**: bug, question
- **Classification**: Bug report / Question
- **Updated**: 2026-06-25T17:19:06Z (comment activity on existing issue, originally opened 2026-04-29)
- **Summary**: User has a genuine NodoShop v2.11 OTGW with PIC16LF1847 and replaced the Wemos D1 board with a Wemos D1 Mini Pro (external antenna). After flashing with the same firmware version (1.4.1), the new board shows "PIC available: false" and the "Run Boot Command" checkbox and Boot Command input are missing from the settings page. The other (original) board with the same firmware version has these features working. User is asking what they are doing wrong. This looks like a PIC hardware detection failure — possibly a UART wiring or baud rate issue on the new board.

---

## Tweakers Forum (last 24h, excluding number3/rvdbreemen)

### Post by jelvank — Heat/Cool mode switching broken with HA integration
- **URL**: https://gathering.tweakers.net/forum/list_message/85569706#85569706
- **Author**: jelvank
- **Posted**: 2026-06-25 19:52 GMT
- **Classification**: Bug report / Question
- **Summary**: User has a TripleSolar PVT system capable of both heating and cooling via OpenTherm, with a Honeywell Round Modulation HeatCool thermostat connected through OTGW. Problem: when cooling mode is activated on the thermostat, the Home Assistant integration changes the entity to a "cooling-only" climate device and the heating mode can no longer be selected (only "cool" appears in the Mode selector). When switching to MQTT instead of the HA integration, the thermostat appears as a "heating thermostat that is off" and only "heating" or "off" modes are available, while the setpoint shows the *cooling* setpoint. The "Heat Cool Mode Control" sensor in HA is set to "Off". User is asking how to properly control heat/cool mode switching via HA.
  - **Relevance to firmware**: The MQTT discovery and HA climate entity mode representation for heat/cool mode may not correctly reflect the actual operating mode, leading to a confusing and broken HA UI when a heat/cool capable boiler/thermostat combination is used. This warrants investigation of how `CH2_MODE` / `COOLING_CONTROL` bits are mapped to MQTT discovery payloads.

### Post by hvxl — Response to jelvank (not a user issue)
- **URL**: https://gathering.tweakers.net/forum/list_message/85570670#85570670
- **Author**: hvxl (Schelte Bron, original PIC firmware developer)
- **Posted**: 2026-06-25 22:22 GMT
- **Classification**: Not relevant (developer response, not a user bug report)
- **Summary**: hvxl replied to jelvank asking for an OTmonitor log of how the HeatCool thermostat drives the system in cooling mode, so hvxl can assess whether PIC code changes are needed. No action required on our side for this post.

---

## Summary

| Source | Reporter | Type | Title |
|--------|----------|------|-------|
| GitHub #557 | dwd1 | Bug/Question | PIC not detected after board swap (Wemos D1 Mini Pro) |
| Tweakers | jelvank | Bug/Question | Heat/Cool mode switching broken in HA when cooling is active |

**Action items:**
- GitHub #557: Investigate whether PIC UART detection works correctly when flashing a Wemos D1 Mini Pro (vs standard D1 Mini). May be a board-specific UART mapping difference.
- Tweakers jelvank: Investigate MQTT discovery payload for heat/cool capable systems — how `cooling_enable` and `ch2_mode` are advertised to HA, and whether the HA climate entity can correctly represent a heat/cool thermostat mode.

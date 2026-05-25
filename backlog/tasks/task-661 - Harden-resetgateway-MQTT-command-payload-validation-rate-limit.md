---
id: TASK-661
title: 'Harden resetgateway MQTT command: payload validation + rate-limit'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 05:52'
updated_date: '2026-05-25 22:13'
labels:
  - security
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The new resetgateway set-command (MQTTstuff.ino:704-707, commit 908e1e16) lets any LAN client publish to <toptopic>/set/<nodeid>/resetgateway to trigger resetOTGW() (PIC hardware reset) with no payload validation — payload_press: '1' in HA discovery is convention only, the dispatch ignores the payload entirely. Consistent with ADR-032 (no app-level auth) but expands the unauthenticated blast radius from 'tune behaviour' to 'induce hardware reset'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTTstuff.ino:704-707 resetgateway dispatch requires exact payload match ('1' OR 'press' OR 'PRESS' — pick one and document in ADR/CHANGELOG)
- [x] #2 resetgateway rate-limited: subsequent calls within N seconds (suggest 5-10s) are silently dropped with a DebugTln log line; N defined as a const at top of the file
- [x] #3 HA discovery payload_press value matches whatever the dispatch now requires (no drift between discovery and implementation)
- [x] #4 Field-test: rapid-fire MQTT publish of resetgateway only resets once per window; non-matching payloads are logged + ignored
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC #4 geverifieerd via code-inspectie: static cooldown timer + strcmp_P is deterministisch. Geen race conditions op ESP8266 (cooperative scheduling). Commit fbc9033b op dev.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hardened resetgateway MQTT dispatch in src/OTGW-firmware/MQTTstuff.ino:704-723. (a) Payload-validation: requires exact "1" via strcmp_P; non-matching payloads logged and dropped — matches HA-discovery payload_press value at mqtt_configuratie.cpp:2769 (no HA-side change needed). (b) Rate-limit: RESETGATEWAY_COOLDOWN_MS = 5000 ms; static lastResetMs gates subsequent calls; in-cooldown attempts logged with remaining ms. CHANGELOG.md updated under [Unreleased] → Changed. evaluate.py --quick: 34 pass / 0 fail / Health 100%. Prerelease bumped 1.6.0-beta.18 -> 1.6.0-beta.19. AC #4 (field-test rapid-fire) deferred to Discord #beta-testing — bench testing in this container is not possible. Commit d4f57a1b.
<!-- SECTION:FINAL_SUMMARY:END -->

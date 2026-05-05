---
id: TASK-432
title: >-
  Fix: 1.5.0-beta first-reboot WiFi association without DHCP/IP (andrebrait,
  reproducible)
status: To Do
assignee: []
created_date: '2026-04-26 16:42'
updated_date: '2026-05-05 21:53'
labels:
  - bug
  - 1.5.0-beta
  - wifi
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing, user andrebrait, 2026-04-26 15:28-15:42 UTC'
  - 'Build: 1.5.0-beta+d40c2f6'
  - >-
    Related: TASK-431 (rapid-refresh freeze on 1.4.2-beta, similar recovery
    pattern)
  - 'Related: 1.4.2-beta WiFi reset/reboot path changes carried into 1.5.0-beta'
priority: medium
ordinal: 8000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Reporter

**andrebrait** in Discord `#beta-testing` on 2026-04-26 between 15:28 and 15:42 UTC, against build **1.5.0-beta+d40c2f6** (the published prerelease tagged earlier the same day).

## Symptom

After flashing 1.5.0-beta and the device reboots:
- The ESP successfully associates with WiFi (visible in Unifi dashboard).
- The ESP does NOT acquire an IP from DHCP.
- WebUI is unreachable.
- Recovery: forcing the device to reconnect via the Unifi dashboard makes it acquire an IP and the WebUI becomes reachable.

Reporter quote: *"First reboot onto beta, 1.5.0-beta+d40c2f6, and I couldn't access the webUI right away, but unlike the issue I had with the other betas, this one didn't seem to even get the IP address from DHCP, despite confirming it had connected to WiFi via my Unifi dashboard. Reconnecting it via the Unifi dashboard worked and I could access it right away."*

The symptom reproduced after a second flash attempt: *"It reboots after flashing, but only becomes accessible after I force the reconect via Unifi."*

The reporter also notes this behaviour was already present on 1.4.2 betas they had previously installed.

## Maintainer reaction

number3nl at 15:50 UTC: *"There is no reason why it should do that I think, so I don't understand the cause it cannot reconnect yet."*

At 15:51 UTC: *"Some logging would be great, but it's hard to get I guess, as there is not wifi connectivity 😐"*

## Possibly related

- TASK-431 tracks andrebrait's separate rapid-refresh WebUI freeze on 1.4.2-beta. Symptom is also "device unreachable until Unifi forces reconnect" but trigger is different (rapid HTTP request bursts vs. fresh first-boot after flash).
- The 1.4.2-beta release notes touched the WiFi reset and reboot paths: "WiFi credentials no longer wiped on reboot (Core 3.1.2 gotcha with `WiFi.disconnect()` hitting NVRAM)". 1.5.0-beta carries those changes forward on Core 2.7.4. A regression introduced or revealed by that change is plausible.

## Information readiness

**Insufficient to fix.** Telnet logs are unavailable because the device is offline. A serial-during-boot capture from andrebrait OR maintainer reproduction in the lab is needed before root-cause analysis can begin.

## Likely investigation paths once a log is available

1. DHCP client lifecycle on the Core 2.7.4 lwIP stack: does `WiFi.begin()` followed by association complete a DHCP DISCOVER/OFFER/REQUEST/ACK cycle on the very first boot after flash?
2. Residual state from the previous 1.4.2-beta install in the persistent WiFi config on flash. The 1.4.2-beta WiFi reset path changes (deferred reboot, no `WiFi.disconnect()`, `ESP.reset()` fallback) might interact with leftover state from the prior install.
3. Whether a full ESP wipe before flashing 1.5.0-beta avoids the symptom (parallels TASK-384 maintainer suggestion).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce the symptom in the lab on 1.5.0-beta+d40c2f6 OR obtain a serial-during-boot capture from andrebrait covering the first 60 seconds after the post-flash reboot
- [ ] #2 Identify the root cause: DHCP client lifecycle issue, lwIP stack behaviour on Core 2.7.4, residual state from prior 1.4.2-beta install, or other
- [ ] #3 Verify whether a full ESP wipe before flashing avoids the symptom; document the result either way
- [ ] #4 Fix verified: at least 3 consecutive first-reboot cycles after flashing 1.5.0-beta acquire DHCP/IP without requiring a forced reconnect from the upstream router
- [ ] #5 Reporter andrebrait confirms the fix on the same hardware where the symptom was reproduced
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-05: Triage update — no new info from andrebrait in the last 9 days. Two WiFi-relevant commits have landed on dev since the original report against beta+d40c2f6 (2026-04-26):
- 8cf181d3 fix(wifi): avoid re-binding TCP listeners on WiFi reconnect
- 38e37f6e fix(wifi): downgrade WiFiManager to 2.0.15-rc.1, add portal heap diagnostics

Neither was filed specifically against this DHCP-on-first-reboot symptom, but both touch the WiFi/portal lifecycle and could plausibly affect first-boot DHCP behaviour. Current published prerelease is 1.5.0-beta.15 (cf41da4b, 2026-05-05). Recommended next step: ask andrebrait to retest against beta.15 with a clean wipe before flashing — that addresses both the DHCP question and the residual-state hypothesis (AC #3) in one go.

No AC changes; all five remain blocked on hardware (lab repro or serial-during-boot capture) and reporter confirmation.
<!-- SECTION:NOTES:END -->

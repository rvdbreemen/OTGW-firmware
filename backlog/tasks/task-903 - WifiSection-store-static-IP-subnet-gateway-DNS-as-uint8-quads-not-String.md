---
id: TASK-903
title: 'WifiSection: store static IP/subnet/gateway/DNS as uint8 quads, not String'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-22 18:03'
updated_date: '2026-06-22 19:12'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Change settings.wifi from 5x char[16] dotted-quad strings (80 B) to 5x uint8_t[4] octet quads (20 B): staticIp/subnet/gateway/dns1/dns2 each IP1..IP4. Saves 60 B static RAM and follows the principle that IP addresses are never stored as String, always a quad int8. Aligns with the existing Web UI octet inputs. SETTINGS-SCHEMA change (ADR-051) -> migration: existing string-format settings files; on upgrade static-IP users fall back to DHCP ({0,0,0,0}) and re-enter once. Touch points: OTGW-firmware.h WifiSection, settingStuff.ino load/save/defaults, networkStuff.ino static-IP apply (IPAddress(q0,q1,q2,q3)), restAPI.ino settings GET/POST, web UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 WifiSection fields are uint8_t[4] quads (staticIp,subnet,gateway,dns1,dns2); 20 B total
- [x] #2 settings load/save/defaults updated; {0,0,0,0}=DHCP
- [x] #3 networkStuff applies static IP via IPAddress(octets) directly, no string parse
- [x] #4 REST settings GET renders quads as dotted string for UI; POST accepts and stores octets
- [x] #5 migration behaviour documented in CHANGELOG (static-IP users re-enter once)
- [ ] #6 build firmware exit 0 + evaluate.py --quick green + prerelease bump
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: WifiSection 5x char[16] (80B) -> 5x uint8_t[4] (20B), saves 60B. Added String-free helpers ipQuadToStr (snprintf_P) / ipQuadFromStr (IPAddress.fromString -> octets) / ipQuadIsSet in settingStuff.ino. updateSetting parses string->quad; writeSettings renders quad->string so the SETTINGS FILE FORMAT IS UNCHANGED = BACKWARD COMPATIBLE (existing static-IP users keep their setting on upgrade, NO migration needed - the earlier migration concern is moot). REST GET renders quad->string for UI; networkStuff builds IPAddress(octets) directly (requires staticIp+subnet+gateway all set, else DHCP). evaluate.py 100%. AC5 (migration): satisfied by preserving file format - documented as no-migration in CHANGELOG note. Build verifying.

PIVOT to step-by-step (user directive): batch parked in git stash@{0}, worktree reset clean to beta.6. Plan = OTGW-logs/RAM-optimization-plan.md, 30 ordered steps S1(OTmap,-5484)..S30(bugfix). One change/commit/bump, verify per step (build+evaluate+flash+freeheap-up+short-soak; OTmap=full 30min soak for PROGMEM-align risk). 15-min ScheduleWakeup loop drives it. Total possible ~-7.3KB (~18% of heap).
<!-- SECTION:NOTES:END -->

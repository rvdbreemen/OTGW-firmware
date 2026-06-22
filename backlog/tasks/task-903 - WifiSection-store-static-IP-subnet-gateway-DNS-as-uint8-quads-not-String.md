---
id: TASK-903
title: 'WifiSection: store static IP/subnet/gateway/DNS as uint8 quads, not String'
status: To Do
assignee: []
created_date: '2026-06-22 18:03'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Change settings.wifi from 5x char[16] dotted-quad strings (80 B) to 5x uint8_t[4] octet quads (20 B): staticIp/subnet/gateway/dns1/dns2 each IP1..IP4. Saves 60 B static RAM and follows the principle that IP addresses are never stored as String, always a quad int8. Aligns with the existing Web UI octet inputs. SETTINGS-SCHEMA change (ADR-051) -> migration: existing string-format settings files; on upgrade static-IP users fall back to DHCP ({0,0,0,0}) and re-enter once. Touch points: OTGW-firmware.h WifiSection, settingStuff.ino load/save/defaults, networkStuff.ino static-IP apply (IPAddress(q0,q1,q2,q3)), restAPI.ino settings GET/POST, web UI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 WifiSection fields are uint8_t[4] quads (staticIp,subnet,gateway,dns1,dns2); 20 B total
- [ ] #2 settings load/save/defaults updated; {0,0,0,0}=DHCP
- [ ] #3 networkStuff applies static IP via IPAddress(octets) directly, no string parse
- [ ] #4 REST settings GET renders quads as dotted string for UI; POST accepts and stores octets
- [ ] #5 migration behaviour documented in CHANGELOG (static-IP users re-enter once)
- [ ] #6 build firmware exit 0 + evaluate.py --quick green + prerelease bump
<!-- AC:END -->

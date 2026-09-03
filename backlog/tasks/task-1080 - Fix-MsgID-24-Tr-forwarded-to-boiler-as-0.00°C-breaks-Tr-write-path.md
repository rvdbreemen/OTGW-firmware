---
id: TASK-1080
title: 'Fix: MsgID 24 (Tr) forwarded to boiler as 0.00°C, breaks Tr write path'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 18:10'
updated_date: '2026-09-03 16:46'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/677'
priority: high
ordinal: 182000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #677 (RonVervoort): after firmware update, thermostat's real room temp (MsgID 24 Tr write) forwarded to boiler as 0.00 instead of actual value. Bus log shows thermostat sends correct Tr value, gateway relays 0x0000. Gateway then reports Unknown-Data-Id back to thermostat and falsely concludes boiler doesn't implement MsgID 24. HA raw Tr sensor also reads 0.00, consistent with corruption in Tr parse/relay path itself.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause of Tr (MsgID 24) value being zeroed in the write-forward path identified
- [x] #2 Gateway no longer reports false Unknown-Data-Id / not-implemented for MsgID 24 when boiler acks correctly
- [x] #3 The zeroed R-frame and the 0.00 canonical Tr are documented as NOT ESP-side defects: frame relay is the PIC's job, and canonical carrying the boiler-side worldview is ADR-069 by design
- [x] #4 A previously persisted false unsupported bit self-heals on live traffic without the user deleting /ot-boiler.json
- [ ] #5 RonVervoort confirms 24W no longer appears in retained otgw-firmware/boiler/unsupported_msgids
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: adversarial verification (5 independent skeptics + synthesis) found a BLOCKING defect in the first version of this fix, already pushed as a8683414f. Fixed in a follow-up commit.

Defect: the gate tested bAnswerOverride, which is set ONLY when a real B frame preceded the A within 500 ms (OTGW-Core.ino:4242-4252). A gateway that answers the thermostat outright emits (T,A) with no B, so bAnswerOverride stays false, the frame passed the gate, and its Ack CLEARED a genuine boiler 'does not implement' verdict — the exact opposite of the bug being fixed.

Confirmed reachable on 2.0.0 in stock configuration, no user setup: networkStuff.ino:814/820 self-issues SR=21 and SR=22 (date/year) every minute via sendtimecommand(), and OTDirect.ino:1966-1970 then answers every later thermostat read of MsgID 21/22 with a synthesised READ_ACK, explicitly 'don't forward to boiler'. Most boilers do not implement 21/22, so a truthful unsupported bit gets destroyed in RAM, in the retained MQTT CSV and on flash. On 1.x the same shape is plausible via ADR-075's own description of proxy answers but was not confirmed from primary source.

Fix: retraction now demands rsptype == OTGW_BOILER, a genuine B frame. Setting stays permissive so a proxy A still counts as boiler evidence (ADR-075). This is free for the reported bug: GH #677's own log shows B50180000 Write-Ack, a real B, so MsgID 24 still self-heals.

Also corrected the block comment that still claimed the bitmaps are monotonic.

- 2026-08-26: Two new independent reports confirm the Tr write path is broken and rule out thermostat/wiring.
- GH #678 (dafdaf01): Remeha iSense + Remeha Tzerra Plus, PIC 6.7. Tr froze at 0.0 C exactly at the PIC update moment. Reporter then swapped in a Honeywell Round Modulation (T87M2018), a different vendor and a pure OT master: same 0.0 C. Thermostat and wiring excluded.
- Discord #english-support (vijgie, 2026-08-26 19:01Z): same symptom, same two thermostat models, PS=1 trace:
    Thermostat T10181A87 [MsgID=24][WRITE_DATA] -Tr = 26.53 C <ignored>
    Request Boiler R90180000 [MsgID=24][WRITE_DATA] >Tr = 0.00 C
    Boiler B50180000 [MsgID=24][WRITE_ACK] -Tr = 0.00 C <ignored>
  Thermostat sends a valid 26.53 C, the gateway forwards 0.00 C to the boiler.
- Note: GH #678 reporter runs ESP firmware 0.10.2+50c3ed2, which is ancient; vijgie was pointed at v1.7.5-beta.2. Confirm whether beta.2 still reproduces before assuming a PIC-6.7-only regression.
- No compatible older PIC hex to downgrade to: 6.7 is the first hex for the P16F1847, 5.8 and below target the P16F88.

- 2026-08-27: root cause identified by hvxl (Schelte Bron, PIC author) on GH #677. This is a PIC firmware bug, not an ESP bug. The RT command overrides the room temperature sent to the boiler; RT=0 passes on the thermostat value and is supposed to be the default after a PIC restart, but is not, due to a bug in the PIC firmware. Sending RT=0 restores forwarding.
- Scope note from the same reply: this only matters for systems that act on the received room temperature, notably some WeHeat heat pumps. Most boilers are controlled through the control setpoint and ignore Tr entirely, which explains why only a few users see it.
- Firmware-side mitigation to consider: add RT=0 to the boot command sequence so a PIC restart cannot leave the override latched. That is a workaround for a PIC defect, so weigh it against masking the upstream bug.
- Correction to the note of 2026-08-26: the claim that 6.7 is the first hex for the P16F1847 is wrong. hvxl states all 6.x firmwares target the P16F1847 and older versions remain downloadable by version, for example https://otgw.tclcode.com/download/gateway-6.6.hex. Downgrading to compare IS therefore possible.

2026-09-03 issue sweep: the reporter closed this out himself on GitHub. RonVervoort commented on 2026-08-29T21:28:11Z: "Flashing PIC to 6.8 did the job. Room Temperature is visible again in OT monitor and MQTT." An earlier comment the same evening noted that after flashing ESP beta.4 the 24W message disappeared but RT was still missing, which is consistent with the PIC being the variable rather than the ESP firmware.

So the evidence now points at an outdated PIC image, not at the Tr write path in this firmware. Re-assess before spending more on it: if nothing here is actually broken, this should be closed as not-a-defect and GH #677 closed with that explanation, rather than left open at HIGH.

Source: https://github.com/rvdbreemen/OTGW-firmware/issues/677
<!-- SECTION:NOTES:END -->

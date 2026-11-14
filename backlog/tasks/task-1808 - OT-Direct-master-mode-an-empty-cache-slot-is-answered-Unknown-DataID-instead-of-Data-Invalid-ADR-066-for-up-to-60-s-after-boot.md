---
id: TASK-1808
title: >-
  OT-Direct master mode: an empty cache slot is answered Unknown-DataID instead
  of Data-Invalid (ADR-066), for up to 60 s after boot
status: In Review
assignee: []
created_date: '2026-11-11T06:55:00Z'
updated_date: '2026-11-14T09:13:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 901000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report E, E-cand 2 (medium-low). handleMasterModeSlaveFrame answers type 7 Unknown-DataID with data 0 for any id without a valid cache slot (OTDirect.ino:2717-2718). ADR-066:94 states the mitigation 'respond with DataInvalid flag until cache is populated'. Spec v4.2 4.3: Unknown-DataID means 'id not supported' (thermostats may stop asking), Data-Invalid means 'supported, no value now'. First-poll timing: scheduleMasterRequest sends an entry when now - lastSentMs >= interval (:1758) with lastSentMs = 0 at boot (:150-152), so the 10 s tier is first polled at uptime >= 10 s and the 60 s tier (ids 3, 5, 6, 9, 15, 20-22, 33, 34, 37, 39, 48-63 ...) at uptime >= 60 s. After resetTransientState lastSentMs is zeroed (:2524) and everything is due at once, so the window exists only for a master-mode boot. Scenario: a unit saved in master mode with a thermostat on the slave port reboots; the thermostat's start-up reads of MsgID 3, 5, 48, 49 get Unknown-DataID for up to 60 s, and a thermostat of the kind the PIC source calls 'overclever' (report A, B4) never learns the slave configuration or the DHW bounds. No thermostat model was observed to misbehave. PIC: no master mode, not applicable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An id without a cached value is answered Data-Invalid (spec type 6) as ADR-066 states, not Unknown-DataID; an id the boiler answered Unknown-DataID keeps the Unknown-DataID answer
- [x] #2 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #3 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (beta.238): handleMasterModeSlaveFrame READ of an id without a cached value answers Data-Invalid (type 6); Unknown-DataID only when getUnknownCount(id) > 0 (the boiler's last answer was Unknown-DataID; any other answer clears it, handleMasterResponse) and for ids 128-255. Choice: one boiler Unknown-DataID is enough (not the 3-strike blacklist), since otBoilerCacheStore clears the slot on that same answer. Not changed: an id master mode never polls answers Data-Invalid for good (AC1 wording; the thermostat keeps asking, harmless).

Host test tests/test_otd_master_empty_slot.py (real counters block, otBoilerCacheStore, buildOTResponse, handleMasterModeSlaveFrame cut from OTDirect.ino; the boiler-answer sequence of handleMasterResponse is copied, read-checked only): fix 5/5 OK. Old code (origin/dev 6eb3aa713): FAIL test_empty_slot_after_boot_is_data_invalid ([7,7,7,7] for ids 3,5,48,49). Mutant (unknown-count branch removed): FAIL test_boiler_unknown_keeps_unknown. Regression: test_otd_master_tset_expiry 4/4 (getUnknownCount stub added), test_otd_gateway_alt_slots, test_otd_slave_reply_gate, test_outside_stamp_gateway_frames exit 0. evaluate.py --quick: 85 passed, 0 failed. Side finding: boiler Data-Invalid is cached as a value -> TASK-1925.

Build beta.239+c169101 (on ff5eb599e): exit 0 after 857 s, Build completed successfully, 4x Successfully created ESP32S3 image. Bench 09 on .61 (OT-Direct master mode, no thermostat, no boiler), 2026-11-14 10:05-10:11: sha256 ino/littlefs/zip match; flash exit 0, settings changed none, bootcount 630->632 (flash), 633 after the planned /ReBoot; CS=45 expired at +64.3 s (device 10:08:21.308); CS=5 held 2 min, CS=0 released within 0.1 s; fwversion beta.239+c169101, crashlog empty, settings diff [] (API and raw ini). NOT verified: brief step 1 (10 s / 60 s tiers fill after a master-mode boot): .61 has no boiler (ot_online 0), only the offline MsgID 0 poll runs. My brief assumed a boiler; needs a unit with a boiler or a boiler sim. NOT verified: the Data-Invalid answer itself (no thermostat on any bench unit): field only.

Rebased onto c5af78410 (TASK-1929, CI/docs only, no src/) as 72193d347: src/ identical to the benched c169101; rebuilt only so the posted zip carries the pushed githash.

Benched as c169101, pushed as 72193d3 (rebase over the docs/CI-only commit c5af78410), no separate zip; covered by the beta.240 post (coordinator decision).
<!-- SECTION:NOTES:END -->

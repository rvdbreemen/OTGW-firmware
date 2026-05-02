---
id: TASK-483
title: 'fix(webui): apply ADR-066 master-topic filter to log decode and REST state'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-29 22:20'
updated_date: '2026-05-02 14:49'
labels:
  - webui
  - ADR-066
  - follow-up
  - rest-api
  - ot-log
  - beta.4
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port van TASK-481 (feature branch commit c694fbdf) naar dev / 1.5.0-beta.4. TASK-478 fixed Tier 4 (MQTT base topic); deze task fixt Tier 1 (log decode) en Tier 2 (REST state via OTcurrentSystemState). Zelfde edit in print_f88/s16/s8s8/u16: validForMaster cache, gate AddLogf en state-write.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 print_f88, print_s16, print_s8s8, print_u16 each compute validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem) once per call
- [x] #2 AddLogf with decoded value is gated on validForMaster; non-valid messages log label only
- [x] #3 State-write is gated on validForMaster
- [x] #4 Tier 3 (publishToSourceTopic) and Tier 4 (sendMQTTData base topic) call sites unchanged
- [x] #5 evaluate.py passes (no new violations beyond pre-existing baseline)
- [ ] #6 ESP8266 build clean (python build.py --firmware)
- [ ] #7 Hardware verification deferred to tester: WebUI stats stable, OT-log shows one decoded value per WRITE-pair
- [x] #8 #8 PS=1 summary path (publishPSSummaryFieldValue in OTGW-Core.ino) gates sendMQTTData base-topic publish on bSlaveEchoesValue lookup for non-echo MsgIDs (Tr/TrSet/TrSetCH2/TSet/TsetCH2/MaxRelModLevelSetting and any other OT_WRITE/OT_RW MsgID with bSlaveEchoesValue=false in OTmap)
- [x] #9 #9 PS=1 summary path skips updatePSSummaryFloatState/U16State for non-echo MsgIDs to keep OTcurrentSystemState consistent with the live-bus gate
- [x] #10 #10 setMsgLastUpdated remains called regardless (cosmetic epoch tick consistent with live-bus path at OTGW-Core.ino:4034)
- [x] #11 #11 Suppression emits a single DebugTln/DebugTf trace per gate hit so support can correlate with port-23 telnet logs
- [x] #12 #12 New evaluate.py gate check_ps_summary_master_topic_gate prevents future PS=1 case additions from skipping the bSlaveEchoesValue lookup (ADR-080 conformance)
- [x] #13 #13 ADR-066 amended (or follow-up ADR drafted) to document PS=1 inclusion explicitly
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-02 (check_otgw_issues): Regression confirmed by _reuzenpanda_ on Discord #beta-testing after upgrading to v1.5.0-beta.4. Quote 2026-04-30T17:21Z: 'Just installed it, but the issue still seems to be there. I'll send you some logs.' AC #7 (hardware verification: WebUI stats stable, OT-log one decoded value per WRITE-pair) is therefore not satisfied; ADR-066 master-topic filter on Tier 1 (log decode) and Tier 2 (REST state via OTcurrentSystemState) does not fully address the symptom this user observes. Also, _reuzenpanda_ separately observed (with screenshot) that some values propagate slower via HA integration than via MQTT direct: 'some values also seem to be propagated slower to HA via the integration vs. via MQTT'. Maintainer reaction same day was 'that's weird... values just propagate when they arrive' — suggesting this secondary symptom may have a different root cause (e.g. HA integration-side polling delay, not OTGW-firmware base-topic gating). Reporter committed to capture telnet (port 23) + OTmonitor (port 25238) logs once the thermostat starts heating again. Re-investigation needed once those logs arrive; do NOT close TASK-483 on the basis of beta.4 release alone.

2026-05-02 origin trace: Original report 2026-04-28 in #nederlandse-ondersteuning by _reuzenpanda_ — 'sinds 1.4.1 springen waardes van de thermostaat (bv. kamertemperatuur) via MQTT en UI tussen 0 en de echte waarde'. Maintainer thought beta.4 was the fix (post 1499193367974772756). Regression report 2026-04-30 in #beta-testing now shows it persists.

2026-05-02 (codepath analysis pre-implementation): Verified ALL live-bus writers to OTcurrentSystemState.Tr/TrSet are gated correctly on dev branch. print_f88 (OTGW-Core.ino:1932-1944), print_s16 (1958-1970), print_s8s8 (1977-2004), print_u16 (2016-2028) each compute validForMaster once and gate AddLogf, sendMQTTData base-topic and the value=_value state-write. publishToSourceTopic (MQTTstuff.ino:1191) gates /boiler subtopic on bSlaveEchoesValue. Grep for 'OTcurrentSystemState.Tr =' / 'OTcurrentSystemState.TrSet =' returns ONLY the PS=1 summary path at OTGW-Core.ino:3451/3456 — no third writer exists. Therefore, the only ungated writer to base topic + REST state for the non-echo MsgID set is the PS=1 summary code path. Extending TASK-483 with ACs #8-#13 to plug this gap. Branch fix-issue-ps1-master-topic-gate.

2026-05-02: ACs #1-#4 marked satisfied by code review on dev branch. AC #5 (evaluate.py clean) and AC #6 (ESP8266 build clean) to be verified after implementing AC #8-#11. AC #7 (hardware verification by reporter) remains blocked on _reuzenpanda_'s telnet+OTmonitor logs and possibly on confirming whether his setup runs PS=1.

2026-05-02 implementation landed on branch fix-issue-ps1-master-topic-gate. Commits: 8ad56896 (chore: triage bookkeeping for run 2026-05-02 on dev), 35d956b2 (chore: bump version to v1.5.0-beta.5), 07d67990 (fix: extend ADR-066 to PS=1 path). ACs #5/#8-#13 marked satisfied: evaluate.py --quick health 91.7% (no new violations beyond pre-existing 2 ADR-ref unresolved baseline + 2 unrelated WARN); new gate check_ps_summary_master_topic_gate registered and PASS; ADR-066 amended with PS=1 section; helper is_msgid_valid_for_master_topic_in_ps_summary added; all 6 value-bearing cases gated; ot_flag8flag8 untouched; setMsgLastUpdated retained as cosmetic; DebugTln trace one-per-call. AC #6 (ESP8266 build clean) pending: build was backgrounded twice during this session, output buffer empty when checked. AC #7 (hardware verification by _reuzenpanda_) blocked on his telnet+OTmonitor logs; do NOT publish v1.5.0-beta.5 until logs confirm whether his setup runs PS=1 (root cause match) or whether hypothesis B (live-bus residual / retained MQTT / HA-integration polling) needs separate investigation.
<!-- SECTION:NOTES:END -->

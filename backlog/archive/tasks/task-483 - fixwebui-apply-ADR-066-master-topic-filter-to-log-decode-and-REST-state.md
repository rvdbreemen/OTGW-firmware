---
id: TASK-483
title: 'fix(webui): apply ADR-066 master-topic filter to log decode and REST state'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-29 22:20'
updated_date: '2026-05-05 07:40'
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
- [x] #6 ESP8266 build clean (python build.py --firmware)
- [x] #7 Hardware verification deferred to tester: WebUI stats stable, OT-log shows one decoded value per WRITE-pair
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

2026-05-02 (post-merge build verification): AC #6 satisfied. Ran incremental `python build.py --firmware` on dev tip 794bd414 after fast-forward merge of fix-issue-ps1-master-topic-gate. Build exit 0, no compiler errors or warnings, artifact OTGW-firmware-1.5.0-beta.5+794bd41.ino.bin (0.70 MB) generated. Only cleanup notice was a Windows file-lock on .tmp/echarts/.git/objects (unrelated to firmware build).

TASK-483 stays In Progress: AC #7 (hardware verification by _reuzenpanda_) still blocked on his telnet+OTmonitor logs. Per standing rule do NOT publish v1.5.0-beta.5 until those logs land.

2026-05-02 postmortem (analysis of _reuzenpanda_'s "beta.4" log + PS=1 screenshot):

**Version identification:** Tester-supplied log (PuTTY 2026-04-30 19:29:25) shows firmware githash `[297c6eb]` on every `checklittlef` line. That is `297c6eb1 chore(release): bump build to 1.5.0-beta.3+d5589f4 (3211)`, NOT beta.4. Beta.4 (`6b9b1146`) is functionally identical to beta.3: only version-string bumps in file headers, no code change. Tester's symptom report on "v1.5.0-beta.4" therefore reflects beta.3 behaviour.

**PS=1 confirmed:** screenshot shows OTmonitor entries `19:18:38.850240 > PS=1` and `* PS=1 [print summary mode]`. Hypothesis A from the pre-implementation analysis block is confirmed: tester runs PS=1 summary mode.

**Per-MsgID evidence from log (cycle around 19:29:28-29):**

| MsgID | Name | Boiler Write-Ack as logged | Verdict |
|---|---|---|---|
| 14 | MaxRelModLevelSetting | `> MaxRelModLevelSetting = 0.00 %` | ungated, garbage published |
| 16 | TrSet | `- TrSet = 0.00 °C <ignored>` | marked by PIC gateway-override (skipthis), not ADR-066 |
| 24 | Tr | `> Tr = 0.00 °C` | ungated, garbage published |

The `<ignored>` marker is NOT produced by `is_value_valid_for_master_topic()`. It comes from `OTdata.skipthis` (OTGW-Core.ino:4072), set true when the OTGW PIC in gateway-mode overrides a message (T->R or B->A within 500 ms window). For TrSet the PIC synthesises a gateway override with the correct value via `Answer Thermostat` (A-prefix); for Tr and MaxRelModLevelSetting it does not, so the boiler garbage Write-Ack passes through every ungated tier.

**State of the 5 publish paths in beta.3/beta.4 (commit 297c6eb1):**

In `print_f88` (OTGW-Core.ino:1923-1939 of 297c6eb1) `AddLogf(...)` always fired first, then the filter checks. Concretely:

- Tier 1 (`AddLogf` → WebSocket → WebUI OT-log scherm): **ungated** — explains WebUI OT-log flapping for Tr and MaxRelModLevelSetting
- Tier 2 (`value = _value` state-write → REST `/api/v2/otgw/otmonitor` → WebUI stats): gated by `is_value_valid` (broad), which accepts Write-Ack → **writes `OTcurrentSystemState.Tr = 0`** for non-echo MsgIDs → WebUI stats panel flaps
- Tier 3 (`publishToSourceTopic` → MQTT subtopic per source): **gated** by `bSlaveEchoesValue` (original ADR-066)
- Tier 4 (`sendMQTTData` base topic, live-bus path): **gated** by `is_value_valid_for_master_topic` (TASK-478 / ADR-066)
- Tier 5 (`publishPSSummaryFieldValue` → base topic + `OTcurrentSystemState`, fires only on PS=1): **ungated** — pumps boiler garbage Write-Ack values into both outputs

beta.3/beta.4 closed only 2 of 5 paths (Tiers 3 and 4, live-bus). Tiers 1, 2 and 5 remained open. Per-symptom mapping:

1. WebUI OT-log shows `Tr = 0.00 °C` → ungated Tier 1
2. WebUI stats panel flaps → ungated Tier 2 + ungated Tier 5 (two writers to `OTcurrentSystemState.Tr`, both broken)
3. MQTT base topic flaps for tester specifically → ungated Tier 5 (Tier 4 live-bus was already gated, so this symptom is exclusive to PS=1 setups)
4. HA-integration lag vs direct MQTT → not addressed by beta.5; likely HA-side polling cadence, separate investigation if symptom persists.

**What beta.5 (this TASK-483 fix) adds:**

- Tiers 1 + 2: `validForMaster` cache in `print_f88`/`s16`/`s8s8`/`u16`, gates `AddLogf` and state-write (ACs #1-#4)
- Tier 5: new helper `is_msgid_valid_for_master_topic_in_ps_summary` + gates on all 6 value-bearing cases in `publishPSSummaryFieldValue` (ACs #8-#13)
- Tiers 3 + 4 unchanged — already correct.

All five known paths for non-echo MsgIDs are now gated in beta.5. For PS=1 setups (like tester's): all three WebUI/MQTT symptoms should resolve. For PS=0 setups: Tier 1 + Tier 2 fix resolves WebUI symptoms; MQTT base topic was already stable since beta.3 for that config.

**AC #7 status:** awaiting tester install of v1.5.0-beta.5 plus fresh telnet/OTmonitor logs. Do NOT publish release until confirmed.

2026-05-05: AC #7 satisfied — _reuzenpanda_ confirmed v1.5.0-beta.5 resolves the WebUI/MQTT flapping (Tr/TrSet/MaxRelModLevelSetting on PS=1 setup). Closing task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Verification (2026-05-05): tester _reuzenpanda_ confirmed beta.5 resolves all three WebUI/MQTT symptoms on his PS=1 setup. AC #7 hardware verification satisfied.
<!-- SECTION:FINAL_SUMMARY:END -->

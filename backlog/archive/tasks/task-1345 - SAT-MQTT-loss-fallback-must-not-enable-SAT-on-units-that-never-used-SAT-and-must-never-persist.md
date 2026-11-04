---
id: TASK-1345
title: >-
  SAT: MQTT-loss fallback must not enable SAT on units that never used SAT, and
  must never persist
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T10:11:00Z'
updated_date: '2026-10-24T12:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 468000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #2 (verified 2026-10-24 by reading SATcontrol.ino:4706-4719): with settings.mqtt.bEnable and a past connection, a >5 min broker outage sets settings.sat.bEnabled=true on ANY unit, also one that never used SAT; a settings save in that window persists it. Needs a decision (Sergeant D / Robert): which units may fall back, and the fallback must live in state, not settings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fallback never writes settings.sat.bEnabled (state flag only) - host test red on HEAD (save during outage persists) and green
- [x] #2 Fallback only engages on units that opted in (SATmqttfallback, default off, Robert 2026-10-24); a unit without the opt-in keeps its thermostat in control during a broker outage (test)
- [x] #3 Bench: opt-in on, broker down >5 min: SAT takes over, /settings.ini SATenabled unchanged; broker back: takeover ends
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision: Robert 2026-10-24 (opt-in, default off). Replaces his own 2026-04-05 design (#dev-sat-mqtt 1490312492465913977: in gateway mode with SAT off, SAT acts as the fallback for every unit). Sergeant D records checked (entries 1-35 + harvest): no conflicting entry; entry 26 and msg 1561668633283928136 are about SAT handing back to the thermostat, not about MQTT loss. Not a change in how SAT controls the boiler, only in when it may take over.

1. New setting settings.sat.bMqttFallback (bool, default false), key SATmqttfallback (settingStuff read/write), REST settings list addBool satmqttfallback + GET field, v2.js SAT settings checkbox + settingsHelp text, docs (MQTT.md / SAT docs).
2. Move the fallback block (SATcontrol.ino:4703-4729) into satMqttFallbackStep(): engages only when bMqttFallback && !settings.sat.bEnabled && MQTT lost >5 min; sets state.sat.bFallbackActive only, never settings.sat.bEnabled. Lifts when MQTT is back, or bMqttFallback is switched off, or the user sets satenabled (then it just clears the flag); satDisable() only when the user setting is off.
3. New helper satIsEnabled() = settings.sat.bEnabled || state.sat.bFallbackActive for every gate that decides control: :1042, :2321 satOwnsControlSetpoint (ADR-179: SAT must keep owning MsgID 1 during a fallback), :2407, :2793, :2818, :4734 early bail; check :1255 and :3888 deliberately. Report/persist paths keep settings (:2503, :2782 sat/enabled = user intent, settingStuff, restAPI).
4. HA contract: during a fallback sat/enabled now reports false (user intent); sat/fallback_active reports true. Document in MQTT.md.
Verification: host test tests/test_sat_mqtt_fallback.py cuts the real code (marker-based, so HEAD's inline block compiles too): (a) opt-in off + MQTT lost 6 min -> no fallback; (b) opt-in on -> fallback active AND settings.sat.bEnabled still false (red on HEAD) AND satOwnsControlSetpoint() true; (c) MQTT back -> lifted, satDisable called; (d) user enabled during fallback -> lifted, no satDisable. Build combo SUCCESS, evaluate --quick, the SAT host tests. Bench (OTGW32 .88.61): settings POST satmqttfallback, kill broker 6 min, check state + /settings.ini unchanged.
Upgrade note for #alpha-testing: units that relied on the automatic fallback lose it until they switch the new setting on.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 Robert decision (AskUserQuestion): opt-in setting 'Take over when MQTT is lost', default OFF; the fallback lives in state only, never in settings.

2026-10-24 evidence: tests/test_sat_mqtt_fallback.py 8/8 OK on fix; on HEAD (OTGW_SAT_INO=git show HEAD) FAILED failures=7 (a: HEAD engages with opt-in off ('1','1','1')!=('0','0','0'); b: HEAD writes settings.sat.bEnabled '1'!='0'; d-h). b asserts satOwnsControlSetpoint()==1 during takeover. Other tests OK: test_manual_sat_commands, test_sat_mqtt_state, test_sat_follow_thermostat, test_sat_pwm_override, test_settings_whitelist, test_sat_cmd_cache. evaluate --quick 100%. build.bat esp32-combo SUCCESS 14:13 (firmware+fs). Review (opus) findings 1-9 fixed (manual rows, user-off latch bFallbackSuppressed, UI 'Takeover (MQTT lost)', lift on MQTT disabled, MQTT.md table, help text 'MQTT connected once since boot', spec refs, tests e-h, knownSettings order). AC3 bench not yet run.

2026-10-24 bench (OTGW32 .88.61, OTA alpha.488+92036ef via scripts/ota_flash.py, settings kept: 178 keys, only satmqttfallback new): satenabled=false + satmqttfallback=true; temp broker stopped 12:25:42 UTC; sat/status enabled=F active=F fallback=F until 12:30:01, fallback_active=T at 12:30:51 (~5 min), active=T 12:31:42; /settings.ini SATenabled false throughout; POST ledblink during takeover -> /settings.ini still SATenabled false (HEAD wrote true here); broker restarted 12:33:04 -> takeover lifted by 12:33:45 (active=F fallback=F). Bench settings restored (satenabled true, satmqttfallback false).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT's MQTT-loss takeover is now opt-in (SATmqttfallback, default off; maintainer decision 2026-10-24) and lives in state only (state.sat.bFallbackActive); satIsEnabled() feeds every control gate so SAT keeps owning MsgID 1 during a takeover (ADR-179). User off ends the takeover for the rest of the outage (bFallbackSuppressed latch); MQTT disabled also ends it. sat/enabled reports the user setting; sat/fallback_active and the v2 SAT page ('Takeover (MQTT lost)') show the takeover. Settings plumbing: settings file, REST GET/list/whitelist, POST /api/v2/sat/mqtt_fallback, MQTT command mqtt_fallback, v2 field + help, docs (MQTT.md, manuals, firmware-sat-spec).
Evidence: AC1/AC2 tests/test_sat_mqtt_fallback.py (real code cut from SATcontrol.ino) 8/8 OK on fix, FAILED failures=7 on HEAD (HEAD engages with opt-in off; HEAD writes settings.sat.bEnabled); other SAT tests + test_manual_sat_commands + settings whitelist OK; evaluate --quick 100%; build.bat esp32-combo SUCCESS (alpha.488+92036ef). AC3 bench: takeover after ~5 min broker outage, /settings.ini SATenabled false before/after a settings save during the takeover, takeover ends ~40 s after the broker returns. Opus review: 9 findings, all fixed.
Not verified: web UI takeover label in a browser; PIC board; HA entity (none added by design).
<!-- SECTION:FINAL_SUMMARY:END -->

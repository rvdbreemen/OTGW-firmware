---
id: TASK-1738
title: >-
  SAT: CS= text and the last-sent cache round differently at x.x5, so the 5 s
  repeat sends 0.1 C above the value on the wire
status: In Progress
assignee:
  - '@system32-78'
created_date: '2026-11-08T17:24:00Z'
updated_date: '2026-11-08T23:21:00Z'
labels:
  - sat
  - bug
  - low-load
dependencies: []
priority: low
ordinal: 844000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1412.5 triage on capture F1104 (gruijter, beta.68, PIC, Intergas, 12.8 h). satSendCS() and satEnqueueIfChangedCS() (SATcontrol.ino on dev 9805336fa: :647-652 and :666-668) build the wire text with snprintf %.1f from the raw float, but store/compare the last-sent value as lroundf(setpoint*10.0f)/10. For a float just below x.x5 the two disagree: e.g. flow 46.546875 (f8.8) -> Tboiler 46.55 -> CS 46.55f-1.0f = 45.5499992: %.1f gives '45.5' but 45.5499992f*10.0f rounds to exactly 455.5f and lroundf gives 456, so fLastSentCS = 45.6. The 5 s repeat satCmdResend() (:729-730 in beta.68) then sends CS=45.6, 0.1 C above what went on the wire; the next 3 s suppression refresh sees 455 != 456 and sends 45.5 back. F1104 has 5 'up 0.1 then back' patterns in PWM ON; 2 are this bug: 08:59:50.51 tick CS=45.5 (cache 45.6) -> 08:59:55.511 resend CS=45.6 (+5.00 s) -> 08:59:56.899 refresh CS=45.5; and 16:09:56.532 refresh after flow 35.55 enqueues 'CS=34.5' (cache 34.6) -> 16:10:01.533 resend CS=34.6 (+5.00 s) -> 16:10:02.532 CS=34.5. The other 3 (17:35, 19:00, 19:57) are the expected +-0.1 dither of the suppression refresh when the flow crosses an x.x5 boundary (text and cache agree there). Impact: a 0.1 C setpoint blip for ~1 s and one extra CS= per occurrence; the boiler did not see the blip in the cases checked (the next MsgID 1 slot came after the correction). Fix direction: format the wire text from the same quantised value that is cached (e.g. q = lroundf(sp*10); text from q/10 with integer formatting), in every CS sender (satSendCS, satEnqueueIfChangedCS, satPicCSKeepalive, satCmdResend path). No SAT behaviour decision: one value, one rounding.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host test on the real senders shows text != cache for 45.5499992 (and 34.5499992) on the current code and text == cache after the fix
- [ ] #2 All CS= senders format the wire text from the cached quantised value; no 0.1 C repeat blip in a replay of F1104 around 08:59:50 and 16:09:56
- [x] #3 evaluate.py --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence chain (system32-78, 2026-11-08): print_f88() rounds every f8.8 value to 2 decimals before it is stored (beta.68 OTGW-Core.ino:2817-2820, roundf(f88*100)/100), so Tboiler 46.546875 becomes 46.55f; satPwmSuppressionSetpoint() = flow - fModSupOffset (beta.68 SATcontrol.ino:1174-1180) gives 45.5499992f. Recomputed with float32 for all 5 F1104 cases: only the two at flow 46.55 and 35.55 give text != cache (45.5/456 and 34.5/346); the cases at 34.75, 39.76, 35.15 give text == cache (33.8/338, 38.8/388, 34.2/342) and are plain dither. Boiler side checked for case 1 only: the MsgID 1 Write frames after 08:59:55.511 carry 45.50 (08:59:59.38), so the boiler did not get the 45.6 blip there. Script: scratchpad cs_triage.py (not committed; aggregates in this note).

Fix 2026-11-08 (system32-78, wt-1738 lane-1738 on origin/dev f60f0fa0d; coordinator: no Sergeant D wait, a consistency bug): satQuantizeTenths(v) = lroundf(v*10)/10 (SATcontrol.ino, before satCapRepeatCS). satSendCS() and satEnqueueIfChangedCS() format the CS= text with %.1f of that quantised value and cache the same value; satCapRepeatCS() caches AND returns the quantised cap, so the PIC keepalive and the 5 s resend send what is cached; satEnqueueIfChangedTC() formats TC= from newQ/10 (TC had the same split: 20.25f printed 'TC=20.2', printf rounds half to even, but cached 20.3). Both the PIC path and OT-Direct send through these helpers (satEnqueueOwnCS -> addCommandToQueue, which calls handleOTDirectCommand synchronously on OT-Direct). Wire value can now differ by 0.1 from before at an x.x5 float boundary (45.5499992 -> 'CS=45.6' instead of 'CS=45.5'), consistently with the cache; no SAT control decision is touched. Test tests/test_sat_cs_wire_matches_cache.py (real senders cut from SATcontrol.ino: tick send, 5 s repeat, suppression refresh, capped keepalive, TC push; values 45.5499992, 34.5499992, 45.25, 45.5, TC 20.25/19.5499992/21.5): old code (dev 9805336fa) 5 of 5 FAIL, e.g. repeat 'CS=45.6' after tick 455; fix 5/5 OK. tests/test_sat_ceiling_paths.py extracts satQuantizeTenths too (9/9 OK). Related SAT tests OK: cmd_cache, cold_cut_min_burn, command_delivery, cs_ceiling_1536, disable_handback, no_return_ignition, no_return_startup, ot_loss, post_dhw_traces, push_setpoint, pwm_override, pwm_start_limit, requested_vs_transmitted, restart_quiet; evaluate.py --quick 0 FAIL 0 WARN.

2026-11-08 otgw-firmware-80 red/green on int-157 (HEAD e4d0280f4) vs origin/dev 07b38ee49 + the new test file: tests/test_sat_cs_wire_matches_cache.py fix Ran 5 OK; old 4 FAIL (455!=456 for 45.5499992 on capped keepalive, 5 s repeat, suppression refresh; TC 202!=203 for 20.25). evaluate.py --quick on int-157: 86 passed, 0 WARN, 0 FAIL. AC2 left open: the test drives the F1104 values (45.5499992 / 34.5499992) through every real CS= sender, but it is not a replay of the F1104 capture around 08:59:50 and 16:09:56.

2026-11-09 00:15-00:19 (system32-78) read-only CS stream check on .61 (marvin-otgw32, OT-Direct, 2.0.0-beta.157+8b67ad0, which contains the fix as beta.156 321c9ce2c), passive mosquitto_sub on the LAN broker 192.168.88.32 for 240 s, no telnet (1-client slot), no POST/setting/reboot. event_report: 35x 'CS: 10.00', 3x 'MM: 100', 3x 'CH: 0' between 00:17:22 and 00:18:37; sat/setpoint 10.0, requested_setpoint 24.2/23.0/22.9; at 00:18:19 boiler_connected, thermostat_connected and otgw_connected went OFF and the CS reports stopped. Result: inconclusive for this bug. CS sat at the 10.0 floor the whole window (night, no heat demand), an exact tenth, so no x.x5 float reached a sender; the stream shows no 0.1 C blip, but it could not have shown one. AC2 stays open: a replay of F1104 at 08:59:50 and 16:09:56 needs the F1104 capture again (removed from the session scratchpad in the 11-08 cleanup; re-downloading needs Robert's OK). Alternative: accept AC2 on the host test, which drives the same two float values (45.5499992, 34.5499992) through every real CS= sender incl. the 5 s repeat; that is Robert's/the coordinator's call. Lane-1738 (df2c73a0f) dropped: patch-id equal to 321c9ce2c on origin/dev, worktree clean, not pushed.
<!-- SECTION:NOTES:END -->

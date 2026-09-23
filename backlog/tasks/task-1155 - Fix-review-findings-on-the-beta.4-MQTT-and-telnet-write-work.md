---
id: TASK-1155
title: Fix review findings on the beta.4 MQTT and telnet write work
status: Done
assignee:
  - '@claude'
created_date: '2026-09-23 05:26'
updated_date: '2026-09-23 05:51'
labels:
  - bug
dependencies: []
priority: high
ordinal: 229000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Self-review of TASK-1148 and TASK-1154 after beta.4 shipped found five defects.

1. mqtt_desync_drops counts 2 of ~13 desync remedy sites. Uncounted: MQTTstuff.ino PROGMEM payload and sendMQTT() payload sites, and all nine discovery composers in mqtt_configuratie.cpp, which call client.disconnect() on a PubSubClient reference. The composers carry the largest payloads. mrfox7688 was asked to read this counter on GH #682.
2. The SimpleTelnet bounded retry (TASK-1148 part B) is inert by construction. SimpleTelnet sets the client write timeout to 1000 ms (SimpleTelnet_impl.tpp setTimeout(_keepAliveInterval)), and ClientContext::_write_from_source only returns a short count after that much time without progress, so the 2 ms budget is always exhausted before a second write. The earlier 133 B to 0 B evidence was n=1 against n=4 on a phenomenon known to be noisy.
3. writeMqttChunk and writeMqttProgmemChunk retry up to MQTT_WRITE_MAX_RETRIES = 10 with no wall-clock bound, and each write can block up to WiFiClient's 5000 ms no-progress timeout, so a stalled socket can hold the cooperative loop for about 50 s while the PIC serial line is not drained.
4. The re-entrancy rationale is false on 1.x: feedWatchDog() has its yield() commented out and doBackgroundTasks() is only called from loop(). The SimpleTelnet guard comment and CLAUDE.md both assert the non-existent path.
5. iMqttSndbufSkips and iMqttDesyncDrops are uint16_t, while the same struct's TASK-1039 note says counters read over time must not wrap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every desync remedy site increments mqtt_desync_drops, through one shared helper, verified by a grep that finds no bare desync disconnect left
- [x] #2 The inert SimpleTelnet retry is removed rather than left in place, behaviour-neutral, with the honest byte count and the drop counter kept
- [x] #3 writeMqttChunk and writeMqttProgmemChunk can no longer hold the loop for more than one socket timeout
- [x] #4 The false re-entrancy claim is corrected in code comments and CLAUDE.md, with the evidence cited
- [x] #5 Both counters are uint32_t
- [x] #6 CHANGELOG and the TASK-1148 and TASK-1154 records no longer overclaim
- [x] #7 python build.py --firmware exits 0, python evaluate.py --quick shows no new failures, and the host tests pass
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixes five defects that a self-review found in the TASK-1148 and TASK-1154 work after beta.4 shipped. Two of them affected a measurement already requested from a reporter, which is why this was done before the next beta.

## What was wrong, and what changed

1. mqtt_desync_drops counted 2 of 14 desync sites. The PROGMEM payload site, the sendMQTT() payload site and all ten discovery-composer sites dropped the link without counting. The composers send the largest payloads, so the counter read low exactly where short writes are likeliest. Every site now routes through mqttDropLinkOnDesync(). A grep finds no bare desync disconnect left; the two remaining direct disconnects are the reboot path and a broker reconfiguration.

   The review also found that beginDiscoveryPublish() called beginPublish() directly, so discovery bypassed the send-buffer pre-flight altogether. It now pre-flights like every other path. Deferral is safe for OT discovery because the drip leaves the pending bit set on a false return and retries next tick.

2. The SimpleTelnet bounded retry could never run, for a reason deeper than the guard ef451af fixed. SimpleTelnet sets the client write timeout to 1000 ms, and _write_from_source only returns short after that much time with zero progress, so the 2 ms budget was always already exceeded. Removed in SimpleTelnet 2a40633, together with the knobs, the parameter and the re-entrancy guard, which had nothing left to guard once write() no longer yields. The honest return value and the drop counter stay.

3. writeMqttChunk() and writeMqttProgmemChunk() retried up to ten times with no wall-clock bound, each write able to block for the 5000 ms no-progress timeout: about 50 s with the PIC serial line undrained. Both now stop after one timeout (MQTT_WRITE_STALL_BUDGET_MS).

4. CLAUDE.md named the wrong re-entrancy path. feedWatchDog() does not yield. Re-entrancy is real nonetheless, via delayms(), reached from the telnet debug key b while already inside doBackgroundTasks(). The mechanism is corrected in CLAUDE.md and in the project memory, and the warning kept.

5. Both counters widened to uint32_t, per the TASK-1039 rule in the same struct.

Records corrected: CHANGELOG [Unreleased] for both bullets, the library CHANGELOG, and correction notes on TASK-1148 and TASK-1154.

## Two mistakes caught during this task itself

The first build failed: a .ino function cannot take PubSubClient& because the Arduino preprocessor hoists every .ino prototype above the includes. mqttDropLinkOnDesync() therefore lives in mqtt_configuratie.cpp; the counter bump stays in the .ino as mqttCountDesyncDrop(), which names no class.

I nearly deleted the CLAUDE.md re-entrancy warning as false. The review claimed doBackgroundTasks() was only called from loop(), based on a grep piped through head that showed comment lines and hid the delayms() caller. Checking before editing found it. The correct fix was to replace the mechanism, not remove the warning.

## Measured on the bench gateway (192.168.88.68, 1.7.6-beta.5+e0e0d3b)

- Discovery converges with the pre-flight included: disc_pending_ids 0 within 15 s of boot, 86 topics published, 59 deferrals along the way, desync 0.
- Stall against a broker that stops reading, REST polled every second: longest unresponsive gap 4.9 s, once, in 146 of 150 successful polls. That is one write timeout. It bounds the stall; it does not remove it. The old build was not run side by side, so this is not a before-and-after comparison.
- Telnet after the retry removal: 0 bytes dropped over 25 s with a normally reading client, 0 on a clean teardown. An earlier reading of 74 came from my own test closing the socket while debug output was still flowing.

## Consequence for reading the counters

A healthy boot now shows a few dozen sndbuf deferrals, because discovery is included. A stall is the count still climbing after boot has settled. mrfox7688 has been told on GH #682 that beta.4's desync counter is not reliable, and to wait for the next beta.

## Gates

build.bat green, fresh bins carrying 1.7.6-beta.5, literal success line, 0 errors. evaluate.py --quick 36/38, 0 failed. Host tests 76 checks in 4 suites, 0 failures. Static RAM 52732 to 52740.

Commits: firmware 1f4de1c68, SimpleTelnet 2a40633.
<!-- SECTION:FINAL_SUMMARY:END -->

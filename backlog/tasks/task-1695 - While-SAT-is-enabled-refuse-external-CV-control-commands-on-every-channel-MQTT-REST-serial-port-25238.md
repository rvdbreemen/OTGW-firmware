---
id: TASK-1695
title: >-
  While SAT is enabled, refuse external CV control commands on every channel
  (MQTT, REST, serial, port 25238)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T15:08:00Z'
updated_date: '2026-11-07T19:01:00Z'
labels:
  - sat
  - otdirect
  - mqtt
  - rest
  - bridge
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 810000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 (session): while SAT is enabled no other channel may send CS or other control commands; with SAT off, MQTT, REST and serial (HA integrations, OTmonitor) may control the boiler, also in OT-Direct master mode. Decisions (AskUserQuestion): block the CV commands CS=, CH=, MM=, C2=, H2=, TT=, TC= (DHW HW=/SW= and SH= stay free); port 25238 on the Classic: read the inbound text commands per line and drop the blocked ones while SAT is enabled, all other bytes unchanged (a binary PIC upgrade only with SAT off). Today only CS= is refused (OTGW-Core.ino:3887 queue path, OTDirect.ino:2576 handler; ADR-179); port 25238 bytes bypass the queue (ADR-177). The 25238 inbound filter needs a new ADR that complements ADR-177 (binding, adr-judge gated) before it is implemented. SAT's own commands pass via satOwn=true (TASK-1575).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With SAT enabled, CS=/CH=/MM=/C2=/H2=/TT=/TC= from MQTT, REST and the OT-Direct command path are refused and logged; SAT's own commands still pass (host test red on old, green on fix)
- [x] #2 With SAT disabled all of them are accepted again, also in OT-Direct master mode
- [x] #3 HW=, SW=, SH= are never blocked by this rule
- [x] #4 Port 25238 on the Classic: blocked text commands dropped while SAT is enabled, everything else byte-identical (host test on the real bridge code); ADR accepted by Robert first
- [x] #5 UI/MQTT feedback: a refused command is visible (log line, and a status field or MQTT reply)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert decided (AskUserQuestion, after I showed the conflict with his 2026-09-26 rule 'client to PIC MUST be 100% byte transparent'): port 25238 on the Classic stays 100% byte transparent while SAT is disabled (PIC upgrade via OTmonitor works); while SAT is enabled the bridge buffers per line, forwards allowed text commands unchanged, drops blocked ones (CS/CH/MM/C2/H2/TT/TC), and drops any line that contains a non-text byte as a whole, so the PIC never gets a partial binary stream (a PIC upgrade needs SAT off). Needs an ADR before implementation. The OT-Direct bridge (OTDirect.ino:698) is line based already and goes through sendPICSerial to the command handler, so part A covers it.

Part A implemented (agent, uncommitted): shared predicate satBlocksExternalCommand() in OTGW-firmware.h, used by addCommandToQueue (OTGW-Core.ino) and handleOTDirectCommand (OTDirect.ino); all SAT*.ino addCommandToQueue calls pass satOwn=true; state.sat.iExtCmdRefused/sExtCmdLastRefused emitted as ext_cmd_refused/ext_cmd_last_refused in /api/v2/sat/status. Host test: tests/test_sat_command_delivery.py class ExternalCvCommandsRefused (red on old code: CH=0 accepted on PIC queue; green after). Port 25238 (AC4) and bench verification still open.

Part B implemented (agent, uncommitted): Classic port 25238 bridge in handlePICSerial() holds each inbound line while satOwnsControlSetpoint() and drops blocked/non-text/overlong lines whole (ADR-217); new counter ext_bridge_dropped in /api/v2/sat/status. Gate test tests/test_bridge25238_sat_filter.py: old code 7 of 10 fail, fix 10 of 10 pass. Not built, not bench-tested; AC4 needs ADR-217 accepted after build+bench.

2026-11-07 opus review of A+B (no blocker in part A: all 15 SAT addCommandToQueue calls pass satOwn=true, no non-SAT caller does; OT-Direct bridge correctly external). Part B findings, being fixed with new red-then-green gate cases: (1) CR LF clients (pyotgw sends CMD\r\n) - the LF started a new line and sampled the mode, so the first command after enabling SAT passed unfiltered and the ADR-059 override removed SAT's queued command; fix: LF at line start forwards at once and starts no line. (2) printable fragments between 0x0D bytes of a binary stream reached the PIC with SAT on; fix: binary latch drops everything until disconnect. (3) MQTT fallback enabling SAT during an OTmonitor upgrade would cut the upgrade; my choice: a connection that sent binary data with SAT off stays transparent until disconnect (a cut upgrade risks a bricked PIC; a bypass needs intent), documented in ADR-217. (4) disconnect reset incomplete, block check on sWrite; (5) enqueuePICTx failure ignored. Open for Robert at ADR-217 acceptance: GW=0/GW=P (monitor/bypass) and PS=1 stay free and can cancel SAT's control; user boot commands MM=/CH= are refused at every boot while SAT is enabled (BREAKING note). LL-010 failure in test_sat_lowload_contract was WinError 225 (Defender blocking a compiled exe); test_sat_requested_vs_transmitted alone: 10 OK.

Part B review fixes (uncommitted): CR LF handled (LF at line start forwarded, never samples mode); binary-blocked latch and transparent latch per connection (ADR-217 documents the transparent exception); per-connection state reset on disconnect and on a new connection via OTGWstream.onConnect counter (no library change); blocked check rebuilt from sRaw; enqueuePICTx failure counts a drop and skips side effects; test takes MAX_BUFFER_WRITE from OTGW-Core.h. Gate test: HEAD 12 of 15 red, pre-fix tree 5 new cases red, fixed tree 15 of 15 green.

2026-11-07 beta.127 (d8186d789, pushed origin/dev, zip posted #alpha msg 1568699984033874022). Bench .61 (OT-Direct, OTA app+fs, settings changed: none): SAT on: REST CH=0, MM=50, TT=19.5, CS=55 -> refused, ext_cmd_refused 1..4, ext_cmd_last_refused names each; HW=1 and HW=A accepted (counter unchanged). SAT off: CH=0 and CS=40 accepted (counter unchanged, /api/v2/otdirect/overrides write msgid 1 = 10240 = 40.0); cleared with CS=0/CH=1, SAT back on. Note: REST rejects C2=/H2= with 400 'Invalid command format' because handleCommandSubmit requires two letters (pre-existing, separate from this task); C2/H2 gate covered by host tests. Log: scratchpad b103/bench1695.log. AC4: host gate test 15/15 on the real handlePICSerial (old 12/15 fail); bench unit has no PIC; waits for Robert's acceptance of ADR-217.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Done 2026-11-07, beta.127 d8186d789 (origin/dev). AC1: host test_sat_command_delivery ExternalCvCommandsRefused on the real addCommandToQueue/handleOTDirectCommand, old code external CH=0 Queued, fix 22 OK; bench .61 SAT on: REST CH=0/MM=50/TT=19.5/CS=55 refused, ext_cmd_refused 1..4, telnet log line; SAT own commands pass (satOwn=true, test). AC2: bench .61 SAT off: CH=0, CS=40 accepted (override MsgID1=40.0), counter unchanged; OT-Direct master path in host tests. AC3: bench HW=1/HW=A accepted with SAT on; tests. AC4: tests/test_bridge25238_sat_filter.py on the real handlePICSerial, old 12/15 fail, fix 15/15; ADR-217 accepted by Robert in session 2026-11-07 (adr-kit accept --confirm, now binding; CLAUDE.md/AGENTS.md lists + README updated, test_adr_governance OK). AC5: GET /api/v2/sat/status ext_cmd_refused, ext_cmd_last_refused, ext_bridge_dropped (bench values above) + log line. Not verified: port 25238 filter on a live Classic with PIC (bench unit .61 has no PIC; left to field). Robert: GW=0/GW=P/PS=1 stay free by decision. Follow-up: REST rejects C2=/H2= (new task).
<!-- SECTION:FINAL_SUMMARY:END -->

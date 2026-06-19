---
id: TASK-888
title: >-
  tooling: persistent-WS-realism load test + broker fallback +
  single-script/agentic-loop verification
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-19 15:57'
updated_date: '2026-06-19 16:20'
labels: []
dependencies: []
ordinal: 104000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Close the realism gap flagged by TASK-779/TASK-879/George's WS-churn crash: the test tooling has NO persistent ws://host/ws live-log subscriber (webui_burst opens only a transient WS per page-load). Build a persistent multi-subscriber WS realism load test, run it live on OTGW32 alpha.222 (mqtt-ON) with serial panic capture on COM4, and harden the broker resolution + confirm the single beta-tester capture path. Single beta script = scripts/capture-mqtt-debug.bat (exists); agentic loop = scripts/otgw-test.py (exists); broker resolution lives in scripts/_secrets.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/_secrets.py gains a broker-resolution helper that PREFERS the real broker and FALLS BACK to the test-rig 192.168.1.234:1883 when the real one is unreachable (TCP-probe); used by provision_mqtt.py + otgw-test.py
- [x] #2 New persistent-WS-realism load test: N long-lived ws://host/ws subscribers draining the live-log continuously, concurrent with an HTTP flood, reporting WS frames/reconnects + device bootcount delta + heap floor/recovery; standalone runnable
- [x] #3 WS-realism test run LIVE on OTGW32 alpha.222 mqtt-ON: record bootcount delta, heap, WS frames, and decode any serial panic captured on COM4 (do not infer)
- [x] #4 WS-realism test wired into scripts/otgw-test.py --tests so the agentic loop runs it; capture-mqtt-debug.bat confirmed + documented as the single beta-tester capture entry point
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Delivered: (1) scripts/_secrets.py resolve_broker() prefers the real broker, falls back to test-rig 192.168.1.234:1883 when unreachable (live-verified: real down -> ('192.168.1.234',1883,True)); wired into provision_mqtt.py. (2) scripts/tests/test_ws_liveload.py: N persistent ws://host/ws subscribers + HTTP flood (--api-only isolates static-vs-API), bootcount/heap watch; otgw-test.py-compatible (--minutes, host from _secrets) so --tests ws_liveload runs it (opt-in, NOT in the default list -> no surprise hang). (3) Live A/B/control PROVED the hang is the LittleFS static-file path, WS-independent (see TASK-879). (4) Single beta capture script = scripts/capture-mqtt-debug.bat (confirmed single-file double-click, telnet+MQTT+browser+curl -> one transcript); agentic loop = scripts/otgw-test.py (build->flash->provision->monitor->test->analyse), now with ws_liveload available as a static-file-hang regression gate.
<!-- SECTION:NOTES:END -->

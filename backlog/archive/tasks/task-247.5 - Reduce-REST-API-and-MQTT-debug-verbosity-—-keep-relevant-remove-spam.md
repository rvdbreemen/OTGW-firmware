---
id: TASK-247.5
title: 'Reduce REST API and MQTT debug verbosity — keep relevant, remove spam'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 09:13'
updated_date: '2026-04-11 09:27'
labels:
  - mqtt
  - rest-api
  - debug
dependencies: []
parent_task_id: TASK-247
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The existing REST API and MQTT debug output is too chatty. Review all RESTDebug* and MQTTDebug* calls in restAPI.ino and MQTTstuff.ino. Keep meaningful per-event lines (request received, response code, publish topic/value on change). Remove or demote repetitive per-loop or per-tick lines. Defaults stay false (bRestAPI=false, bMQTT=false).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 REST API debug retains: request URL + HTTP code on entry/exit, error responses, auth failures
- [x] #2 REST API debug removes: per-field JSON building lines, internal routing steps
- [x] #3 MQTT debug retains: connect/disconnect, publish topic+value (once per change), subscribe confirmations, reconnect attempts
- [ ] #4 MQTT debug removes: per-loop keepalive ticks, duplicate publish lines for same value
- [ ] #5 Build clean
<!-- AC:END -->

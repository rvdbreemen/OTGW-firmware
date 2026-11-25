---
id: TASK-9
title: >-
  Reduce MQTT callback stack pressure and protect publishToSourceTopic from
  re-entrancy
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:52'
updated_date: '2026-03-12 21:52'
labels:
  - safety
  - mqtt
  - re-entrancy
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:886-896'
  - 'src/OTGW-firmware/MQTTstuff.ino:446-447'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The MQTT code has two re-entrancy/stack hazards related to the ESP8266 cooperative scheduler:

1. **publishToSourceTopic static buffer race** (MQTTstuff.ino:891): Uses `static char sourceTopic[MQTT_TOPIC_MAX_LEN]` without any guard. This function is called from `processOT()` during normal OT message flow. Because `doBackgroundTasks()` is re-entrant (via feedWatchDog → yield), a second call to `publishToSourceTopic` can overwrite `sourceTopic` while the first call's `sendMQTTData()` is still using it. Unlike the autoconfigure scratch buffers which have an `inUse` guard, this static buffer is unprotected.

   Fix: Either make sourceTopic a stack-local (it's only 200 bytes, acceptable for this call depth), or add a simple `static bool inUse` guard similar to mqttAutoCfgScratch.

2. **handleMQTTcallback stack accumulation** (MQTTstuff.ino:446-447): The callback allocates ~200 bytes on stack: `otgwcmd[51]` + `topicToken[96]` + `msgPayload[50]`. This callback is invoked by PubSubClient from within `MQTTclient.loop()`, which is called from `doBackgroundTasks()`. If the call chain is deep (loop → doBackgroundTasks → MQTTclient.loop → callback), these stack buffers add to an already deep stack.

   Fix: Make the largest buffers (`topicToken[96]`) static to reduce per-call stack impact.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 publishToSourceTopic's sourceTopic buffer is either stack-local or protected by an inUse guard
- [ ] #2 handleMQTTcallback's topicToken buffer is made static to reduce stack pressure
- [ ] #3 No behavioral change to MQTT publishing or command handling
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed two MQTT re-entrancy/stack hazards:

1. publishToSourceTopic: Added static bool inUse guard around the static sourceTopic buffer. If re-entered via feedWatchDog → yield → processOT, the second call is safely skipped.
2. handleMQTTcallback: Made topicToken[96] static to reduce per-call stack pressure (~96 bytes saved from the callback stack frame). Added explicit zeroing at entry since static buffers aren't re-initialized.

Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->

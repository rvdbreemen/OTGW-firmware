---
id: TASK-3
title: const-correctness pass on MQTT and helper functions
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:12'
updated_date: '2026-03-12 20:36'
labels:
  - refactor
  - code-quality
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Many functions in MQTTstuff.ino and helperStuff.ino take `char*` parameters where `const char*` would be correct. This prevents the compiler from catching accidental mutations, forces callers to cast away const, and can hide real latent bugs. Audit all function signatures in MQTTstuff.ino and helperStuff.ino and add `const` where the parameter is not mutated.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All non-mutating char* parameters in MQTTstuff.ino use const char*
- [x] #2 All non-mutating char* parameters in helperStuff.ino use const char*
- [x] #3 No casts needed at call sites to satisfy const
- [x] #4 Firmware builds cleanly with zero new warnings
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Full audit of MQTTstuff.ino and helperStuff.ino: the codebase is already const-correct. All non-mutating char* parameters (topic, json, payload-in, path) already use const char*. The remaining non-const parameters are intentionally mutable (output buffers like dest/summary/buffer, in-place mutators like trimInPlace/splitLine/trimwhitespace, and handleMQTTcallback which cannot be changed due to PubSubClient library API contract). No call-site casts were found. No changes needed.
<!-- SECTION:FINAL_SUMMARY:END -->

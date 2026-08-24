---
id: TASK-1084
title: >-
  feat-2.0.0: port TASK-1082 — settings value silently truncated, plus {tr}
  emits invalid JSON
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 19:38'
updated_date: '2026-08-24 21:13'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: medium
ordinal: 262000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Peer of the 1.x TASK-1082. Two defects on this line.

A) extractJsonField reported success on a truncated value. Note this tree's scanner differs from 1.x: BOTH branches truncated silently here (xjfReadString tracked a 'full' flag it never reported, and the bare-token branch did strlcpy-style truncation at jsonStuff.ino:912). postSettings also read into char newValue[150] while settings.webhook.sPayload is char[201].

B) 2.0.0-only: Tr is NAN-initialised (OTGW-Core.h:73, TASK-522) and the {tr} webhook substitution emitted the literal '--', which is not valid JSON in a numeric position, so the documented example template produced {"tr":--} until a room temperature was observed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 extractJsonField returns false rather than reporting success for any value that does not fit, in both the string and the bare-token branch
- [x] #2 The destination buffer is left empty and NUL-terminated on every false return, so return-ignoring callers cannot read a partial value
- [x] #3 postSettings accepts the full 200-character payload and returns 400 for an oversized value
- [x] #4 {tr} with no reading expands to the JSON literal null; 0.0f-initialised variables are unchanged
- [x] #5 A host-compiled harness exercises the real shipped code (not a copy) and demonstrably fails before the fix and passes after
- [x] #6 All extractJsonField callers audited for the changed return contract; behaviour changes documented
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: also carries the GH #677 unsupported-bitmap fix ported from 1.x TASK-1080, in its hardened form.

Adversarial verification (5 skeptics + synthesis) found the first version of that fix was unsafe ON THIS LINE SPECIFICALLY, in stock configuration. The gate tested bAnswerOverride, which is set only when a real B preceded the A within 500 ms. This line answers the thermostat itself in several places without ever emitting a B: networkStuff.ino:814/820 self-issues SR=21 and SR=22 every minute via sendtimecommand(), and OTDirect.ino:1966-1970 then answers every later thermostat read of those ids with a synthesised READ_ACK, explicitly not forwarding to the boiler. Those (T,A) pairs have bAnswerOverride false, passed the gate, and their Ack cleared a genuine 'boiler does not implement' verdict for MsgID 21/22 in RAM, in the retained MQTT CSV and on flash.

Retraction now demands rsptype == OTGW_BOILER. Setting stays permissive so a proxy A still counts as boiler evidence (ADR-103). Master-mode set/clear oscillation from the cold/warm otBoilerCacheValid transition is closed by the same change.

Residual, tracked as TASK-1086: loopback mode fabricates frames labelled 'B' (OTDirect.ino:1213-1215), which this guard cannot exclude, and the synthesised type-7 A frames still SET unsupported bits. TASK-1086 scope should cover retraction as well as set.
<!-- SECTION:NOTES:END -->

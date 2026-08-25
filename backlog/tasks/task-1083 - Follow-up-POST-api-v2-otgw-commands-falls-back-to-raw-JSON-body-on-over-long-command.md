---
id: TASK-1083
title: >-
  Follow-up: POST /api/v2/otgw/commands falls back to raw JSON body on over-long
  command
status: Done
assignee:
  - '@claude'
created_date: '2026-08-24 19:05'
updated_date: '2026-08-25 18:13'
labels:
  - bug
dependencies: []
priority: low
ordinal: 185000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Surfaced by the extractJsonField return-contract change in TASK-1082. At restAPI.ino:459 the /api/v2/otgw/commands handler falls back to strlcpy(cmdBuf, body.c_str(), 64) when extractJsonField returns false. With the corrected contract, a command value of 64+ chars now yields the first 63 chars of the raw JSON body instead of the first 63 chars of the command. Both are invalid PIC commands so neither reaches the queue, but the fallback is now reached for a new reason. A clean fix needs a tri-state return (found/not-found/too-long) rather than bool, which redesigns a shared helper and was deliberately left out of TASK-1082's scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 extractJsonField distinguishes not-found from too-long at its call sites, or the commands handler stops using the raw-body fallback for an over-long command value
- [x] #2 An over-long command value produces an explicit error rather than a silently mangled command
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: verified the real-world impact, correcting the original framing. Both the old and the new path REJECT an over-long command; nothing reaches the PIC queue either way. Only the HTTP status code changes.

Verified facts:
- cmdqueue entry is char cmd[15] (OTGW-Core.h:522), so kMaxCmdLen is 14.
- handleCommandSubmit (restAPI.ino:232-241) checks shape (chars 0,1 alphabetic and char 2 '=') then length against kMaxCmdLen.
- OLD: extractJsonField truncated to 63 chars and returned true. 'PS=1111...' passed the shape check but 63 > 14, so it was rejected 413 Command too long.
- NEW: extractJsonField returns false, the fallback copies the raw JSON body, cmdBuf starts with '{' which is not alphabetic, so it is rejected 400 Invalid command format.
- The path is only reachable for a command value of 64+ chars, which can never be valid since the queue caps at 14.

So this is a status-code change (413 -> 400), not a functional regression. The underlying issue is a design smell: a bool now carries two distinct outcomes (not-found vs too-long) that a fallback caller cannot tell apart. Harmless here; a hazard for the next caller that adds a fallback path. Priority stays low.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Took the second route the AC offered rather than the tri-state redesign: the commands handler no longer takes the raw-body fallback blind.

POST /api/v2/otgw/commands accepts either {"command":"XX=y"} or a bare XX=y body, and used a false return from extractJsonField to mean 'not JSON with a command key'. Since TASK-1082 that helper also returns false when the value does not fit cmdBuf[64], and the fallback could not tell the two apart: on a JSON body with an over-long value it handed the raw JSON to the PIC command path as if it were the command.

The handler now checks whether the body names the key before falling back. If it does, a false return can only mean the value was too long, so it answers 400 with a message that says so. If it does not, the plain-text fallback runs exactly as before.

This deliberately avoids redesigning extractJsonField's return type. A tri-state would touch a shared helper and all three call sites to fix one endpoint whose real-world impact was a status code, and the smaller change makes the error message truthful, which the tri-state would not have improved on.

Verified by walking all four body shapes: valid JSON command, bare text command, JSON with an over-long value, and JSON without a command key. Only the third changes, and it changes from a misleading 'Invalid command format' to an accurate over-length error. Build exit 0, evaluator 35/37 with 0 failures.

Known edge: a plain-text body that literally contains the string "command" now takes the error path instead of the fallback. Both paths already rejected such a body, since it cannot satisfy the two-letter-then-equals shape check within 14 characters.
<!-- SECTION:FINAL_SUMMARY:END -->

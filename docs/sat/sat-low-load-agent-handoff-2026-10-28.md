# SAT execution handoff to building agents

Robert explicitly requested this handoff and execution-graph update in the Codex review chat on 2026-10-28. It is backed by TASK-1412. Read this before selecting the next SAT task. Backlog task metadata is the live dependency authority; the [execution index](sat-low-load-execution-backlog-2026-10-28.md) and [manifest](sat-low-load-execution-backlog-2026-10-28.json) explain and snapshot it.

## Current ownership and immediate work

- Continue the existing TASK-1368 implementation and TASK-1409 reporting fix. This coordination update does not change firmware, task statuses or assignees.
- Decision entry 111 answers Q55 and unblocks TASK-1368. Reopen the recorded decision and the task's latest plan before continuing.
- Keep SATpid.ino writers serial: **1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415**. These are Backlog dependencies, not just this suggested reading order. Revalidate each task after its predecessors: if a preceding change already fixes its premise, record proof and re-scope through the normal task workflow instead of duplicating a change.
- TASK-1381 branches after TASK-1270. It still waits for Q53; do not put that unanswered question in front of independent PID fixes. Coordinate any shared SATcontrol.ino edits with the active owner.
- TASK-1415 still requires Robert's persistence write-cadence decision. Its position at the end avoids blocking earlier PID fixes on that decision.
- TASK-1409 can proceed independently of the PID chain. Its outcome and TASK-1425 feed TASK-1412.15 telemetry verification.

## Required integration links

- TASK-1412.7 includes completed TASK-1422 and TASK-1424 for combined post-DHW regression evidence.
- TASK-1412.16 includes TASK-1413 so the repaired-baseline/model comparison cannot bypass auto-tune removal.
- Preserve the reconciliation already committed in 6dda1e15a: TASK-1428/1429 feed TASK-1412.8; TASK-1423 feeds TASK-1412.10; TASK-1425 feeds TASK-1412.15; TASK-1426 feeds TASK-1412.18. TASK-1428 retains its TASK-1412.5 measurement-contract prerequisite.
- Do not reinstate TASK-1412.3 as a blanket prerequisite for fixes that already carry their own approved contract and red/green proof. It remains responsible for integrating those tests into the LL gates, including tests/test_sat_pwm_anchors.py. Missing capability/freshness/mode contracts still block their affected implementations.

## Decisions are separate gates

Read docs/sat/sergeant-d-corrections.md first. Entry 111 settles Q55; entry 110 updates the underfloor default decision. Q53 (continuous clamp), Q54 / TASK-1426 (heat-pump MM), the remaining MaxTSet defaults/write behaviour and TASK-1415 write cadence remain governed by their recorded questions/owner. Entry 100 and the OT ID6 support/writability contract still need an explicit interoperability resolution. This handoff settles none of those policies. Interpretations in entries 104/107/108 are not silently upgraded to designer confirmation.

## Bench and release gates

Before a tester alpha containing a control fix, archive old-versus-new board bench evidence for the exact candidate and settings, including combined fixes where relevant. A host build or source test is insufficient for that readiness claim. This preserves Robert's recorded per-fix alpha policy; it does not require the entire formal field programme before every alpha.

Formal sequence remains **1412.18 host integration -> 1412.19 closed-loop simulation -> 1412.21 real-board bench simulation -> 1412.22 independent bench acceptance -> 1412.23 controlled equipment -> 1412.24 tester readiness -> 1412.25 authorized field validation -> 1412.26 final independent evidence review**. TASK-1412.20 prepares the bench rig independently once its prerequisites permit it. Candidate changes invalidate affected acceptance and require reruns.

OTGW replay blocks bus transmission. Label its results board-executed controller replay, not PIC wire delivery or real boiler/compressor proof. Keep PH-PIC, PH-OTD, PH-PROFILE and PH-FIELD distinct. Unavailable checks and unapproved decisions remain blockers.

## Shared-checkout coordination

Do not overwrite or stage another worker's source edits. Read the latest task plan/notes before starting. The Codex graph updater owns only the execution index/manifest, graph-audit snapshot, this handoff, navigation pointers and its dependency changes. TASK-1368/TASK-1409 remain owned by their active builders; their new handoff comments are for the owners to retain when they commit. No firmware implementation, build result, acceptance checkbox or Done status is produced by this scheduling update.

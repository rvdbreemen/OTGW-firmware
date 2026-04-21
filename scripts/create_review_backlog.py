#!/usr/bin/env python3
"""
Create backlog tasks from the C code review captured in HANDOFF.md.

Cross-platform (Windows / macOS / Linux). Uses subprocess list-args so there is
no shell quoting. Auto-locates `backlog`, `backlog.cmd`, or `backlog.exe`.

Usage:
    python scripts/create_review_backlog.py --dry-run
    python scripts/create_review_backlog.py
    python scripts/create_review_backlog.py --only re-entrancy
    python scripts/create_review_backlog.py --only P0

Requires the Backlog.md CLI: https://backlog.md
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from typing import List, Optional


REVIEW_LABEL = "code-review"


@dataclass
class Task:
    title: str
    description: str
    acs: List[str]
    priority: str           # high | medium | low
    labels: List[str]       # extra labels (REVIEW_LABEL is always added)


TASKS: List[Task] = [
    # ======================================================================
    # P0 — crash / stability / safety
    # ======================================================================
    Task(
        title="P0-1: Harden ot_log_buffer against re-entrancy",
        description=(
            "ot_log_buffer[512] (OTGW-Core.ino:97) is shared by AddLog* macros, "
            "Telnet, and WebSocket broadcast. sendEventToWebSocket_P() calls "
            "feedWatchDog() mid-flight, so doBackgroundTasks() can re-enter and "
            "corrupt the buffer. Current 'safety' is a comment at "
            "OTGW-Core.ino:3928-3930, not an enforced contract. Replace the "
            "single shared buffer with either a small ring buffer or an explicit "
            "re-entry guard (assert on re-entry and drop the nested log line)."
        ),
        acs=[
            "Identify every writer of ot_log_buffer and every yield point between write and consume",
            "Introduce a reentry guard (static bool inAddLog) that drops nested writes and increments a dropped-log counter",
            "Alternative: convert to a small SPSC ring buffer sized for the longest log line",
            "Document the chosen invariant in a comment at the buffer declaration site",
            "Add a stress path (burst of OT messages while WebSocket client connected) and confirm no corruption in telnet output",
            "Build passes and evaluate.py is clean",
        ],
        priority="high",
        labels=["re-entrancy", "safety", "P0"],
    ),
    Task(
        title="P0-2: Add guard for cmdqueue under re-entrant callers",
        description=(
            "cmdqueue[] / cmdQueueSize (OTGW-Core.ino ~401) is mutated by "
            "addOTWGcmdtoqueue() from MQTT, REST, SAT, SATcycles AND by "
            "processSerialTwo() which yields. There is no mutual exclusion. "
            "Under burst load, a command can be reordered or dropped. Introduce "
            "a re-entry counter / busy flag and either defer or assert on "
            "conflicting access."
        ),
        acs=[
            "Enumerate every caller of addOTWGcmdtoqueue()",
            "Add a static queue-busy guard; on contention, defer the enqueue to the next background tick (do not spin)",
            "Unit-observable: telnet counter for deferred enqueues",
            "Verify no queue entry is lost under a 10s burst test",
            "Build + evaluate.py pass",
        ],
        priority="high",
        labels=["re-entrancy", "safety", "P0"],
    ),
    Task(
        title="P0-3: Protect sourceTopic static buffer across yield",
        description=(
            "MQTTstuff.ino:1224-1228 fills a static char sourceTopic[MQTT_TOPIC_MAX_LEN] "
            "with snprintf() then immediately calls sendMQTTData() which yields. "
            "Anything re-entering doBackgroundTasks() that touches this buffer "
            "corrupts the topic. Either move the buffer to the caller's stack, "
            "or document and enforce the no-yield contract with a compile-time "
            "assertion."
        ),
        acs=[
            "Move sourceTopic to a local variable on the caller's stack (or a heap-free stack buffer struct)",
            "If static is kept, add a short comment explaining the contract and assert no foreign writer in between",
            "Build + evaluate.py pass",
        ],
        priority="high",
        labels=["re-entrancy", "safety", "P0"],
    ),
    Task(
        title="P0-4: Fix null-deref after strstr in FSexplorer",
        description=(
            "FSexplorer.ino:132-146 checks strstr_P(lineBuf, PSTR(...)) then "
            "separately calls strstr(lineBuf, \"...\") on line 133 and "
            "dereferences the result with *pos = '\\0' without a null check. "
            "The two searches can disagree (alignment, case) and if the second "
            "one returns NULL, the write crashes the ESP."
        ),
        acs=[
            "Add null check on every strstr() return before dereference",
            "Collapse the two searches so the needle is searched exactly once",
            "Manually verify with an edge-case index.html that still hits the legacy branch",
            "Build + evaluate.py pass",
        ],
        priority="high",
        labels=["safety", "P0"],
    ),
    Task(
        title="P0-5: Typed OT status accessors, replace bit-bang in SAT",
        description=(
            "SATcontrol.ino:257, 406, 593 reads OT status with hand-coded "
            "masks like (OTcurrentSystemState.MasterStatus & 0x08). The same "
            "fields are decoded with enum-based parsers elsewhere in OTGW-Core. "
            "A one-bit mistake silently changes heating behaviour. Add "
            "accessor inline functions: isFlameOn(), isCHActive(), "
            "isDHWActive(), getBoilerTemp() etc. and replace all bit-bang sites."
        ),
        acs=[
            "Add inline accessors in OTGW-Core.h covering MasterStatus and SlaveStatus bits used outside OTGW-Core.ino",
            "Replace every bit-bang in SATcontrol.ino, SATcycles.ino, SATpid.ino with the typed accessor",
            "Grep confirms zero remaining hex masks on OT status fields outside OTGW-Core.ino",
            "Build + evaluate.py pass",
        ],
        priority="high",
        labels=["architecture", "safety", "P0"],
    ),

    # ======================================================================
    # P1 — correctness / design
    # ======================================================================
    Task(
        title="P1-1: Extract OT protocol module from OTGW-Core.ino",
        description=(
            "OTGW-Core.ino is 4739 lines mixing OT decode, MQTT throttle arrays, "
            "WebSocket fan-out, REST timestamps, PIC-settings state machine, "
            "and command queue. Split into dedicated files. Do NOT attempt "
            "until P1-2 (ADR-051 completion) lands, otherwise globals leak "
            "across the new boundary."
        ),
        acs=[
            "Create OTprotocol.ino containing only OT decode/encode and OTdata structs",
            "Move WebSocket dispatch to webSocketStuff.ino (already exists, tighten the boundary)",
            "Move REST update-timestamp bookkeeping to restAPI.ino",
            "Move PIC-settings state machine to its own .ino",
            "OTGW-Core.ino ends up < 1500 lines",
            "Build + evaluate.py pass, runtime smoke test on hardware",
        ],
        priority="medium",
        labels=["architecture", "P1"],
    ),
    Task(
        title="P1-2: Complete ADR-051 for MQTT throttle flags",
        description=(
            "ADR-051 split settings vs runtime state into structs, but stragglers "
            "remain as loose file-statics: mqttPublishAllowed (OTGW-Core.ino:247), "
            "statusBurstActive, statusBurstPublishCount, verifyActive, "
            "verifyWildcard[128] (all MQTTstuff.ino:60-184). Promote these into "
            "state.mqtt.* fields. This is a precondition for any split of "
            "OTGW-Core.ino."
        ),
        acs=[
            "Move mqttPublishAllowed, statusBurstActive, statusBurstPublishCount, verifyActive, verifyWildcard into state.mqtt",
            "Update all readers/writers to use state.mqtt.*",
            "Confirm no file-static MQTT control flag remains outside state.mqtt",
            "Build + evaluate.py pass, MQTT burst still quiesced correctly",
        ],
        priority="medium",
        labels=["architecture", "P1"],
    ),
    Task(
        title="P1-3: REST API must route through the OT data throttle gate",
        description=(
            "restAPI.ino reads OTcurrentSystemState directly, bypassing the "
            "throttle/gate logic that governs freshness for MQTT consumers. "
            "Two presenters, two truths. Introduce a single data-reader "
            "interface used by both REST and MQTT."
        ),
        acs=[
            "Define a small OT data accessor layer (const-ref or inline getters)",
            "Replace every REST handler's direct OTcurrentSystemState access with the accessor",
            "MQTT path unchanged but opportunistically use the same accessor",
            "Build + evaluate.py pass",
        ],
        priority="medium",
        labels=["architecture", "P1"],
    ),
    Task(
        title="P1-4: De-duplicate SAT vs OTGW TSet publishes",
        description=(
            "SATcontrol.ino:~1730 satPublishMQTT() publishes TSet in the same "
            "tick OTGW-Core.ino may publish the same topic. Under burst this "
            "emits two conflicting messages for the same retained key."
        ),
        acs=[
            "Identify every MQTT topic written by both SAT and OTGW paths",
            "Choose a single owner per topic (SAT when SAT is active, OTGW otherwise)",
            "Add a compile-time or runtime assertion so a future regression is loud",
            "Build + evaluate.py pass",
        ],
        priority="medium",
        labels=["architecture", "P1"],
    ),
    Task(
        title="P1-5: Remove String from dBmtoQuality hot path",
        description=(
            "helperStuff.ino:646-656 returns a String from dBmtoQuality(). "
            "Called during WiFi quality checks — heap churn on ESP8266 is a "
            "stability risk (ADR-004). Replace with a static const char* const "
            "lookup table and return const char*."
        ),
        acs=[
            "dBmtoQuality() signature returns const char*",
            "No String object allocated in the function",
            "All call sites updated",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["quality", "progmem", "P1"],
    ),
    Task(
        title="P1-6: Remove String allocations in PIC upgrade handler",
        description=(
            "OTGW-Core.ino:4668-4670 allocates three String objects during the "
            "PIC firmware upgrade control path. Replace with char[] buffers "
            "and strlcpy from httpServer.arg().c_str()."
        ),
        acs=[
            "All three arg() calls replaced with stack buffers sized appropriately",
            "strlcpy used instead of String assignment",
            "Build + evaluate.py pass, PIC upgrade flow manually tested",
        ],
        priority="low",
        labels=["quality", "P1"],
    ),
    Task(
        title="P1-7: Fix dangling c_str() in FSexplorer",
        description=(
            "FSexplorer.ino:98, 187, 201 call strcmp(httpServer.arg(\"v\").c_str(), ...). "
            "arg() returns String by value; the temporary's c_str() is dangling "
            "once the rvalue is destroyed. Works today by accident of "
            "same-statement lifetime. Bind the String to a const reference or "
            "copy into a char buffer first."
        ),
        acs=[
            "Each site binds httpServer.arg(...) to a named const String& before calling c_str()",
            "Or copies into a stack char[] with strlcpy",
            "Build + evaluate.py pass",
        ],
        priority="medium",
        labels=["quality", "safety", "P1"],
    ),
    Task(
        title="P1-8: Fix EVALBOOLEAN macro multi-evaluation",
        description=(
            "OTGW-firmware.h:94 defines EVALBOOLEAN(x) which evaluates x three "
            "times and lacks parens. Any caller passing a function triggers "
            "the side effect thrice. Replace with an inline function or "
            "parenthesise and evaluate once via a temp."
        ),
        acs=[
            "Replace with static inline bool evalBoolean(const char* s) in OTGW-firmware.h or a helper",
            "Or macro with parens and do { const char* _v = (x); ... } while (0) style",
            "All callers still compile",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["quality", "P1"],
    ),
    Task(
        title="P1-9: Collapse duplicate JSON escaper",
        description=(
            "jsonStuff.ino:14 defines escapeJsonStringTo(). OTGW-Core.ino:4342 "
            "defines a private static jsonEscape() that does the same thing. "
            "Delete the duplicate; route OTGW-Core.ino callers through "
            "escapeJsonStringTo()."
        ),
        acs=[
            "jsonEscape() in OTGW-Core.ino removed",
            "Callers at OTGW-Core.ino:4399-4400, 4438 use escapeJsonStringTo()",
            "Diff of generated JSON before/after is byte-identical for a representative sample",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["dedup", "P1"],
    ),
    Task(
        title="P1-10: Remove duplicate strlcpy_P in mqtt_configuratie.cpp",
        description=(
            "OTGW-firmware.h:22 provides an inline strlcpy_P under a header "
            "guard. mqtt_configuratie.cpp:1665 redefines the same function. "
            "Delete the .cpp copy; rely on the header."
        ),
        acs=[
            "mqtt_configuratie.cpp no longer defines strlcpy_P",
            "Build passes (generator tool respects header include order)",
            "evaluate.py clean",
        ],
        priority="low",
        labels=["dedup", "P1"],
    ),
    Task(
        title="P1-11: Convert remaining snprintf to snprintf_P in MQTTstuff",
        description=(
            "MQTTstuff.ino:1182, 1188, 1227 use snprintf() with RAM format "
            "strings. Convert to snprintf_P(..., PSTR(\"%d\")) / "
            "snprintf_P(..., PSTR(\"%s/%s\")) per CLAUDE.md PROGMEM rules."
        ),
        acs=[
            "Three call sites converted to snprintf_P with PSTR()",
            "No PROGMEM pointer passed to %s (verified — these args are RAM)",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["progmem", "quality", "P1"],
    ),
    Task(
        title="P1-12: Convert strcmp to strcmp_P for 'unknown' literal",
        description=(
            "OTGW-Core.ino:4600 and :4683 call strcmp(state.pic.sDeviceid, \"unknown\"). "
            "The literal lives in RAM. Convert to strcmp_P(state.pic.sDeviceid, "
            "PSTR(\"unknown\"))."
        ),
        acs=[
            "Both sites use strcmp_P with PSTR",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["progmem", "quality", "P1"],
    ),

    # ======================================================================
    # P2 — quality / maintenance
    # ======================================================================
    Task(
        title="P2: Remove deprecated /api/firmwarefilelist and /api/listfiles",
        description=(
            "FSexplorer.ino:252-253 registers /api/firmwarefilelist and "
            "/api/listfiles. Comments mark them as removed in v1.3.0 (long "
            "since shipped). Handlers at FSexplorer.ino:283 and :384. Delete "
            "both endpoints and their handlers."
        ),
        acs=[
            "Two httpServer.on() registrations removed",
            "apifirmwarefilelist() and apilistfiles() functions removed",
            "Any other dead references cleaned up",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["cleanup", "dead-code", "P2"],
    ),
    Task(
        title="P2: Delete unreachable gas-consumption block in SAT",
        description=(
            "SATcontrol.ino:2116-2119 declares float minCons = 0.0f; float "
            "maxCons = 0.0f; then wraps logic in if (minCons > 0 && maxCons > 0) "
            "which is always false. Decide: implement from settings, or delete."
        ),
        acs=[
            "Either implement minCons/maxCons from settings and test with real values",
            "Or delete the dead block and remove the TODO comments",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["cleanup", "dead-code", "P2"],
    ),
    Task(
        title="P2: Remove unused webhookInitialized flag",
        description=(
            "webhook.ino:17 declares static bool webhookInitialized = false; "
            "assigned once, never read. Only webhookLastState is actually "
            "used in evalWebhook()."
        ),
        acs=[
            "webhookInitialized declaration and assignment removed",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["cleanup", "dead-code", "P2"],
    ),
    Task(
        title="P2: Wire evaluate.py into CI or delete it",
        description=(
            "scripts-level evaluate.py is a 785-line code-quality checker that "
            "enforces PROGMEM, memory, ADR-064 etc. rules. It is NOT invoked "
            "by any .github/workflows/ job. Either add a workflow that runs "
            "it on PR, or delete the script."
        ),
        acs=[
            "Option A: add .github/workflows/evaluate.yml that runs `python evaluate.py` on push/PR",
            "Option B: delete evaluate.py and this backlog note",
            "Decision documented as an ADR if kept in CI",
        ],
        priority="medium",
        labels=["cleanup", "ci", "P2"],
    ),
    Task(
        title="P2: Wire tests/ into a CI test runner or delete",
        description=(
            "tests/test_dallas_address.cpp is orphaned — not referenced by "
            "Makefile, build.py, or any workflow. Either set up a test "
            "harness (PlatformIO or native g++) and run it in CI, or delete "
            "the file."
        ),
        acs=[
            "Option A: PlatformIO native test env + CI job that runs bonus tests",
            "Option B: tests/ directory removed",
            "Decision documented",
        ],
        priority="medium",
        labels=["cleanup", "ci", "P2"],
    ),
    Task(
        title="P2: Remove commented-out debug lines (batch)",
        description=(
            "Commented-out code blocks scattered across the codebase. Bulk "
            "delete in one grooming PR. Sites:\n"
            "- helperStuff.ino:149-153, 331, 653\n"
            "- OTGW-firmware.ino:125, 183\n"
            "- restAPI.ino:760-761, 1076-1079\n"
            "- versionStuff.ino:25, 31, 41, 51-53\n"
            "- FSexplorer.ino:254\n"
            "- OTGW-Core.ino:2858"
        ),
        acs=[
            "All listed commented-out code blocks removed",
            "If any comment captured genuine rationale (e.g. OTGW-Core.ino:2858 re: feedWatchDog), convert to a proper comment and keep the reasoning",
            "Build + evaluate.py pass",
        ],
        priority="low",
        labels=["cleanup", "hygiene", "P2"],
    ),
]


def find_backlog_cli(dry_run: bool) -> str:
    for name in ("backlog", "backlog.cmd", "backlog.exe"):
        path = shutil.which(name)
        if path:
            return path
    if dry_run:
        return "backlog"
    sys.stderr.write(
        "ERROR: Backlog.md CLI not found on PATH.\n"
        "Install it from https://backlog.md and ensure `backlog` resolves.\n"
    )
    sys.exit(1)


def build_command(cli: str, task: Task) -> List[str]:
    cmd = [cli, "task", "create", task.title, "-d", task.description]
    for ac in task.acs:
        cmd.extend(["--ac", ac])
    cmd.extend(["--priority", task.priority])
    labels = [REVIEW_LABEL] + task.labels
    cmd.extend(["-l", ",".join(labels)])
    return cmd


def matches_filter(task: Task, needle: Optional[str]) -> bool:
    if not needle:
        return True
    needle_lower = needle.lower()
    haystacks = [task.title.lower(), *(l.lower() for l in task.labels)]
    return any(needle_lower in h for h in haystacks)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the backlog commands without executing them.",
    )
    ap.add_argument(
        "--only",
        metavar="FILTER",
        help="Only create tasks whose title or label contains FILTER "
             "(e.g. re-entrancy, architecture, cleanup, P0).",
    )
    ap.add_argument(
        "--stop-on-error",
        action="store_true",
        help="Abort on first backlog CLI failure (default: continue).",
    )
    args = ap.parse_args()

    cli = find_backlog_cli(args.dry_run)
    tasks = [t for t in TASKS if matches_filter(t, args.only)]

    if not tasks:
        print(f"No tasks match filter '{args.only}'.", file=sys.stderr)
        return 1

    print(f"Planning to create {len(tasks)} task(s) via: {cli}")
    if args.dry_run:
        print("(dry run — no tasks will be created)\n")

    failures = 0
    for i, task in enumerate(tasks, 1):
        cmd = build_command(cli, task)
        print(f"\n[{i}/{len(tasks)}] {task.title}")
        print(f"  priority={task.priority}  labels={','.join([REVIEW_LABEL] + task.labels)}")

        if args.dry_run:
            print("  CMD: " + " ".join(repr(a) for a in cmd))
            continue

        try:
            result = subprocess.run(
                cmd,
                check=True,
                capture_output=True,
                text=True,
            )
            out = (result.stdout or "").strip().splitlines()
            last = out[-1] if out else "(created)"
            print(f"  OK: {last}")
        except FileNotFoundError:
            print(f"  ERROR: cannot execute {cli}", file=sys.stderr)
            return 2
        except subprocess.CalledProcessError as e:
            failures += 1
            err = (e.stderr or e.stdout or "").strip()
            print(f"  FAIL (exit {e.returncode}): {err}", file=sys.stderr)
            if args.stop_on_error:
                return 1

    print()
    if args.dry_run:
        print(f"Dry run complete. {len(tasks)} task(s) would be created.")
    elif failures:
        print(f"Done with {failures} failure(s) out of {len(tasks)}.")
        return 1
    else:
        print(f"Created {len(tasks)} task(s).")
        print("Next: backlog task list --plain -l code-review")

    return 0


if __name__ == "__main__":
    sys.exit(main())

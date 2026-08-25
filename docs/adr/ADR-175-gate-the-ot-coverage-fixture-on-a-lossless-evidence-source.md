---
id: "ADR-175"
title: "Gate the OT coverage fixture on a lossless evidence source"
status: "Proposed"
date: "2026-08-09"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "testing"
  - "regression-gate"
  - "mqtt"
  - "observability"
aliases:
  - "coverage gate evidence source"
  - "telnet capture is lossy"
  - "broker subscription for topic presence"
components:
  - "scripts/tests/run_coverage_test.py"
  - "scripts/tests/coverage_baseline.py"
  - "scripts/tests/mqtt_topic_capture.py"
symbols:
  - "mqtt_presence_gated"
  - "PRESENCE_GATED"
  - "mqtt_source"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-175 Gate the OT coverage fixture on a lossless evidence source

## Status

Proposed, 2026-08-09.

## Status History

```yaml
status_history:
  - date: 2026-08-09
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context and Problem Statement

The OT coverage gate reduces a replay-fixture run to a normalized fingerprint and
diffs it against a committed baseline, so a decode or publish regression fails
one command instead of needing an eyeball. It has two halves: the decoded OT
frames, and the MQTT topics with their payloads.

Ported from `otgw-1.x.x` unchanged (TASK-1070), the gate failed on healthy
firmware. The debug telnet on this branch drops output when it is outrun. A
MsgID that fans out to about nine topics emits its publish lines in roughly
10 ms; the stream loses whatever follows. On the bench S3 that swallowed the
`processOT` line for `AC0630000` while the frame itself was processed normally,
as its own gate line printed. The tell is a truncated line with the next
timestamp spliced onto it, for example:

```
21:14:56.141905 ... sendMQTTData(1402): ... => To21:14:56.881179 (  43620| 31732) logMQTTValue(1678): MQT
```

Measured back to back on the same firmware and fixture: with MQTT debug **on**,
24 spliced lines and only 363 of 366 OT keys reproducible between the two halves
of one capture; with it **off**, 1 spliced line, 369 of 369, and all 376 distinct
fixture frames seen at least twice. The MQTT debug output is therefore at once
the source of the topic half and the reason the OT half loses frames.

Two independent two-loop runs then quantified the topic half. Presence differed
by 9 topics, with the smaller set a strict subset of the larger and zero spurious
topics, while payload sets differed on **0** of the 215 shared topics. Loss is
subtractive sampling from a lossy stream, not behaviour.

The loss is probabilistic rather than positional: `AC0630000` was absent from
both loops of one run and present once in the next. Extra loops therefore reduce
but never settle it. `otgw-1.x.x` does not show this: far less concurrent publish
volume and no SAT/BLE traces sharing the stream.

## Decision Drivers

* A gate that fails on healthy firmware is worse than no gate: it trains the
  reader to ignore it.
* Presence and payload have measurably different reproducibility, so treating
  them alike discards signal or invents noise.
* The bench and the maintainer's production gateway share one MQTT broker.
* The test rig is standard-library only; adding a dependency is a real cost.

## Considered Options

* Option A: read decoded frames from telnet with MQTT debug off, and read topics
  from an MQTT broker subscription; gate presence only where it is observable.
* Option B: keep one telnet capture and gate presence with more loops.
* Option C: keep one telnet capture and take the intersection of several runs as
  the baseline.
* Option D: do nothing, and accept a gate that reports differences a reader must
  triage by hand every run.

## Decision Outcome

Chosen option: **Option A**, because it is the only one that makes topic presence
an observation rather than a sample, and because a firmware detail makes it free:
`MQTTDebugTf` gates the log line and never the publish, so the device can run
with MQTT debug off, which is what keeps the decode lines intact, while the
broker still sees every publish. Both collectors therefore share a single 694 s
window instead of two, halving the run.

Gating is source-aware. The fingerprint records `mqtt_source`, and `mqtt`
presence is gated only when baseline and run both came from the broker. A
telnet-sourced run is compared on payloads alone, with presence differences
reported and not counted. Mixing the two is refused by construction: a broker
baseline compared against a telnet run would report every dropped log line as a
regression.

### Confirmation

`python scripts/tests/run_coverage_test.py --host <ip> --http-password <pw>`
exits 0 against the committed baseline. Verified on the bench S3 (Classic + PIC
6.6, `alpha.354+a7e06f8`): baseline of 369 OT keys, 143 distinct MsgIDs and 225
MQTT topics, with an independent run reporting PASS and zero differences.
`python scripts/tests/coverage_baseline.py selftest <capture>` reports 0 unstable
keys across 369.

## Decision Contract

### Must

* The `ot`, `msgtypes` and `sources` sections gate on MISSING, NEW and CHANGED.
* The `mqtt` section gates on CHANGED always, and on MISSING and NEW only when
  `mqtt_presence_gated()` holds, that is when baseline and run both carry
  `mqtt_source: "broker"`.
* A broker collector scopes its observations to the device under test, drains and
  discards the retained flush before the window opens, and discards the backlog
  buffered between subscribe and window start.
* The broker host, port, user and top topic come from the device under test, and
  the runner raises rather than falling back when the device does not report a
  usable broker and MAC.
* The capture window is at least two full fixture loops.
* A run confirms the replay is advancing before committing to the full window.

### Must Not

* Do not gate topic presence read from the debug log.
* Do not refresh the baseline to make a diff disappear without explaining the
  diff first.
* Do not take the intersection of several runs as a baseline.

### Exceptions

* `--topics telnet` remains available for a bench with no broker access. It sets
  `mqtt_source: "telnet"`, which downgrades presence to reporting automatically;
  no separate opt-out exists or is needed.

### Verification

* `scripts/tests/coverage_baseline.py::mqtt_presence_gated`
* `scripts/tests/coverage_baseline.py::PRESENCE_GATED`
* `python scripts/tests/run_coverage_test.py --host <ip> --http-password <pw>`

## Consequences

### Positive

* Topic presence became a real gate rather than a source of noise: an independent
  run matched the baseline exactly on all 225 topics.
* The broker observes one topic more than the best telnet run ever managed (225
  against 224, with a low of 215), so the gate now covers publishes the log
  silently dropped.
* One window instead of two halves the run to about 12 minutes.
* The OT half is exactly reproducible: 369 keys and 143 MsgIDs across independent
  runs.

### Negative

* The gate now needs broker credentials, from `MqttPassword` in the out-of-repo
  `capture-settings.json` or `$OTGW_MQTT_PASSWORD`. A bench without them falls
  back to telnet and loses presence gating. Mitigation: the fallback is explicit,
  labelled in the output, and reflected in `mqtt_source` so a baseline can never
  silently mix sources.
* A hand-rolled MQTT 3.1.1 client is code this project now maintains. Mitigation:
  it is subscribe-only and needs four packet types; the alternative was a
  dependency in a rig that is otherwise standard-library only.
* Collection must be scoped to the device under test, because the broker carries
  the maintainer's production gateway too and `normalise_topic()` strips exactly
  the id that distinguishes them. Mitigation: the scope is derived from the
  device's own MAC and is not optional in the runner.
* A 25 s preflight is added to every run. Mitigation: it replaces a failure mode
  that cost a full 12-minute window plus, under `--record`, a baseline reflecting
  30 of 376 frames.

## Pros and Cons of the Options

### Option A

* Good, because presence becomes observable, so the strongest available signal is
  gated rather than discarded.
* Good, because the two collectors use different transports and fit in one
  window.
* Bad, because it introduces a credential requirement and a small protocol
  implementation.

### Option B

* Good, because it needs no new transport or credential.
* Bad, because the loss correlates with publish load rather than position, so
  extra loops reduce the probability without bounding it. Measured directly: a
  frame absent from both loops of one run appeared once in the next.

### Option C

* Good, because it produces a baseline that every run can satisfy.
* Bad, because each run drops a different random subset, so the intersection
  erodes with every run instead of converging on a stable core. There is no fixed
  point to converge to.

### Option D

* Good, because it costs nothing to implement.
* Bad, because a gate whose failures are usually noise stops being read, which
  removes the regression detection the fixture exists to provide.

## Open Questions

* None.

## Related Decisions

* **ADR-140** (Single-Device HA Discovery Topology with Source-Prefix Entity
  Clustering): fixes the topic shapes this gate fingerprints.
* **ADR-116** (MQTT On-Change Publishing as the Default with One-Time Interval
  Migration): determines how often a topic reappears inside a capture window, and
  therefore what a two-loop window can be expected to see.

## References

* `scripts/tests/README-coverage-gate.md`: the operational write-up, including
  the four subscriber traps and the record-time checklist.
* `scripts/tests/run_coverage_test.py`, `scripts/tests/coverage_baseline.py`,
  `scripts/tests/mqtt_topic_capture.py`.
* Commit `ca55863b`, "test(coverage): gate OT decode and MQTT topics against a
  recorded baseline".
* TASK-1070, which carries the full measurement log including the two-run
  presence and payload comparison.
* `src/OTGW-firmware/MQTTstuff.ino`: `MQTTDebugTf` gates the log line only, never
  the publish, which is what lets one window carry both halves.

## Enforcement

```json
{
  "require_pattern": [
    {
      "pattern": "mqtt_presence_gated",
      "path_glob": "scripts/tests/coverage_baseline.py",
      "message": "ADR-175: mqtt presence is gated only when both sides came from the broker. Removing this check turns a lossy sample back into a blocking signal."
    },
    {
      "pattern": "mqtt_source",
      "path_glob": "scripts/tests/coverage_baseline.py",
      "message": "ADR-175: the fingerprint records where its topics came from; without it a broker baseline can be compared against a telnet run."
    }
  ],
  "llm_judge": true
}
```

The declarative rules pin the two symbols that carry the source-aware gating.
`llm_judge` covers what regex cannot see: that presence stays ungated for
telnet-sourced runs, that the collector remains scoped and flushed, and that the
baseline is not refreshed to silence a diff.

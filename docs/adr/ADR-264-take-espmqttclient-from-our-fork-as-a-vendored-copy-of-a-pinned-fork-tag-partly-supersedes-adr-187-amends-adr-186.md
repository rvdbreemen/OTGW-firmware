---
id: "ADR-264"
title: "Take espMqttClient from our fork as a vendored copy of a pinned fork tag (partly supersedes ADR-187, amends ADR-186)"
status: "Accepted"
date: "2026-11-20"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-186"
  - "ADR-187"
topics:
  - "mqtt"
  - "espmqttclient"
  - "vendored-library"
  - "dependency-source"
aliases:
  - "espMqttClient fork"
  - "rvdbreemen/espMqttClient"
  - "vendored from fork tag"
components:
  - "vendored espMqttClient from fork tag (src/libraries/espMqttClient)"
  - "espMqttClient provenance file (OTGW-PROVENANCE.md)"
symbols:
  - "ClientSync::write"
  - "EMC_CONNECT_TIMEOUT_MS"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-264 Take espMqttClient from our fork as a vendored copy of a pinned fork tag (partly supersedes ADR-187, amends ADR-186)

## Status

Accepted, 2026-11-20.

**Decision Maker:** User: Robert van den Breemen decided on 2026-11-20, relayed by the coordinator, that our fork is the dependency source from now on: "Voer de wijzigingen op onze fork door, hou daarbij onze fork als depencency voortaan." The dependency form (vendored from a tag) was the agent's recommendation; Robert accepted it on 2026-11-20 (reply "A", Discord DM 1573300999525769237).

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal on Robert's decision 2026-11-20 (fork as dependency)
    changed_via: adr-kit
  - date: 2026-11-20
    status: Accepted
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Maintainer Robert van den Breemen accepted (reply 'A', Discord DM 1573300999525769237, 2026-11-20); recorded by the agent, relayed by the coordinator
    changed_via: adr-kit lifecycle
```

## Context

`src/libraries/espMqttClient` holds upstream bertmelis/espMqttClient `main` at de31cb2 (library.json 1.7.3) with two local patches:

1. **Non-blocking `ClientSync::write()`** (ADR-186, TASK-1213), at `Transport/ClientSync.cpp:60-74`. It is offered upstream as PR #191, open since 2026-10-05 with no response.
2. **TCP connect timeout** (TASK-1325). `EMC_CONNECT_TIMEOUT_MS` is defined at `src/Config.h:35-41` and used at `Transport/ClientSync.cpp:23-29` and `:43-47`; `platformio.ini:55` sets it to 300. Without it, an unreachable broker stalls `loop()` for 3 s per attempt.

ADR-187's Must Not allows only the first patch. The second has no ADR, so the copy has already drifted from its ADR.

The fork rvdbreemen/espMqttClient exists. Checked on 2026-11-20 by clone and diff against the vendored copy:

- fork `main` equals upstream `main` (de31cb2);
- branch `nonblocking-clientsync-write` (fe78785) carries patch 1;
- patch 2 is in no fork branch.

Robert wants changes made on the fork, and the fork used as the dependency from now on. A follow-up library change, the per-loop packet cap of ADR-266, is conditional on a measurement. This ADR covers only where the library comes from, and it has value without the cap.

## Decision

espMqttClient comes from our fork. `src/libraries/espMqttClient` is a byte copy of the library sources of a pinned fork tag, named `v<upstream version>-otgw.<n>`. The first tag is `v1.7.3-otgw.1`: upstream de31cb2, plus the PR #191 commit, plus a TASK-1325 commit.

A provenance file, `src/libraries/espMqttClient/OTGW-PROVENANCE.md`, records:

- the fork URL, tag and commit;
- the upstream base commit;
- the sha256 of `src/`.

A host test recomputes that hash. A change to the library goes to the fork first, gets a new tag, and is then synced. It is never made in the copy.

**Return path.** Once upstream releases every fork change (or equivalents), the copy moves back to that upstream release. A new ADR supersedes this one, the provenance file names upstream, and the fork is archived.

## Decision Contract

### Must

- The copy equals the fork tag named in `OTGW-PROVENANCE.md` (hash test green).
- Each fork change is its own commit, with a code comment naming its ADR or task.
- Every update of the copy repeats the ADR-187 slow-broker bench (TASK-1213, `tc netem` 500 ms) and an esp32-combo build.

### Must Not

- Must not edit `src/libraries/espMqttClient` in place.
- Must not fetch the library over the network at build time.
- Must not change espMqttClient's threading model (ADR-131, ADR-186).

### Exceptions

- None.

### Verification

- `tests/test_espmqttclient_provenance.py` (TASK-2949.4) passes on the synced copy and fails after a one-byte edit of any file under `src/libraries/espMqttClient/src`.
- `diff -r` of the `v1.7.3-otgw.1` tag's `src/` against today's copy shows only comment text (TASK-2949.2).

## Alternatives Considered

- **`lib_deps` git URL pinned to a fork tag or commit.** Not chosen. The host tests compile the real library sources and need them in the repository; a fresh worktree has no `.pio/libdeps` until it has built once. It also adds a network fetch inside the machine-wide build lock (TASK-1488), fails offline, and puts a second library pattern beside the `scripts/patch_pio_libs.py` libdeps patching of ADR-232.
- **Git submodule of the fork.** Not chosen. Every clone and worktree then needs `git submodule update`, and the project has silently lost a submodule pointer during a merge before (2026-11-19, OpenTherm gitlink).
- **Keep tracking upstream with local patches (ADR-187 as is).** Not chosen: it is against Robert's decision. It has already let one undocumented patch in (TASK-1325), and upstreaming would stay hand work without history.

## Consequences

**Positive:**

- One recorded source with history. The TASK-1325 patch gets a home, and the copy matches its ADR again.
- Builds stay offline and deterministic. Host tests keep the real source in the tree.
- Upstream PRs come from a fork that carries the exact commits.

**Negative:**

- We maintain a fork. Each upstream release needs our commits rebased, a new tag and a sync. Mitigation: the hash test catches a copy that drifts from its tag, and the return path is written down.
- A fork push is a push to a remote outside `origin`, so it goes through the coordinator like every push.

## Open Questions

- [x] 1. Maintainer accepts the dependency form: vendored from a pinned fork tag, instead of a `lib_deps` git URL. — **Answered 2026-11-20 by Agent: Claude (otgw-architect):** Accepted by Robert van den Breemen: vendored from a pinned fork tag with a provenance hash (reply 'A', Discord DM 1573300999525769237, 2026-11-20, to the coordinator's question 1573299540629524531)

## Related Decisions

- **ADR-187** (track upstream espMqttClient): partly superseded. The source is our fork tag, not upstream `main`, and the allowed changes are the fork's commits. The bench-on-update rule stays.
- **ADR-186** (vendor espMqttClient, non-blocking write): amended. The copy now comes from the fork; the write patch and threading model stay.
- **ADR-266** (per-loop PUBLISH cap, Proposed and conditional): would be the next fork change.
- **ADR-265** (shared MQTT pacer, Proposed): independent of this ADR.
- **ADR-232** (AsyncTCP build-time patch): the libdeps patch route, and why it does not fit a library whose source the host tests compile.

## References

- Fork: https://github.com/rvdbreemen/espMqttClient (main = de31cb2; branch `nonblocking-clientsync-write` = fe78785). Upstream PR: https://github.com/bertmelis/espMqttClient/pull/191.
- `src/libraries/espMqttClient/src/Config.h:35-41`; `Transport/ClientSync.cpp:23-29`, `:43-47`, `:60-74`; `platformio.ini:20` (`lib_extra_dirs`), `:55`; `evaluate.py:512`.
- TASK-1213, TASK-1325; epic TASK-2949, subtasks TASK-2949.2 (fork tag) and TASK-2949.4 (switch).

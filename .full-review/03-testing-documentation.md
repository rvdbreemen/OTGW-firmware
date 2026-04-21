# Phase 3: Testing & Documentation Review

**Target**: branch `1.4.1` vs `dev`
**Full reports**: `phase3a-testing.md` (23 KB) · `phase3b-documentation.md` (15 KB)

## Aggregate severity counts

| Severity | Testing (3A) | Documentation (3B) | Total |
|---|---|---|---|
| Critical | 0 | 0 | **0** |
| High | 4 | 4 | **8** |
| Medium | 2 | 7 | **9** |
| Low | 3 | 5 | **8** |

---

## 3A: Testing — key finding

**The branch is not under-tested, it is under-gated.** Every HIGH and CRITICAL finding from Phase 1 and Phase 2 falls in one of three buckets that `evaluate.py` can already enforce: (a) arithmetic (buffer sizes, cooldown timings), (b) regex-level symmetry (burst-wrap on non-VH but not VH, incrementer-call on 5/5 helpers but not guaranteed on 6/6), (c) reference integrity (ADR citations). The branch already shipped the template (`check_time_boundary_single_caller`, ~10 lines of Python). Four more checks in the same pattern (~80 lines total) would have caught 7 of the 9 most-severe findings **before a human saw them**.

### HIGH (4) — proposed evaluate.py gates

1. **`check_json_buffer_arithmetic`** — parses `snprintf_P` format strings, computes worst-case size, fails if `sizeof(buffer)` insufficient. Would catch Phase 2B CRITICAL (heapdiag JSON 384→465).
2. **`check_status_burst_cooldown_bound`** — regex `STATUS_BURST_COOLDOWN_MS\s*=\s*(\d+)`; fail if ≥ 3000 unless `// verified tuning` escape hatch. Would catch Phase 2B HIGH-1 (cooldown default stalls drip).
3. **`check_status_publishers_wrap_burst`** — every `publish(Master|Slave)Status.*State` function must contain `beginStatusBurst` + `endStatusBurst`. Would catch Phase 1A HIGH #1 / Phase 2B HIGH-2 (VH gap).
4. **`check_adr_references_resolve`** — every `ADR-\d{3}` mention in code or docs must resolve to `docs/adr/ADR-NNN-*.md`. Would catch Phase 1B CRITICAL (ghost ADR-077/078/080).

### MEDIUM (2)

- **`check_discovery_stream_helpers_increment`** — the CI gate ADR-062 promised but wasn't implemented. Already a backlog task (TASK-364).
- **`getHeapHealth` hysteresis host-compiled unit test** — pure integer arithmetic, host-testable if refactored to accept `freeHeap` and `maxBlock` as arguments. ~60 lines total (refactor + test). Would validate Phase 1A MED + Phase 2B MED-2.

### LOW (3)

- `evaluate.py:183, 194` — dead `definition_sites` local in ADR-064 gate (3-line fix).
- `iVerifyRunCount` semantics host test — premature; wait for the outcome-enum refactor (TASK-361) to land first.
- `tests/test_dallas_address.cpp` orphan — either delete or rewrite as host-compilable (~30 lines). External review also flagged this.

### CI state today

`evaluate.py` (785 lines, 11 check functions) is **NOT wired into CI**. `.github/workflows/` has only `opentherm-v42-spec-audit.yml` and a Copilot helper. Recommendation: a `evaluate.yml` workflow running `python evaluate.py --quick` on PRs to `dev|1.4.*|release/**`.

---

## 3B: Documentation — key finding

**The code documentation is in good shape; the user-facing documentation is not.** Inline comments on new state sections are solid. Constants mostly lack rationale but that's cosmetic. The real gap is that **every user-facing change on this branch is undocumented outside ADR-062**: no release notes for 1.4.1, no README entry, no `openapi.yaml` update for three new endpoints, no `MQTT.md` entry for the new `stats/heap` retained topic or the discovery-verification mechanism. A user landing on the repo cold would not know those features exist.

### HIGH (4)

1. **No release notes or changelog entry for 1.4.1** — `RELEASE_NOTES_1.4.1.md` missing, `BREAKING_CHANGES.md` stops at v1.3.5, `docs/releases/` ends at 1.3.4. Users need: heap-diag MQTT topic, auto-verify setting, three REST endpoints, behavioural changes (drip 1s→2s, slow-mode 30s→10s, nightly restart now at hourChanged).
2. **README missing "What's new in v1.4.1"** — README's latest section is v1.4.0 (SAT/ESP32). First-time readers see an older version than they flash.
3. **Three new REST endpoints absent from `openapi.yaml`** — `GET /api/v2/discovery`, `POST /api/v2/discovery/verify`, `POST /api/v2/discovery/republish` visible only in C code. Documentation-first clients (Postman, Swagger UI, HA REST integration) won't see them.
4. **Three backlog task Final Summaries misrepresent shipped behaviour**:
   - TASK-342: claims "ALL three call sites" covered — VH publishers not wrapped.
   - TASK-349 + TASK-351: claim NTP + uptime preconditions enforced — they aren't.
   - TASK-346: claims `doTaskEvery60s` call site — actually `doTaskMinuteChanged` after TASK-350.

### MEDIUM (7)

- MQTT topic `stats/heap` undocumented in `docs/api/MQTT.md` (17-field JSON schema, cadence, retention).
- Discovery-verification mechanism undocumented in `docs/api/MQTT.md`.
- Four `VERIFICATION_*` constants lack inline rationale (vs. 5th which has it).
- `STATUS_BURST_COOLDOWN_MS = 10000` ships with documented trade-off but no in-place rationale for why 10000 was chosen.
- Stale comment on `sendMQTTheapdiag` location (already in Phase 1A HIGH #4 / TASK-360).
- ADR-062 Consequences section doesn't acknowledge Phase 2 behaviours (heap-abort telemetry mask, boot-time first-minute dispatch).
- ADR-064 Consequences section omits same boot-time behaviour (runNightlyRestart saved by uptime>3600, but sendMQTTheapdiag publishes near-zero snapshot at boot-minute-1).

### LOW (5)

- OpenAPI spec LOW echo of HIGH #3.
- "Plan file expressive-growing-yao" reference in 4 backlog task Descriptions (TASK-348/349/350/351).
- TASK-355 presence confirmation (no defect — just visibility).
- `docs/c4/` directory referenced in session memory does not exist on disk.
- Heap-diag struct comment documents 9 fields but MQTT wire format emits 17 keys.

### Leak scan result

No new machine-specific paths introduced on this branch outside the two ADR plan-file lines already flagged by Phase 1B. No new TODO/FIXME/HACK personal tags. No credentials or PII in configs.

---

## New items vs already-tracked

Cross-referenced against the 13 existing backlog tasks (TASK-352 to TASK-364):

| Phase 3 finding | Already in backlog? | Action |
|---|---|---|
| `evaluate.py` incPublishedTopicCount gate | Yes — TASK-364 | none |
| Stale comments on heapdiag + drip + ADR-077 | Yes — TASK-360 | none |
| NTP/uptime preconditions | Yes — TASK-359 | none |
| Verify outcome enum | Yes — TASK-361 | none |
| VH publishers burst-wrap | Yes — TASK-354 | none |
| ADR-062 Consequences additions | Partially — TASK-355 scope | **extend TASK-355** |
| `VERIFICATION_*` inline rationale | Partially — TASK-360 is code-comment scope | **extend TASK-360** |
| Release notes + README + BREAKING_CHANGES | No | **new task T14** |
| openapi.yaml + MQTT.md updates | No | **new task T15** |
| Backlog Final Summary corrections (342/346/349/351) | No | **new task T16** |
| evaluate.py gates (4 checks) + CI wiring | Partially — TASK-364 has 1 | **new task T17** covers other 4 + CI |
| test_dallas_address.cpp disposition | No | **new task T18** (LOW) |
| `docs/c4/` absence | No | **not blocking 1.4.1**; follow-up only |

Net new backlog items to create: **5** (T14, T15, T16, T17, T18) plus extensions to TASK-355 and TASK-360.

---

## Merge-readiness update

Phase 3 doesn't introduce new blockers on the **code** itself (no Critical, all HIGH items are doc/task-hygiene and CI gap). The code-side merge gate from Phase 1+2 stands:

1. TASK-352 heapdiag buffer
2. TASK-353 cooldown 10000→2000
3. TASK-354 VH burst-wrap
4. TASK-355 ADR revert + citations

**Recommended additions to ship with 1.4.1** (not blockers, but natural release-PR scope):
- T14 release notes (users will search for these)
- T15 API docs (broken openapi contract is a real regression for integrators)
- T16 backlog Final Summary corrections (audit trail)

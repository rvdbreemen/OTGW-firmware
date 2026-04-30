# Phase 4B: CI/CD & DevOps Practices

Baseline: `feature-dev-2.0.0-otgw32-esp32-sat-support` @ HEAD `6a01ae5d`.
Session range reviewed: `ace21a48..6a01ae5d` (13 commits incl. post-fix
follow-ups TASK-493/494/495/496/497).

## Summary

For a single-maintainer embedded-firmware project the CI/CD posture is
above-average: a working `evaluate.py --quick` gate runs on PR, an
OpenTherm v4.2 spec audit runs in parallel, and a Claude-driven code-review
job auto-triggers on every PR. The local `build.py` is the proper
centrepiece (replicates the CI build on developer machines) and a
hand-curated `pr-checklist.md` covers the hardware/browser/MQTT smoke
tests that build-clean cannot see. PROGMEM, ADR-088, ADR-089, and ADR-091
all have machine-enforced gates per ADR-080.

The gaps are mostly around release-channel rigour and observability of
runtime drift. There is no CI workflow that actually invokes
`pio run` / `arduino-cli compile` for ESP8266 + ESP32, so build breakage
on either target only surfaces locally. PR artefacts (firmware.bin /
littlefs.bin) are not published — every reviewer must rebuild. There is
no dependency-vulnerability scan on `lib_deps`, no CodeQL, no SBOM. The
ESP32 partition table commits the firmware to a single-app-slot layout
(no `ota_1`), so the "safe OTA rollback" claim in the partition CSV
header comment and in CHANGELOG `[2.0.0-beta]` is currently inaccurate.
Finally, the `evaluate.yml` PR trigger filter does **not** include
`feature-dev-*` branches — every PR landing into the active 2.0.0 work
branch in this session executed without the gate.

## Critical findings

(none)

## High findings

### 4B-H1 — `evaluate.yml` does not gate PRs into the active 2.0.0 branch

**File**: `.github/workflows/evaluate.yml:6-9`

```yaml
on:
  pull_request:
    branches:
      - dev
      - "1.4.*"
      - "release/**"
```

The active development branch for this entire session is
`feature-dev-2.0.0-otgw32-esp32-sat-support`. PRs targeting it skip the
`evaluate.py --quick` gate entirely. The gate is the project's only
machine-enforced check for PROGMEM / no-String-in-hot-path / ADR-088 /
ADR-089 / ADR-091. The session's own commits demonstrate the cost: the
1B-H1 `platformio.ini` reformat that erased ~90 lines of build-rationale
prose was re-introduced in the same branch and only caught by manual
review.

**Operational risk**: high. Every binding ADR currently goes
unenforced on the branch where most code is being written. The
`opentherm-v42-spec-audit.yml` workflow has the same issue
(branches: `main`, `dev`, `dev-*`, `copilot-*`, `release/**` — the
literal pattern `dev-*` does not match `feature-dev-*`).

**Recommendation**: add `"feature-dev-*"` and `"feature-*"` to the
branches filter on both `evaluate.yml` and
`opentherm-v42-spec-audit.yml`. Alternative (cleaner): drop the branches
filter entirely so PRs into any branch are gated — the workflow is
cheap and runs in <1 minute.

### 4B-H2 — No CI compilation step for either target

**File**: `.github/workflows/` — no `build.yml` exists.

The CI pipeline never compiles the firmware. `evaluate.py` is a static
checker. `tests/check_otdirect_fixture.py` is a Markdown table linter.
There is no `pio run -e esp8266` or `pio run -e esp32` step, no
`arduino-cli compile`, no firmware.bin / littlefs.bin upload as PR
artefacts. Build breakage on either ESP8266 or ESP32 only surfaces when
the maintainer runs `python build.py` locally — and the project's own
git history shows multiple commits (2026-04 ctags forward-decl drift,
the 4M2M partition rename, the 1B-H1 platformio.ini reformat) where
"compiles on my machine" was the entire safety net.

**Operational risk**: high. The 2.0.0 line is dual-target by design.
A change that breaks ESP8266 (e.g. an OTDirect helper that leaks past
its `#if HAS_DIRECT_OT` guard via ctags) costs the maintainer one local
build-cycle to reproduce and one more to debug. With CI, the same
failure surfaces in 4 minutes on a clean Ubuntu runner with full logs.

**Recommendation**: add `.github/workflows/build.yml` with two jobs
(matrix: `[esp8266, esp32]`), each running:
```yaml
- run: pip install platformio
- run: pio run -e ${{ matrix.env }}
- uses: actions/upload-artifact@v4
  with: { name: fw-${{ matrix.env }}-${{ github.run_id }}, path: .pio/build/${{ matrix.env }}/firmware.* }
```
Cost: ~6-8 min wall-clock per PR (PlatformIO toolchain caches well).
Yield: every PR carries downloadable, flashable artefacts and the
two-target compile contract is enforced before merge.

### 4B-H3 — `MERGED_APP_SIZE` constant drifted from active partition table

**File**: `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h:137`
**File**: `src/OTGW-firmware/partitions.csv:10` (stale duplicate)

Active partition table (`partitions_otgw_esp32.csv:14`):
```
app0, app, ota_0, 0x10000, 0x1E0000
```
ESP32 OTA upload server (`OTGW-ModUpdateServer-esp32.h:135-137`):
```cpp
static constexpr size_t MERGED_APP_OFFSET = 0x10000;
static constexpr size_t MERGED_APP_SIZE   = 0x2E0000;  // app0 partition size
```
The comment claims it matches the partition table; it does not. The
2.875 MB constant is left over from before the 4M2M layout switch
(documented in `build.py:99-104` as the previous size). When a user
uploads a merged binary the code path at line 207 calls
`Update.begin(MERGED_APP_SIZE, U_FLASH)` requesting 2.875 MB on a
partition that is 1.875 MB — the request would be rejected by
`Update.begin()` with "Not enough space to begin OTA", surfacing as a
flash error rather than a clean failure mode.

There is also a stale partitions file at `src/OTGW-firmware/partitions.csv`
that still describes the old layout (`0x2E0000` app slot). The active
CSV is `partitions_otgw_esp32.csv` at repo root — the stale one
appears to be unused but is committed and will mislead anyone reading
the source tree.

**Operational risk**: high. Merged-binary OTA uploads are the path
that a user takes when downloading the GitHub release ZIP. The failure
is not silent corruption — `Update.begin()` does range-check — but it
is a working-under-test path that breaks at user-flash time.

**Recommendation**:
1. Change `MERGED_APP_SIZE` to `0x1E0000` to match
   `partitions_otgw_esp32.csv`.
2. Delete or update the stale `src/OTGW-firmware/partitions.csv`.
3. Add a build-time `static_assert` or evaluate.py check that
   `MERGED_APP_SIZE` parsed from the header matches the active CSV.
   Pattern matches ADR-080's "binding rules need a CI gate" rule —
   this constant is a binding configuration with no automated check.

## Medium findings

### 4B-M1 — "Safe OTA rollback" is documented but not implementable on current partition layout

**File**: `partitions_otgw_esp32.csv:1-16`
**File**: `platformio.ini:175-177`
**File**: `CHANGELOG.md:41` ("OTA-capable with 2x 1.5MB app + 768KB LittleFS")

The CSV header comment is honest:
> "single app (no OTA), 4MB flash"
> "To enable proper OTA (two app slots) a board with 8MB flash is required."

But `platformio.ini:175-177` claims:
> "OTA-capable with 2x 1.5MB app + 768KB LittleFS"
> "Supports safe OTA rollback if an update fails"

This contradicts the CSV. The actual layout has one `ota_0` slot, no
`ota_1`, no `factory` slot — there is no second app to roll back to. A
bad OTA leaves the device in `app0` with broken firmware until USB
re-flash. The codebase also has no calls to
`esp_ota_mark_app_valid_cancel_rollback_and_reboot()` or any
boot-failure detection — verified via grep: zero matches for
`esp_ota_mark_*`, `valid_pending`, or `ota_set_boot` across `src/`.

The browser-side OTA flow at `updateServerHtml.h:506` does poll
`/api/v2/health` after reboot to confirm the device came back, but
this is a UX confirmation, not the ESP-IDF rollback mechanism. If the
new firmware crashes on boot before the HTTP server starts, the user
sees a failed health check and the device sits in `app0` with the
crashing firmware — they still need a USB cable to recover.

**Operational risk**: medium. The single-slot reality matches the
ESP8266 model the project has lived with for years; users have a
working recovery path (USB re-flash via `flash_otgw.bat`/`.sh`).
The risk is the documentation contradiction: a user reading
`platformio.ini` thinks rollback is automatic and may skip recovery
prep. Also the ESP32 partition layout silently forecloses ever
implementing rollback without a flash-size bump.

**Recommendation**: pick one of (a) update the `platformio.ini`
comment to match the CSV header (single-slot, USB recovery), or (b)
adopt an 8MB flash variant and a real OTA dual-slot layout
(`partitions_otgw_esp32_8mb.csv` + selectable build flag), then wire
`esp_ota_mark_app_valid_cancel_rollback_and_reboot()` into the
post-boot health-check flow. Option (a) is the YAGNI choice given
NodoShop ships 4MB modules; option (b) is the right choice if a
hardware refresh is on the roadmap.

### 4B-M2 — `otCmdQueueHighWater` is module-static, not exposed via REST or MQTT

**File**: `src/OTGW-firmware/OTDirect.ino:272`

```cpp
static uint8_t  otCmdQueueHighWater = 0;  // TASK-494: peak depth observed
```

This counter is the only quantitative signal that ADR-068's queue depth
(12 entries) is or is not being exhausted under real load. It is
incremented at `OTDirect.ino:328` but never read — confirmed by full-tree
grep: zero callers outside `OTDirect.ino` itself. The CHANGELOG entry
for TASK-494 claims it is "exposed via telnet debug for diagnostic
visibility" but the diagnostic surface for non-developers (REST
`/api/v2/state`, MQTT discovery topics) does not see it. Heap-tier
counters next to it (`hd_enter_low`, `hd_enter_warning`,
`hd_enter_critical` at `restAPI.ino:1588-1590`) are exposed correctly —
the same pattern should apply.

**Operational risk**: medium. The counter exists, the data is correct,
but a user reporting "OT commands occasionally drop" cannot give the
maintainer the high-water value over Discord without learning telnet.
The diagnostic that motivated the field name is invisible to the
people who would benefit from it most.

**Recommendation**: add one line to `restAPI.ino` near the heap-tier
block:
```cpp
sendJsonMapEntry(F("ot_cmd_queue_high_water"), otCmdQueueHighWater);
```
Requires exposing `otCmdQueueHighWater` via `extern` in
`OTGW-firmware.h` or a getter — the latter is cleaner per the
encapsulating-globals pattern in ADR-044. ~10 lines total.

### 4B-M3 — No dependency vulnerability scan on `lib_deps`

**File**: `platformio.ini:26-40, 156-165`

`lib_deps` pins 13+ external libraries (PubSubClient, WiFiManager,
TelnetStream, NimBLE-Arduino, EthernetESP32, …). There is no Dependabot
config, no CodeQL workflow, no `safety` scan, no equivalent. The
project is on the receiving end of a supply-chain risk equal to the
union of those libraries' transitive dependencies. The phase-2 review
noted "NimBLE-Arduino 2.5.0 has no known CVEs at audit time" — this
was a manual one-shot check; there is no recurring mechanism.

For embedded firmware on a trusted LAN the practical risk is small
(no internet-facing sockets except the MQTT/HTTP that the operator
configures), but ESP32 BLE / WiFi stacks have had real CVEs
(BLEEDINGTOOTH, WPA2 KRACK, etc.) and pinned versions can drift behind
patched releases without anyone noticing.

**Operational risk**: medium. Highest sensitivity is the BLE host
stack (NimBLE) which receives untrusted broadcaster input over RF.

**Recommendation**: add a minimal `dependabot.yml` covering the
`pip` + `github-actions` ecosystems first (cheap, no false positives).
For `platformio` lib_deps the upstream tool support is weaker — a
quarterly manual review of `pio pkg outdated -e esp32` and
`pio pkg outdated -e esp8266` plus changelog-spot-check is realistic.
Document the cadence in `docs/process/`.

### 4B-M4 — No SHA256 / signature on published release artefacts

**File**: `docs/guides/pr-checklist.md:66`

The PR checklist says:
> "`flash_esp.py` downloads the release, SHA256 passes if the release
> publishes `SHA256SUMS`."

The conditional ("if") flags the gap. There is no automated step that
publishes `SHA256SUMS` next to the firmware.bin / littlefs.bin on the
GitHub release. Field users running `flash_esp.py` against an arbitrary
release have no integrity check to fall back on. For a firmware that
takes administrative control of someone's central heating, a tampered
binary is a non-trivial concern even on a trusted-LAN posture.

**Operational risk**: medium. The dominant attack model is still
"someone uploads a wrong file to a release by mistake", which is
real-world but rare.

**Recommendation**: add a release-time CI step:
```yaml
- run: |
    cd dist
    sha256sum *.bin > SHA256SUMS
- uses: softprops/action-gh-release@v2
  with: { files: dist/* }
```
Combined with 4B-H2 (CI builds), this gives every release a verifiable
trail with no extra maintainer effort.

### 4B-M5 — No formal incident-response runbook for hardware-only verification gates

**File**: `backlog/tasks/task-487*.md`, `task-488*.md`, `task-494*.md`

The session's TASK-487, TASK-488, TASK-494 each have AC #11 of the form
"Verified on real OTGW32 hardware". These are the gates that build-clean
+ evaluate.py + comprehensive-review cannot replace. The backlog records
the AC was checked (`-x`) but does not capture: (a) which device, (b)
which firmware version was running before, (c) what tests were run, (d)
who ran them. When a regression appears 3 weeks later in the field, the
provenance trail is "one person's memory plus the Discord log".

The PR checklist (`docs/guides/pr-checklist.md`) has the right
categorical thinking ("OTA / flash path changes", "MQTT / Home
Assistant discovery changes") but no template for capturing the
verification evidence (device ID, firmware-pre, firmware-post, tests
run, mosquitto_sub log excerpt).

**Operational risk**: medium. Single-maintainer projects survive on
this informally; add a second contributor and the cost of "what was
verified on which device when" multiplies. The 2.0.0 line is the right
moment to formalise.

**Recommendation**: add a one-page hardware-verification log template
under `docs/process/` (or wire it into the backlog task body via
`mcp__backlog__task_edit --append-notes`). Three rows: device-ID,
pre-firmware, post-firmware, tests-checked. Costs ~3 minutes per
hardware AC, captures permanent provenance.

## Low findings

- **4B-L1**: `claude-code-review.yml` and `claude.yml` both call
  `anthropics/claude-code-action@v1` with no SHA pin — a moving v1 tag
  is convenient for the Claude pipeline but means the CI behaviour can
  change between PRs without a commit in this repo. Consider pinning
  to a specific SHA or `@vN.N.N` and updating intentionally.
- **4B-L2**: `evaluate.yml` uses `actions/setup-python@v5` with
  `python-version: "3.x"` (floating). Pin to `"3.12"` or whatever the
  evaluate.py / build.py target is to prevent silent CPython upgrades
  changing regex/`pathlib` behaviour. Cost: zero. Yield: reproducibility.
- **4B-L3**: There is no workflow that runs `python tests/test_evaluate.py`
  or `python tests/test_build.py` on PR. They are listed in
  `pr-checklist.md` as a baseline gate but only the maintainer
  remembers to run them. They add ~5 seconds to CI; should be a
  follow-up step in `evaluate.yml`.
- **4B-L4**: `Makefile:20` still pins ESP8266 platform URL to 2.7.4
  (Arduino Core 2.7.4) which is correct per ADR memory but is parallel
  data to `platformio.ini:64`'s `espressif8266@2.6.3` pin and to the
  copilot-instructions.md narrative. Three sources of truth for the
  same fact ("ESP8266 line uses Core 2.7.4 LTS") with no automated
  consistency check; refactor candidate.
- **4B-L5**: `partitions.csv` (stale duplicate at
  `src/OTGW-firmware/partitions.csv`) is not referenced by either
  build path. Delete it. (See 4B-H3.)
- **4B-L6**: `build.py` is 2,505 lines and does flashing, building,
  asset gzipping, image verification, manifest generation, and SHA
  computation. Single-script-as-Swiss-Army-knife is fine for embedded
  but the test surface (`tests/test_build.py` is 168 lines) covers a
  small fraction of what the script can do. Not a CI/CD gap per se,
  but the script is closer to "infrastructure code" than "build
  helper" and warrants a dedicated review pass against IaC review
  norms.
- **4B-L7**: Settings persistence has no schema version field. The
  CHANGELOG documents semantic shifts (`iBleInterval` repurposed
  TASK-494) but the on-disk format is unversioned. `MQTTstuff.ino`
  contains a one-shot V2 migration helper (`V2_MIGRATION_WINDOW_MS`
  retained-cleanup), which is a workaround at the MQTT layer. A
  proper `iSettingsVersion` field plus a forward-compat migration
  block in `settingStuff.ino` would make future shifts safer.
  Out-of-scope for this session but worth a backlog task.

## Strengths observed

- **Working `evaluate.py --quick` gate**: 30+ named checks, all named
  `check_*`, exit code disciplined, JSON report uploaded as artefact
  on every PR run. ADR-080's "binding ADRs need a CI gate" is honoured
  for ADR-088, ADR-089, ADR-091 with the gate names called out
  explicitly in CLAUDE.md.
- **OpenTherm v4.2 spec audit**: separate workflow runs a Python audit
  against the OT v4.2 reference spec and uploads the matrix CSV — a
  real spec-driven check, rare in firmware projects.
- **Hand-curated `pr-checklist.md`**: the categorical structure
  (Frontend / MQTT-HA / Auth / OTA / OT-PIC / Build / Settings / ADR)
  reflects deep operator knowledge about what build-clean misses.
  This is the highest-leverage single document in the repo for a new
  contributor.
- **`build.py` as build-IaC**: replicates CI locally, cross-platform
  (Windows/macOS/Linux), retries network ops with exponential
  backoff, supports incremental builds via PlatformIO object cache.
  The pattern of the local script being the same script CI would run
  is the right shape; the gap is just that CI is not actually running
  `pio run` (4B-H2).
- **OTA UX in browser**: `updateServerHtml.h:506` does poll
  `/api/v2/health` post-reboot to confirm device came back. Not the
  ESP-IDF rollback mechanism but a real UX-level health-check that
  catches "OTA succeeded then device died" cases.
- **CHANGELOG hygiene**: the post-Phase-3 sweep (commit `ccee66ed`)
  filled in the `[Unreleased]` block with the 8 session commits
  including user-visible behaviour changes (TT vs TC, iBleInterval
  semantic shift, NimBLE port). Closes 3B-H3.
- **PROGMEM gate self-test**: `tests/test_evaluate.py` (520 lines)
  unit-tests the `evaluate.py` checks themselves — meta-testing of
  the gate. This is uncommon and valuable; the build.py side could
  follow the same pattern at scale.
- **Centralised settings persistence with documented Hungarian
  prefixes (ADR-051)**: while there is no version field (4B-L7), the
  two-level encapsulating struct shape means future migrations have a
  single point to wire schema-version handling into. The ground for
  4B-L7's improvement is well prepared.

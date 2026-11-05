# SAT research scripts

Simulation studies and analysis scripts behind the SAT rounds under `docs/sat/` (curve self-tune TASK-1519,
heat-pump low load TASK-1527, the TASK-1412 studies, the housing-stock rounds). They are research tools, not
operational tooling: nothing here runs in the build, the bench loop or a release.

Moved here from `scripts/` by TASK-1634 (2026-11-05). The operational SAT tools stay in `scripts/`:
`sat_boiler_emulator.py`, `sat_log_to_csv.py`, `sat_corrections_index.py`, `sat_open_questions.py`,
`sat_bench_scenarios_1412_20.py` and `sat_transport_delay_1412_5.py`.

Run a study from the repo root, for example `python tools/research/sat/sat_comfort_band_study25.py`. The scripts
import each other from this folder (each one puts its own folder on `sys.path`), so keep them together.

Records written before the move (`docs/sat/sergeant-d-corrections.md`, Accepted ADRs such as ADR-205, backlog
task notes) still name the old `scripts/` paths. They are immutable history: read `scripts/sat_X.py` there as
`tools/research/sat/sat_X.py`. Script hashes stored in a round's `meta` were taken from the file at its old path
and content; the move changed only path strings in docstrings and comments.

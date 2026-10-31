# Host test runner

`scripts/run_tests.py` (Python stdlib only) runs the host test suite in parallel.
Every `tests/test_*.py` file is started as its own subprocess (each file already
runs standalone with `python tests/test_x.py`), so coverage is the same as running
them one by one, only faster.

```bash
python scripts/run_tests.py                  # all files, 4 in parallel, 600 s timeout each
python scripts/run_tests.py -j 8 --timeout 900
python scripts/run_tests.py sat_pid          # only files whose name contains "sat_pid"
python scripts/run_tests.py --changed        # only tests affected by git diff origin/dev (+ untracked)
python scripts/run_tests.py --changed HEAD~3 --list   # show the selection, do not run
python scripts/run_tests.py --webui          # also node --test tests/webui/*.test.mjs
```

Output is one line per file (`PASS`, `FAILED`, `SKIP`, `TIMEOUT`, seconds), then the last
40 lines of every failing file, then a summary (files, passed, failed, total time).
Exit code is 1 when any file fails or times out. `SKIP` means a file ran zero tests.
A timed-out file is killed together with its child processes (compilers).
Node tests count as `PASS` when the exit code is 0 and the output has `ℹ fail 0`.

## `--changed [REF]`

Selects tests from `git diff --name-only REF` (default `origin/dev`, includes uncommitted
edits) plus untracked files. Changes under `docs/`, `backlog/`, `.wolf/`, `.claude/` and
`*.md` are ignored. A test is selected when:

1. the test file itself changed,
2. its source text mentions the basename of a changed file (`SATcontrol.ino`, `v2.js`), or
3. its name shares a word of 4+ letters with the changed file (`otgw_capture.py` selects
   `test_capture_*`).

The runner prints which tests were selected and why. When nothing maps, it runs the full
suite. The selection is a heuristic for fast feedback: run the full suite before a push.

## Parallel safety

Tests that compile C++ use their own temporary directories, so they do not collide.
Files that prove to collide are listed in `SERIAL_ONLY` in the script and run one by one
after the pool. See the measurements recorded with the task that introduced the runner.

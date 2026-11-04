#!/usr/bin/env python3
"""PostToolUse hook: run `evaluate.py --quick` after an edit to firmware source (TASK-1627).

Reads the Claude Code hook payload (JSON) on stdin. For an edited .ino/.cpp/.h file it runs
evaluate.py in the git worktree that holds that file, so a session working in a second
worktree checks its own tree. Before TASK-1627 this hook was an inline command with a
hardcoded `cd 'D:/.../OTGW-firmware'` and a trailing `|| true`: on any other machine or
worktree it checked the wrong tree or nothing, and a FAIL never reached the agent.

Exit codes (Claude Code hook contract):
  0  nothing to report: not a firmware file, evaluate passed or only warned, or evaluate
     could not run (reported on stdout, never blocks).
  2  evaluate.py reported FAIL: the FAIL lines go to stderr, which Claude Code feeds back to
     the agent. The edit itself stays; the agent is expected to fix the violation.
"""
import json
import os
import subprocess
import sys

FIRMWARE_SUFFIXES = (".ino", ".cpp", ".h")
TIMEOUT_S = 25  # the hook entry in .claude/settings.json allows 30 s


def edited_file(payload):
    tool_input = payload.get("tool_input") or {}
    tool_response = payload.get("tool_response") or {}
    return tool_input.get("file_path") or tool_response.get("filePath") or ""


def repo_root(path):
    start = os.path.dirname(os.path.abspath(path)) if path else os.getcwd()
    try:
        out = subprocess.run(["git", "-C", start, "rev-parse", "--show-toplevel"],
                             capture_output=True, text=True, timeout=5)
        if out.returncode == 0 and out.stdout.strip():
            return out.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass
    return os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()


def main():
    try:
        payload = json.load(sys.stdin)
    except (ValueError, OSError):
        return 0
    path = edited_file(payload)
    if not path.endswith(FIRMWARE_SUFFIXES):
        return 0
    root = repo_root(path)
    if not os.path.exists(os.path.join(root, "evaluate.py")):
        return 0
    try:
        run = subprocess.run([sys.executable, "evaluate.py", "--quick", "--no-color"], cwd=root,
                             capture_output=True, text=True, encoding="utf-8", errors="replace",
                             timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        print(f"post-edit-evaluate: evaluate.py --quick did not finish within {TIMEOUT_S} s; run it by hand")
        return 0
    except OSError as exc:
        print(f"post-edit-evaluate: could not run evaluate.py ({exc})")
        return 0
    if run.returncode != 1:  # 0 = pass, 2 = only warnings
        return 0
    lines = [l for l in run.stdout.splitlines() if "FAIL" in l or "\u2717" in l]
    print(f"evaluate.py --quick reported FAIL in {root} after editing {os.path.basename(path)}:",
          file=sys.stderr)
    print("\n".join(lines[:20]) or run.stdout[-1500:], file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())

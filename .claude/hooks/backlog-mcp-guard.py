#!/usr/bin/env python3
"""PreToolUse guard: enforce Backlog.md MCP-over-CLI policy.

Hard-blocks two patterns against the project's task tracker:
  1. Direct Edit/Write/MultiEdit on backlog/tasks/*.md
     (CLAUDE.md CRITICAL rule: never edit those files by hand).
  2. Bash invocations of the `backlog` CLI when the MCP server is the
     preferred interface for this project.

Both blocks emit a stderr hint listing MCP equivalents so the model can
self-correct without another round-trip.

Exit codes:
  0  allow
  2  deny (Claude Code surfaces stderr to the model)

Hook payload (JSON on stdin):
  {
    "tool_name": "Edit" | "Write" | "MultiEdit" | "Bash" | ...,
    "tool_input": {
      "file_path": "...",        # for Edit/Write/MultiEdit
      "command":   "..."         # for Bash
    }
  }
"""
import json
import re
import sys


def _block(reason: str) -> None:
    sys.stderr.write(reason + "\n")
    sys.exit(2)


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        # Fail-open: a malformed hook payload should never block real work.
        sys.exit(0)

    tool_name = payload.get("tool_name", "")
    tool_input = payload.get("tool_input") or {}

    if tool_name in ("Edit", "Write", "MultiEdit"):
        file_path = (tool_input.get("file_path") or "").replace("\\", "/")
        if re.search(r"(?:^|/)backlog/tasks/[^/]+\.md$", file_path, re.IGNORECASE):
            _block(
                "Blocked: never edit backlog/tasks/*.md directly "
                "(CLAUDE.md CRITICAL rule).\n"
                "Use the Backlog MCP server instead:\n"
                "  - mcp__backlog__task_create   add a task\n"
                "  - mcp__backlog__task_edit     mutate fields, ACs, plan, notes\n"
                "  - mcp__backlog__task_complete flip Done\n"
                "  - mcp__backlog__task_archive  retire\n"
                "  - mcp__backlog__task_view     read-only inspect"
            )

    if tool_name == "Bash":
        command = tool_input.get("command", "") or ""
        # Match `backlog <subcmd>` only when it sits at a real command
        # boundary: start of the line, after `;` `&` `|` `(` `` ` `` or a
        # newline. This blocks genuine CLI invocations while leaving alone:
        #   - paths       (cd backlog/tasks, ls backlog/, cat backlog/foo.md)
        #   - flags       (backlog --version, backlog -h)
        #   - vars        (export BACKLOG_FOO=...)
        #   - filenames   (git add backlog-readme.md)
        #   - quoted text (echo "backlog task X", grep "backlog" file)
        # Subshells `$(backlog ...)` and backticks both hit the `(` / `` ` ``
        # boundary, so those still block correctly.
        cli_pattern = re.compile(
            r"(?:^|[\n;&|()`])\s*backlog\s+\w",
            re.IGNORECASE,
        )
        if cli_pattern.search(command):
            _block(
                "Blocked: prefer the Backlog MCP server over the `backlog` CLI.\n"
                "MCP equivalents (no shell-escape pain, structured args):\n"
                "  - backlog search/list   -> mcp__backlog__task_search / task_list\n"
                "  - backlog task view     -> mcp__backlog__task_view\n"
                "  - backlog task create   -> mcp__backlog__task_create\n"
                "  - backlog task edit     -> mcp__backlog__task_edit\n"
                "  - backlog task complete -> mcp__backlog__task_complete\n"
                "  - backlog task archive  -> mcp__backlog__task_archive\n"
                "  - backlog doc/milestone -> mcp__backlog__document_* / milestone_*\n"
                "If MCP is genuinely down, edit .claude/settings.json to disable\n"
                "this hook for the duration."
            )

    sys.exit(0)


if __name__ == "__main__":
    main()

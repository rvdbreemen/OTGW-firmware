#!/usr/bin/env python3
"""PreToolUse guard: prevent direct editing of Backlog.md task files.

Hard-blocks one pattern:
  1. Direct Edit/Write/MultiEdit on backlog/tasks/*.md
     (CLAUDE.md CRITICAL rule: never edit those files by hand).
     Always use the `backlog` CLI instead.

The guard does NOT block `backlog` CLI invocations — CLI is the
preferred interface for all task operations (see CLAUDE.md).

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

    sys.exit(0)


if __name__ == "__main__":
    main()

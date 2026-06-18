#!/usr/bin/env python
"""One-shot (TASK-881): add `security: [basicAuth]` to every /v2 POST/PUT
operation that lacks it. Firmware gates all v2 POST/PUT centrally
(restAPI.ino H5 check), so the spec must declare auth on those operations.
GET stays open; DELETE bypasses the central gate so it is left untouched."""
import re, sys

path = "docs/api/openapi.yaml"
lines = open(path, encoding="utf-8").read().split("\n")

# Identify operation boundaries. An operation starts at "    <method>:" (4 spaces)
# and ends at the next 4-space "    <method>:", a 2-space "  /path:", or a
# top-level key. We only act inside the paths section.
op_start = re.compile(r"^    (get|post|put|delete|patch|options|head):\s*$")
boundary = re.compile(r"^(    (get|post|put|delete|patch|options|head):|  \S|\S)")

inserts = []  # (line_index_to_insert_before, indent_method)
i = 0
n = len(lines)
while i < n:
    m = op_start.match(lines[i])
    if m and m.group(1) in ("post", "put"):
        # scan this operation for an existing 6-space "security:"
        j = i + 1
        has_sec = False
        while j < n:
            if boundary.match(lines[j]) and not lines[j].startswith("      "):
                break
            if lines[j] == "      security:":
                has_sec = True
                break
            j += 1
        if not has_sec:
            inserts.append(i + 1)  # insert right after the method line
    i += 1

# apply bottom-up so indices stay valid
block = ["      security:", "        - basicAuth: []"]
for idx in sorted(inserts, reverse=True):
    lines[idx:idx] = block

open(path, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
sys.stderr.write("inserted security on %d operations\n" % len(inserts))

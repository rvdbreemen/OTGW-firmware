---
id: TASK-905
title: 'fix(build): build.bat parse-time BASE_PYTHON expansion broke venv bootstrap'
status: To Do
assignee: []
created_date: '2026-06-23 16:06'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.bat printed 'ERROR: Python 3 not found' even with python/py -3 on PATH. Root cause: the venv-bootstrap lived in an if-block where %BASE_PYTHON% (set by :find_python inside the same block) expands at parse time = empty, so the venv command no-opped and .build-venv was never created. Moved bootstrap to a flat :bootstrap_build_venv subroutine so BASE_PYTHON expands at runtime.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.bat finds python and creates .build-venv on a clean tree
- [ ] #2 No 'Python 3 not found' when python/py -3 is on PATH
<!-- AC:END -->

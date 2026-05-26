---
id: TASK-715
title: 'adr-kit Feature 6: Multi-Language Script Generation'
status: To Do
assignee: []
created_date: '2026-05-26 13:42'
labels:
  - adr-kit
  - phase-3
  - week-6
  - generation
  - multi-language
milestone: v0.15.0
dependencies: []
references:
  - bin/adr-generate-scripts
  - .generated/
  - skills/init/SKILL.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement bin/adr-generate-scripts tool to produce standalone validation scripts in Go, Rust, and shell. Scripts are executable without adr-kit dependency and validate against same Enforcement rules as adr-judge.

PLAN - WEEK 6 (v0.15.0 Phase 3):

1. Core Tool (DONE - skeleton):
   - bin/adr-generate-scripts created (~350 lines)
   - Framework for 3 language generators
   - Output to .generated/ directory

2. Implement generate_go_script():
   - ~80 lines of Go code
   - checkForbidImport() function
   - Reads git diff or file
   - Validates patterns via regex
   - Returns exit code 0/1

3. Implement generate_rust_script():
   - ~80 lines of Rust code
   - Uses regex crate
   - Standalone executable
   - Same validation logic as Go
   - Error handling

4. Implement generate_shell_script():
   - ~50 lines of shell
   - Uses grep for validation
   - Portable (bash compatible)
   - Fallback for any language
   - Simple and reliable

5. Script Requirements:
   - 100% standalone (no adr-kit imports)
   - No external dependencies (besides regex crate for Rust)
   - Same Enforcement rules as adr-judge
   - Executable in CI/CD pipelines
   - Parse patterns from ADR YAML

6. Integration:
   - skills/init/SKILL.md: "Generate validation scripts? (Y/n)"
   - Output scripts to .generated/ by default
   - Scripts inherit Enforcement from ADRs

7. Testing (15+ test cases):
   - Go script generation and compilation
   - Rust script generation and compilation
   - Shell script generation and execution
   - Scripts are standalone
   - Scripts validate same rules
   - CI/CD integration
   - And more

DELIVERABLES:
✓ bin/adr-generate-scripts (~350 lines)
→ templates/validate_adr_template.go (~80 lines)
→ templates/validate_adr_template.rs (~80 lines)
→ templates/validate_adr_template.sh (~50 lines)
→ skills/init/SKILL.md (script prompt)
→ tests/test_adr_generate_scripts.py (15+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/adr-generate-scripts tool created with all 3 generators
- [ ] #2 Go template ~80 lines with checkForbidImport()
- [ ] #3 Rust template ~80 lines with regex crate
- [ ] #4 Shell template ~50 lines with grep
- [ ] #5 Scripts are 100% standalone
- [ ] #6 Scripts validate same rules as adr-judge
- [ ] #7 Scripts tested in CI/CD
- [ ] #8 skills/init/SKILL.md updated
- [ ] #9 15+ test cases passing
- [ ] #10 All existing tests pass
<!-- AC:END -->

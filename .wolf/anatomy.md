# anatomy.md

> Auto-maintained by OpenWolf. Last scanned: 2026-05-27T13:22:22.335Z
> Files: 17 tracked | Anatomy hits: 0 | Misses: 0

## ./

- `build.sh` (~1471 tok)
- `evaluate.py` — drift: strip_css_comments, strip_js_comments, extract_classes_from_html, extract_classes_from_js + 5 (~36584 tok)

## .claude/

- `adr-kit-guide.md` — Project-local adr-kit reference guide; used by pre-commit hook and CI agents that lack plugin context (~3500 tok)

## .claude/commands/


## .claude/docs/


## .claude/hooks/


## .claude/rules/


## .claude/skills/firmware-design-handoff/


## .claude/skills/flash/


## .claude/skills/release/


## .claude/skills/update-docs/


## .copilot-tracking/research/


## .external-reviews/


## .full-review-archive-20260421-085044/


## .full-review/


## .full-review/archive/


## .full-review/archive/2026-04-16-dev-merge/


## .githooks/

- `pre-commit` — Pre-commit hook: firmware version-bump check + adr-kit ADR-compliance check (adr-judge, declarative + LLM pass) (~900 tok)

## bin/

- `bump-prerelease.sh` — Increments the trailing integer in `_VERSION_PRERELEASE` in version.h and regenerates version.hash (~500 tok)
- `adr-audit` — adr-kit v0.15.0: scans project root for decision-shaped artefacts; feeds /adr-kit:init (~5000 tok)
- `adr-context` — adr-kit v0.15.0: generates context summary of relevant ADRs for a given diff (~3000 tok)
- `adr-generate-scripts` — adr-kit v0.15.0: generates validate.sh shell scripts from Enforcement blocks (~3000 tok)
- `adr-judge` — adr-kit v0.15.0: checks staged diff against Enforcement blocks; pre-commit hook + /adr-kit:judge; security fixes: llm_cmd allowlist, ReDoS guard, path traversal protection (~6000 tok)
- `adr-lint` — adr-kit v0.15.0: validates ADR content against four verification gates (~6000 tok)
- `adr-quality` — adr-kit v0.15.0: quality score per ADR (~3000 tok)
- `adr-retire` — adr-kit v0.15.0: archives/deprecates an ADR with correct status update (~2000 tok)
- `adr-status` — adr-kit v0.15.0: overview of ADR statuses in the project (~2000 tok)

## .github/


## .github/actions/setup/


## .github/agents/


## .github/instructions/


## .github/prompts/


## .github/skills/adr/


## .github/skills/algorithmic-art/


## .github/skills/algorithmic-art/templates/


## .github/skills/brand-guidelines/


## .github/skills/canvas-design/


## .github/skills/canvas-design/canvas-fonts/


## .github/skills/doc-coauthoring/


## .github/skills/docx/


## .github/skills/docx/scripts/


## .github/skills/docx/scripts/office/


## .github/skills/docx/scripts/office/helpers/


## .github/skills/docx/scripts/office/schemas/ISO-IEC29500-4_2016/


## .github/skills/docx/scripts/office/schemas/ecma/fouth-edition/


## .github/skills/docx/scripts/office/schemas/mce/


## .github/skills/docx/scripts/office/schemas/microsoft/


## .github/skills/docx/scripts/office/validators/


## .github/skills/docx/scripts/templates/


## .github/skills/frontend-design/


## .github/skills/internal-comms/


## .github/skills/internal-comms/examples/


## .github/skills/mcp-builder/


## .github/skills/mcp-builder/reference/


## .github/skills/mcp-builder/scripts/


## .github/skills/pdf/


## .github/skills/pdf/scripts/


## .github/skills/pptx/


## .github/skills/pptx/scripts/


## .github/skills/pptx/scripts/office/


## .github/skills/pptx/scripts/office/helpers/


## .github/skills/pptx/scripts/office/schemas/ISO-IEC29500-4_2016/


## .github/skills/pptx/scripts/office/schemas/ecma/fouth-edition/


## .github/skills/pptx/scripts/office/schemas/mce/


## .github/skills/pptx/scripts/office/schemas/microsoft/


## .github/skills/pptx/scripts/office/validators/


## .github/skills/refactor/


## .github/skills/skill-creator/


## .github/skills/skill-creator/references/


## .github/skills/skill-creator/scripts/


## .github/skills/template-skill/


## .github/skills/theme-factory/


## .github/skills/theme-factory/themes/


## .github/skills/web-artifacts-builder/


## .github/skills/web-artifacts-builder/scripts/


## .github/skills/webapp-testing/


## .github/skills/webapp-testing/examples/


## .github/skills/webapp-testing/scripts/


## .github/skills/xlsx/


## .github/skills/xlsx/scripts/


## .github/skills/xlsx/scripts/office/


## .github/skills/xlsx/scripts/office/helpers/


## .github/skills/xlsx/scripts/office/schemas/ISO-IEC29500-4_2016/


## .github/skills/xlsx/scripts/office/schemas/ecma/fouth-edition/


## .github/skills/xlsx/scripts/office/schemas/mce/


## .github/skills/xlsx/scripts/office/schemas/microsoft/


## .github/skills/xlsx/scripts/office/validators/


## .github/workflows/


## .pio/libdeps/esp32/


## .pio/libdeps/esp32/AceCommon/


## .pio/libdeps/esp32/AceCommon/.github/workflows/


## .pio/libdeps/esp32/AceCommon/examples/


## .pio/libdeps/esp32/AceCommon/examples/AutoBenchmark/


## .pio/libdeps/esp32/AceCommon/examples/MemoryBenchmark/


## .pio/libdeps/esp32/AceCommon/examples/TimingStats/


## .pio/libdeps/esp32/AceCommon/examples/UrlEncodingBenchmark/


## .pio/libdeps/esp32/AceCommon/src/


## .pio/libdeps/esp32/AceCommon/src/algorithms/


## .pio/libdeps/esp32/AceCommon/src/arithmetic/


## .pio/libdeps/esp32/AceCommon/src/backslash_x_encoding/


## .pio/libdeps/esp32/AceCommon/src/cstrings/


## .pio/libdeps/esp32/AceCommon/src/fstrings/


## .pio/libdeps/esp32/AceCommon/src/hash/


## .pio/libdeps/esp32/AceCommon/src/kstrings/


## .pio/libdeps/esp32/AceCommon/src/print_str/


## .pio/libdeps/esp32/AceCommon/src/print_utils/


## .pio/libdeps/esp32/AceCommon/src/pstrings/


## .pio/libdeps/esp32/AceCommon/src/timing_stats/


## .pio/libdeps/esp32/AceCommon/src/tstrings/


## .pio/libdeps/esp32/AceCommon/src/url_encoding/


## .pio/libdeps/esp32/AceSorting/


## .pio/libdeps/esp32/AceSorting/.github/workflows/


## .pio/libdeps/esp32/AceSorting/examples/


## .pio/libdeps/esp32/AceSorting/examples/AutoBenchmark/


## .pio/libdeps/esp32/AceSorting/examples/CompoundSortingDemo/


## .pio/libdeps/esp32/AceSorting/examples/HelloSorting/


## .pio/libdeps/esp32/AceSorting/examples/MemoryBenchmark/


## .pio/libdeps/esp32/AceSorting/examples/WorstCaseBenchmark/


## .wolf/


## C:/Users/rvdbr/.claude/projects/D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware/memory/

- `feedback_auto_advance_2_0_0.md` (~251 tok)
- `MEMORY.md` — OTGW-firmware Project Memory (~3223 tok)

## docs/adr/

- `ADR-094-ha-discovery-state-reconciliation-on-ota-upgrade.md` — ADR-094: Home Assistant discovery state reconciliation on OTA upgrade (feature-2.0.0 port of ADR-067 (~7153 tok)
- `ADR-095-bseparatesources-mutually-exclusive-base-and-source-variants.md` — ADR-095: bSeparateSources Makes Base and Source-Variant Entities Mutually Exclusive (feature-2.0.0 p (~5773 tok)
- `ADR-096-mqtt-source-topic-worldview-semantics.md` — ADR-096: MQTT Source-Subtopic Worldview Semantics (~6803 tok)
- `ADR-097-mqtt-publish-gating-by-source-and-slave-echo.md` — ADR-097: MQTT Publish Gating by Source and Per-MsgID Slave-Echo Classification (~3356 tok)
- `ADR-103-mqtt-source-topic-proxy-answer-routing.md` — ADR-103: MQTT Source-Topic Worldview Routing — Proxy-Answer (no-B) Refinement (~3192 tok)
- `ADR-112-pure-jit-mqtt-discovery.md` — ADR-112: Pure JIT MQTT Discovery (2.0.0 sibling of dev ADR-073) (~1235 tok)
- `README.md` — Project documentation (~10169 tok)

## docs/api/

- `MQTT.md` — OTGW-firmware MQTT Topic Documentation (~23412 tok)

## docs/audits/


## plan/


## src/OTGW-firmware/

- `MQTTHaDiscovery.cpp` — ======================================================================= (~66032 tok)
- `MQTTstuff.ino` — include <PubSubClient.h>           // MQTT client publish and subscribe functionality (~38556 tok)
- `OTGW-Core.h` — ifndef OTGWCore_h (~10661 tok)
- `OTGW-Core.ino` — Declares char (~68038 tok)

## src/OTGW-firmware/data/


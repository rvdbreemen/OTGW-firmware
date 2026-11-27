# First five minutes, recheck: README and the beta.451 release notes (2026-11-27)

Author: otgw-ux-designer. Follow-up to `docs/reviews/2026-11-20_ux-first-five-minutes/README.md` (R1-R7). Read at origin/dev 71a08d515 plus my local README commit. Findings only; the release notes belong to BlueFlame and otgw-docs. There is no beta.452 draft in `docs/releases/` yet (only `RELEASE_NOTES_2.0.0-draft.md`), so I read beta.451.

## What improved since 20 November

- README now opens with "Which firmware do you need?" (R1, Robert chose A) and the Quick Start sits right under the intro (R5).
- Steps 1 to 4 explain the erase, the USB port, the `/update` address, how to find the unit's IP and what a broker is (TASK-3044).
- README jargon is now replaced or explained and the spelling is WiFi throughout (TASK-3045, commit 21f5b1358).
- Release notes have 0 em-dashes.

## Still open

**N1 (high, R4 not done). The beta.451 notes bury the owner's instructions under test evidence.** Measured on `RELEASE_NOTES_2.0.0-beta.451.md`:

| What | Measure |
|---|---|
| Whole file | 1034 lines, about 32,000 words |
| The disclaimer box (lines 8 to 44) | 37 lines, about 6,600 words: one bullet of test evidence per beta range back to beta.77 |
| "A unit that already runs 2.0.0 updates over WiFi" (the instruction an owner needs) | line 631 |
| "What this beta is" (the 8 lines an owner needs first) | line 45, after the disclaimer |

A new owner reads the disclaimer, finds no instruction, and leaves or flashes blind. Proposed order, as in R4: a short head of about 15 lines (what this is, who it is for, the 3 known issues that matter, what changed, how to update), then the test provenance as an appendix. Keep the full provenance: it is evidence, just not first-five-minutes reading.

**N2 (medium). Internal jargon in the first 120 lines.** Counted in lines 1 to 120: task numbers 166 times, "host test" 53, "OT-Direct" 47, "PSRAM" 16, "soak" 11, "heap" 8, "discovery" 8, "PWM" 6, "MsgID" 5, "ADR" 2. The disclaimer is the main source. An owner of an OTGW Classic does not know that OT-Direct is a prototype board they do not own. Proposed: in the owner-facing head say "tested on bench units, not yet on long field runs" and keep task and ADR numbers in the appendix; define PWM and "retained" once, or drop them from the head.

**N3 (medium). The update instruction hides in "Before you flash" further down.** Line 631 is correct and plain, but it competes with 600 lines before it. Move it into the head (N1). The one-line warning from my 25 November review belongs next to it: the heating gets no help from the gateway for 1 to 2 minutes during an update (README step 1 already says it).

**N4 (low). The compare line is addressed to testers.** "What changed since beta.438 is new for a beta.438 user; a user of an older beta also reads the sections below it." An owner who is new has no beta.438. Add one sentence: "New here? Skip to How to update."

**N5 (low). Section order of "What changed since ...".** Each beta adds a full section on top; by beta.451 there are about 30 of them. Keep the latest two in the notes and link the older ones, or fold them into the appendix.

## Not verified

- I counted words and lines with `wc` and `grep`; I did not read all 1034 lines, only the head, the section list, and the update instruction.
- No non-technical reader read these notes; the severity is my judgement.
- No beta.452 draft exists in `docs/releases/`, so nothing is checked for 452. Apply N1 to it when it is written.
- The GitHub release page may render the notes differently from the file; I did not open the page.
- Whether BlueFlame's `/beta-release-2x` template produces this layout (R4 AC) was not read.

## Proposed AC for the next beta's notes (extends R4)

1. The first 20 lines after the title say what the beta is, who it is for, how to update and what is not tested, in plain words, with no task or ADR number.
2. The per-range test provenance moves to an appendix at the end; its content is unchanged.
3. "How to update" is in the first screen.

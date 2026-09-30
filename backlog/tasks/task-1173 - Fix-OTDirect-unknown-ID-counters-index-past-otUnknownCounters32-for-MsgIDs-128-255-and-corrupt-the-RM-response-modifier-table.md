---
id: TASK-1173
title: >-
  Fix: OTDirect unknown-ID counters index past otUnknownCounters[32] for MsgIDs
  128-255 and corrupt the RM response-modifier table
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 08:44'
updated_date: '2026-09-30 10:22'
labels:
  - bug
  - otdirect
  - memory-safety
dependencies: []
priority: medium
ordinal: 296000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Out-of-bounds RAM write (and read) in the OTDirect 3-strike unknown-ID logic. dev (2.0.0) only. Verified on HEAD 3621a3830 (OTDirect.ino has no uncommitted changes) and on today's esp32 and esp32-combo ELFs.

DEFECT
src/OTGW-firmware/OTDirect.ino:533 declares `static uint8_t otUnknownCounters[32] = {0};`, 2 bits per MsgID, covering 0-127. All three helpers index `otUnknownCounters[msgId >> 2]` with no bound:
- getUnknownCount :535-539 (read at :538)
- incUnknownCount :540-548 (read-modify-write at :543, :545-546)
- clearUnknownCount :549-553 (read-modify-write at :552)

MECHANISM
handleMasterResponse (:1277-1404) computes `uint8_t respMsgId = (response >> 16) & 0xFF;` (:1317), range 0-255, and passes it on unbounded:
- UNKNOWN_DATA_ID: incUnknownCount, then getUnknownCount (:1318-1321).
- Any other successful reply, i.e. READ_ACK or WRITE_ACK: clearUnknownCount (:1331-1333).

For MsgID 128-255 the byte index is 32-63. Each exchange therefore does a 2-bit read-modify-write up to 32 bytes past the array. clear always stores; inc saturates at 3.

The boiler-cache index a few lines above is masked (`uint8_t cacheId = (response >> 16) & 0x7F;`, :1292), so the omission is local to the counter path.

DATA_INVALID never gets here. The vendored library's isValidResponse accepts only READ_ACK, WRITE_ACK and UNKNOWN_DATA_ID (src/libraries/OpenTherm/src/OpenTherm.cpp:518-527, applied at :444).

The other two callers are bounded: AA= (check :3226, call :3237) and KI= (check :3380, call :3387).

TRIGGER
Preconditions:
- OTGW32 hardware running OTDirect: the esp32 target, or esp32-combo when boot detection selects OTDirect. The esp32-classic ELF contains no OTDirect symbols.
- Gateway mode (the default, OTDirecttypes.h:107 `uint8_t iMode = 1;`) or monitor mode.
- A thermostat sends READ_DATA or WRITE_DATA for a MsgID in 128-255, and the boiler answers READ_ACK, WRITE_ACK or UNKNOWN_DATA_ID.

Why nothing stops it:
- The thermostat-side slave accepts any parity-valid READ/WRITE, whatever the ID (OpenTherm.cpp:529-538; OTDirect.ino:602-606).
- Monitor mode forwards unconditionally (:1960-1965).
- Gateway mode forwards via :1996-2005. The UI and SR tables could intercept, but they only take IDs up to 127 (UI= :3365; SR= :3305, :3320, :3322).
- The library does not compare the reply's Data-ID with the request's (OpenTherm.cpp:444, :526).
- handleMasterResponse runs for every completed request (:1935-1936).

Not reachable through:
- Loopback: the early `return true` at :1230-1246 never sets otMasterRequestActive, so the gate at :1935 is never taken.
- Master mode for thermostat frames: they are answered locally (:1950-1957).
- Gateway-originated requests: otSchedule tops out at MsgID 127 (:250). The command handlers reject IDs above 127 (:3226-:3413), and so does REST (restAPI.ino:606).

A boiler reply whose Data-ID is 128 or higher and differs from the request would also get in, in any mode. That needs a non-compliant slave or bit errors that parity misses; the likelihood is unquantified.

IDs 128-255 are the OT v4.2 member Test and Diagnostic area (spec :1275-1279). Remeha qSense/Tzerra uses 131-133 (docs/opentherm specification/New OT data-ids.txt:20-23; OTGW-Core.h:505-507). No field report of such traffic on an OTGW32 exists yet.

IMPACT (build-specific, verified on the current ELFs)
Memory layout:
- The 32 bytes after the array are exactly otResponseModifiers[8], the RM= response-modifier table (:524-530). Each 4-byte slot is {uint8 msgId, bool active, uint16 value}.
- `xtensa-esp32s3-elf-nm -n -S` shows otUnknownCounters at 0x3fca54fc (size 0x20), immediately followed by otResponseModifiers at 0x3fca551c (size 0x20). That is the esp32-combo ELF built 10:09; esp32 (10:01) has the same adjacency.
- The compiled clearUnknownCount (combo, 0x42043334) is `srli a8,a2,2; add.n a9,a9,a8; ... s8i a8,a9,0` with base literal 0x3fca54fc. There is no compare before the store.

Mapping for MsgID N of 128 or higher: slot (N-128)>>4, byte (N>>2)&3, bits 2*(N&3).
- A 132 ACK clears slot0.active bits 0-1. The user's first RM rule silently switches off; slot 0 is the first free slot (:2573-2574).
- A 131 ACK clears slot0.msgId bits 6-7. An RM rule on MsgID 64-127 is retargeted to MsgID-64.
- UNKNOWN_DATA_ID on 132/133 raises slot0.active to 1..15; values above 1 are invalid bool representations. With an empty table and msgId still 0, the first strike creates a phantom rule {msgId 0, value 0x0000}. In gateway mode applyResponseModifiers (:2597-2607) would apply it to the thermostat's MsgID 0 replies (:1375-1387). What later strikes do depends on codegen.

Other effects:
- The RM table holds only plain data, so current builds are not expected to crash.
- The victim is chosen by LTO and link order, not source order: the source declares otResponseModifiers (:530) before otUnknownCounters (:533). Another build can place different state in the window.
- A mode change or GW=R clears the table and the counters (setOTDirectMode :2396 calls resetTransientState :2346, :2375). The next 128-255 exchange corrupts them again.

HISTORY
- Introduced in 96a7a4d6c (2026-04-04, "feat: PIC firmware command parity for OTDirect").
- TASK-151 audited this logic and called it "Confirmed correct", but it checked only the AA=/KI= bounds.
- TASK-1072 fixed the same 0-255 versus 128-entry mismatch in simulateLoopbackResponse (:1204-1215).

1.x STATUS
Not affected; no port needed. The otgw-1.x.x worktree (8ba6f7ef9) has no OTDirect source, and `git grep` finds none of these symbols. 1.x is ESP8266 + PIC, and the PIC handles unknown IDs itself.

SEVERITY: medium.
- For: this is a memory-safety defect on the default OTDirect gateway path. It is driven by spec-legal bus traffic, and the linker decides the blast radius.
- Against: today the visible effect is limited to the opt-in RM table plus a possible phantom rule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 getUnknownCount, incUnknownCount and clearUnknownCount (src/OTGW-firmware/OTDirect.ino:535-553) each compare the computed byte index with the array size before any access, for example `if (byteIdx >= sizeof(otUnknownCounters))`. For MsgID 128-255, getUnknownCount returns 0, and incUnknownCount and clearUnknownCount return without touching memory. The bound is derived from the declaration, not a literal, so it cannot drift (the TASK-1072 idiom). The diff changes nothing else in the helpers or in their callers at :1316-1334.
- [x] #2 The comment at OTDirect.ino:532-533 states that the counters cover MsgIDs 0-127, the range otSchedule polls, and that 128-255 are ignored. 128-255 is the OT Test and Diagnostic area, for example Remeha 131-133.
- [x] #3 Old-vs-fix host reproduction (required proof). (1) The harness, for example tests/test_otdirect_unknown_counters.cpp plus a runner, compiles the :533 declaration and the three helpers. They are extracted verbatim from a given OTDirect.ino at test time by text anchors. A hand-copied mirror like tests/test_otdirect_override.cpp does not qualify. (2) It is built with bounds checking (g++ -fsanitize=bounds -fsanitize-undefined-trap-on-error, or clang -fsanitize=address,undefined) and prints each MsgID before the call. (3) OLD, from `git show HEAD:src/OTGW-firmware/OTDirect.ino`: the run stops at `id=131` with a bounds trap or an ASan report naming otUnknownCounters. A bare nonzero exit does not count. (4) FIX: every case passes and it exits 0. (5) Both transcripts are quoted in the Final Summary.
- [x] #4 In-range behaviour is unchanged (TASK-151 contract), shown by the same harness on the fixed source. For every MsgID 1-127, four incUnknownCount calls give getUnknownCount == 3 (saturation), and clearUnknownCount then gives 0. For MsgID 128-255, getUnknownCount returns 0 after incUnknownCount.
- [ ] #5 On-device old-vs-fix on the OTGW32 bench. HARDWARE-GATED: needs an OT master on the thermostat port that can emit MsgID 132, and an OT slave on the boiler port that ACKs it. If no such rig exists, leave the task In Progress and name this AC as the blocker. (1) Preconditions: `xtensa-esp32s3-elf-nm -n -S` on the OLD build's firmware.elf shows otResponseModifiers directly after otUnknownCounters. Send GW=1 first, because a mode change wipes the RM table. Then, with an empty RM table, POST /api/v2/otdirect/overrides?action=rm&msgid=5&value=1234; GET must show "modify":[{"msgid":5,"value":4660}]. (2) Stimulus: READ_DATA or WRITE_DATA 132 answered with READ_ACK or WRITE_ACK. Telnet must show `OTD: resp MsgID=132` (OTDirect.ino:1370). (3) Pass condition: on the old build the rule is gone from "modify" after the first 132 exchange; on the fixed build it is still listed after at least 10 exchanges. (4) Evidence: the telnet transcript and GET responses from both builds are attached.
- [x] #6 Build and process gates: (1) build.bat is green for esp32 and esp32-combo, with the per-env SUCCESS line and fresh firmware.bin and littlefs.bin. (2) python evaluate.py --quick shows no new failures. (3) The prerelease is bumped with bin/bump-prerelease.sh in the same commit. (4) dev only; no otgw-1.x.x port (1.x has no OTDirect).
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from the backlog triage (workflow wf_1c568508-5f2) after an adversarial verification (workflow wf_4f2df5e8-285): 3 independent verifiers (trace / refute / impact) all voted real; judge severity medium.

FIX PLAN:
1. Guard the helpers (src/OTGW-firmware/OTDirect.ino:535-553). In each helper, directly after `uint8_t byteIdx = msgId >> 2;`:
- getUnknownCount: `if (byteIdx >= sizeof(otUnknownCounters)) return 0;`
- incUnknownCount and clearUnknownCount: `if (byteIdx >= sizeof(otUnknownCounters)) return;`
Extend the comment at :532: the counters cover MsgIDs 0-127, and 128-255 (the OT Test and Diagnostic area, e.g. Remeha 131-133) are ignored. This is 3 code lines and 0 bytes of RAM.

2. Why this shape. The counter only feeds the otSchedule auto-disable (:1321-1328). otSchedule holds IDs 0-127 only: 104 entries, the highest being 127 at :250. Ignoring 128-255 therefore loses nothing, since getUnknownCount returning 0 means the disable branch is not taken. Guarding inside the helpers also fixes the out-of-bounds read at :1321 and protects any future caller.
Rejected alternatives:
(a) Growing the array to 64 bytes: 32 bytes of RAM to count IDs nothing consumes.
(b) Masking with `& 0x7F`: aliases 131 onto MsgID 3's counter, so Remeha UNKNOWN replies would count toward disabling MsgID 3.
(c) Guarding only at :1317: leaves the helpers unsafe.

3. Add the regression harness: tests/test_otdirect_unknown_counters.cpp plus a small runner, e.g. tests/run_otdirect_unknown_counters.py.
- The runner takes --source <OTDirect.ino>.
- It slices from the line starting `// Phase 8: Unknown ID 3-strike` up to, but not including, `// Apply overrides to a thermostat frame`. Both anchors are unique today (:532, :555).
- It compiles the slice with bounds checking and runs it.
The same runner does the old-vs-fix run and then stays as the regression guard.

4. Process:
- Create the backlog task and set it In Progress first.
- Bump with bin/bump-prerelease.sh in the same commit.
- Run build.bat for esp32 and esp32-combo, then python evaluate.py --quick.
- Reference TASK-151 (the audit that missed this) and TASK-1072 (the same idiom).
- dev only.

5. Separate follow-ups, not part of this fix and not blocking it:
(a) otBoilerCache aliasing. The `& 0x7F` masks at :1237-1239, :1292-1294, :1306-1310 and :2523-2524 map MsgID 128+n onto cache[n].
- A Remeha 131 READ_ACK overwrites cached MsgID 3, which is read at :280-281 (otIsVentSlave) and :298 (otDirectBoilerPresent).
- A MsgID 128 reply drives flameRatioSet with 128's data.
- In master mode, READ 131 is answered with cache[3].
This stays in bounds, so it produces wrong data rather than memory corruption. It needs its own task.
(b) Static finding, not verified on a device: gateway mode appears never to answer an override-modified thermostat frame.
- :2002-2003 forwards the frame with GATEWAY origin.
- The only thermostat reply in handleMasterResponse is gated on THERMOSTAT origin (:1375, :1387).
- The five otSlave.sendResponse sites are :1243, :1387, :1976, :1989 and :2558.
This deserves its own claim and verification.

REPRO PLAN:
A. HOST: the required old-vs-fix proof, no device needed.
1. Toolchain. No host C++ compiler was found on the Git Bash PATH during verification; only the xtensa cross compilers are there. Provide MinGW-w64 g++ or clang, or run under WSL/macOS. Confirm this before scheduling the work.
2. Sources. Old: `git show HEAD:src/OTGW-firmware/OTDirect.ino > old.ino`. Fix: the patched working-tree file.
3. Extraction. From each file, slice from the line starting `// Phase 8: Unknown ID 3-strike` up to, but not including, `// Apply overrides to a thermostat frame`. Today that is :532-554, the declaration plus the three helpers, verbatim. This is not a mirror.
4. Test file (e.g. tests/test_otdirect_unknown_counters.cpp). It includes <cstdint>, <cstdio>, then the slice. main() runs two blocks:
(a) In range: for id 1..127, call incUnknownCount 4 times and expect getUnknownCount == 3; then call clearUnknownCount and expect 0.
(b) Out of range: for 131, 132, 133, then 128..255, first `printf("id=%u\n", id); fflush(stdout);`, then call incUnknownCount 3 times, expect getUnknownCount == 0, then call clearUnknownCount.
It exits 0 only if every case passes.
5. Build each variant with `g++ -std=c++17 -O0 -g -fsanitize=bounds -fsanitize-undefined-trap-on-error` (no sanitizer runtime needed) or `clang++ -std=c++17 -g -fsanitize=address,undefined`.
6. Expected results:
- OLD: the output ends at `id=131`, then either a trap (illegal instruction) or ASan `global-buffer-overflow ... 0 bytes after global variable 'otUnknownCounters' ... of size 32`.
- FIX: every case passes and it exits 0.
Keep both transcripts.
7. Scope. This exercises the real helper text, built by a host compiler. It does not exercise handleMasterResponse or xtensa codegen. The call-site path is established from source (:1317-1333) and closed by B.
Optional: after the fix, re-run nm and hand-decode the helpers in the new combo ELF (objdump prints raw words only). Inlining may differ.

B. BENCH: the real situation. It is hardware-gated; run it only after the current bench test finishes.
Rig:
- DUT: the OTGW32 bench unit running esp32 or esp32-combo with OTDirect active.
- Thermostat port: an OT master that can send READ_DATA or WRITE_DATA 132. Options:
  - a Remeha qSense/iSense;
  - a spare ESP32 plus an OT master adapter running the vendored OpenTherm library, sending buildRequest(READ_DATA, 132, 0);
  - UNVERIFIED: a PIC gateway with no thermostat attached, sent PM=132. PM= takes a byte ID with no range check (other-projects/otgw-6.6/gateway.asm:4984-4995), and the PIC generates its own traffic when no thermostat is connected (gateway.asm:25-27). Whether a priority message goes out in stand-alone mode was not traced.
- Boiler port: an OT slave that ACKs 132. Options:
  - a Remeha boiler;
  - a scripted OpenTherm-library slave;
  - a second OTGW32 in OTDirect master mode. Its thermostat port echoes WRITE_DATA as WRITE_ACK for any ID, and answers READ_DATA with UNKNOWN_DATAID while cache[id & 0x7F] is empty (OTDirect.ino:2521-2532). Use WRITE_DATA 132 for the ACK path.
Steps, identical on the old and fixed builds:
1. On the OLD ELF, run `xtensa-esp32s3-elf-nm -n -S firmware.elf | grep -A1 otUnknownCounters`. It must show otResponseModifiers next. If it does not, record what is adjacent and adapt the observation.
2. If a flash writes a filesystem image, save the settings first: `curl http://<ip>/api/v2/settings`.
3. Send GW=1 first. A mode change wipes the RM table (:2396, :2375).
4. On an empty table, POST /api/v2/otdirect/overrides?action=rm&msgid=5&value=1234. GET must show "modify":[{"msgid":5,"value":4660}] (slot 0, :2573-2574).
5. Telnet. The OTDirect trace is ON by default (debugStuff.h:57). Key 6 only toggles it (handleDebug.ino:418-419), so do not press it blindly. Record with scripts/capture-mqtt-debug.bat.
6. Stimulus: 132 exchanges answered with an ACK. Telnet must show `OTD: resp MsgID=132 ...` (:1370).
7. GET /api/v2/otdirect/overrides. OLD: "modify" is []. FIX: the rule is still listed after 10 or more exchanges.
Observation only, not pass/fail: with an empty table and UNKNOWN_DATA_ID answers to 132, the OLD build may list a phantom {"msgid":0,"value":0} and log `OTD: resp-modify MsgID=0 ... new=0x0000` (:1381). This is deterministic only for the first strike.

C. Routes that cannot reproduce it:
- OTDirect loopback: handleMasterResponse is never reached (:1230-1246, :1935).
- /api/v2/simulate: PIC path only on OTGW32, per project memory; not re-checked in code.
- sat_boiler_emulator.py: its TCP input cannot inject a boiler response (scripts/sat_boiler_emulator.py:74-81, :402-404), and its force-boiler hook only asserts boiler-present.
- A hand-copied mirror of the helpers proves nothing about the real code.

DISSENT / CORRECTIONS:
No verifier voted against: 3 of 3 real, all high confidence, all medium severity. The judge re-checked every decisive line and found no refutation: the source, both library validators, nm on today's rebuilt ELFs, and a hand decode of the compiled clearUnknownCount.

Corrections to the original claim:
(1) The loopback citation :1298-1300 is only a comment. The code that enforces it is the early `return true` at :1230-1246, which never sets otMasterRequestActive, so the gate at :1935 is never taken.
(2) The claim omits the out-of-bounds READ in getUnknownCount (:538, called at :1321).
(3) WRITE_ACK also reaches clearUnknownCount (:1331-1333), not only READ_ACK.
(4) Verifier 3 called Remeha 133 a write. OTmap marks 131 as OT_RW and 132/133 as OT_READ (OTGW-Core.h:505-507).
(5) Field occurrence on OTDirect is unverified. The 131-133 frames in 1.x TASK-1064 came from the TASK-1065 coverage-gate replay on the PIC path, not from a live Remeha thermostat on an OTGW32.
(6) The phantom Status-rewrite scenario (verifiers 1 and 2) is deterministic only for the first UNKNOWN strike on 132. Later strikes leave `active` at 2-15, an invalid bool whose handling depends on codegen. It is recorded as an observation, not an AC.
(7) Verifier 3's combo ELF was replaced by a concurrent build at 10:09. The judge re-ran the checks on the new ELF. The adjacency is the same (0x3fca54fc, then 0x3fca551c). clearUnknownCount moved to 0x42043334, and its l32r literal at 0x420416d4 is still 0x3fca54fc with no compare before the store.

Refutation angles checked and rejected:
- The library filters Data-IDs on neither side (OpenTherm.cpp:518-538).
- The UI/SR tables cannot intercept IDs above 127.
- Every other helper caller is bounded (AA= :3226, KI= :3380).
- The default mode is gateway (OTDirecttypes.h:107).

2026-09-30 fix (alpha.384) and host proof, re-run in-session on HEAD cc4d4addf.
- AC#1/#2 OTDirect.ino: getUnknownCount returns 0, incUnknownCount and clearUnknownCount return, when 'byteIdx >= sizeof(otUnknownCounters)' (directly after byteIdx is computed). The comment at the declaration says the counters cover MsgIDs 0-127 (the range otSchedule polls, max R_ENTRY(127)), that 128-255 (OT Test & Diagnostic area, e.g. Remeha 131-133) are ignored, and why a reply can still carry such an id (the pass-through relays them and nothing checks a reply's MsgID against the request).
- AC#3 test/host/run_unknown_counters.ps1 slices the REAL declaration and the three helpers verbatim (slice audit in the workflow review: byte-identical) with a guard region behind the array:
  * -Rev HEAD (OLD): inc and clear write past the array for 128 of 128 MsgIDs (128-255), get reads past it for 128 of 128; exit 1.
  * FIX (-DiffAgainst HEAD): 7 of 7 checks PASS, no access past the 32-byte array.
  * AddressSanitizer, OLD (-Asan -Rev HEAD): 'global-buffer-overflow ... located 0 bytes after global variable otUnknownCounters defined in OTDirect.ino:533:15 ... of size 32', in incUnknownCount, right after id=131; exit 1. FIX (-Asan): no report, exit 0.
  * Workflow mutation matrix: removing any one bound, off-by-one variants, a '& 0x7F' mask and a clamp to 127 are all caught.
- AC#4: 4 x incUnknownCount gives 3 and clearUnknownCount gives 0 for MsgIDs 0-127 on the fix; differential over 0-127: 1003072 operations, 0 differences vs HEAD.
- Impact detail from the review: on the otgw32 and combo ELFs otUnknownCounters is directly followed by otResponseModifiers (0x20 each), so the overflow corrupted the RM= response-modifier table. esp32-classic is not affected (HAS_DIRECT_OT 0 wraps the whole of OTDirect.ino); 1.x has no OT-Direct.
- AC#6: bin/bump-prerelease.sh alpha.383 -> alpha.384 in this commit; build.bat --target esp32 and --target esp32-combo SUCCESS for firmware and filesystem (fresh 12:17-12:21, alpha.384+cc4d4ad, images under %LOCALAPPDATA%/OTGW-capture/img-alpha384); evaluate.py --quick 70 passed / 0 / 0. docs/c4/c4-code-otdirect.md cites the new location and the 0-127 range.
OPEN (hardware-gated, per the AC): AC#5 needs an OT master on the thermostat port that emits MsgID 132 and a slave that ACKs it; the bench has no thermostat.
<!-- SECTION:NOTES:END -->

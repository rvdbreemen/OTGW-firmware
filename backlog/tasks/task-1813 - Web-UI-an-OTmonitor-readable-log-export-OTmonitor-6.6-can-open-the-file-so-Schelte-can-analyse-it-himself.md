---
id: TASK-1813
title: >-
  Web UI: an OTmonitor-readable log export (OTmonitor 6.6 can open the file, so
  Schelte can analyse it himself)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T12:59:00Z'
updated_date: '2026-11-11T13:50:00Z'
labels:
  - webui
  - otmonitor
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 906000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (2026-11-11, via the coordinator session): "the web UI's export format must be the same as OTmonitor's. OTmonitor must be able to read the file, so Schelte can easily do analyses himself."

## How OTmonitor 6.6 reads a log file (other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl, read-only)
- Opening a file: `otmonitor <file>` on the command line sets connection type "file" (:2205-2209). connectcoro opens it, switches off logging, MQTT, alerts and the like, and runs `filedata` with the file's mtime as end time (:1508-1520).
- `filedata` (:1749-1788): gzip is detected and unpacked (:1752-1755). `fileanal` picks the timestamp layout, then each line goes through `binary scan $line $def ts frac otmsg` (:1760). A line that does not split into 3 fields is skipped silently. If the rest starts with [ABRT] + 8 hex, everything after those 9 characters is cut off (:1761-1763), so a decode after the frame is allowed and ignored. Lines are read with `gets`, so LF and CRLF both work.
- `fileanal` (:1790-1842) reads the first 16384 bytes and keeps the lines matching `\m[ABRT][0-9A-F]{8}\M` (upper-case hex only, :1796-1797). It takes the column where that pattern most often starts. Only 7 columns are known: 16, 17 and 18 (time or epoch only), 21, 22, 23 and 26. Any other column gives no format, and then not a single line is read.
- Time-only layouts (column 16/17, `%T`) take the date from the file's mtime: base = mtime - 6 h (:1757), plus one day per midnight crossing (:1766-1771). The layouts with a date (22: `%Y%m%d %T`; 23 and 26: `%Y%m%d-%H%M%S`) carry their own date.
- `process` (:467-) uses a frame only when the message is exactly [ABRT] + 8 hex (:478). It also handles `Error NN` lines (:490), PS=1 summary lines and version banners. Other text is only shown.
- OTmonitor's own log file (`output` :913-929, `untab` :931-946, `otmessage` :978-986) is `HH:MM:SS.uuuuuu` padded to column 17, the frame padded to column 28, then the decode. The default name is otlog-%Y%m%d.txt (:2233). That is layout 17.

## What the web UI writes today
- downloadLog() (src/OTGW-firmware/data/v2.js:5657-5659) joins logBuf with '\n' into otgw-log-<ts>.txt. Stream to file (:5671-) and the 15-minute auto-download write the same lines.
- A frame line is built in OTGW-Core.ino:5815-5816 and :6034-6056: `HH:MM:SS.uuuuuu` (helperStuff.ino:37, device time zone), a space, an 18-character source label ("Boiler            ", "Thermostat        ", "Request Boiler    ", "Answer Thermostat "), a space, then the frame, id, type and decode. Example from tests/webui/fixtures/msgid1-field-cap-1138.txt: `11:38:23.646144 Thermostat         T10010A00`. The frame starts at column 35. OT-Direct writes the frame in upper case too (OTDirect.ino:788, `%c%08lX`).
- logBuf also holds lines that are not frames: events with a prefix ('>' command, '<' response, '!' error, '*' event; OTGW-Core.ino:110-142), `PS1 ...` lines (:5096-5099), SAT narration (`HH:MM:SS S ...`, v2.js:2714) and DIAG chunks.
- exportCsv() (v2.js:5569-5573) exports graph samples (epoch_ms plus value columns), not OT frames. OTmonitor cannot use it, and it stays as it is.

## Evidence: OTmonitor's own reader code run on both files
tclsh 8.6 (/ucrt64/bin/tclsh) loaded the real `fileanal` and `filedata` procs from otmonitor.tcl, with stubs for gui, status, connected and process. Harness: C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\87fc885c-0e3d-40a7-9313-4a23af8d2bc3\scratchpad\otmon\run.tcl (a scratch file; copy it into tests/ for AC 2).
- The current web UI format (frame at column 35): `fileanal -> {}`, no timestamp format recognised, 0 frames processed. OTmonitor shows an empty file.
- The target format below: `fileanal -> {a15xa6xa* %Y%m%d-%H%M%S}`. All 5 frames are processed with the right date, also across midnight (2026-11-11 23:59:59, then 2026-11-12 00:00:00).
- OTmonitor's own time-only layout (column 17), for a log from 20:00 to 08:59 with an mtime of 09:00 the next day: dated one day late (2026-11-12 20:00:01 instead of 2026-11-11). A downloaded file's mtime is its download time. A time-only export therefore gets the wrong date when the log starts more than about 6 h before the download and that span crosses midnight, or when the file is opened from a later copy.

## Target format (recommendation)
OTmonitor layout 23: `YYYYMMDD-HHMMSS.uuuuuu`, one space, the frame. One OT frame per line, no header, CRLF line ends (OTmonitor writes CRLF on Windows; LF reads too), upper-case hex, ASCII. File name otlog-YYYYMMDD-HHMMSS.txt. This is one of the 7 layouts OTmonitor 6.6 itself reads (otmonitor.tcl:1835-1838). Unlike its own time-only log file, it carries the date. Sample (5 lines):

```
20261111-113823.646144 T10010A00
20261111-113829.445993 BD0010A00
20261111-235959.900000 T00000300
20261112-000000.200000 B40000300
20261112-000004.100000 R10013C00
```

Only frame lines go in ([ABRT] + 8 hex, including the gateway's own R and A frames), plus `Error NN` lines in the same timestamp layout (OTmonitor counts them, :490-494). Events, PS1, SAT narration and DIAG lines are left out. A decode after the frame would be ignored by OTmonitor (:1761-1763); without one the file stays small. If Robert or Schelte wants the exact own layout instead (column 17, time only), that also reads (verified above), at the cost of the date problem.

## What logBuf has and lacks
- Has: the device's time of day with microseconds, and the frame with its source letter, id, type and decode.
- Lacks the date. Only the device time of day is stored, and logRecvTimes keeps the browser receive times of the last 10 s only (v2.js:2719-2721). Options for the implementer:
  - (a) Keep the browser receive epoch per line and take the date from it. Roll the day forward when the device time of day goes backwards past midnight. Watch out when the browser and the device time zone differ (device time zone: settings.ntp.sTimezone).
  - (b) Have the firmware add the date to the WebSocket line. That changes the WS log format for every consumer (docs/api/WEBSOCKET_FLOW.md, the web UI parsers, capture tools), so it needs its own impact check.
  State the choice and the reason in the task notes.
- Depth: 2000 lines by default, 200k in Capture mode (v2.js:2706). Pause drops frames from logBuf (:2726). The OTmonitor export carries what logBuf holds; say so in the UI or the manual.

Related: TASK-1727 (manual: OTmonitor over TCP port 25238), TASK-1720 (USB serial as an OTmonitor outlet). d3 implements this after beta.176.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The web UI has an 'OTmonitor log' download (next to the existing log download) that writes the logBuf frames in the target format from the description: one line per OT frame, `YYYYMMDD-HHMMSS.uuuuuu <frame>`, with the right date also across midnight, plus Error lines; no event, PS1, SAT or DIAG lines
- [x] #2 An automated test parses the exported file with OTmonitor's own reader logic: preferably the real fileanal and filedata procs from other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl under tclsh (as in the harness named in the description), otherwise a faithful port that names its source lines. The test is red on the current downloadLog() output (no format found, 0 frames) and green on the new export (layout found, every frame read with its date)
- [ ] #3 Manual check in OTmonitor 6.6: open the exported file (`otmonitor <file>` or the file connection); the graph and the message views show the data at the right times; screenshot as evidence
- [x] #4 The user manual (EN and NL) describes the OTmonitor log download: what it holds (logBuf depth, Pause), how to open it in OTmonitor, and what to send along for an analysis
- [ ] #5 The web UI test files touched (one file per run) and python evaluate.py --quick show no new FAIL; the download is checked on a bench unit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reader harness used for the evidence in the description (run: tclsh run.tcl <path to otmonitor.tcl> <log files...>; it loads the real fileanal/filedata procs and stubs gui/status/connected/process). Kept here because the scratch copy is temporary:

```tcl
set fh [open [lindex $argv 0]]; set src [read $fh]; close $fh
set lines [split $src \n]
foreach p {filedata fileanal} {
  set i [lsearch -regexp $lines "^proc $p "]
  set j [lsearch -start $i -exact $lines "\}"]
  eval [join [lrange $lines $i $j] \n]
}
proc connected {args} {}
proc status {args} {}
proc gui {args} {}
proc process {msg us} { puts "  process [list $msg] -> [clock format [expr {$us/1000000}] -format {%Y-%m-%d %T}]" }
set connected 1; set start 9999999999
foreach f [lrange $argv 1 end] {
  puts "== $f"
  set dev [open $f]
  set r [fileanal]
  close $dev
  puts "  fileanal -> [list $r]"
  if {$r eq ""} {puts "  no timestamp format recognised"}
  set dev [open $f]
  set co c[incr n]; coroutine $co filedata [clock scan "20261112 090000" -format "%Y%m%d %H%M%S"]
  while {[info commands $co] ne ""} {update; after 5}
  close $dev
}
```

2026-11-11 d3: design option (a) as decided by the coordinator: the web UI keeps the device wall clock per logBuf line (logBufDev, devClockBase convention from TASK-1229; browser local time until the first device/time answer) and the pure otmonLogText(lines, devMs) writes layout 23. Date rule: the line's own HH:MM:SS.uuuuuu plus the date of the device clock at arrival; when stamp and arrival lie more than 12 h apart they straddle midnight and the date moves one day (no rolling state, works for any line order). Frame = [ABRT]+8 upper-case hex right after the 18-character source label (OTGW-Core.ino:6034-6050), so a response or command echo with a frame-like token is never taken; Error 01-04 from the '!' event lines (OTGW-Core.ino:6176-6206). CRLF, empty file when nothing exportable. Option (b) not taken: a WS line format change touches every consumer. No library needed (a few lines, project-specific).

2026-11-11 tests: tests/webui/otmonitor-log-export.test.mjs (5 tests) cuts the real otmonLogText out of v2.js; the OTmonitor part feeds the files to the REAL fileanal/filedata procs of other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl under tclsh (Git for Windows ucrt64 tclsh 8.6) via tests/webui/lib/otmon_reader.tcl; skipped with the reason when tclsh or the private submodule is missing. On the new code 5/5 pass: plain download -> fileanal {} and 0 messages (the bug); export -> layout {a15xa6xa* %Y%m%d-%H%M%S}, all 4 frames and the Error 01 line with their dates across midnight. On origin/dev v2.js (OTGW_V2_JS): 1 pass, 4 fail (otmonLogText not found). Full web UI suite one file per run: 63 files, 0 fail (after npm ci --prefix tests/webui for acorn). log-autoscroll-off-freezes.test.mjs stub scope extended with logBufDev/devClockBase/devClockAt. evaluate.py --quick: 86 passed, 0 failed.

2026-11-11 strings: EN in I18N_EN, NL, DE and EL packs (i18n-lang requires every pack to carry every key). The EL text is mine, not reviewed by a native speaker. Manual: docs/manuals/en/ch03-web-interface.md and docs/manuals/nl/h03-webinterface.md (what it holds, Capture/Pause/Clear, how to open, what to send). Opening: only the command line, otmonitor <file> (otmonitor.tcl:2207-2211); no file connection found in the GUI code, so the manual does not claim one. AC3 not done: OTmonitor's GUI needs its starkit runtime (sqlite3, tdom, Thread); wish from Git fails 'Error in startup script'. Needs otmonitor.exe 6.6 on a machine that has it (Robert). AC5 bench download check waits for the build freeze (09 beta.177/178).

2026-11-11: the two Greek strings are put to Sergeant D as Q-1813a by comms 92 (#dev-sat-mqtt msg 1570071042880176200; A fine / B corrections, recommendation A). Apply his answer before Done.

2026-11-11 13:46 UTC: Sergeant D answered Q-1813a with A (msg 1570072123983011951): both Greek strings stay as they are. Recorded by comms 92 as an entry.
<!-- SECTION:NOTES:END -->

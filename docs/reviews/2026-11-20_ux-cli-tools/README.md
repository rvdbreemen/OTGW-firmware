# UX review: tester CLI tools (capture and flash), 2026-11-20

Reviewer: otgw-ux-designer. Task: TASK-2963. Assigned by the coordinator. Line numbers are from origin/dev 25d13036f. Triage: the coordinator turns findings into tasks; nothing here is a decision.

## Scope and method

- Tools: `scripts/capture-mqtt-debug.bat` (Windows), `scripts/capture-mqtt-debug.sh` (Linux, macOS), `scripts/capture-mqtt-debug-macos.sh` (wrapper), `scripts/otgw_capture.py` (the capture itself, ADR-202), `flash_otgw.bat` and `flash_otgw.sh`. Code at origin/dev 25d13036f.
- **Read-only.** Nothing was run against a unit and no tool was started, except `otgw_capture.py --help` (no network access; 111 lines of output). Every claim cites `file:line`. What only a real run can show is under "Not verified".
- Heuristics: Nielsen H1 to H10, with the focus the coordinator asked for: first use, error messages, what a tester sees when something fails, and Windows versus Linux/macOS parity.
- Severity 0 to 4 (4 blocks the task or causes damage, 3 major, 2 minor, 1 cosmetic). Effort S (a few lines), M (one flow), L (redesign).
- There is no `capture-otgw.sh`; the coordinator's name refers to `capture-mqtt-debug.sh`.

## Summary

| ID | Tool | Heuristic | Sev | Effort | Evidence |
|---|---|---|---|---|---|
| C1 | flash (both) | H5 error prevention, H3 control | 3 | S | `flash_otgw.bat:87, 202-237`; `flash_otgw.sh:564-580` |
| C2 | capture and flash (.bat) | H1 visibility, H9 recovery | 3 | S | `capture-mqtt-debug.bat:38-40`; `flash_otgw.bat` (no `pause`); `otgw_capture.py:4155-4176` |
| C3 | flash (both) + manual | H2 real world, H4 consistency | 3 | S | `flash_otgw.bat:283-284`; `flash_otgw.sh:585`; `networkStuff.ino:321-332`; `OTGW-firmware.h:136`; manual ch02:253 |
| C4 | capture (interactive) | H7 flexibility, H2 | 3 | S | `otgw_capture.py:3720-3776, 3559-3562`; `field-run-recipe-2.0.0.md` §1.3 |
| C5 | flash (.bat and .sh) | H4 consistency, security UX | 3 | S | `flash_otgw.bat:110-140`; `flash_otgw.sh:265-270`; vs `capture-mqtt-debug.bat:72-89` |
| C6 | flash (both) | H9 recovery | 2 | S | `flash_otgw.bat:235-237`; `flash_otgw.sh:576-580` |
| C7 | capture (end of run) | H6 recognition | 2 | S | `otgw_capture.py:4163-4171` |
| C8 | flash (.sh) | H4 parity | 1 | S | `flash_otgw.sh:214-216` vs `flash_otgw.bat:87` |
| C9 | web UI link (spec T4, F5) | H2 | 2 | S | `docs/sat/field-run-recipe-2.0.0.md:1-3` |

Recommended order: C1 and C3 first: both can leave a tester with a wiped or unreachable unit. Then C2 and C4, which decide whether a double-clicking tester can use the tools at all. C5 is a one-for-one copy of a fix that already exists in the capture launcher.

## Findings

### C1. A plain run of the flash tool erases everything, without asking (H5, H3, sev 3)

- **What:** with no arguments, which is what a double-click gives, `flash_otgw.bat` runs in FACTORY mode: `write_flash -e`, which erases WiFi credentials and settings (`:87`, `:233`). The summary says "WiFi credentials and settings WILL be removed." (`:272-273`), and esptool starts right after it (`:222-233`). There is no prompt in between. The only `set /p` in the script picks a firmware file when there are several (`:340`). `flash_otgw.sh` is the same (`:564-580`).
- **Why it matters:** the most destructive action is also the default, and it cannot be stopped once the window opens. A tester who wanted an update (keep settings) loses the whole configuration.
- **Proposal:**
  - Run with no arguments: show a short menu instead of acting. "1) Update the firmware, keep WiFi and settings (recommended)  2) Factory reset: erase everything  Q) Quit".
  - Factory mode, chosen from the menu or with explicit arguments: ask "Type ERASE to erase the flash, or anything else to stop:". A new `--yes` skips the question, for scripts and the bench automation.
- **Proposed AC:** a run without arguments never erases flash without a typed confirmation; `--yes` keeps scripted factory flashes working; both .bat and .sh behave the same; `scripts/test_flash_automation.py` or the bench recipe uses `--yes`.

### C2. On a double-click the window closes before the tester can read it (H1, H9, sev 3)

- **What:** neither `.bat` pauses at the end. The capture launcher returns the Python exit code (`capture-mqtt-debug.bat:38-40`), and `otgw_capture.py` ends with the line "Upload this one file: …" and returns (`otgw_capture.py:4167-4176`). `flash_otgw.bat` ends with `exit /b` on every path. Neither script checks `%cmdcmdline%` (0 hits), so they cannot tell a double-click from a terminal.
- **What a tester sees:** on success the window closes, and with it the path of the file they are asked to upload. On an error (no Python, download failed, device not found, `write_flash failed`), the message closes before it can be read, and only a flash of a window remains.
- **Proposal:** detect a double-click in both `.bat` files (`echo %cmdcmdline% | find /i "/c"` together with the script's own path) and then end with "Press any key to close this window." (`pause >nul`). In a terminal, no pause, so scripts and CI are unchanged. Linux and macOS terminals stay open after the run, so no change is needed there.
- **Proposed AC:** started by double-click, both `.bat` tools wait for a key before closing, on success and on every error path; started from `cmd` or PowerShell, they exit without waiting.

### C3. After a factory reset the flash tools name a WiFi network that does not exist (H2, H4, sev 3)

- **What:** after a factory reset the gateway opens its setup portal under `<hostname>-<MAC with colons>` (`networkStuff.ino:321-332`), and the 2.0.0 default hostname is `OTGW2` (`OTGW-firmware.h:136`). So the network is `OTGW2-A4:CF:12:21:B4:F8`. The tools say:
  - `flash_otgw.bat:283-284`: "connect to the open `OTGW-<MAC>` AP" (the 1.x prefix);
  - `flash_otgw.sh:585`: "connect to WiFi AP `OTGW-AP`" (a name that never exists).
- The manual is split too: ch02:239 and ch06:11 have `OTGW2-A4:CF:12:21:B4:F8`, but ch02:253 still has `OTGW-<MAC>`. The separate fallback AP `OTGW-XXXXXX` with password `otgw1234` (`networkStuff.ino:503-513`) is another network, for a unit that lost its WiFi, and the manual describes it correctly elsewhere (ch03:98, ch06:45).
- **Proposal:** both tools print the same text: "After the reset the gateway opens the WiFi network `OTGW2-<its MAC address>` (for example `OTGW2-A4:CF:12:21:B4:F8`). Connect to it to set up WiFi." Fix the manual line ch02:253 the same way (otgw-docs).
- **Proposed AC:** the .bat, the .sh and the manual name the setup network as `OTGW2-<MAC>` with the same example; no `OTGW-AP` or `OTGW-<MAC>` remains for the setup portal.

### C4. The long run the testers are asked for is not reachable by double-click (H7, H2, sev 3)

- **What:** the field-run recipe asks for a long run (`--long-run`, or `--duration` above 2 h) for at least 24 h (`field-run-recipe-2.0.0.md` §1.3). The interactive flow asks only three things: device host, MQTT broker and MQTT username (`otgw_capture.py:3731, 3769, 3775`). Nothing asks how long to run. A double-click cannot pass arguments, so the tester gets the default: run until Q, in the short mode (`:3559-3562`).
- **Why it matters:** a 24 h run in the short mode writes one large file per stream, without the hourly zips and the index the analysis relies on (`:3533-3537`).
- **Proposal:** one more question in the interactive flow, after the broker: "How long? 1) Until you press Q  2) 24 hours (long run, recommended for SAT field data)  3) 3 days". Choices 2 and 3 set `--long-run` and the duration. The answer is saved with the other settings, so the next run offers it as the default.
- **Proposed AC:** an interactive run asks for the duration with these three choices; choice 2 gives the same run as `--long-run --duration 86400`; `--no-prompt` and explicit arguments skip the question.

### C5. The flash tool downloads esptool the way that set off Defender before, and checks no hash (H4, security UX, sev 3)

- **What:** `flash_otgw.bat:110-140` downloads esptool with `powershell -NoProfile -ExecutionPolicy Bypass … Invoke-WebRequest` and unpacks it with `Expand-Archive`. TASK-1711 removed exactly that pattern from the capture launcher, after a tester's Defender quarantined it as a Trojan (`capture-mqtt-debug.bat:72-75`, comment). Neither flash tool checks the SHA-256 of the esptool zip (`flash_otgw.bat:110-140`, `flash_otgw.sh:265-270`; no hash command in either), while both capture launchers pin and check one. The .bat also calls PowerShell in three more places: the partition-layout check (`:450`), a timestamp (`:468`) and the settings comparison with `-ExecutionPolicy Bypass` (`:523`).
- **What a tester sees:** at best nothing; at worst the download is blocked or the file quarantined, and the tool says "Could not download esptool. Check internet connection." (`:121`), which points at the wrong cause.
- **Proposal:** use the capture launcher's way on Windows (`curl.exe`, `certutil -hashfile`, `tar.exe` from System32), and pin the SHA-256 of the esptool v4.8.1 assets in both tools. On a mismatch, say so and delete the file.
- **Proposed AC:** the esptool download in `flash_otgw.bat` uses no PowerShell (the other three PowerShell calls get the same treatment or a written reason to stay); both tools refuse an esptool archive whose SHA-256 does not match the pinned value, with a message that says so.

### C6. A failed flash gives no next step (H9, sev 2)

- **What:** when esptool fails, the .bat prints "[ERROR] write_flash failed." and exits (`flash_otgw.bat:235-237`); the .sh shows esptool's own output and stops (`flash_otgw.sh:576-580`). The usual causes are well known: another program holds the serial port (OTmonitor, a serial monitor), a charge-only USB cable, the wrong port when several ESP32-S3 boards are connected (`:188-200` explains the auto-detect, but only in a comment), or a speed of 921600 that a cable does not sustain (`:181`).
- **Proposal:** after a failure, print a short checklist: "1. Close other programs that use the serial port (OTmonitor, Arduino, a serial monitor). 2. Use a USB data cable, not a charge-only one. 3. More than one board connected? Add --port COMx. 4. Still failing? Try --baud 115200." The same four lines in the .sh, with `/dev/ttyACM0` as the example.
- **Proposed AC:** both tools print the four-step checklist after a failed `write_flash`, with the platform's port example.

### C7. The upload file is hard to find after the run (H6, sev 2)

- **What:** the run ends with the full path of the upload zip (`otgw_capture.py:4167-4171`) under `logs/mqtt-diagnostics/<run>` relative to the folder the tool was started from (`:3558`). The tester has to find that folder in Explorer or Finder by hand, and on Windows the line disappears with the window (C2).
- **Proposal:** after a successful interactive run, ask "Open the folder with the upload file? [Y/n]" and open it, with the file selected where the OS allows: `explorer /select,<file>` on Windows, `open -R <file>` on macOS, `xdg-open <folder>` on Linux. No question with `--no-prompt`.
- **Proposed AC:** an interactive run offers to open the folder and selects the upload zip on Windows and macOS; a non-interactive run is unchanged.

### C8. The Linux flash header leaves out that WiFi is erased (H4 parity, sev 1)

- **What:** the .bat header says "Mode: factory reset (erase flash + WiFi credentials)" (`flash_otgw.bat:87`); the .sh header says "Mode: factory reset (erase flash, write firmware + filesystem)" (`flash_otgw.sh:216`). The .sh does say it lower down (`:572`), but the first line a tester reads differs per platform.
- **Proposed AC:** both headers name the WiFi erase in the mode line.

### C9. The banner link agreed in spec T4 points to an internal document (H2, sev 2)

- **What:** found while reading the recipe for C4. The F5 banner copy (spec T4, TASK-2959) links to `docs/sat/field-run-recipe-2.0.0.md`. That file opens with "Internal recipe for the testers' field runs" and is full of source references (`SATcontrol.ino:NNNN`). A user who clicks from the web UI lands in text written for the team.
- **Decided (coordinator, 2026-11-20) and done:** the banner links to the public note "Reporting SAT field results" (`docs/manuals/en/ch05-sat-thermostat.md#overview`, NL `h05-sat-thermostaat.md#wat-doet-sat`). Spec T4 and TASK-2959 are updated, and BlueFlame agreed.
- **Left over (for otgw-docs, found by BlueFlame):** both manual notes end with "The same steps: …/docs/sat/field-run-recipe-2.0.0.md" (NL "Dezelfde stappen: …"), so a user still reaches the internal recipe one click further. That sentence should go or point to a public page.
- **Proposed AC:** no user-facing page (web UI or manual) links to `docs/sat/field-run-recipe-2.0.0.md`.

## What works well

- **The capture launchers** find Python 3.9+, and otherwise download a portable Python once, with a pinned SHA-256, into a private per-user folder. They explain the manual install per OS when that fails (`capture-mqtt-debug.bat:27-98`, `capture-mqtt-debug.sh:84-176`). No PowerShell on Windows.
- **Start without typing:** the capture finds gateways by mDNS and offers them (`otgw_capture.py:3725-3731`). It reads the MQTT broker and user from the gateway itself and warns when a typed broker differs (`:3735-3764`). It reads the MQTT password from the gateway when the length matches (`:3777-3800`).
- **Both flag styles work** (`--device` and `-DeviceHost`, `:3482`, `:3538`), so the Windows and Linux examples in the launchers are both right.
- **Clean stop on Windows:** `start /b /wait` keeps cmd from asking "Terminate batch job?", so Ctrl+C still writes the transcript (`capture-mqtt-debug.bat:36-38`). The console says "Press Q to stop cleanly" (`otgw_capture.py:4043`).
- **The upload respects Discord's limit:** a run too big for one attachment becomes two files, and the tool says which two (`:4167-4171`).
- **The flash tool states its plan before acting**: mode, firmware, board, speed and the effect on WiFi and settings (`flash_otgw.bat:202-208, 252-274`). C1 only adds the confirmation after that.

## Not verified

- Nothing was run against a unit, and no tool was started except `otgw_capture.py --help`. All behaviour is read from the code.
- The double-click behaviour (C2) follows from how Windows runs a `.bat` from Explorer (`cmd /c`, the window closes when the script ends); it was not observed on this machine in this session.
- What esptool prints for each failure cause (C6), and whether Defender still flags the PowerShell download (C5), were not reproduced.
- `capture-serial.py` and `ota_flash.py` were not reviewed (not tester-facing in the release zip, as far as the recipe and manual show; also not verified).
- No macOS machine: macOS paths are read from the scripts only.

# Spec: flash tool interaction (TASK-2968, CLI review C1 and C3)

Author: otgw-ux-designer, 2026-11-25. Builder: otgw-dev (scripts only; no firmware change). Applies to `flash_otgw.bat` and `flash_otgw.sh`, with identical behaviour and identical text. Read at origin/dev 68973c00d. Nothing here has been run: I read the scripts and changed none.

## What is there today

- Both tools already know `--update`, `--factory`, `--migrate`, `--bin`, `--app`, `--fs` (`flash_otgw.bat:310-320`, `flash_otgw.sh:162-173`). `--factory` only sets "not update", which is also the default, so a bare run cannot be told from an explicit `--factory`.
- A bare run goes straight to FACTORY mode: summary printed, then esptool with `-e` (`flash_otgw.bat:222-237`, `flash_otgw.sh:576-580`). The only prompt in either tool is the firmware picker, "Select firmware number" (`flash_otgw.bat:360`, `flash_otgw.sh:505`, read from `/dev/tty`).
- The README (origin/dev, after otgw-docs' R2 fix) says a factory install "erases the flash, so WiFi settings and all other settings are gone" (README.md:32). The menu text below uses the same words.
- Callers that must keep working without a prompt: the bench recipes call `flash_otgw.bat --board esp32 --migrate --host <ip>` and `--update` (`docs/guides/agent-build-and-bench.md:91, 102`); `--migrate` and `--update` never erase WiFi, so they need no new confirmation.

## Design rules

1. **Erasing needs a typed word.** Factory mode, however reached, asks the user to type `ERASE` unless `--yes` is given.
2. **A bare run asks what you want.** No mode flag means a two-choice menu, with the safe choice first and the default.
3. **Fail safe.** When input is not available (stdin closed, redirected, CI, no tty), every prompt counts as "no": the tool prints why and exits with code 2. It never erases because nobody answered.
4. **One word for each thing, the same in `.bat` and `.sh`.** Same text, same order, same exit codes.
5. **`--yes` skips only the ERASE question.** It does not choose a mode and does not skip the firmware picker. Short form `-y`, as the project's other flash script already uses (`scripts/test_flash_automation.py` calls `flash_esp.py --build -y`).

**Mode flags** (any of these counts as "a mode was chosen", so no menu): `--update`, `--factory`, `--migrate`, `--bin`, `--app`, `--fs`. `--port`, `--board`, `--baud`, `--host`, `--password`, `--no-backup` alone do not choose a mode. `--bin <file>` without `--update` or `--migrate` is a factory flash (as the README describes it): it asks ERASE like any factory flash.

## The menu (bare run, interactive)

```
 OTGW Flash Tool
 What do you want to do?

   1) Update the firmware, keep my WiFi and settings   (recommended)
   2) Factory reset: erase everything and install from scratch
   Q) Quit

 Choose 1, 2 or Q [1]:
```

- Enter alone = 1. `q`, `Q`, EOF or anything else that is not 1 or 2 = quit (exit 0 for Q, exit 2 for EOF or invalid input after one retry).
- 1 behaves exactly as `--update` today (auto-picks app and filesystem). Its existing summary stays, including the line that a filesystem image resets the settings.
- 2 continues as factory mode and then shows the ERASE question below.
- Unit on 1.x or an old layout: unchanged, the existing `check_device_layout` message ("needs `--migrate` first") still applies after choice 1.

## The ERASE question (factory mode)

Shown after the existing "Ready to flash" summary, before esptool starts:

```
 This erases the WHOLE flash of the connected unit.
 Its WiFi login and all its settings will be gone.
 Settings can only be kept by saving http://<device>/settings.ini first.

 Type ERASE to continue, or press Enter to stop:
```

- Case-sensitive `ERASE`. Anything else, an empty line or EOF: print `Cancelled: nothing was flashed.` and exit 1.
- The text uses "unit" not "gateway" so it reads the same for a gateway that is not yet on the network.
- `--yes` skips this block. It still prints the summary, so a log shows what was about to happen.

## After the flash: the setup network (C3)

Replace the closing text in both tools (`flash_otgw.bat:283-284`, `flash_otgw.sh:585`) with:

```
 Factory reset done.
 The unit now opens its own WiFi network for setup:  OTGW2-<MAC address>
 (for example OTGW2-A4:CF:12:21:B4:F8). Connect to it and enter your home
 WiFi. The MAC address is printed on the unit and shown by esptool above.
```

Basis: the portal name is `<hostname>-<MAC with colons>` with hostname `OTGW2` (`networkStuff.ino:321-332`, `OTGW-firmware.h:136`). A user who renamed the hostname before the reset still gets the default after an erase, because the settings are gone. The `OTGW-<MAC>` and `OTGW-AP` wordings are removed.

The separate fallback network `OTGW-XXXXXX` (password `otgw1234`, `networkStuff.ino:503-513`) belongs to a unit that lost its WiFi; it is not mentioned here.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | flashed, or the user chose Q in the menu |
| 1 | cancelled at the ERASE question, or esptool failed |
| 2 | wrong arguments, or no answer possible (closed stdin) where one was needed |

## Edge cases

- `flash_otgw.bat --factory` interactive: ERASE question, no menu. `--factory --yes`: no question.
- `flash_otgw.sh --bin x.bin` from a script without a tty: refuses with `No terminal and no --yes: refusing to erase. Add --yes to confirm a factory flash.` and exit 2. Existing callers of a bare factory flash must add `--yes`; the bench recipes use `--update` and `--migrate`, so they are unaffected.
- Double-click on Windows: the menu appears; the window already waits for a key at the end (C2 is built, `tests/test_bat_doubleclick_pause.py`). The pause must also happen after a cancelled ERASE or a quit.
- Several `*-merged-full.bin` candidates: the firmware picker comes after the menu or after the explicit mode, never before the ERASE question.
- Wrong board plugged in: unchanged (esptool port selection). The summary already prints board and port.

## Acceptance criteria (extends TASK-2968; the existing ACs stay)

1. A bare run, interactive, shows the menu above with 1 as the default; Enter selects 1.
2. A bare run with no input (stdin closed) exits 2 with an explanatory line and flashes nothing.
3. Factory mode asks for `ERASE` (case-sensitive); anything else prints `Cancelled: nothing was flashed.`, exits 1 and flashes nothing; `--yes` skips only this question.
4. `--update` and `--migrate` ask nothing new (the bench recipes still run unchanged).
5. The closing text names `OTGW2-<MAC address>` with the example; neither tool contains `OTGW-AP` or `OTGW-<MAC>` for the setup portal.
6. `.bat` and `.sh` produce the same text and exit codes; evidence is a transcript of each with no board attached, for: bare run + Q, bare run + 2 + wrong word, bare run + closed stdin, `--factory --yes` stopping at "no board found".
7. The existing tests that run the tools stay green (`tests/test_flash_otgw_sh_update.py`, `tests/test_flash_usb_pid_filter.py`, `tests/test_flash_migrate_settings_compare.py`, `tests/test_bat_doubleclick_pause.py`); new tests cover the menu, the ERASE question (right word, wrong word, closed stdin) and `--yes`; guides and the README Quick Start are checked for a bare or `--bin` factory call. (`scripts/test_flash_automation.py` tests `flash_esp.py`, not these tools.)

## Not verified

- I did not run either tool (no board, and the tools would start an esptool download on first use). The behaviour above is designed from the code.
- Windows `set /p` with redirected stdin: I expect it to return empty on EOF, which the "no answer = no" rule relies on; otgw-dev should confirm that on a real `cmd` with `< NUL`.
- The README's Quick Start wording (otgw-docs) should match this menu once built.

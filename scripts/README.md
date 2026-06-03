# Scripts Directory

This directory contains utility scripts for the OTGW-firmware project.

## autoinc-semver.py

Version management script that automatically increments the build number and updates version information in `version.h`.

### Usage

```bash
python scripts/autoinc-semver.py . --filename version.h --githash <hash>
```

### Options

- `directory` - Directory containing version.h
- `--filename` - Version file name (default: version.h)
- `--git` - Enable git integration (auto-commit)
- `--increment-build` - Amount to increment build number (default: 1)
- `--githash` - Override git hash
- `--githash-length` - Length of short git hash (default: 7)
- `--prerelease` - Override prerelease identifier
- `--update-all` - Update version strings in all project files (automatically enabled when version mismatch detected)

### What it does

1. Parses `version.h` to extract version components
2. Increments the build number
3. Updates timestamp and git hash in `version.h`
4. Generates semantic version strings (_SEMVER_FULL, _SEMVER_CORE, etc.) in `version.h`
5. Updates `data/version.hash` with the git hash
6. **Automatically detects version mismatches** between `version.h` and project files (`.h`, `.ino`, `.cpp`, `.c`)
7. Updates all project files with correct version when mismatch detected or `--update-all` flag is used
8. Optionally commits changes to git

### Automatic Version Propagation

The script now automatically detects when the semantic version (MAJOR.MINOR.PATCH-PRERELEASE) in `version.h` differs from version strings in project files. When a mismatch is detected, all project files are automatically updated to match `version.h`.

**Note**: Only semantic version changes trigger automatic updates. Build number changes do not trigger updates since build numbers are not included in project file version strings.

**Excluded directories**: The script skips the following directories when scanning/updating:

- `build`, `scripts`, `data`, `.github`, `docs`, `hardware`, `example-api`
- `arduino`, `Arduino`, `libraries`, `staging`
- `node_modules`, `.git`, `__pycache__`
- `Specification`, `specification`, `theme`
- Any hidden directories (starting with `.`)

This script is used both by the CI/CD workflow and the local build script.

## branch-hygiene-queue.ps1

Generates a branch hygiene review queue for remote branches and exports it as CSV.

### Branch Hygiene Usage

```bash
pwsh -File scripts/branch-hygiene-queue.ps1 -Remote origin -BaseBranch dev -InactiveDays 14 -OutputCsv docs/process/branch-hygiene-queue.csv
```

### Branch Hygiene Options

- `-Remote` - Remote name to inspect (default: `origin`)
- `-BaseBranch` - Base branch used for merge classification (default: `dev`)
- `-InactiveDays` - Inactivity threshold for stale-unmerged classification (default: `14`)
- `-OutputCsv` - Path to output CSV file

### Branch Hygiene Behavior

1. Fetches and prunes remote refs
2. Enumerates remote branches excluding `origin/HEAD`, `origin/main`, and `origin/dev`
3. Classifies each branch as `active`, `stale-merged`, or `stale-unmerged`
4. Adds owner/decision/notes columns for manual review
5. Exports a sorted review queue CSV for branch governance

## sat_boiler_emulator.py

Host-side synthetic boiler emulator for OTGW32 bench testing (TASK-802). Connects to
the OTDirect TCP bridge on port 25238 and periodically sends synthetic OpenTherm frames
in OTGW monitor-stream format so the TASK-795 simulation availability gate
(satOnBoilerDetected / SAT simulation contract §4.2) can be exercised without a
physical boiler wired to the OT bus.

Requires Python 3 only (standard library: socket, argparse, time, sys). No pip deps.

### Quick start

```powershell
# Dry-run: print computed frames and exit (no socket opened)
python scripts\sat_boiler_emulator.py --dry-run

# Dry-run with specific member-id and slave config flags
python scripts\sat_boiler_emulator.py --dry-run --member-id 4 --slave-config 0x01

# Connect and emit frames every second
python scripts\sat_boiler_emulator.py --host 192.168.1.x

# Connect with custom interval and member-id
python scripts\sat_boiler_emulator.py --host 192.168.1.x --port 25238 --member-id 4 --interval 2.0
```

### Options

| Option | Default | Description |
|---|---|---|
| `--host HOST` | (required unless `--dry-run`) | OTGW32 hostname or IP address |
| `--port PORT` | `25238` | TCP port of the OTDirect bridge |
| `--member-id N` | `4` | Slave MemberID code (0-255; 0=customer non-specific) |
| `--slave-config FLAGS` | `0x00` | Slave configuration flags byte for MsgID 3 HB (decimal/hex/binary) |
| `--interval SEC` | `1.0` | Seconds between frame bursts |
| `--dry-run` | off | Print frames and exit without connecting |

### Frames emitted

**MsgID 3 - Slave Configuration / MemberID (READ-ACK)**

The key frame for the §4.2 availability gate: `otDirectBoilerPresent()` in
`OTDirect.ino:292-295` returns `otBoilerCacheValid[3]`, which is set in
`handleMasterResponse()` when a real boiler ACKs MsgID 3.

Frame layout (32 bits, MSB first):

```
bit31   bits30-28   bits27-24   bits23-16   bits15-8           bits7-0
Parity  MsgType     Spare       Data-ID     HB (slave cfg)     LB (MemberID)
  0     100(=4)     0000        0x03        0x00               member_id
```

For `--member-id 4 --slave-config 0x00`: wire line `B40030004`
For `--member-id 4 --slave-config 0x01` (DHW present): wire line `BC0030104`

**MsgID 0 - Status (READ-ACK, all-zero slave status)**

Wire line `BC0000000` (healthy idle boiler: no fault, no flame, no active CH/DHW).

### Port 25238 input/output asymmetry - bench validation required

The OTGW32 port 25238 bridge is bidirectional but asymmetric:

- **Output** (firmware to client): monitor-stream lines with `B`/`T`/`R`/`A` prefixes
- **Input** (client to firmware): PIC-style commands (`XX=value` format)

A raw `B40030004` line sent as INPUT does not match the `XX=value` parser, so it will
NOT populate `otBoilerCacheValid[3]` via the TCP path.

**AC#2 and AC#3 of TASK-795** (actual §4.2 edge-trip on a real OTGW32: simulation
auto-disables within 1 second of first boiler slave frame) require bench validation by
the maintainer with actual OT bus hardware. The script is intended to provide:

1. A reference for what a correctly-formed slave-response frame looks like.
2. A TCP connection harness so bench state can be observed over the monitor stream.
3. A starting point for adapting to an OT-bus hardware slave device (e.g. via a
   USB-to-OT adapter or a microcontroller wired to the OTGW32 OT master pins).

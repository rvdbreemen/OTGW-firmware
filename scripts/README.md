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

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

### What it does

1. Parses `version.h` to extract version components
2. Increments the build number
3. Updates timestamp and git hash in `version.h`
4. Generates semantic version strings (_SEMVER_FULL, _SEMVER_CORE, etc.) in `version.h`
5. Updates `data/version.hash` with the git hash
6. Optionally commits changes to git

This script is used both by the CI/CD workflow and the local build script.

# ADR-014: Dual Build System (Makefile + Python Script)

**Status:** Accepted  
**Date:** 2020-01-01 (build.py added)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needs a build system that:
- Compiles Arduino sketch to ESP8266 binary
- Builds filesystem image from `data/` directory
- Generates versioned artifacts
- Works across platforms (Windows, macOS, Linux)
- Supports CI/CD pipelines
- Allows developers without Arduino IDE to build
- Auto-manages dependencies (arduino-cli)

**Historical context:**
- Original builds: Arduino IDE only
- Added Makefile for CI/CD
- Added build.py for cross-platform developer experience

## Decision

**Provide dual build system: Makefile (primary) + build.py wrapper (convenience).**

**Architecture:**
1. **arduino-cli:** Core build tool (downloads if missing)
2. **Makefile:** Build logic, targets, CI/CD integration
3. **build.py:** Python wrapper for ease of use, auto-installs arduino-cli
4. **version.h:** Git-derived version information

**Build targets:**
```bash
# Makefile
make                    # Build both firmware and filesystem
make firmware           # Build firmware only
make data               # Build filesystem image only
make clean              # Clean build artifacts

# build.py
python build.py                # Build everything
python build.py --firmware     # Firmware only
python build.py --filesystem   # Filesystem only
python build.py --clean        # Clean first
```

**Version management:**
- Git tags determine version (e.g., `v1.0.0`)
- Git commit hash embedded in firmware
- Build date/time captured
- All stored in `version.h` (generated)

## Alternatives Considered

### Alternative 1: Arduino IDE Only
**Pros:**
- Simple for beginners
- Visual interface
- Integrated serial monitor

**Cons:**
- No CI/CD support
- Manual library installation
- No automation
- Platform-specific
- Hard to version control build settings

**Why not chosen:** Cannot support CI/CD or automated builds.

### Alternative 2: PlatformIO
**Pros:**
- Modern build system
- Excellent IDE integration (VS Code)
- Dependency management
- Multi-platform support
- Library management

**Cons:**
- Requires all developers to use PlatformIO
- Different build configuration (platformio.ini vs Makefile)
- Arduino IDE users excluded
- Migration effort from Arduino

**Why not chosen:** Want to support both Arduino IDE and command-line builds. PlatformIO is either/or.

### Alternative 3: CMake
**Pros:**
- Industry standard
- Powerful
- Cross-platform
- IDE integration

**Cons:**
- Steep learning curve
- Overkill for Arduino projects
- Verbose configuration
- No Arduino ecosystem integration
- Complex for simple builds

**Why not chosen:** Too complex for Arduino ecosystem. Make is sufficient.

### Alternative 4: Custom Shell Scripts
**Pros:**
- Simple
- Lightweight
- Direct control

**Cons:**
- Platform-specific (bash vs cmd.exe)
- No dependency management
- Hard to maintain
- No standard conventions

**Why not chosen:** Make provides better structure and platform support.

## Consequences

### Positive
- **Flexibility:** Developers choose their preferred method
- **CI/CD:** Makefile integrates with GitHub Actions
- **Cross-platform:** build.py works on Windows/Mac/Linux
- **Automation:** Auto-installs arduino-cli if missing
- **Versioning:** Git integration provides automatic version tracking
- **Artifacts:** Versioned build outputs (firmware-v1.0.0.bin)
- **Arduino compatible:** Still works with Arduino IDE

### Negative
- **Dual maintenance:** Two build scripts to maintain
  - Mitigation: build.py is thin wrapper around Makefile
- **Dependency:** Requires Python for build.py (optional)
  - Accepted: Most developers have Python
- **Makefile complexity:** Make syntax can be confusing
  - Mitigation: Well-commented Makefile
- **arduino-cli download:** First build downloads ~50MB
  - Accepted: One-time cost, cached locally

### Risks & Mitigation
- **Build script divergence:** Makefile and build.py differ
  - **Mitigation:** build.py calls Make when available
- **arduino-cli version:** Different versions may behave differently
  - **Mitigation:** Pin to minimum version (0.35+)
- **Path issues:** arduino-cli not in PATH
  - **Mitigation:** build.py downloads to known location
- **Platform differences:** Makefiles can be platform-specific
  - **Mitigation:** Use portable Make syntax, test on all platforms

## Implementation Details

**Makefile targets:**
```makefile
SKETCH := OTGW-firmware.ino
BOARD := esp8266:esp8266:nodemcuv2
BUILD_DIR := build

.PHONY: all firmware data clean

all: firmware data

firmware:
	arduino-cli compile --fqbn $(BOARD) \
		--build-path $(BUILD_DIR) \
		$(SKETCH)

data:
	arduino-cli compile --fqbn $(BOARD) \
		--build-path $(BUILD_DIR) \
		--build-property "build.extra_flags=-DBUILD_FILESYSTEM" \
		$(SKETCH)
	# Generate LittleFS image from data/
	mklittlefs -c data -s 2097152 $(BUILD_DIR)/littlefs.bin

clean:
	rm -rf $(BUILD_DIR)
```

**build.py key features:**
```python
def ensure_arduino_cli():
    """Download arduino-cli if not present."""
    if not find_executable('arduino-cli'):
        download_arduino_cli()
        install_esp8266_core()
        install_libraries()

def build_firmware():
    """Build firmware binary."""
    run_command(['arduino-cli', 'compile', ...])

def build_filesystem():
    """Build LittleFS filesystem image."""
    run_command(['mklittlefs', ...])

def update_version():
    """Extract version from git, update version.h."""
    version = get_git_version()
    commit = get_git_commit()
    write_version_header(version, commit)
```

**Version extraction:**
```python
def get_git_version():
    # Get latest tag
    tag = subprocess.check_output(['git', 'describe', '--tags'])
    # Format: v1.0.0-5-gabcd1234
    # Parse into: 1.0.0 (5 commits ahead)
    return parse_version(tag)
```

**Versioned artifacts:**
```
build/
├── OTGW-firmware-v1.0.0-rc4.bin        # Firmware
├── OTGW-firmware-v1.0.0-rc4.elf        # Debug symbols
├── littlefs-v1.0.0-rc4.bin             # Filesystem
└── firmware-v1.0.0-rc4_1MB.bin         # Combined (OTA)
```

## Build Process Flow

**Firmware build:**
1. Update `version.h` from git
2. arduino-cli compiles all .ino files
3. Link libraries
4. Generate .bin and .elf files
5. Copy to build/ with version suffix

**Filesystem build:**
1. Scan `data/` directory
2. Generate LittleFS image (2MB)
3. Copy to build/ with version suffix

**CI/CD (GitHub Actions):**
```yaml
- name: Build firmware
  run: make firmware

- name: Build filesystem  
  run: make data

- name: Create release
  uses: actions/upload-artifact@v2
  with:
    path: build/*.bin
```

## Developer Workflows

**Arduino IDE users:**
```
1. Open OTGW-firmware.ino in Arduino IDE
2. Select board: NodeMCU 1.0 (ESP-12E Module)
3. Click "Upload" or "Verify"
```

**Command-line users:**
```bash
# One-time setup
python build.py  # Downloads arduino-cli, installs dependencies

# Regular builds
python build.py --firmware
python build.py --filesystem
python build.py --clean

# Or use Make
make clean all
```

**CI/CD pipelines:**
```bash
make firmware
make data
# Artifacts in build/
```

## Related Decisions
- ADR-013: Arduino Framework (arduino-cli compatibility)
- ADR-002: Modular .ino File Architecture (all files compiled together)

## References
- arduino-cli: https://arduino.github.io/arduino-cli/
- Makefile: `Makefile` in repository root
- Build script: `build.py` in repository root
- Build documentation: `BUILD.md`
- CI/CD workflow: `.github/workflows/build.yml`
- mklittlefs: https://github.com/earlephilhower/mklittlefs

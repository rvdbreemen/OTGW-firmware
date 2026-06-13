# Local Build Guide for OTGW-firmware

This guide explains how to build OTGW-firmware locally on Windows, Linux, or macOS using the provided build wrappers.

## Quick Start

```bash
# Clone the repository
git clone https://github.com/rvdbreemen/OTGW-firmware.git
cd OTGW-firmware

# Run the build script
build        # Windows Command Prompt
./build.sh   # Linux/macOS
```

Windows examples assume Command Prompt. In PowerShell, use `.\build.bat` unless the repository root is on PATH.

The build script will automatically:

1. Check for required dependencies
2. Check for unresolved merge-conflict markers in source files
3. Install PlatformIO if not present for the default backend
4. Install arduino-cli if not present when the legacy backend is selected
5. Update version information
6. Build the firmware and filesystem
7. Create versioned build artifacts in the `build/` directory

If unresolved Git merge markers such as `<<<<<<<`, `=======`, or `>>>>>>>` are present, `build.py` now fails early with the affected file paths instead of letting the compiler emit harder-to-diagnose downstream errors.

## Prerequisites

### Required Software

1. **Python 3.x**
   - Download from: <https://www.python.org/downloads/>
   - Make sure to check "Add Python to PATH" during installation

2. **Git** (for version information)
   - Windows: <https://git-scm.com/download/win>
   - Mac: Install via Xcode Command Line Tools (see below)

3. **make** (legacy arduino-cli backend only)
   - **Mac**: Install Xcode Command Line Tools

     ```bash
     xcode-select --install
     ```

   - **Windows**: Install via one of these methods:
     - Chocolatey: `choco install make`
     - GnuWin32: <http://gnuwin32.sourceforge.net/packages/make.htm>
     - MinGW/MSYS2: <https://www.msys2.org/>

### Automatic Dependencies

The build wrappers use or create a local `.build-venv` automatically. If `requirements-build.txt` or
`requirements.txt` exists, the listed Python packages are installed quietly before `build.py` is started. If Python is
not on PATH but an existing `.venv` works, the wrappers can use that as a fallback.

The build script will automatically install:

- **PlatformIO** - Installed via pip if not present for the default backend
- **arduino-cli** - Downloaded and installed to your local bin directory if not present for the legacy backend

## Configuration

The project uses a `config.py` file for configuration logic in python scripts (`build.py`, `evaluate.py`, `flash_esp.py`).

You can customize the following variable in `config.py` or via environment variable:

- `OTGW_BUILD_DIR`: Override the build output directory (default: `build`)

Example `config.py` (checked into repository):

```python
import os
from pathlib import Path

# Base Paths
PROJECT_DIR = Path(__file__).parent.resolve()

# Structural Config (Fixed)
PROJECT_NAME = "OTGW-firmware"
FIRMWARE_ROOT = PROJECT_DIR / "src" / "OTGW-firmware"
DATA_DIR = FIRMWARE_ROOT / "data"

# Environment Config (Overridable)
BUILD_DIR = PROJECT_DIR / os.getenv("OTGW_BUILD_DIR", "build")
```

## Build Script Usage

### Full Build (Firmware + Filesystem)

```bash
build        # Windows Command Prompt
./build.sh   # Linux/macOS
```

This builds both the firmware and filesystem images with versioned filenames.

### Build Firmware Only

```bash
build --firmware        # Windows Command Prompt
./build.sh --firmware   # Linux/macOS
```

### Build Filesystem Only

```bash
build --filesystem        # Windows Command Prompt
./build.sh --filesystem   # Linux/macOS
```

### Clean Build Artifacts

```bash
build --clean        # Windows Command Prompt
./build.sh --clean   # Linux/macOS
```

This removes all build artifacts and downloaded dependencies.

### Additional Options

```bash
build --target esp32          # Build OTGW32 (OTDirect) only
build --target esp32-classic  # Build S3-in-Classic-socket (PIC) only
build --no-merged        # Skip merged binary generation
build --no-zip           # Skip distribution zip generation
build --arduino-cli      # Use the legacy arduino-cli backend
build --no-color         # Disable colored output
build --help             # Show all options
```

## Build Output

Build artifacts are created in the `build/` directory:

```text
build/
├── OTGW-firmware-<version>.ino.bin       # Firmware binary
├── OTGW-firmware-<version>.ino.elf       # ELF file (for debugging)
└── OTGW-firmware.<version>.littlefs.bin  # Filesystem binary
```

Example:

```text
build/
├── OTGW-firmware-0.10.4+abc1234.ino.bin
├── OTGW-firmware-0.10.4+abc1234.ino.elf
└── OTGW-firmware.0.10.4+abc1234.littlefs.bin
```

## Platform-Specific Notes

### macOS

1. **Install Xcode Command Line Tools** (includes make and git):

   ```bash
   xcode-select --install
   ```

2. **Python**: macOS includes Python, but you may want to install Python 3 via:

   ```bash
   brew install python3
   ```

3. **arduino-cli installation path**: `~/.local/bin/arduino-cli`

### Windows

1. **Install Python 3**: Download from <https://www.python.org/downloads/>
   - Check "Add Python to PATH" during installation

2. **Install make**: Choose one of these options:
   - **Chocolatey** (recommended if you have it):

     ```cmd
     choco install make
     ```

   - **GnuWin32**: Download from <http://gnuwin32.sourceforge.net/packages/make.htm>
   - **MSYS2**: Full Unix-like environment for Windows

3. **arduino-cli installation path**: `%LOCALAPPDATA%\Arduino15\bin\arduino-cli.exe`

4. **PATH configuration**: You may need to add arduino-cli to your PATH:
   - Open "Environment Variables" in Windows settings
   - Add `%LOCALAPPDATA%\Arduino15\bin` to your PATH

## Troubleshooting

### "make not found"

**Mac**:

```bash
xcode-select --install
```

**Windows**: Install make using one of the methods mentioned above.

### "arduino-cli not found" (after installation)

The script installs arduino-cli to your local bin directory. Add it to your PATH:

**Mac/Linux**:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Add this line to your `~/.bashrc` or `~/.zshrc` for persistence.

**Windows**:
Add `%LOCALAPPDATA%\Arduino15\bin` to your system PATH via Environment Variables.

### Python version error

Make sure you have Python 3.x installed:

```bash
python --version
# or
python3 --version
```

### Build fails during compilation

1. **Clean and rebuild**:

   ```bash
   build --clean        # Windows Command Prompt
   build                # Windows Command Prompt
   ./build.sh --clean   # Linux/macOS
   ./build.sh           # Linux/macOS
   ```

2. **Check internet connection**: The first build downloads dependencies

3. **Check disk space**: Build requires several hundred MB

### Permission errors (Mac/Linux)

Make the script executable:

```bash
chmod +x build.sh
./build.sh
```

## Advanced Usage

### Using Makefile Directly

The build script uses the Makefile underneath. You can also use make directly:

```bash
make -j$(nproc)              # Build firmware (parallel)
make filesystem              # Build filesystem
make clean                   # Clean build files
make distclean              # Deep clean (removes arduino-cli, libraries, etc.)
```

### Custom Build Flags

Edit the `Makefile` to customize build flags:

```make
CFLAGS = $(CFLAGS_DEFAULT) -DYOUR_FLAG
```

### Debug Build

```bash
make debug
```

This builds with debug output enabled.

## Manual arduino-cli Installation

If the automatic installation doesn't work, you can install arduino-cli manually:

1. Download from: <https://arduino.github.io/arduino-cli/latest/installation/>
2. Extract the executable
3. Add to your PATH

**Mac/Linux**:

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

**Windows**:
Download the appropriate ZIP from <https://github.com/arduino/arduino-cli/releases>

## What the Build Script Does

The `build.py` script replicates the GitHub Actions CI/CD workflow locally:

1. **Checks dependencies**: Python, arduino-cli
2. **Installs arduino-cli**: If not already present
3. **Updates version**: Runs `scripts/autoinc-semver.py` to update `version.h`
4. **Creates build directory**: `build/`
5. **Builds firmware**: Runs `arduino-cli compile`
6. **Builds filesystem**: Runs `mklittlefs` to create LittleFS image
7. **Renames artifacts**: Adds version to filenames
8. **Lists output**: Shows all build artifacts with sizes

## Continuous Integration

The GitHub Actions workflow (`.github/workflows/main.yml`) uses the same Makefile and processes as this local build script, ensuring consistency between local and CI builds.

## Further Reading

- **Main README**: [../../README.md](../../README.md)
- **Wiki**: <https://github.com/rvdbreemen/OTGW-firmware/wiki>
- **Makefile reference**: [../../Makefile](../../Makefile)
- **Version script**: [../../scripts/autoinc-semver.py](../../scripts/autoinc-semver.py)

## Getting Help

- **Discord**: <https://discord.gg/zjW3ju7vGQ>
- **GitHub Issues**: <https://github.com/rvdbreemen/OTGW-firmware/issues>

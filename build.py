#!/usr/bin/env python3
"""
Local build script for OTGW-firmware
This script automates the build process for Windows and Mac platforms,
replicating the CI/CD workflow for local development.

Requirements:
- Python 3.x
- PlatformIO (installed automatically if not found)  [default backend]
- arduino-cli (installed automatically if not found) [legacy backend]

Usage:
    python build.py                      # Full build for ESP8266 + ESP32 (PlatformIO, incremental)
    python build.py --target esp8266     # Build for ESP8266 only
    python build.py --target esp32       # Build for ESP32 only
    python build.py --firmware           # Build firmware only
    python build.py --filesystem         # Build filesystem only
    python build.py --clean              # Clean build artifacts
    python build.py --distclean          # Clean build + cached dependencies
    python build.py --arduino-cli        # Use arduino-cli backend instead
    python build.py --help               # Show help
"""

import argparse
import gzip
import io
import multiprocessing
import os
import platform
import shutil
import ssl
import stat
import subprocess
import sys
import tarfile

# Ensure stdout/stderr can handle Unicode on Windows
if sys.platform == 'win32':
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    elif not isinstance(sys.stdout, io.TextIOWrapper) or sys.stdout.encoding != 'utf-8':
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
import traceback
import urllib.request
import zipfile
from pathlib import Path
import config

# =============================================================================
# Target definitions — one entry per supported chip family
# =============================================================================
TARGETS = {
    "esp8266": {
        "name": "ESP8266",
        "core": "esp8266:esp8266",
        "board_manager_url": "https://github.com/esp8266/Arduino/releases/download/3.1.2/package_esp8266com_index.json",
        "fqbn": "esp8266:esp8266:d1_mini:eesz=4M2M,xtal=160,ip=lm2f",
        "build_flags": "-DNO_GLOBAL_HTTPUPDATE -DBOARD_NODOSHOP_ESP8266",
        "chip": "esp8266",
        "flash_mode": "dio",
        "flash_freq": "40m",
        "flash_size": "4MB",
        "firmware_offset": "0x0",
        "fs_offset": "0x200000",
        "fs_tool_path": "esp8266/tools/mklittlefs",
        "fs_block": 8192,
        "fs_page": 256,
        "fs_size": 2072576,    # FS_PHYS_SIZE from eagle.flash.4m2m.ld (0x1FA000)
    },
    "esp32": {
        "name": "ESP32-S3",
        "core": "esp32:esp32",
        "board_manager_url": "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json",
        "fqbn": "esp32:esp32:esp32s3:PartitionScheme=custom",
        "build_flags": "-DNO_GLOBAL_HTTPUPDATE -DBOARD_NODOSHOP_ESP32",
        "chip": "esp32s3",       # Nodoshop OTGW32 uses ESP32-S3
        "flash_mode": "qio",     # ESP32-S3 uses QIO (not DIO)
        "flash_freq": "80m",
        "flash_size": "4MB",
        "firmware_offset": "0x10000",
        "fs_offset": "0x2F0000",   # from partitions_otgw_esp32.csv (partition labeled "spiffs" for LittleFS.begin() compat)
        "fs_tool_path": "esp32/tools/mklittlefs",
        "fs_block": 4096,
        "fs_page": 256,
        "fs_size": 1048576,       # 0x100000 = 1 MB — matches partitions_otgw_esp32.csv
        "bootloader_offset": "0x0",  # ESP32-S3 bootloader is at 0x0 (not 0x1000)
    },
}

class Colors:
    """ANSI color codes for terminal output"""

    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

    @staticmethod
    def disable():
        """Disable colors for Windows if not supported"""
        Colors.HEADER = ''
        Colors.OKBLUE = ''
        Colors.OKCYAN = ''
        Colors.OKGREEN = ''
        Colors.WARNING = ''
        Colors.FAIL = ''
        Colors.ENDC = ''
        Colors.BOLD = ''
        Colors.UNDERLINE = ''


def print_step(message):
    """Print a build step message"""
    print(f"\n{Colors.OKBLUE}{'='*60}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.OKBLUE}[STEP] {message}{Colors.ENDC}")
    print(f"{Colors.OKBLUE}{'='*60}{Colors.ENDC}\n")


def print_success(message):
    """Print a success message"""
    print(f"{Colors.OKGREEN}✓ {message}{Colors.ENDC}")


def print_error(message):
    """Print an error message"""
    print(f"{Colors.FAIL}✗ ERROR: {message}{Colors.ENDC}", file=sys.stderr)


def print_warning(message):
    """Print a warning message"""
    print(f"{Colors.WARNING}⚠ WARNING: {message}{Colors.ENDC}")


def print_info(message):
    """Print an info message"""
    print(f"{Colors.OKCYAN}ℹ {message}{Colors.ENDC}")


def run_command(cmd, cwd=None, env=None, check=True, capture_output=False, show_output=True):
    """Run a shell command and handle errors"""
    try:
        if isinstance(cmd, str):
            cmd_str = cmd
            shell = True
        else:
            cmd_str = ' '.join(cmd)
            shell = False
        
        print_info(f"Running: {cmd_str}")
        
        if capture_output:
            # Capture output for checking
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=env,
                check=check,
                shell=shell,
                capture_output=True,
                text=True
            )
        else:
            # Stream output in real-time for better visibility
            result = subprocess.run(
                cmd,
                cwd=cwd,
                env=env,
                check=check,
                shell=shell,
                stdout=None if show_output else subprocess.DEVNULL,
                stderr=None if show_output else subprocess.DEVNULL,
                text=True
            )
        return result
    except subprocess.CalledProcessError as e:
        print_error(f"Command failed: {cmd_str}")
        if capture_output:
            if e.stdout:
                print(f"STDOUT:\n{e.stdout}")
            if e.stderr:
                print(f"STDERR:\n{e.stderr}")
        sys.exit(1)


def get_system_info():
    """Get system information"""
    system = platform.system()
    machine = platform.machine()
    print_info(f"Detected system: {system} ({machine})")
    return system, machine


def check_python_version():
    """Check if Python version is adequate"""
    print_step("Checking Python version")
    version = sys.version_info
    if version.major < 3:
        print_error(f"Python 3.x is required, but found Python {version.major}.{version.minor}")
        sys.exit(1)
    print_success(f"Python {version.major}.{version.minor}.{version.micro}")


def setup_arduino_config(project_dir, target_names):
    """Setup arduino-cli configuration"""
    print_step("Configuring arduino-cli")

    arduino_dir = project_dir / "arduino"
    arduino_dir.mkdir(exist_ok=True)

    config_file = arduino_dir / "arduino-cli.yaml"

    # Initialize config
    run_command(["arduino-cli", "config", "init", "--dest-file", str(config_file), "--overwrite"])

    # Collect unique board manager URLs for the requested targets
    urls = list(dict.fromkeys(
        TARGETS[t]["board_manager_url"] for t in target_names
    ))

    # Set config values
    configs = [
        ["directories.data", str(arduino_dir)],
        ["board_manager.additional_urls"] + urls,
        ["directories.downloads", str(project_dir / "staging")],
        ["directories.user", str(project_dir)],
        ["sketch.always_export_binaries", "true"],
        ["library.enable_unsafe_install", "true"]
    ]

    for entry in configs:
        key = entry[0]
        values = entry[1:]
        run_command(["arduino-cli", "config", "set", key] + values + ["--config-file", str(config_file)])

    return config_file


def install_dependencies(project_dir, config_file, target_names):
    """Install cores and libraries for the requested targets"""
    print_step("Installing dependencies")

    cmd_base = ["arduino-cli", "--config-file", str(config_file)]

    # Update index
    print_info("Updating core index...")
    run_command(cmd_base + ["core", "update-index"])

    # Install cores for each requested target
    installed_cores = set()
    for t in target_names:
        core = TARGETS[t]["core"]
        if core not in installed_cores:
            print_info(f"Installing {TARGETS[t]['name']} core ({core})...")
            run_command(cmd_base + ["core", "install", core])
            installed_cores.add(core)

    # Update lib index
    print_info("Updating library index...")
    run_command(cmd_base + ["lib", "update-index"])

    # Install libraries (shared across all targets)
    libraries = [
        "WiFiManager@2.0.17",
        "pubsubclient@2.8.0",
        "TelnetStream@1.3.0",
        "NetApiHelpers@1.0.2",
        "AceCommon@1.6.2",
        "AceSorting@1.0.0",
        "AceTime@2.0.1",
        "OneWire@2.3.8",
        "DallasTemperature@4.0.6",
        "WebSockets@2.3.6"
    ]

    for lib in libraries:
        print_info(f"Installing {lib}...")
        run_command(cmd_base + ["lib", "install", lib])


def install_arduino_cli(system):
    """Install arduino-cli if not present. Returns the installation directory."""
    print_step("Checking for arduino-cli")
    
    # Determine installation directory
    if system == "Windows":
        install_dir = Path.home() / "AppData" / "Local" / "Arduino15" / "bin"
    else:
        install_dir = Path.home() / ".local" / "bin"
    
    # Check if arduino-cli is already installed in PATH or install_dir
    try:
        result = run_command(["arduino-cli", "version"], capture_output=True, check=False)
        if result.returncode == 0:
            print_success(f"arduino-cli is already installed: {result.stdout.strip()}")
            return install_dir
    except FileNotFoundError:
        # Check in the expected install directory
        cli_exe_name = "arduino-cli.exe" if system == "Windows" else "arduino-cli"
        cli_path = install_dir / cli_exe_name
        if cli_path.exists():
            print_success(f"arduino-cli found at {cli_path}")
            return install_dir
    
    print_info("arduino-cli not found, installing...")
    
    # Determine download URL based on system
    base_url = "https://downloads.arduino.cc/arduino-cli"
    version = "latest"
    
    if system == "Darwin":  # macOS
        if platform.machine() == "arm64":
            filename = "arduino-cli_latest_macOS_ARM64.tar.gz"
        else:
            filename = "arduino-cli_latest_macOS_64bit.tar.gz"
    elif system == "Windows":
        if platform.machine().endswith("64"):
            filename = "arduino-cli_latest_Windows_64bit.zip"
        else:
            filename = "arduino-cli_latest_Windows_32bit.zip"
    elif system == "Linux":
        if "arm" in platform.machine().lower():
            if "64" in platform.machine():
                filename = "arduino-cli_latest_Linux_ARM64.tar.gz"
            else:
                filename = "arduino-cli_latest_Linux_ARMv7.tar.gz"
        elif "64" in platform.machine():
            filename = "arduino-cli_latest_Linux_64bit.tar.gz"
        else:
            filename = "arduino-cli_latest_Linux_32bit.tar.gz"
    else:
        print_error(f"Unsupported system: {system}")
        sys.exit(1)
    
    url = f"{base_url}/{filename}"
    
    # Create a temporary directory for download
    temp_dir = Path.cwd() / "temp_arduino_cli"
    temp_dir.mkdir(exist_ok=True)
    
    try:
        # Download arduino-cli
        print_info(f"Downloading from {url}...")
        download_path = temp_dir / filename
        
        # Use unverified SSL context to avoid certificate errors on macOS
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        
        with urllib.request.urlopen(url, context=ctx) as response, open(download_path, 'wb') as out_file:
            shutil.copyfileobj(response, out_file)
            
        print_success("Download complete")
        
        # Extract
        print_info("Extracting...")
        if filename.endswith(".tar.gz"):
            with tarfile.open(download_path, "r:gz") as tar:
                tar.extractall(temp_dir)
        elif filename.endswith(".zip"):
            with zipfile.ZipFile(download_path, "r") as zip_ref:
                zip_ref.extractall(temp_dir)
        
        # Find the extracted executable
        if system == "Windows":
            cli_exe = temp_dir / "arduino-cli.exe"
        else:
            cli_exe = temp_dir / "arduino-cli"
        
        if not cli_exe.exists():
            print_error("arduino-cli executable not found after extraction")
            sys.exit(1)
        
        # Make executable on Unix systems
        if system != "Windows":
            cli_exe.chmod(cli_exe.stat().st_mode | stat.S_IEXEC)
        
        # Determine installation directory
        if system == "Windows":
            install_dir = Path.home() / "AppData" / "Local" / "Arduino15" / "bin"
        else:
            install_dir = Path.home() / ".local" / "bin"
        
        install_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy to installation directory
        dest_path = install_dir / cli_exe.name
        shutil.copy2(cli_exe, dest_path)
        
        # Make executable
        if system != "Windows":
            dest_path.chmod(dest_path.stat().st_mode | stat.S_IEXEC)
        
        print_success(f"arduino-cli installed to {dest_path}")
        
        # Return the installation directory
        return install_dir
        
    finally:
        # Clean up
        if temp_dir.exists():
            shutil.rmtree(temp_dir, ignore_errors=True)


def update_version(project_dir):
    """Update version.h using autoinc-semver.py"""
    print_step("Updating version information")
    
    script_path = project_dir / "scripts" / "autoinc-semver.py"
    if not script_path.exists():
        print_error(f"autoinc-semver.py not found at {script_path}")
        sys.exit(1)
    
    # Get git hash if available
    try:
        result = run_command(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=project_dir,
            capture_output=True,
            check=False
        )
        githash = result.stdout.strip() if result.returncode == 0 else "local"
    except (subprocess.SubprocessError, FileNotFoundError, OSError):
        githash = "local"
    
    # Run autoinc-semver.py
    cmd = [
        sys.executable,
        str(script_path),
        str(project_dir / "src" / "OTGW-firmware"),
        "--filename", "version.h",
        "--githash", githash
    ]
    run_command(cmd)
    print_success("Version information updated")


def get_semver(project_dir):
    """Extract semantic version from version.h"""
    version_file = project_dir / "src" / "OTGW-firmware" / "version.h"
    if not version_file.exists():
        return "unknown"
    
    try:
        with open(version_file, 'r') as f:
            for line in f:
                if '#define _SEMVER_FULL' in line:
                    # Extract version string between quotes
                    parts = line.split('"')
                    if len(parts) >= 2:
                        return parts[1]
    except (IOError, OSError):
        pass
    return "unknown"


def create_build_directory(project_dir):
    """Create and clean build directory"""
    print_step("Preparing build directory")
    
    build_dir = config.BUILD_DIR
    
    # Clean existing build directory if it exists
    if build_dir.exists():
        print_info("Cleaning existing build directory...")
        try:
            shutil.rmtree(build_dir)
            print_success("Build directory cleaned")
        except Exception as e:
            print_warning(f"Could not clean build directory: {e}")
    
    # Create fresh build directory
    build_dir.mkdir(exist_ok=True)
    print_success(f"Build directory ready: {build_dir}")
    return build_dir


def build_firmware(project_dir, config_file, target):
    """Build firmware using arduino-cli for the given target"""
    tcfg = TARGETS[target]
    print_step(f"Building firmware [{tcfg['name']}]")

    # Use target-specific temporary directory for build artifacts
    temp_build_dir = config.TEMP_DIR / f"build-{target}"
    temp_build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        "arduino-cli",
        "compile",
        "--fqbn", tcfg["fqbn"],
        "--warnings", "default",
        "--verbose",
        "--libraries", str(project_dir / "src" / "libraries"),
        "--build-property", f"compiler.cpp.extra_flags=\"{tcfg['build_flags']}\"",
        "--build-path", str(temp_build_dir),
        "--config-file", str(config_file),
        str(config.FIRMWARE_ROOT)
    ]

    run_command(cmd, cwd=project_dir, show_output=True)
    print_success(f"Firmware build complete [{tcfg['name']}]")


def build_filesystem(project_dir, config_file, target):
    """Build filesystem using mklittlefs for the given target"""
    tcfg = TARGETS[target]
    print_step(f"Building filesystem [{tcfg['name']}]")

    # Find mklittlefs under the target's tool path
    # e.g. arduino/packages/esp8266/tools/mklittlefs/*/mklittlefs(.exe)
    tools_dir = project_dir / "arduino" / "packages" / tcfg["fs_tool_path"]

    mklittlefs_path = None
    if tools_dir.exists():
        for path in tools_dir.glob("**/mklittlefs*"):
            if path.is_file() and path.name in ("mklittlefs", "mklittlefs.exe"):
                mklittlefs_path = path
                break

    if not mklittlefs_path:
        print_error(f"mklittlefs not found for {tcfg['name']}. Make sure the {tcfg['core']} core is installed.")
        sys.exit(1)

    print_info(f"Using mklittlefs: {mklittlefs_path}")

    fs_dir = config.DATA_DIR
    output_file = config.BUILD_DIR / f"{config.PROJECT_NAME}-{target}.littlefs.bin"

    # Ensure build dir exists
    output_file.parent.mkdir(exist_ok=True)

    cmd = [
        str(mklittlefs_path),
        "-p", str(tcfg["fs_page"]),
        "-b", str(tcfg["fs_block"]),
        "-s", str(tcfg["fs_size"]),
        "-c", str(fs_dir),
        str(output_file)
    ]

    run_command(cmd, cwd=project_dir, show_output=True)
    print_success(f"Filesystem build complete [{tcfg['name']}]")


def consolidate_build_artifacts(project_dir, target):
    """Move build artifacts from the target's temporary directory to build root"""
    tcfg = TARGETS[target]
    print_step(f"Consolidating build artifacts [{tcfg['name']}]")

    build_dir = config.BUILD_DIR
    build_dir.mkdir(exist_ok=True)

    # Target-specific temporary build directory
    temp_build_dir = config.TEMP_DIR / f"build-{target}"

    moved = []

    def process_artifact(source_path, dest_dir, rename_to=None):
        if not source_path.exists():
            return False
        dest_name = rename_to or source_path.name
        dest_path = dest_dir / dest_name
        if dest_path.exists() and dest_path != source_path:
            try:
                if source_path.resolve() == dest_path.resolve():
                    return False
            except OSError:
                pass
            print_warning(f"File {dest_name} already exists, overwriting")
            dest_path.unlink()
        if source_path != dest_path:
            shutil.move(str(source_path), str(dest_path))
            print_info(f"Moved: {source_path.name} -> {dest_name}")
            moved.append(dest_path)
            return True
        return False

    # Move firmware .bin from temp build dir, renaming to include target
    if temp_build_dir.exists():
        for file_path in temp_build_dir.glob("**/*.ino.bin"):
            # Rename: OTGW-firmware.ino.bin -> OTGW-firmware-esp8266.ino.bin
            new_name = file_path.name.replace(".ino.bin", f"-{target}.ino.bin")
            process_artifact(file_path, build_dir, rename_to=new_name)

    # Move any remaining artifacts from subdirectories in build/
    for pattern in ["**/*.ino.bin", "**/*.littlefs.bin"]:
        for file_path in build_dir.glob(pattern):
            if file_path.parent == build_dir:
                continue
            process_artifact(file_path, build_dir)

    if moved:
        print_success(f"Consolidated {len(moved)} artifact(s) to build root")

    # Remove empty subdirectories in build/
    for item in build_dir.iterdir():
        if item.is_dir():
            try:
                shutil.rmtree(item)
                print_info(f"Removed directory: {item.name}")
            except Exception as e:
                print_warning(f"Could not remove {item.name}: {e}")

    print_success(f"Build directory cleaned [{tcfg['name']}]")


def rename_build_artifacts(project_dir, semver, target):
    """Rename build artifacts with version number for the given target"""
    tcfg = TARGETS[target]
    print_step(f"Renaming build artifacts [{tcfg['name']}]")

    build_dir = config.BUILD_DIR
    if not build_dir.exists():
        print_warning("Build directory not found, skipping rename")
        return

    renamed = []

    # Rename firmware binaries that belong to this target
    for file_path in build_dir.glob(f"*-{target}.ino.bin"):
        new_name = file_path.stem.replace(".ino", "") + f"-{semver}.ino.bin"
        new_path = file_path.parent / new_name
        file_path.rename(new_path)
        renamed.append(new_path)
        print_info(f"Renamed: {file_path.name} -> {new_name}")

    # Rename filesystem binaries that belong to this target
    for file_path in build_dir.glob(f"*-{target}.littlefs.bin"):
        base_name = file_path.stem.replace(".littlefs", "")
        new_name = base_name + f"-{semver}.littlefs.bin"
        new_path = file_path.parent / new_name
        file_path.rename(new_path)
        renamed.append(new_path)
        print_info(f"Renamed: {file_path.name} -> {new_name}")

    # Rename ELF file that belongs to this target
    for file_path in build_dir.glob(f"*-{target}.elf"):
        new_name = file_path.stem + f"-{semver}.elf"
        new_path = file_path.parent / new_name
        file_path.rename(new_path)
        renamed.append(new_path)
        print_info(f"Renamed: {file_path.name} -> {new_name}")

    if renamed:
        print_success(f"Renamed {len(renamed)} artifact(s)")

    return renamed


def list_build_artifacts(project_dir):
    """List all build artifacts"""
    print_step("Build artifacts")
    
    build_dir = project_dir / "build"
    if not build_dir.exists():
        print_warning("Build directory not found")
        return
    
    artifacts = list(build_dir.glob("*.bin")) + list(build_dir.glob("*.elf"))
    
    if not artifacts:
        print_warning("No build artifacts found")
        return
    
    print(f"\n{Colors.BOLD}Build artifacts in {build_dir}:{Colors.ENDC}")
    for artifact in sorted(artifacts):
        size = artifact.stat().st_size
        size_mb = size / (1024 * 1024)
        print(f"  • {artifact.name} ({size_mb:.2f} MB)")
    print()


def create_merged_binary(project_dir, semver, target, compress=False, include_filesystem=True):
    """Create a merged binary using esptool merge_bin.

    Args:
        project_dir:        Project directory path
        semver:             Semantic version string
        target:             Target key ("esp8266" or "esp32")
        compress:           If True, also create a gzip-compressed version
        include_filesystem: If True, include the LittleFS image (full factory binary).
                            If False, produce bootloader + partitions + app only
                            (preserves existing filesystem when flashed).

    Returns:
        Path to the merged binary (or compressed version if compress=True)
    """
    tcfg = TARGETS[target]
    variant = "full" if include_filesystem else "fw"
    print_step(f"Creating merged binary [{tcfg['name']}] (variant: {variant})")

    build_dir = project_dir / "build"
    if not build_dir.exists():
        print_error("Build directory not found")
        return None

    # Find firmware and filesystem files for this target
    firmware_file = None
    filesystem_file = None

    for pattern in [f"*-{target}-{semver}*.ino.bin", f"*-{target}*.ino.bin"]:
        matches = list(build_dir.glob(pattern))
        matches = [m for m in matches if "littlefs" not in m.name.lower() and "merged" not in m.name.lower()]
        if matches:
            firmware_file = sorted(matches)[0]
            break

    for pattern in [f"*-{target}-{semver}*.littlefs.bin", f"*-{target}*.littlefs.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            filesystem_file = sorted(matches)[0]
            break

    if not firmware_file:
        print_error(f"Firmware binary not found for {tcfg['name']}")
        return None

    if include_filesystem and not filesystem_file:
        print_warning("Filesystem binary not found - falling back to firmware-only merged binary")
        include_filesystem = False

    print_info(f"Firmware: {firmware_file.name}")
    if include_filesystem and filesystem_file:
        print_info(f"Filesystem: {filesystem_file.name}")

    # Create output filename
    # -merged-full.bin  = bootloader + partitions + app + filesystem (factory flash)
    # -merged.bin       = bootloader + partitions + app only (preserves existing filesystem)
    suffix = "merged-full" if include_filesystem else "merged"
    if semver and semver != "unknown":
        merged_name = f"OTGW-firmware-{target}-{semver}-{suffix}.bin"
    else:
        merged_name = f"OTGW-firmware-{target}-{suffix}.bin"

    merged_file = build_dir / merged_name

    # Build esptool merge_bin command
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", tcfg["chip"],
        "merge_bin",
        "-o", str(merged_file),
        "--flash_mode", tcfg["flash_mode"],
        "--flash_freq", tcfg["flash_freq"],
        "--flash_size", tcfg["flash_size"],
    ]

    # ESP32 needs bootloader + partition table in the merged image
    if "bootloader_offset" in tcfg:
        # Search paths: arduino-cli temp dir, build/ (PlatformIO artifacts), and .pio/build/<env>
        temp_build_dir = config.TEMP_DIR / f"build-{target}"
        pio_build_dir = project_dir / ".pio" / "build" / PIO_ENV_MAP.get(target, target)
        search_dirs = [d for d in [temp_build_dir, pio_build_dir, build_dir] if d.exists()]

        # Find bootloader
        bootloader = None
        for sd in search_dirs:
            for bl in sd.glob("**/bootloader.bin"):
                bootloader = bl
                break
            if bootloader:
                break
        if bootloader:
            cmd.extend([tcfg["bootloader_offset"], str(bootloader)])
            print_info(f"Bootloader: {bootloader.name}")
        else:
            print_warning("Bootloader not found in build output — merged image may not be bootable")

        # Find partition table
        partitions = None
        for sd in search_dirs:
            for pt in sd.glob("**/partitions.bin"):
                partitions = pt
                break
            if partitions:
                break
        if not partitions:
            for sd in search_dirs:
                for pt in sd.glob("**/*.partitions.bin"):
                    partitions = pt
                    break
                if partitions:
                    break
        if partitions:
            cmd.extend(["0x8000", str(partitions)])
            print_info(f"Partitions: {partitions.name}")

        # Find boot_app0.bin (arduino-cli packages or PlatformIO packages)
        boot_app = None
        for packages_dir in [project_dir / "arduino" / "packages" / "esp32",
                             project_dir / ".platformio" / "packages"]:
            if packages_dir.exists():
                for ba in packages_dir.glob("**/boot_app0.bin"):
                    boot_app = ba
                    break
            if boot_app:
                break
        if boot_app:
            cmd.extend(["0xe000", str(boot_app)])

    # Firmware
    cmd.extend([tcfg["firmware_offset"], str(firmware_file)])

    # Filesystem (only for full variant)
    if include_filesystem and filesystem_file:
        cmd.extend([tcfg["fs_offset"], str(filesystem_file)])

    print_info("Running: esptool merge_bin...")

    try:
        run_command(cmd, cwd=project_dir, capture_output=True, show_output=False)

        if merged_file.exists():
            size_mb = merged_file.stat().st_size / (1024 * 1024)
            print_success(f"Created merged binary: {merged_name} ({size_mb:.2f} MB)")

            if compress:
                print_info("Compressing merged binary with gzip...")
                compressed_file = build_dir / f"{merged_name}.gz"

                try:
                    with open(merged_file, 'rb') as f_in:
                        with gzip.open(compressed_file, 'wb', compresslevel=9) as f_out:
                            shutil.copyfileobj(f_in, f_out)

                    compressed_size_mb = compressed_file.stat().st_size / (1024 * 1024)
                    compression_ratio = (1 - compressed_file.stat().st_size / merged_file.stat().st_size) * 100
                    print_success(f"Created compressed binary: {compressed_file.name} ({compressed_size_mb:.2f} MB, {compression_ratio:.1f}% reduction)")
                    return compressed_file

                except Exception as e:
                    print_warning(f"Compression failed: {e}")
                    return merged_file

            return merged_file
        else:
            print_error("Merged binary was not created")
            return None

    except Exception as e:
        print_error(f"Failed to create merged binary: {e}")
        print_info("Make sure esptool is installed: pip install esptool")
        return None


def check_esptool():
    """Check if esptool is installed, and install it if not."""
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "version"],
            capture_output=True,
            text=True,
            check=False
        )
        if result.returncode == 0:
            print_success("esptool is already installed")
            return True
    except Exception:
        pass

    print_info("esptool not found. Installing...")
    
    # Try multiple installation strategies
    install_attempts = [
        ([sys.executable, "-m", "pip", "install", "--user", "esptool"], "user installation"),
        ([sys.executable, "-m", "pip", "install", "--break-system-packages", "esptool"], "system installation with override"),
        ([sys.executable, "-m", "pip", "install", "esptool"], "standard installation"),
    ]
    
    for cmd, description in install_attempts:
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=False
            )
            if result.returncode == 0:
                print_success(f"esptool installed successfully ({description})")
                return True
        except Exception:
            continue
    
    print_error("Failed to install esptool automatically")
    print_info("Please install esptool manually: pip install esptool")
    return False


def cleanup_temp_directory(project_dir):
    """Clean up temporary build directory"""
    print_step("Cleaning up temporary files")

    temp_dir = config.TEMP_DIR

    if temp_dir.exists():
        print_info(f"Removing {temp_dir}...")
        try:
            shutil.rmtree(temp_dir)
            print_success("Temporary directory cleaned")
        except Exception as e:
            print_warning(f"Could not remove {temp_dir}: {e}")
    else:
        print_info("No temporary directory to clean")


# =============================================================================
# PlatformIO backend
# =============================================================================

# Map target names to PlatformIO environment names
PIO_ENV_MAP = {
    "esp8266": "esp8266",
    "esp32": "esp32",
}


def check_platformio():
    """Find PlatformIO CLI, add to PATH if needed, auto-install if missing."""
    print_step("Checking for PlatformIO")
    system = platform.system()

    def _pio_in_path():
        try:
            r = subprocess.run(["pio", "--version"], capture_output=True, text=True, check=False)
            if r.returncode == 0:
                print_success(f"PlatformIO: {r.stdout.strip()}")
                return True
        except FileNotFoundError:
            pass
        return False

    # Common install locations that may not be in PATH.
    # Always check the PlatformIO penv FIRST: it runs under PlatformIO's own
    # bundled Python (3.11.x) and will not be rejected by PlatformIO's own
    # Python-version guard. System-wide pip-installed pio may run under a newer
    # Python (e.g. 3.14) that PlatformIO rejects.
    if system == "Windows":
        python_scripts = (
            Path(os.environ.get("LOCALAPPDATA", ""))
            / "Programs" / "Python"
            / f"Python{sys.version_info.major}{sys.version_info.minor}"
            / "Scripts"
        )
        candidates = [
            Path.home() / ".platformio" / "penv" / "Scripts",
            python_scripts,
        ]
    else:
        candidates = [
            Path.home() / ".platformio" / "penv" / "bin",
            Path.home() / ".local" / "bin",
        ]

    exe_name = "pio.exe" if system == "Windows" else "pio"
    for scripts_dir in candidates:
        if (scripts_dir / exe_name).is_file():
            os.environ["PATH"] = f"{scripts_dir}{os.pathsep}{os.environ.get('PATH', '')}"
            print_info(f"Added {scripts_dir} to PATH")
            if _pio_in_path():
                return True

    # penv not found or non-functional — fall back to whatever pio is in PATH
    if _pio_in_path():
        return True

    # Not found anywhere — try to install via pip
    print_info("PlatformIO not found — installing via pip...")
    for cmd in [
        [sys.executable, "-m", "pip", "install", "--user", "platformio"],
        [sys.executable, "-m", "pip", "install", "platformio"],
    ]:
        r = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if r.returncode == 0:
            print_success("PlatformIO installed successfully")
            if _pio_in_path():
                return True
            # Re-check the candidate paths after install
            for scripts_dir in candidates:
                if (scripts_dir / exe_name).is_file():
                    os.environ["PATH"] = f"{scripts_dir}{os.pathsep}{os.environ.get('PATH', '')}"
                    if _pio_in_path():
                        return True
            break

    print_error("Could not find or install PlatformIO. Run: pip install platformio")
    sys.exit(1)


def build_firmware_pio(project_dir, target):
    """Build firmware using PlatformIO."""
    tcfg = TARGETS[target]
    env_name = PIO_ENV_MAP[target]
    print_step(f"Building firmware [{tcfg['name']}] (PlatformIO)")
    run_command(["pio", "run", "-e", env_name], cwd=project_dir)
    print_success(f"Firmware build complete [{tcfg['name']}]")


def build_filesystem_pio(project_dir, target):
    """Build LittleFS filesystem using PlatformIO."""
    tcfg = TARGETS[target]
    env_name = PIO_ENV_MAP[target]
    print_step(f"Building filesystem [{tcfg['name']}] (PlatformIO)")
    run_command(["pio", "run", "-e", env_name, "-t", "buildfs"], cwd=project_dir)
    print_success(f"Filesystem build complete [{tcfg['name']}]")


def collect_pio_artifacts(project_dir, target):
    """Copy PlatformIO build artifacts to the common build/ directory."""
    tcfg = TARGETS[target]
    env_name = PIO_ENV_MAP[target]
    print_step(f"Collecting build artifacts [{tcfg['name']}]")

    pio_build_dir = project_dir / ".pio" / "build" / env_name
    build_dir = config.BUILD_DIR
    build_dir.mkdir(exist_ok=True)

    collected = []

    # Firmware binary
    fw_src = pio_build_dir / "firmware.bin"
    if fw_src.exists():
        fw_dest = build_dir / f"{config.PROJECT_NAME}-{target}.ino.bin"
        shutil.copy2(fw_src, fw_dest)
        print_info(f"Copied: firmware.bin -> {fw_dest.name}")
        collected.append(fw_dest)

    # Filesystem binary
    fs_src = pio_build_dir / "littlefs.bin"
    if fs_src.exists():
        fs_dest = build_dir / f"{config.PROJECT_NAME}-{target}.littlefs.bin"
        shutil.copy2(fs_src, fs_dest)
        print_info(f"Copied: littlefs.bin -> {fs_dest.name}")
        collected.append(fs_dest)

    # ELF file — always collected for crash debugging (addr2line, exception decoder)
    elf_src = pio_build_dir / "firmware.elf"
    if elf_src.exists():
        elf_dest = build_dir / f"{config.PROJECT_NAME}-{target}.elf"
        shutil.copy2(elf_src, elf_dest)
        print_info(f"Copied: firmware.elf -> {elf_dest.name}")
        collected.append(elf_dest)

    # ESP32 extras needed for merged binary
    if target == "esp32":
        for extra in ["bootloader.bin", "partitions.bin"]:
            src = pio_build_dir / extra
            if src.exists():
                dest = build_dir / f"{target}-{extra}"
                shutil.copy2(src, dest)
                print_info(f"Copied: {extra} -> {dest.name}")
                collected.append(dest)

    if collected:
        print_success(f"Collected {len(collected)} artifact(s)")
    else:
        print_warning("No build artifacts found in PlatformIO output")

    return collected


def clean_build(project_dir, distclean=False):
    """Clean build artifacts. distclean also removes downloaded cores/libraries and PlatformIO cache."""
    print_step("Cleaning build artifacts" + (" (distclean)" if distclean else ""))

    # Always clean
    always_clean = [
        config.BUILD_DIR,
        config.TEMP_DIR,
        project_dir / ".pio" / "build",   # PlatformIO incremental build cache
    ]

    # Only on distclean: arduino-cli downloads and full PlatformIO directory
    distclean_extras = [
        project_dir / "arduino",
        project_dir / "staging",
        project_dir / ".pio",             # Also removes libdeps, cache, etc.
    ]

    targets = always_clean + (distclean_extras if distclean else [])

    for d in targets:
        if d.exists():
            print_info(f"Removing {d}...")
            try:
                shutil.rmtree(d)
            except Exception as e:
                print_warning(f"Could not remove {d}: {e}")

    print_success("Clean complete")


def main():
    """Main build script entry point"""
    parser = argparse.ArgumentParser(
        description="Local build script for OTGW-firmware",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py                              # Full build (PlatformIO, incremental, default)
  python build.py --target esp8266             # ESP8266 only
  python build.py --target esp32               # ESP32 only
  python build.py --firmware                   # Build firmware only
  python build.py --filesystem                 # Build filesystem only
  python build.py --merged                     # Build and create merged binary
  python build.py --merged --compress          # Build and create compressed merged binary
  python build.py --clean                      # Clean build artifacts (keeps PlatformIO libdeps)
  python build.py --distclean                  # Full clean including cores/libraries cache
  python build.py --arduino-cli                # Use legacy arduino-cli backend
        """
    )

    # Backend selection
    backend_group = parser.add_mutually_exclusive_group()
    backend_group.add_argument(
        "--backend",
        choices=["arduino-cli", "platformio"],
        default="platformio",
        help="Build backend (default: platformio)"
    )
    backend_group.add_argument(
        "--arduino-cli",
        action="store_const",
        const="arduino-cli",
        dest="backend",
        help="Shorthand for --backend arduino-cli"
    )
    backend_group.add_argument(
        "--pio",
        action="store_const",
        const="platformio",
        dest="backend",
        help="Shorthand for --backend platformio"
    )

    parser.add_argument(
        "--firmware",
        action="store_true",
        help="Build firmware only"
    )
    parser.add_argument(
        "--filesystem",
        action="store_true",
        help="Build filesystem only"
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build artifacts"
    )
    parser.add_argument(
        "--distclean",
        action="store_true",
        help="Remove build artifacts plus downloaded cores/libraries (slower)"
    )
    parser.add_argument(
        "--merged",
        action="store_true",
        help="Create a single merged binary containing firmware and filesystem (easier flashing)"
    )
    parser.add_argument(
        "--compress",
        action="store_true",
        help="Compress the merged binary with gzip (requires --merged)"
    )
    parser.add_argument(
        "--target",
        choices=["esp8266", "esp32", "all"],
        default="all",
        help="Target platform: esp8266, esp32, or all (default)"
    )
    parser.add_argument(
        "--no-install-cli",
        action="store_true",
        help="Skip arduino-cli installation check/install (arduino-cli backend only)"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable colored output"
    )

    args = parser.parse_args()
    
    # Disable colors if requested or on Windows (unless ANSICON is present)
    if args.no_color or (platform.system() == "Windows" and not os.environ.get("ANSICON")):
        Colors.disable()
    
    # Validate arguments
    if args.compress and not args.merged:
        print_error("--compress requires --merged flag")
        sys.exit(2)
    
    # Get project directory (script should be in project root)
    project_dir = Path(__file__).parent.resolve()
    
    use_pio = (args.backend == "platformio")

    print(f"{Colors.HEADER}{Colors.BOLD}")
    print("=" * 60)
    print("  OTGW-firmware Local Build Script")
    print(f"  Backend: {'PlatformIO' if use_pio else 'arduino-cli'}")
    print("=" * 60)
    print(f"{Colors.ENDC}")

    # Check system
    system, machine = get_system_info()

    # Check Python version
    check_python_version()

    # Handle cleaning options
    if args.distclean and args.clean:
        print_error("Use only one of --clean or --distclean")
        sys.exit(2)
    if args.clean:
        clean_build(project_dir)
        return
    if args.distclean:
        clean_build(project_dir, distclean=True)
        return

    # Resolve target list
    if args.target == "all":
        target_names = list(TARGETS.keys())
    else:
        target_names = [args.target]

    print_info(f"Target(s): {', '.join(TARGETS[t]['name'] for t in target_names)}")

    # Backend-specific setup
    config_file = None
    if use_pio:
        check_platformio()
    else:
        # Arduino-CLI setup
        if not args.no_install_cli:
            cli_install_dir = install_arduino_cli(system)
            if cli_install_dir:
                current_path = os.environ.get("PATH", "")
                os.environ["PATH"] = f"{cli_install_dir}{os.pathsep}{current_path}"
                print_info(f"Added {cli_install_dir} to PATH for this build session")

        config_file = setup_arduino_config(project_dir, target_names)
        install_dependencies(project_dir, config_file, target_names)

    # Update version
    update_version(project_dir)

    # Get semantic version
    semver = get_semver(project_dir)
    print_info(f"Building version: {semver}")

    # Create build directory
    create_build_directory(project_dir)

    # Build each target
    for target in target_names:
        tcfg = TARGETS[target]
        print(f"\n{Colors.HEADER}{Colors.BOLD}--- Building for {tcfg['name']} ---{Colors.ENDC}\n")

        if use_pio:
            # PlatformIO build path
            if args.firmware and not args.filesystem:
                build_firmware_pio(project_dir, target)
            elif args.filesystem and not args.firmware:
                build_filesystem_pio(project_dir, target)
            else:
                build_firmware_pio(project_dir, target)
                build_filesystem_pio(project_dir, target)

            collect_pio_artifacts(project_dir, target)
        else:
            # Arduino-CLI build path
            if args.firmware and not args.filesystem:
                build_firmware(project_dir, config_file, target)
            elif args.filesystem and not args.firmware:
                build_filesystem(project_dir, config_file, target)
            else:
                build_firmware(project_dir, config_file, target)
                build_filesystem(project_dir, config_file, target)

            consolidate_build_artifacts(project_dir, target)

        # Rename artifacts with version
        rename_build_artifacts(project_dir, semver, target)

        # Create merged binaries if requested
        if args.merged:
            if not check_esptool():
                print_error("esptool is required for creating merged binaries")
                sys.exit(1)

            flash_offset = "0x0" if target == "esp8266" else tcfg.get("bootloader_offset", "0x0")

            if target == "esp8266":
                # ESP8266 has no separate bootloader — one merged binary covers all cases
                merged_file = create_merged_binary(project_dir, semver, target,
                                                   compress=args.compress, include_filesystem=True)
                if not merged_file:
                    print_error(f"Failed to create merged binary for {tcfg['name']}")
                    sys.exit(1)
                print_info(f"Flash command: esptool.py --chip {tcfg['chip']} --port <PORT> -b 460800 write_flash {flash_offset} {merged_file}")
            else:
                # ESP32: produce both variants
                # 1) firmware-only merged (preserves existing filesystem/settings on flash)
                fw_merged = create_merged_binary(project_dir, semver, target,
                                                 compress=False, include_filesystem=False)
                if not fw_merged:
                    print_error(f"Failed to create firmware-only merged binary for {tcfg['name']}")
                    sys.exit(1)
                print_info(f"Firmware flash:  esptool.py --chip {tcfg['chip']} --port <PORT> -b 460800 write_flash {flash_offset} {fw_merged.name}")

                # 2) full merged (bootloader + partitions + app + filesystem, for factory install)
                full_merged = create_merged_binary(project_dir, semver, target,
                                                   compress=args.compress, include_filesystem=True)
                if not full_merged:
                    print_error(f"Failed to create full merged binary for {tcfg['name']}")
                    sys.exit(1)
                print_info(f"Factory flash:   esptool.py --chip {tcfg['chip']} --port <PORT> -b 460800 write_flash {flash_offset} {full_merged.name}")

    # List build artifacts
    list_build_artifacts(project_dir)

    # Clean up temporary directory (arduino-cli only; PlatformIO uses .pio/)
    if not use_pio:
        cleanup_temp_directory(project_dir)

    print(f"\n{Colors.OKGREEN}{Colors.BOLD}")
    print("=" * 60)
    print("  Build completed successfully!")
    print("=" * 60)
    print(f"{Colors.ENDC}")
    print_info("Build artifacts are in the 'build' directory")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print_error("\nBuild interrupted by user")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        traceback.print_exc()
        sys.exit(1)

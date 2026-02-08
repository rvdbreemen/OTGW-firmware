#!/usr/bin/env python3
"""
Local build script for OTGW-firmware
This script automates the build process for Windows and Mac platforms,
replicating the CI/CD workflow for local development.

Requirements:
- Python 3.x
- arduino-cli (installed automatically if not found)

Usage:
    python build.py              # Full build (firmware + filesystem)
    python build.py --firmware   # Build firmware only
    python build.py --filesystem # Build filesystem only
    python build.py --clean      # Clean build artifacts
    python build.py --distclean  # Clean build + cached dependencies
    python build.py --help       # Show help
"""

import argparse
import gzip
import multiprocessing
import os
import platform
import shutil
import ssl
import stat
import subprocess
import sys
import tarfile
import traceback
import urllib.request
import zipfile
from pathlib import Path
import config

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


def setup_arduino_config(project_dir):
    """Setup arduino-cli configuration"""
    print_step("Configuring arduino-cli")
    
    arduino_dir = project_dir / "arduino"
    arduino_dir.mkdir(exist_ok=True)
    
    config_file = arduino_dir / "arduino-cli.yaml"
    
    # Initialize config
    run_command(["arduino-cli", "config", "init", "--dest-file", str(config_file), "--overwrite"])
    
    # Set config values
    configs = [
        ["directories.data", str(arduino_dir)],
        ["board_manager.additional_urls", "https://github.com/esp8266/Arduino/releases/download/2.7.4/package_esp8266com_index.json"],
        ["directories.downloads", str(project_dir / "staging")],
        ["directories.user", str(project_dir)],
        ["sketch.always_export_binaries", "true"],
        ["library.enable_unsafe_install", "true"]
    ]
    
    for key, value in configs:
        run_command(["arduino-cli", "config", "set", key, value, "--config-file", str(config_file)])
        
    return config_file


def install_dependencies(project_dir, config_file):
    """Install core and libraries"""
    print_step("Installing dependencies")
    
    cmd_base = ["arduino-cli", "--config-file", str(config_file)]
    
    # Update index
    print_info("Updating core index...")
    run_command(cmd_base + ["core", "update-index"])
    
    # Install core
    print_info("Installing ESP8266 core...")
    run_command(cmd_base + ["core", "install", "esp8266:esp8266"])
    
    # Update lib index
    print_info("Updating library index...")
    run_command(cmd_base + ["lib", "update-index"])
    
    # Install libraries
    libraries = [
        "WiFiManager@2.0.15-rc.1",
        "ArduinoJson@6.17.2",
        "pubsubclient@2.8.0",
        "TelnetStream@1.2.4",
        "AceCommon@1.6.2",
        "AceSorting@1.0.0",
        "AceTime@2.0.1",
        "OneWire@2.3.8",
        "DallasTemperature@3.9.0",
        "WebSockets@2.3.5"
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


def build_firmware(project_dir, config_file):
    """Build firmware using arduino-cli"""
    print_step("Building firmware")
    
    fqbn = "esp8266:esp8266:d1_mini:eesz=4M2M,xtal=160"
    cflags = "-DNO_GLOBAL_HTTPUPDATE"
    
    cmd = [
        "arduino-cli",
        "compile",
        "--fqbn", fqbn,
        "--warnings", "default",
        "--verbose",
        "--libraries", str(project_dir / "src" / "libraries"),
        "--build-property", f"compiler.cpp.extra_flags=\"{cflags}\"",
        "--config-file", str(config_file),
        str(config.FIRMWARE_ROOT)
    ]
    
    run_command(cmd, cwd=project_dir, show_output=True)
    print_success("Firmware build complete")


def build_filesystem(project_dir, config_file):
    """Build filesystem using mklittlefs"""
    print_step("Building filesystem")
    
    # Find mklittlefs
    # It should be in arduino/packages/esp8266/tools/mklittlefs/*/mklittlefs(.exe)
    tools_dir = project_dir / "arduino" / "packages" / "esp8266" / "tools" / "mklittlefs"
    
    mklittlefs_path = None
    if tools_dir.exists():
        for path in tools_dir.glob("**/mklittlefs*"):
            if path.is_file() and (path.name == "mklittlefs" or path.name == "mklittlefs.exe"):
                mklittlefs_path = path
                break
    
    if not mklittlefs_path:
        print_error("mklittlefs not found. Make sure the ESP8266 core is installed.")
        sys.exit(1)
        
    print_info(f"Using mklittlefs: {mklittlefs_path}")
    
    fs_dir = config.DATA_DIR
    output_file = config.BUILD_DIR / f"{config.PROJECT_NAME}.littlefs.bin"
    
    # Ensure build dir exists
    output_file.parent.mkdir(exist_ok=True)
    
    cmd = [
        str(mklittlefs_path),
        "-p", "256",
        "-b", "8192",
        "-s", "1024000",
        "-c", str(fs_dir),
        str(output_file)
    ]
    
    run_command(cmd, cwd=project_dir, show_output=True)
    print_success("Filesystem build complete")


def consolidate_build_artifacts(project_dir):
    """Move all build artifacts from subdirectories to build root and clean up"""
    print_step("Consolidating build artifacts")
    
    build_dir = config.BUILD_DIR
    # Ensure build dir exists
    build_dir.mkdir(exist_ok=True)
    
    # Source build directory (where arduino-cli outputs)
    src_build_dir = config.FIRMWARE_ROOT / "build"
    
    moved = []
    
    # Helper to move artifact
    def process_artifact(source_path, dest_dir, action="copy"):
        if not source_path.exists():
            return False
            
        dest_path = dest_dir / source_path.name
        
        # Handle name conflicts
        if dest_path.exists() and dest_path != source_path:
            # Check if it is the same file
            try:
                if source_path.resolve() == dest_path.resolve():
                    return False
            except OSError:
                pass
            print_warning(f"File {dest_path.name} already exists, overwriting")
            dest_path.unlink()
        
        if source_path != dest_path:
            if action == "move":
                shutil.move(str(source_path), str(dest_path))
                print_info(f"Moved: {source_path.name}")
            else:
                shutil.copy2(str(source_path), str(dest_path))
                print_info(f"Copied: {source_path.name}")
            moved.append(dest_path)
            return True
        return False

    # 1. Copy artifacts from src/OTGW-firmware/build (arduino-cli output)
    if src_build_dir.exists():
        patterns = ["**/*.ino.bin", "**/*.ino.elf", "**/*.ino.map"]
        for pattern in patterns:
            for file_path in src_build_dir.glob(pattern):
                process_artifact(file_path, build_dir, action="copy")

    # 2. Move artifacts from subdirectories in build/ (if any)
    patterns = ["**/*.ino.bin", "**/*.ino.elf", "**/*.littlefs.bin"]
    for pattern in patterns:
        for file_path in build_dir.glob(pattern):
            # Skip files already in build root
            if file_path.parent == build_dir:
                continue
            
            process_artifact(file_path, build_dir, action="move")
            
    if moved:
        print_success(f"Consolidated {len(moved)} artifact(s) to build root")
    
    # Remove empty subdirectories and any remaining files in build/
    for item in build_dir.iterdir():
        if item.is_dir():
            try:
                # Remove the entire subdirectory tree
                shutil.rmtree(item)
                print_info(f"Removed directory: {item.name}")
            except Exception as e:
                print_warning(f"Could not remove {item.name}: {e}")

    print_success("Build directory cleaned")


def rename_build_artifacts(project_dir, semver):
    """Rename build artifacts with version number"""
    print_step("Renaming build artifacts")
    
    build_dir = config.BUILD_DIR
    if not build_dir.exists():
        print_warning("Build directory not found, skipping rename")
        return
    
    renamed = []
    
    # Find and rename .bin and .elf files (only in build root)
    for pattern in ["*.ino.bin", "*.ino.elf"]:
        for file_path in build_dir.glob(pattern):
            # Create new name with version
            if file_path.suffix == ".bin":
                new_name = file_path.stem.replace(".ino", "") + f"-{semver}.ino.bin"
            elif file_path.suffix == ".elf":
                new_name = file_path.stem.replace(".ino", "") + f"-{semver}.ino.elf"
            else:
                continue
            
            new_path = file_path.parent / new_name
            file_path.rename(new_path)
            renamed.append(new_path)
            print_info(f"Renamed: {file_path.name} -> {new_name}")
    
    # Rename filesystem
    for file_path in build_dir.glob("*.littlefs.bin"):
        # Handle both *.ino.littlefs.bin and *.littlefs.bin patterns
        if ".ino.littlefs" in file_path.name:
            new_name = file_path.stem.replace(".ino.littlefs", "") + f".{semver}.littlefs.bin"
        else:
            # Remove .littlefs from stem to avoid double extension
            base_name = file_path.stem.replace(".littlefs", "")
            new_name = base_name + f".{semver}.littlefs.bin"
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


def create_merged_binary(project_dir, semver, compress=False):
    """Create a merged binary containing both firmware and filesystem using esptool merge_bin.
    
    Args:
        project_dir: Project directory path
        semver: Semantic version string
        compress: If True, also create a gzip-compressed version
    
    Returns:
        Path to the merged binary (or compressed version if compress=True)
    """
    print_step("Creating merged binary")
    
    build_dir = project_dir / "build"
    if not build_dir.exists():
        print_error("Build directory not found")
        return None
    
    # Find firmware and filesystem files
    firmware_file = None
    filesystem_file = None
    
    # Look for firmware (try versioned first, then unversioned)
    for pattern in [f"*{semver}*.ino.bin", "*.ino.bin", "OTGW-firmware*.bin"]:
        matches = list(build_dir.glob(pattern))
        # Filter out littlefs and merged files
        matches = [m for m in matches if "littlefs" not in m.name.lower() and "merged" not in m.name.lower()]
        if matches:
            firmware_file = sorted(matches)[0]  # Take the first match
            break
    
    # Look for filesystem (try versioned first, then unversioned)
    for pattern in [f"*{semver}*.littlefs.bin", "*.littlefs.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            filesystem_file = sorted(matches)[0]
            break
    
    if not firmware_file:
        print_error("Firmware binary not found in build directory")
        return None
    
    if not filesystem_file:
        print_warning("Filesystem binary not found - creating firmware-only merged binary")
    
    print_info(f"Firmware: {firmware_file.name}")
    if filesystem_file:
        print_info(f"Filesystem: {filesystem_file.name}")
    
    # Create output filename
    if semver and semver != "unknown":
        merged_name = f"OTGW-firmware-{semver}-merged.bin"
    else:
        merged_name = "OTGW-firmware-merged.bin"
    
    merged_file = build_dir / merged_name
    
    # Build esptool merge_bin command
    # ESP8266 flash layout: firmware at 0x0, filesystem at 0x200000
    # Flash size: 4MB (0x400000)
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp8266",
        "merge_bin",
        "-o", str(merged_file),
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", "4MB",
        "0x0", str(firmware_file)
    ]
    
    # Add filesystem if available
    if filesystem_file:
        cmd.extend(["0x200000", str(filesystem_file)])
    
    print_info(f"Running: esptool merge_bin...")
    
    try:
        result = run_command(cmd, cwd=project_dir, capture_output=True, show_output=False)
        
        if merged_file.exists():
            size_mb = merged_file.stat().st_size / (1024 * 1024)
            print_success(f"Created merged binary: {merged_name} ({size_mb:.2f} MB)")
            
            # Optionally compress the merged binary
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


def clean_build(project_dir):
    """Clean build artifacts"""
    print_step("Cleaning build artifacts")
    
    build_dir = config.BUILD_DIR
    arduino_dir = project_dir / "arduino"
    staging_dir = project_dir / "staging"
    
    for d in [build_dir, arduino_dir, staging_dir]:
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
  python build.py                  # Full build (firmware + filesystem)
  python build.py --firmware       # Build firmware only
  python build.py --filesystem     # Build filesystem only
  python build.py --merged         # Build and create merged binary (single flashable file)
  python build.py --merged --compress  # Build and create compressed merged binary
  python build.py --clean          # Clean build artifacts
  python build.py --distclean      # Also remove cores/libraries cache
        """
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
        "--no-install-cli",
        action="store_true",
        help="Skip arduino-cli installation check/install"
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
    
    print(f"{Colors.HEADER}{Colors.BOLD}")
    print("=" * 60)
    print("  OTGW-firmware Local Build Script")
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
        clean_build(project_dir)
        return
    
    # Install arduino-cli if needed and add to PATH
    if not args.no_install_cli:
        cli_install_dir = install_arduino_cli(system)
        # Add arduino-cli to PATH for subprocess calls
        if cli_install_dir:
            current_path = os.environ.get("PATH", "")
            os.environ["PATH"] = f"{cli_install_dir}{os.pathsep}{current_path}"
            print_info(f"Added {cli_install_dir} to PATH for this build session")
            
    # Setup arduino-cli config
    config_file = setup_arduino_config(project_dir)
    
    # Install dependencies
    install_dependencies(project_dir, config_file)
    
    # Update version
    update_version(project_dir)
    
    # Get semantic version
    semver = get_semver(project_dir)
    print_info(f"Building version: {semver}")
    
    # Create build directory
    create_build_directory(project_dir)
    
    # Build based on arguments
    if args.firmware and not args.filesystem:
        # Firmware only
        build_firmware(project_dir, config_file)
    elif args.filesystem and not args.firmware:
        # Filesystem only
        build_filesystem(project_dir, config_file)
    else:
        # Full build (default)
        build_firmware(project_dir, config_file)
        build_filesystem(project_dir, config_file)
    
    # Consolidate build artifacts from subdirectories
    consolidate_build_artifacts(project_dir)
    
    # Rename artifacts with version
    rename_build_artifacts(project_dir, semver)
    
    # Create merged binary if requested
    if args.merged:
        # Check/install esptool first
        if not check_esptool():
            print_error("esptool is required for creating merged binaries")
            sys.exit(1)
        
        merged_file = create_merged_binary(project_dir, semver, compress=args.compress)
        if not merged_file:
            print_error("Failed to create merged binary")
            sys.exit(1)
        
        print_info(f"Merged binary ready for flashing: {merged_file.name}")
        print_info("Flash command: esptool.py --port <PORT> -b 460800 write_flash 0x0 " + str(merged_file))
    
    # List build artifacts
    list_build_artifacts(project_dir)
    
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

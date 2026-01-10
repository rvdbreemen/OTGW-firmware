#!/usr/bin/env python3
"""
ESP8266 Flash Tool for OTGW-firmware
Platform-independent script to flash firmware and filesystem to ESP8266 devices.

This script automates the flashing process for NodeMCU and Wemos D1 mini ESP8266 boards.
It handles dependency installation, port detection, and provides an interactive interface.

Usage:
    python3 flash_esp.py              # Interactive mode
    python3 flash_esp.py --help       # Show all options
"""

import argparse
import glob
import gzip
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

# Flash addresses for ESP8266
FIRMWARE_ADDRESS = "0x0"
FILESYSTEM_ADDRESS = "0x200000"

# Default baud rate
DEFAULT_BAUD = 460800

# GitHub repository
GITHUB_REPO = "rvdbreemen/OTGW-firmware"
GITHUB_API_URL = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"


class Colors:
    """ANSI color codes for terminal output."""
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
        """Disable colors for non-terminal output."""
        Colors.HEADER = ''
        Colors.OKBLUE = ''
        Colors.OKCYAN = ''
        Colors.OKGREEN = ''
        Colors.WARNING = ''
        Colors.FAIL = ''
        Colors.ENDC = ''
        Colors.BOLD = ''
        Colors.UNDERLINE = ''


def print_header(text):
    """Print formatted header."""
    print(f"\n{Colors.BOLD}{Colors.HEADER}{'=' * 60}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.HEADER}{text}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.HEADER}{'=' * 60}{Colors.ENDC}\n")


def print_success(text):
    """Print success message."""
    print(f"{Colors.OKGREEN}✓ {text}{Colors.ENDC}")


def print_error(text):
    """Print error message."""
    print(f"{Colors.FAIL}✗ {text}{Colors.ENDC}", file=sys.stderr)


def print_warning(text):
    """Print warning message."""
    print(f"{Colors.WARNING}⚠ {text}{Colors.ENDC}")


def print_info(text):
    """Print info message."""
    print(f"{Colors.OKCYAN}ℹ {text}{Colors.ENDC}")


def get_latest_release_info():
    """Get information about the latest GitHub release."""
    try:
        print_info("Fetching latest release information from GitHub...")
        
        req = urllib.request.Request(GITHUB_API_URL)
        req.add_header('Accept', 'application/vnd.github.v3+json')
        req.add_header('User-Agent', 'OTGW-firmware-flash-tool')
        
        with urllib.request.urlopen(req, timeout=10) as response:
            data = json.loads(response.read().decode())
        
        tag_name = data.get('tag_name', 'unknown')
        release_name = data.get('name', tag_name)
        assets = data.get('assets', [])
        
        print_success(f"Found release: {release_name} ({tag_name})")
        
        return {
            'tag_name': tag_name,
            'name': release_name,
            'assets': assets,
            'zipball_url': data.get('zipball_url'),
        }
    except Exception as e:
        print_error(f"Failed to fetch release information: {e}")
        return None


def download_release_assets(release_info, download_dir):
    """Download firmware and filesystem files from GitHub release."""
    assets = release_info.get('assets', [])
    
    # Look for firmware and filesystem files
    firmware_asset = None
    filesystem_asset = None
    
    for asset in assets:
        name = asset['name'].lower()
        # Match firmware files: .ino.bin, -fw.bin, firmware.bin
        if not firmware_asset and ('.ino.bin' in name or 'fw.bin' in name or 'firmware.bin' in name) and 'littlefs' not in name:
            firmware_asset = asset
        # Match filesystem files: .littlefs.bin, -fs.bin, filesystem.bin
        elif not filesystem_asset and ('littlefs.bin' in name or 'fs.bin' in name or 'filesystem.bin' in name):
            filesystem_asset = asset
    
    downloaded_files = {}
    
    # Download firmware
    if firmware_asset:
        print_info(f"Downloading firmware: {firmware_asset['name']}...")
        firmware_path = download_dir / firmware_asset['name']
        
        try:
            urllib.request.urlretrieve(
                firmware_asset['browser_download_url'],
                firmware_path
            )
            print_success(f"Downloaded: {firmware_asset['name']} ({firmware_asset['size']} bytes)")
            downloaded_files['firmware'] = firmware_path
        except Exception as e:
            print_error(f"Failed to download firmware: {e}")
    
    # Download filesystem
    if filesystem_asset:
        print_info(f"Downloading filesystem: {filesystem_asset['name']}...")
        filesystem_path = download_dir / filesystem_asset['name']
        
        try:
            urllib.request.urlretrieve(
                filesystem_asset['browser_download_url'],
                filesystem_path
            )
            print_success(f"Downloaded: {filesystem_asset['name']} ({filesystem_asset['size']} bytes)")
            downloaded_files['filesystem'] = filesystem_path
        except Exception as e:
            print_error(f"Failed to download filesystem: {e}")
    
    if not downloaded_files:
        print_warning("No firmware or filesystem files found in release assets")
    
    return downloaded_files


def build_firmware():
    """Build the firmware using build.py script."""
    script_dir = Path(__file__).parent.resolve()
    build_script = script_dir / "build.py"
    
    if not build_script.exists():
        print_error("build.py script not found in repository root")
        return None
    
    print_header("Building Firmware")
    print_info("Running build.py to build firmware and filesystem...")
    print_info("This may take several minutes...")
    print_info("The build script will automatically install arduino-cli if needed...")
    
    try:
        # Run build.py script with --no-rename to keep simple filenames
        result = subprocess.run(
            [sys.executable, str(build_script), "--no-rename"],
            cwd=script_dir,
            check=False
        )
        
        if result.returncode != 0:
            print_error("Build failed!")
            return None
        
        print_success("Build completed successfully")
        
        # Look for the build artifacts
        build_dir = script_dir / "build"
        if not build_dir.exists():
            print_error("Build directory not found")
            return None
        
        # Check for merged binary first
        merged_file = None
        for pattern in ["*-merged.bin.gz", "*-merged.bin"]:
            matches = list(build_dir.glob(pattern))
            if matches:
                merged_file = matches[0]
                print_info(f"Found merged binary: {merged_file.name}")
                break
        
        # Find firmware file
        firmware_file = None
        for pattern in ["*.ino.bin", "OTGW-firmware.ino.bin"]:
            matches = list(build_dir.glob(pattern))
            # Filter out merged files
            matches = [m for m in matches if "merged" not in m.name.lower()]
            if matches:
                firmware_file = matches[0]
                break
        
        if not firmware_file and not merged_file:
            print_error("Firmware binary not found in build directory")
            return None
        
        if firmware_file:
            print_info(f"Found firmware: {firmware_file.name}")
        
        # Find filesystem file
        filesystem_file = None
        for pattern in ["*.littlefs.bin", "OTGW-firmware.ino.littlefs.bin"]:
            matches = list(build_dir.glob(pattern))
            if matches:
                filesystem_file = matches[0]
                break
        
        if filesystem_file:
            print_info(f"Found filesystem: {filesystem_file.name}")
        else:
            print_warning("Filesystem binary not found (optional)")
        
        return {
            'merged': merged_file,
            'firmware': firmware_file,
            'filesystem': filesystem_file
        }
        
    except Exception as e:
        print_error(f"Build failed: {e}")
        return None


def check_python_version():
    """Ensure Python 3.6 or higher is being used."""
    if sys.version_info < (3, 6):
        print_error("Python 3.6 or higher is required.")
        sys.exit(1)


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
            print_success(f"esptool is already installed")
            return True
    except Exception:
        pass

    print_info("esptool not found. Installing...")
    
    # Try multiple installation strategies for different environments
    install_attempts = [
        # Try with --user flag first (works on most systems)
        ([sys.executable, "-m", "pip", "install", "--user", "esptool"], "user installation"),
        # Try with --break-system-packages for PEP 668 environments (newer macOS/Python)
        ([sys.executable, "-m", "pip", "install", "--break-system-packages", "esptool"], "system installation with override"),
        # Try without any flags (works in virtual environments)
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
    
    # All installation attempts failed
    print_error("Failed to install esptool automatically")
    print_info("\nPlease install esptool manually using one of these methods:")
    print_info("  1. Using pipx (recommended on macOS with Homebrew):")
    print_info("     brew install pipx")
    print_info("     pipx install esptool")
    print_info("  2. Using Homebrew:")
    print_info("     brew install esptool")
    print_info("  3. Using pip in a virtual environment:")
    print_info("     python3 -m venv venv")
    print_info("     source venv/bin/activate")
    print_info("     pip install esptool")
    print_info("  4. Using pip with --break-system-packages (not recommended):")
    print_info("     pip install --break-system-packages esptool")
    
    return False


def detect_serial_ports():
    """Detect available serial ports based on the operating system."""
    ports = []
    system = platform.system()

    if system == "Windows":
        # Windows COM ports
        for i in range(1, 257):
            port = f"COM{i}"
            try:
                # Try to open the port to check if it exists
                import serial
                s = serial.Serial(port)
                s.close()
                ports.append(port)
            except:
                pass
        
        # If pyserial is not available, use glob patterns
        if not ports:
            for i in range(1, 257):
                port = f"COM{i}"
                # Just add common ports if we can't detect
                if i <= 20:
                    ports.append(port)
    
    elif system == "Darwin":
        # macOS
        ports = glob.glob("/dev/tty.usb*")
        ports.extend(glob.glob("/dev/cu.usb*"))
        ports.extend(glob.glob("/dev/tty.wchusbserial*"))
        ports.extend(glob.glob("/dev/cu.wchusbserial*"))
    
    elif system == "Linux":
        # Linux
        ports = glob.glob("/dev/ttyUSB*")
        ports.extend(glob.glob("/dev/ttyACM*"))
        ports.extend(glob.glob("/dev/serial/by-id/*"))
    
    return sorted(set(ports))


def select_port(ports, default_port=None):
    """Interactively select a serial port from the list."""
    if not ports:
        print_error("No serial ports detected!")
        print_info("Please ensure your ESP8266 is connected via USB.")
        print_info("You may need to install USB drivers (CP210x or CH340).")
        
        # Offer manual input
        manual = input(f"\n{Colors.BOLD}Enter port manually (or press Enter to exit): {Colors.ENDC}").strip()
        if manual:
            return manual
        sys.exit(1)
    
    if len(ports) == 1:
        print_info(f"Auto-detected port: {ports[0]}")
        return ports[0]
    
    print_info("Available serial ports:")
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port}")
    
    if default_port and default_port in ports:
        default_idx = ports.index(default_port) + 1
        prompt = f"Select port (1-{len(ports)}) [default: {default_idx}]: "
    else:
        prompt = f"Select port (1-{len(ports)}): "
    
    while True:
        choice = input(f"\n{Colors.BOLD}{prompt}{Colors.ENDC}").strip()
        
        if not choice and default_port and default_port in ports:
            return default_port
        
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                return ports[idx]
        except ValueError:
            pass
        
        print_error("Invalid selection. Please try again.")


def find_firmware_files():
    """Find firmware and filesystem binary files."""
    script_dir = Path(__file__).parent.resolve()
    
    # Look for binary files in common locations
    search_paths = [
        script_dir,
        script_dir / "build",
        script_dir / "releases",
    ]
    
    firmware_files = []
    filesystem_files = []
    merged_files = []
    
    for search_path in search_paths:
        if search_path.exists():
            # Look for merged files first
            merged_files.extend(search_path.glob("*-merged.bin"))
            merged_files.extend(search_path.glob("*-merged.bin.gz"))
            
            # Look for firmware files
            firmware_files.extend(search_path.glob("*.bin"))
            firmware_files.extend(search_path.glob("*-fw.bin"))
            firmware_files.extend(search_path.glob("*.ino.bin"))
            
            # Look for filesystem files
            filesystem_files.extend(search_path.glob("*-fs.bin"))
            filesystem_files.extend(search_path.glob("*.littlefs.bin"))
    
    # Filter out merged and littlefs from firmware list
    firmware_files = [f for f in firmware_files if "merged" not in f.name.lower() and "littlefs" not in f.name.lower()]
    
    # Remove duplicates and sort
    firmware_files = sorted(set(firmware_files))
    filesystem_files = sorted(set(filesystem_files))
    merged_files = sorted(set(merged_files))
    
    return firmware_files, filesystem_files, merged_files


def check_build_artifacts():
    """Check if build artifacts exist in the build directory."""
    script_dir = Path(__file__).parent.resolve()
    build_dir = script_dir / "build"
    
    if not build_dir.exists():
        return None
    
    # Look for merged binary first (preferred)
    merged_file = None
    for pattern in ["*-merged.bin.gz", "*-merged.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            # Prefer compressed version if both exist
            merged_file = matches[0]
            break
    
    # Look for firmware file
    firmware_file = None
    for pattern in ["*.ino.bin", "OTGW-firmware.ino.bin"]:
        matches = list(build_dir.glob(pattern))
        # Filter out merged files
        matches = [m for m in matches if "merged" not in m.name.lower()]
        if matches:
            firmware_file = matches[0]
            break
    
    # Look for filesystem file
    filesystem_file = None
    for pattern in ["*.littlefs.bin", "OTGW-firmware.ino.littlefs.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            filesystem_file = matches[0]
            break
    
    if merged_file or firmware_file:
        return {
            'merged': merged_file,
            'firmware': firmware_file,
            'filesystem': filesystem_file
        }
    
    return None


def interactive_mode_selection():
    """Interactive menu for selecting flash mode when no arguments provided."""
    print_header("OTGW-firmware Flash Tool - Interactive Mode")
    
    print(f"{Colors.BOLD}Available Options:{Colors.ENDC}\n")
    print(f"{Colors.OKBLUE}1. BUILD MODE{Colors.ENDC}")
    print("   - Build firmware locally from source code")
    print("   - Requires build tools (arduino-cli, make)")
    print("   - Best for developers making code changes")
    print("   - Takes several minutes to complete\n")
    
    print(f"{Colors.OKBLUE}2. DOWNLOAD MODE{Colors.ENDC}")
    print("   - Download latest official release from GitHub")
    print("   - Fast and easy")
    print("   - Best for regular users")
    print("   - Requires internet connection\n")
    
    # Check for existing build artifacts
    artifacts = check_build_artifacts()
    
    if artifacts:
        print_success("Found existing build artifacts!")
        if artifacts.get('merged'):
            print(f"  {Colors.OKGREEN}Merged Binary:{Colors.ENDC} {artifacts['merged'].name} (firmware + filesystem in one file)")
        if artifacts.get('firmware'):
            print(f"  Firmware: {artifacts['firmware'].name}")
        if artifacts.get('filesystem'):
            print(f"  Filesystem: {artifacts['filesystem'].name}")
        
        print(f"\n{Colors.BOLD}What would you like to do?{Colors.ENDC}")
        print("  1. Flash existing build artifacts")
        print("  2. Rebuild and flash")
        print("  3. Download latest release and flash")
        print("  4. Exit")
        
        while True:
            choice = input(f"\n{Colors.BOLD}Enter your choice (1-4): {Colors.ENDC}").strip()
            
            if choice == "1":
                return "flash_artifacts", artifacts
            elif choice == "2":
                return "build", None
            elif choice == "3":
                return "download", None
            elif choice == "4":
                print_info("Exiting...")
                sys.exit(0)
            else:
                print_error("Invalid choice. Please enter 1, 2, 3, or 4.")
    else:
        print_info("No existing build artifacts found in build/ directory.\n")
        
        print(f"{Colors.BOLD}What would you like to do?{Colors.ENDC}")
        print("  1. Build firmware locally and flash")
        print("  2. Download latest release and flash")
        print("  3. Exit")
        
        while True:
            choice = input(f"\n{Colors.BOLD}Enter your choice (1-3): {Colors.ENDC}").strip()
            
            if choice == "1":
                return "build", None
            elif choice == "2":
                return "download", None
            elif choice == "3":
                print_info("Exiting...")
                sys.exit(0)
            else:
                print_error("Invalid choice. Please enter 1, 2, or 3.")


def select_file(files, file_type):
    """Interactively select a file from the list."""
    if not files:
        print_warning(f"No {file_type} files found automatically.")
        manual = input(f"\n{Colors.BOLD}Enter {file_type} file path (or press Enter to skip): {Colors.ENDC}").strip()
        if manual:
            path = Path(manual)
            if path.exists():
                return path
            else:
                print_error(f"File not found: {manual}")
        return None
    
    if len(files) == 1:
        print_info(f"Auto-detected {file_type}: {files[0].name}")
        return files[0]
    
    print_info(f"Available {file_type} files:")
    for i, file in enumerate(files, 1):
        print(f"  {i}. {file}")
    
    print(f"  {len(files) + 1}. Skip {file_type}")
    
    while True:
        choice = input(f"\n{Colors.BOLD}Select {file_type} (1-{len(files) + 1}): {Colors.ENDC}").strip()
        
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(files):
                return files[idx]
            elif idx == len(files):
                return None
        except ValueError:
            pass
        
        print_error("Invalid selection. Please try again.")


def flash_esp8266(port, firmware_file=None, filesystem_file=None, merged_file=None, baud=DEFAULT_BAUD, erase_flash=False, mode="manual", version_info=None):
    """Flash the ESP8266 with firmware and/or filesystem, or a merged binary."""
    
    # Print mode and version information
    print_header("Flash Information")
    print(f"{Colors.BOLD}Mode:{Colors.ENDC} {mode.upper()}")
    
    if version_info:
        print(f"{Colors.BOLD}Version:{Colors.ENDC} {version_info}")
    
    # Handle merged binary (decompressed if needed)
    actual_merged_file = None
    temp_decompressed = None
    
    if merged_file:
        print(f"{Colors.BOLD}Merged Binary:{Colors.ENDC} {merged_file}")
        
        # Check if it's compressed
        if merged_file.suffix == ".gz":
            print_info("Decompressing merged binary...")
            try:
                temp_decompressed = merged_file.parent / merged_file.stem  # Remove .gz extension
                with gzip.open(merged_file, 'rb') as f_in:
                    with open(temp_decompressed, 'wb') as f_out:
                        shutil.copyfileobj(f_in, f_out)
                actual_merged_file = temp_decompressed
                print_success(f"Decompressed to: {actual_merged_file.name}")
            except Exception as e:
                print_error(f"Failed to decompress: {e}")
                return False
        else:
            actual_merged_file = merged_file
        
        if actual_merged_file.exists():
            size_mb = actual_merged_file.stat().st_size / 1024 / 1024
            print(f"{Colors.BOLD}Binary Size:{Colors.ENDC} {size_mb:.2f} MB")
    else:
        if firmware_file:
            print(f"{Colors.BOLD}Firmware:{Colors.ENDC} {firmware_file}")
            if firmware_file.exists():
                size_mb = firmware_file.stat().st_size / 1024 / 1024
                print(f"{Colors.BOLD}Firmware Size:{Colors.ENDC} {size_mb:.2f} MB")
        
        if filesystem_file:
            print(f"{Colors.BOLD}Filesystem:{Colors.ENDC} {filesystem_file}")
            if filesystem_file.exists():
                size_mb = filesystem_file.stat().st_size / 1024 / 1024
                print(f"{Colors.BOLD}Filesystem Size:{Colors.ENDC} {size_mb:.2f} MB")
    
    print(f"{Colors.BOLD}Port:{Colors.ENDC} {port}")
    print(f"{Colors.BOLD}Baud Rate:{Colors.ENDC} {baud}")
    
    if erase_flash:
        print_header("Erasing Flash")
        print_info(f"Erasing flash on {port}...")
        
        cmd = [
            sys.executable, "-m", "esptool",
            "--port", port,
            "erase_flash"
        ]
        
        print_info(f"Command: {' '.join(cmd)}")
        
        try:
            subprocess.run(cmd, check=True)
            print_success("Flash erased successfully")
        except subprocess.CalledProcessError as e:
            print_error(f"Failed to erase flash: {e}")
            if temp_decompressed and temp_decompressed.exists():
                temp_decompressed.unlink()
            return False
    
    # Build flash command
    if not actual_merged_file and not firmware_file and not filesystem_file:
        print_error("No files to flash!")
        if temp_decompressed and temp_decompressed.exists():
            temp_decompressed.unlink()
        return False
    
    print_header("Flashing ESP8266")
    
    cmd = [
        sys.executable, "-m", "esptool",
        "--port", port,
        "-b", str(baud),
        "write_flash"
    ]
    
    # Use merged binary if available, otherwise use separate files
    if actual_merged_file:
        cmd.extend(["0x0", str(actual_merged_file)])
        print_info(f"Merged binary: {actual_merged_file.name} @ 0x0")
        print_info("This single file contains both firmware and filesystem")
    else:
        if firmware_file:
            cmd.extend([FIRMWARE_ADDRESS, str(firmware_file)])
            print_info(f"Firmware: {firmware_file.name} @ {FIRMWARE_ADDRESS}")
        
        if filesystem_file:
            cmd.extend([FILESYSTEM_ADDRESS, str(filesystem_file)])
            print_info(f"Filesystem: {filesystem_file.name} @ {FILESYSTEM_ADDRESS}")
    
    print_info(f"\nCommand: {' '.join(cmd)}\n")
    
    try:
        subprocess.run(cmd, check=True)
        print_success("\n✓ Flashing completed successfully!")
        print_info("\nYou can now:")
        print_info("  1. Disconnect the ESP8266 from USB")
        print_info("  2. Reconnect it to the OTGW")
        print_info("  3. Power on the device")
        print_info("  4. Connect to the Web UI or configure WiFi via AP mode")
        
        # Clean up temp file
        if temp_decompressed and temp_decompressed.exists():
            temp_decompressed.unlink()
        
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"\nFlashing failed: {e}")
        print_info("\nTroubleshooting tips:")
        print_info("  - Ensure the ESP8266 is properly connected")
        print_info("  - Try a different USB cable")
        print_info("  - Check if drivers are installed (CP210x or CH340)")
        print_info("  - Try reducing baud rate with --baud 115200")
        
        # Clean up temp file
        if temp_decompressed and temp_decompressed.exists():
            temp_decompressed.unlink()
        
        return False


def main():
    """Main entry point."""
    check_python_version()
    
    # Disable colors on Windows if not supported
    if platform.system() == "Windows" and not os.environ.get("ANSICON"):
        Colors.disable()
    
    parser = argparse.ArgumentParser(
        description="Flash OTGW-firmware to ESP8266 (NodeMCU/Wemos D1 mini)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Flash Modes:
  (default)         Interactive mode - choose between build or download
  --download        Explicitly use download mode (fetch latest release from GitHub)
  --build           Build firmware locally then flash (developer mode)
  --firmware/--filesystem    Use manual mode with specific files

Examples:
  # Interactive mode - explains options and guides you (default)
  python3 flash_esp.py
  
  # Explicitly download and flash latest release
  python3 flash_esp.py --download
  
  # Build locally and flash (developer mode)
  python3 flash_esp.py --build
  
  # Flash merged binary (single file with firmware + filesystem)
  python3 flash_esp.py --merged build/OTGW-firmware-merged.bin
  
  # Flash specific firmware file (manual mode)
  python3 flash_esp.py --firmware build/OTGW-firmware.ino.bin
  
  # Flash both firmware and filesystem (manual mode)
  python3 flash_esp.py --firmware build/OTGW-firmware.ino.bin --filesystem build/OTGW-firmware.ino.littlefs.bin
  
  # Specify port and baud rate
  python3 flash_esp.py --port /dev/ttyUSB0 --baud 115200
  
  # Erase flash before flashing (recommended for first install)
  python3 flash_esp.py --erase
  
For more information, see: https://github.com/rvdbreemen/OTGW-firmware/wiki
"""
    )
    
    # Mode selection
    mode_group = parser.add_argument_group('Flash Mode')
    mode_group.add_argument(
        "-d", "--download",
        action="store_true",
        help="Download latest release from GitHub and flash"
    )
    mode_group.add_argument(
        "--build",
        action="store_true",
        help="Build firmware locally and flash (developer mode)"
    )
    
    # Connection options
    conn_group = parser.add_argument_group('Connection Options')
    conn_group.add_argument(
        "-p", "--port",
        help="Serial port (e.g., COM5, /dev/ttyUSB0). If not specified, will auto-detect."
    )
    conn_group.add_argument(
        "-b", "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"Baud rate for flashing (default: {DEFAULT_BAUD})"
    )
    
    # File options
    file_group = parser.add_argument_group('Manual File Options')
    file_group.add_argument(
        "-f", "--firmware",
        help="Path to firmware binary file (.bin)"
    )
    file_group.add_argument(
        "-s", "--filesystem",
        help="Path to filesystem binary file (.littlefs.bin)"
    )
    file_group.add_argument(
        "-m", "--merged",
        help="Path to merged binary file (.bin or .bin.gz) containing both firmware and filesystem"
    )
    
    # Additional options
    parser.add_argument(
        "-e", "--erase",
        action="store_true",
        help="Erase flash before flashing (recommended for first install)"
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Disable interactive prompts (for automation)"
    )
    
    args = parser.parse_args()
    
    # Determine mode
    mode = "manual"
    version_info = None
    
    if args.download and args.build:
        print_error("Cannot specify both --download and --build modes")
        sys.exit(1)
    
    # Print header
    print_header("OTGW-firmware ESP8266 Flash Tool")
    print(f"{Colors.BOLD}Platform:{Colors.ENDC} {platform.system()} {platform.machine()}")
    print(f"{Colors.BOLD}Python:{Colors.ENDC} {sys.version.split()[0]}\n")
    
    # Check and install esptool
    if not check_esptool():
        sys.exit(1)
    
    # Get firmware files based on mode
    firmware_file = None
    filesystem_file = None
    merged_file = None
    
    if args.download:
        # Download mode
        mode = "download"
        print_header("Download Mode - Fetching Latest Release")
        
        release_info = get_latest_release_info()
        if not release_info:
            print_error("Failed to fetch release information")
            sys.exit(1)
        
        version_info = f"{release_info['name']} ({release_info['tag_name']})"
        
        # Create temporary directory for downloads
        temp_dir = Path(tempfile.mkdtemp(prefix="otgw_flash_"))
        print_info(f"Download directory: {temp_dir}")
        
        try:
            downloaded = download_release_assets(release_info, temp_dir)
            firmware_file = downloaded.get('firmware')
            filesystem_file = downloaded.get('filesystem')
            
            if not firmware_file:
                print_error("No firmware file found in release")
                sys.exit(1)
        except Exception as e:
            print_error(f"Download failed: {e}")
            sys.exit(1)
    
    elif args.build:
        # Build mode
        mode = "build"
        print_header("Build Mode - Building Firmware Locally")
        
        build_result = build_firmware()
        if not build_result:
            print_error("Build failed")
            sys.exit(1)
        
        merged_file = build_result.get('merged')
        firmware_file = build_result.get('firmware')
        filesystem_file = build_result.get('filesystem')
        version_info = "Local Build"
        
        if not merged_file and not firmware_file:
            print_error("Build did not produce firmware or merged binary file")
            sys.exit(1)
    
    else:
        # No mode specified - check if manual files provided or use interactive mode
        if not args.firmware and not args.filesystem:
            # No files specified - use interactive mode (unless --no-interactive)
            if args.no_interactive:
                print_error("When using --no-interactive, you must specify a mode (--download, --build) or files (--firmware/--filesystem)")
                sys.exit(1)
            
            # Interactive mode selection
            selected_mode, artifacts = interactive_mode_selection()
            
            if selected_mode == "flash_artifacts":
                # Flash existing build artifacts
                mode = "artifacts"
                merged_file = artifacts.get('merged')
                firmware_file = artifacts.get('firmware')
                filesystem_file = artifacts.get('filesystem')
                version_info = "Existing Build Artifacts"
                print_header("Flashing Existing Build Artifacts")
                
            elif selected_mode == "build":
                # Build mode
                mode = "build"
                print_header("Build Mode - Building Firmware Locally")
                
                build_result = build_firmware()
                if not build_result:
                    print_error("Build failed")
                    sys.exit(1)
                
                merged_file = build_result.get('merged')
                firmware_file = build_result.get('firmware')
                filesystem_file = build_result.get('filesystem')
                version_info = "Local Build"
                
                if not merged_file and not firmware_file:
                    print_error("Build did not produce firmware or merged binary file")
                    sys.exit(1)
                    
            elif selected_mode == "download":
                # Download mode
                mode = "download"
                print_header("Download Mode - Fetching Latest Release")
                
                release_info = get_latest_release_info()
                if not release_info:
                    print_error("Failed to fetch release information")
                    sys.exit(1)
                
                version_info = f"{release_info['name']} ({release_info['tag_name']})"
                
                # Create temporary directory for downloads
                temp_dir = Path(tempfile.mkdtemp(prefix="otgw_flash_"))
                print_info(f"Download directory: {temp_dir}")
                
                try:
                    downloaded = download_release_assets(release_info, temp_dir)
                    firmware_file = downloaded.get('firmware')
                    filesystem_file = downloaded.get('filesystem')
                    
                    if not firmware_file:
                        print_error("No firmware file found in release")
                        sys.exit(1)
                except Exception as e:
                    print_error(f"Download failed: {e}")
                    sys.exit(1)
        else:
            # Manual mode - use provided files or search for them
            if args.merged:
                merged_file = Path(args.merged)
                if not merged_file.exists():
                    print_error(f"Merged file not found: {args.merged}")
                    sys.exit(1)
            
            if args.firmware:
                firmware_file = Path(args.firmware)
                if not firmware_file.exists():
                    print_error(f"Firmware file not found: {args.firmware}")
                    sys.exit(1)
            
            if args.filesystem:
                filesystem_file = Path(args.filesystem)
                if not filesystem_file.exists():
                    print_error(f"Filesystem file not found: {args.filesystem}")
                    sys.exit(1)
            
            # If no files specified and not in no-interactive mode, search for files
            if not merged_file and not firmware_file and not filesystem_file and not args.no_interactive:
                print_header("Manual Mode - Searching for Binary Files")
                firmware_files, filesystem_files, merged_files = find_firmware_files()
                
                # Prioritize merged files if available
                if merged_files:
                    print_info("Found merged binaries (firmware + filesystem in one file):")
                    merged_file = select_file(merged_files, "merged binary")
                
                # If no merged file selected, look for separate firmware/filesystem
                if not merged_file:
                    firmware_file = select_file(firmware_files, "firmware")
                    filesystem_file = select_file(filesystem_files, "filesystem")
            
            version_info = "Manual Selection"
    
    # Detect or select port
    port = args.port
    if not port:
        ports = detect_serial_ports()
        if args.no_interactive:
            if ports:
                port = ports[0]
                print_info(f"Auto-selected port: {port}")
            else:
                print_error("No serial port detected and --no-interactive specified")
                sys.exit(1)
        else:
            port = select_port(ports, default_port="/dev/ttyUSB0" if platform.system() == "Linux" else None)
    
    # Confirm before flashing
    if not args.no_interactive:
        print("\n" + "=" * 60)
        print(f"{Colors.BOLD}Ready to flash:{Colors.ENDC}")
        print(f"  Mode: {mode.upper()}")
        if version_info:
            print(f"  Version: {version_info}")
        print(f"  Port: {port}")
        if merged_file:
            print(f"  {Colors.OKGREEN}Merged Binary:{Colors.ENDC} {merged_file} (firmware + filesystem)")
        else:
            if firmware_file:
                print(f"  Firmware: {firmware_file}")
            if filesystem_file:
                print(f"  Filesystem: {filesystem_file}")
        print(f"  Baud rate: {args.baud}")
        if args.erase:
            print(f"  {Colors.WARNING}Erase flash: Yes{Colors.ENDC}")
        print("=" * 60)
        
        confirm = input(f"\n{Colors.BOLD}Proceed with flashing? (y/N): {Colors.ENDC}").strip().lower()
        if confirm != 'y':
            print_info("Flashing cancelled.")
            sys.exit(0)
    
    # Flash the device
    success = flash_esp8266(
        port=port,
        firmware_file=firmware_file,
        filesystem_file=filesystem_file,
        merged_file=merged_file,
        baud=args.baud,
        erase_flash=args.erase,
        mode=mode,
        version_info=version_info
    )
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n\n{Colors.WARNING}Interrupted by user.{Colors.ENDC}")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

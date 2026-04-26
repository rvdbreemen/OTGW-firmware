#!/usr/bin/env python3
"""
ESP Flash Tool for OTGW-firmware
Platform-independent script to flash firmware and filesystem to ESP8266 and ESP32 devices.

This script automates the flashing process for Nodoshop OTGW WiFi (ESP8266)
and Nodoshop OTGW32 (ESP32) boards.

Usage:
    python3 flash_esp.py              # Interactive mode
    python3 flash_esp.py --help       # Show all options
"""

import argparse
import glob
import hashlib
import json
import os
import platform
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
import shutil
from pathlib import Path
import config

# ---- Board configuration ---------------------------------------------------

BOARD_CONFIGS = {
    "esp8266": {
        "name": "Nodoshop OTGW WiFi (ESP8266)",
        "chip": "esp8266",
        "default_baud": 460800,
        "firmware_address": "0x0",
        "filesystem_address": "0x200000",
        "bootloader_address": None,
        "partitions_address": None,
        # Common USB-serial adapters used on ESP8266 NodeMCU/D1 mini boards
        "usb_ids": [
            (0x1A86, 0x7523),  # CH340
            (0x10C4, 0xEA60),  # CP2102
            (0x0403, 0x6001),  # FTDI FT232R
            (0x0403, 0x6015),  # FTDI FT231X
        ],
    },
    "esp32": {
        "name": "Nodoshop OTGW32 (ESP32-S3)",
        "chip": "esp32s3",
        "default_baud": 921600,
        "firmware_address": "0x10000",
        "filesystem_address": "0x1F0000",  # from partitions_otgw_esp32.csv (4M2M layout)
        "bootloader_address": "0x0",       # ESP32-S3 bootloader is at 0x0, not 0x1000
        "partitions_address": "0x8000",
        # ESP32-S3 built-in USB-Serial/JTAG (same VID/PID as OT-Thing)
        "usb_ids": [
            (0x303A, 0x1001),  # Espressif ESP32-S3 USB-Serial/JTAG
        ],
    },
}

# PlatformIO intermediate build output (bootloader/partitions live here)
PIO_BUILD_DIR = Path(__file__).parent / ".pio" / "build"

# GitHub repository
GITHUB_REPO = "rvdbreemen/OTGW-firmware"
GITHUB_API_URL = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"


# ---- Terminal colours ------------------------------------------------------

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
    print(f"\n{Colors.BOLD}{Colors.HEADER}{'=' * 60}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.HEADER}{text}{Colors.ENDC}")
    print(f"{Colors.BOLD}{Colors.HEADER}{'=' * 60}{Colors.ENDC}\n")


def print_success(text):
    print(f"{Colors.OKGREEN}v {text}{Colors.ENDC}")


def print_error(text):
    print(f"{Colors.FAIL}x {text}{Colors.ENDC}", file=sys.stderr)


def print_warning(text):
    print(f"{Colors.WARNING}! {text}{Colors.ENDC}")


def print_info(text):
    print(f"{Colors.OKCYAN}i {text}{Colors.ENDC}")


# ---- Python / tool checks --------------------------------------------------

def check_python_version():
    if sys.version_info < (3, 6):
        print_error("Python 3.6 or higher is required.")
        sys.exit(1)


def check_esptool():
    """Check if esptool is available, install if missing."""
    try:
        result = subprocess.run(
            [sys.executable, "-m", "esptool", "version"],
            capture_output=True, text=True, check=False
        )
        if result.returncode == 0:
            print_success("esptool is available")
            return True
    except Exception:
        pass

    print_info("esptool not found. Installing...")

    install_attempts = [
        ([sys.executable, "-m", "pip", "install", "--user", "esptool"], "user installation"),
        ([sys.executable, "-m", "pip", "install", "--break-system-packages", "esptool"], "system installation"),
        ([sys.executable, "-m", "pip", "install", "esptool"], "standard installation"),
    ]

    for cmd, description in install_attempts:
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if result.returncode == 0:
                print_success(f"esptool installed ({description})")
                return True
        except Exception:
            continue

    print_error("Failed to install esptool automatically")
    print_info("Install it manually:  pip install esptool")
    return False


# ---- Board selection -------------------------------------------------------

def select_board(args_board=None):
    """Return board key ('esp8266' or 'esp32'), either from arg or interactive menu."""
    if args_board:
        if args_board not in BOARD_CONFIGS:
            print_error(f"Unknown board '{args_board}'. Choose: {', '.join(BOARD_CONFIGS)}")
            sys.exit(1)
        return args_board

    print_header("Select Target Board")
    boards = list(BOARD_CONFIGS.items())
    for i, (key, cfg) in enumerate(boards, 1):
        print(f"  {i}. {cfg['name']} ({key})")

    while True:
        choice = input(f"\n{Colors.BOLD}Select board (1-{len(boards)}): {Colors.ENDC}").strip()
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(boards):
                board_key = boards[idx][0]
                print_success(f"Selected: {BOARD_CONFIGS[board_key]['name']}")
                return board_key
        except ValueError:
            pass
        print_error("Invalid selection.")


# ---- Serial port detection -------------------------------------------------

def detect_serial_ports():
    """Detect available serial ports."""
    ports = []
    system = platform.system()

    if system == "Windows":
        try:
            import serial.tools.list_ports
            ports = [p.device for p in serial.tools.list_ports.comports()]
        except ImportError:
            # Fallback: probe COM1..COM30
            try:
                import serial
                for i in range(1, 31):
                    port = f"COM{i}"
                    try:
                        s = serial.Serial(port)
                        s.close()
                        ports.append(port)
                    except Exception:
                        pass
            except ImportError:
                pass
    elif system == "Darwin":
        ports = glob.glob("/dev/tty.usb*") + glob.glob("/dev/cu.usb*")
        ports += glob.glob("/dev/tty.wchusbserial*") + glob.glob("/dev/cu.wchusbserial*")
    elif system == "Linux":
        ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")

    return sorted(set(ports))


def detect_board_port(board):
    """Try to find the port for a specific board using USB VID/PID matching.

    Returns the matched port string, or None if no match found.
    Requires pyserial (serial.tools.list_ports).
    """
    usb_ids = BOARD_CONFIGS.get(board, {}).get("usb_ids", [])
    if not usb_ids:
        return None
    try:
        from serial.tools import list_ports
        for port_info in list_ports.comports():
            if port_info.vid is None:
                continue
            for vid, pid in usb_ids:
                if port_info.vid == vid and port_info.pid == pid:
                    return port_info.device
    except ImportError:
        pass
    return None


def select_port(ports, default_port=None):
    """Interactively select a serial port."""
    if not ports:
        print_error("No serial ports detected!")
        print_info("Ensure the device is connected and drivers are installed.")
        manual = input(f"\n{Colors.BOLD}Enter port manually (or Enter to exit): {Colors.ENDC}").strip()
        if manual:
            return manual
        sys.exit(1)

    if len(ports) == 1:
        print_info(f"Auto-detected port: {ports[0]}")
        return ports[0]

    print_info("Available serial ports:")
    for i, port in enumerate(ports, 1):
        print(f"  {i}. {port}")

    while True:
        prompt = f"Select port (1-{len(ports)}): "
        choice = input(f"\n{Colors.BOLD}{prompt}{Colors.ENDC}").strip()
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                return ports[idx]
        except ValueError:
            pass
        print_error("Invalid selection.")


# ---- Build artifact detection ----------------------------------------------

def find_build_artifacts(board):
    """Find firmware and filesystem binaries in the build directory for the given board.

    build.py copies artifacts to config.BUILD_DIR with these naming conventions:
      Firmware:    OTGW-firmware-<board>[-<semver>].ino.bin
      Filesystem:  OTGW-firmware-<board>[-<semver>].littlefs.bin
      Merged:      OTGW-firmware-<board>[-<semver>]-merged.bin
      Bootloader:  esp32-bootloader.bin     (ESP32 only)
      Partitions:  esp32-partitions.bin     (ESP32 only)
    """
    build_dir = config.BUILD_DIR
    if not build_dir.exists():
        return {}

    result = {}

    # Firmware: prefer versioned names, fall back to unversioned
    for pattern in [f"*-{board}-*.ino.bin", f"*-{board}.ino.bin"]:
        matches = [m for m in build_dir.glob(pattern) if "merged" not in m.name]
        if matches:
            result["firmware"] = sorted(matches)[-1]
            break

    # Filesystem
    for pattern in [f"*-{board}-*.littlefs.bin", f"*-{board}.littlefs.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            result["filesystem"] = sorted(matches)[-1]
            break

    # Merged binaries (ESP32: two variants produced by build.py)
    #   merged_full = bootloader + partitions + app + filesystem  (factory install)
    #   merged      = bootloader + partitions + app only          (firmware update, preserves filesystem)
    for pattern in [f"*-{board}-*-merged-full.bin", f"*-{board}-merged-full.bin"]:
        matches = list(build_dir.glob(pattern))
        if matches:
            result["merged_full"] = sorted(matches)[-1]
            break

    for pattern in [f"*-{board}-*-merged.bin", f"*-{board}-merged.bin"]:
        matches = [m for m in build_dir.glob(pattern) if "full" not in m.name]
        if matches:
            result["merged"] = sorted(matches)[-1]
            break

    # ESP32: bootloader and partition table (build.py copies these as esp32-*.bin)
    if board == "esp32":
        bl = build_dir / "esp32-bootloader.bin"
        pt = build_dir / "esp32-partitions.bin"
        if bl.exists():
            result["bootloader"] = bl
        elif (PIO_BUILD_DIR / "esp32" / "bootloader.bin").exists():
            # Fallback to PlatformIO intermediate output
            result["bootloader"] = PIO_BUILD_DIR / "esp32" / "bootloader.bin"
        if pt.exists():
            result["partitions"] = pt
        elif (PIO_BUILD_DIR / "esp32" / "partitions.bin").exists():
            result["partitions"] = PIO_BUILD_DIR / "esp32" / "partitions.bin"

    return result


def check_build_artifacts(board):
    """Return build artifacts dict for board, or None if firmware missing."""
    artifacts = find_build_artifacts(board)
    if artifacts.get("firmware") or artifacts.get("merged") or artifacts.get("merged_full"):
        return artifacts
    return None


# ---- GitHub release --------------------------------------------------------

def get_latest_release_info():
    try:
        print_info("Fetching latest release info from GitHub...")
        req = urllib.request.Request(GITHUB_API_URL)
        req.add_header('Accept', 'application/vnd.github.v3+json')
        req.add_header('User-Agent', 'OTGW-firmware-flash-tool')
        with urllib.request.urlopen(req, timeout=10) as response:
            data = json.loads(response.read().decode())

        tag_name = data.get('tag_name', 'unknown')
        print_success(f"Found release: {data.get('name', tag_name)} ({tag_name})")
        return {
            'tag_name': tag_name,
            'name': data.get('name', tag_name),
            'assets': data.get('assets', []),
        }
    except Exception as e:
        print_error(f"Failed to fetch release info: {e}")
        return None


def _load_sha256sums(assets, download_dir):
    """Look for a SHA256SUMS asset in the release and return a {filename: hexdigest}
    mapping. Returns {} when the release has no SHA256SUMS (older releases). A
    warning is printed in that case so users know verification was skipped."""
    sums_asset = None
    for asset in assets:
        name = asset['name']
        if name.upper() in ("SHA256SUMS", "SHA256SUMS.TXT", "CHECKSUMS.SHA256"):
            sums_asset = asset
            break
    if not sums_asset:
        print_warning("No SHA256SUMS asset in release; cannot verify downloads.")
        return {}
    sums_path = download_dir / sums_asset['name']
    try:
        urllib.request.urlretrieve(sums_asset['browser_download_url'], sums_path)
    except Exception as e:
        print_warning(f"Failed to download {sums_asset['name']}: {e}")
        return {}
    mapping = {}
    for line in sums_path.read_text(encoding='utf-8', errors='replace').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        # Format: "<hexdigest>  <filename>" (two spaces) or "<hexdigest> <filename>"
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        digest, fname = parts[0].lower(), parts[1].lstrip('*').strip()
        if len(digest) == 64 and all(c in '0123456789abcdef' for c in digest):
            mapping[fname] = digest
    print_info(f"Loaded {len(mapping)} SHA256 entries from {sums_asset['name']}")
    return mapping


def _sha256_of_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def download_release_assets(release_info, download_dir, board):
    """Download firmware and filesystem files for the given board from a GitHub release."""
    assets = release_info.get('assets', [])
    downloaded = {}

    # Pull SHA256SUMS first so every subsequent download can be verified before
    # any bytes are written to flash. If the release does not publish a sums
    # file we fall back to unverified downloads with a loud warning.
    sums = _load_sha256sums(assets, download_dir)

    # For merged binary (preferred for ESP32), firmware .ino.bin, and littlefs.bin
    # Assets are expected to contain the board name (esp8266 / esp32) in the filename.
    merged_full_asset = None
    merged_fw_asset   = None
    firmware_asset    = None
    filesystem_asset  = None

    for asset in assets:
        name = asset['name'].lower()
        if board not in name:
            continue
        if "merged-full" in name and name.endswith(".bin"):
            merged_full_asset = asset
        elif "merged" in name and "full" not in name and name.endswith(".bin"):
            merged_fw_asset = asset
        elif ("ino.bin" in name or "fw.bin" in name) and "littlefs" not in name and "merged" not in name:
            if not firmware_asset:
                firmware_asset = asset
        elif "littlefs.bin" in name or "fs.bin" in name:
            if not filesystem_asset:
                filesystem_asset = asset

    def _download(asset, key, label):
        path = download_dir / asset['name']
        try:
            print_info(f"Downloading {label}: {asset['name']}...")
            urllib.request.urlretrieve(asset['browser_download_url'], path)
            print_success(f"Downloaded {asset['name']} ({asset['size']} bytes)")
            # Verify SHA256 when available. A mismatch MUST abort before the
            # bytes have any chance of reaching esptool write-flash.
            expected = sums.get(asset['name'])
            if expected:
                actual = _sha256_of_file(path)
                if actual != expected:
                    print_error(f"SHA256 mismatch for {asset['name']}")
                    print_error(f"  expected: {expected}")
                    print_error(f"  actual:   {actual}")
                    try:
                        path.unlink()
                    except Exception:
                        pass
                    return  # do NOT add to downloaded; caller will see it missing
                print_success(f"SHA256 OK for {asset['name']}")
            elif sums:
                # We have a SHA256SUMS file but no entry for this asset: treat
                # as suspect because the release packager meant to cover it.
                print_warning(f"No SHA256 entry for {asset['name']}; proceeding without verification")
            downloaded[key] = path
        except Exception as e:
            print_error(f"Failed to download {label}: {e}")

    if merged_full_asset:
        _download(merged_full_asset, "merged_full", "merged binary (full)")
    if merged_fw_asset:
        _download(merged_fw_asset, "merged", "merged binary (firmware-only)")
    if firmware_asset:
        _download(firmware_asset, "firmware", "firmware")
    if filesystem_asset:
        _download(filesystem_asset, "filesystem", "filesystem")

    if not downloaded:
        print_warning(f"No {board} firmware files found in release assets")

    return downloaded


# ---- Build -----------------------------------------------------------------

def build_firmware():
    """Build firmware + filesystem using build.py."""
    script_dir = Path(__file__).parent.resolve()
    build_script = script_dir / "build.py"

    if not build_script.exists():
        print_error("build.py not found")
        return None

    print_header("Building Firmware")
    print_info("Running build.py (this may take a few minutes)...")

    result = subprocess.run([sys.executable, str(build_script)], cwd=script_dir, check=False)
    if result.returncode != 0:
        print_error("Build failed")
        return None

    print_success("Build completed")
    return True


# ---- Flashing --------------------------------------------------------------

def erase_flash(port, chip):
    """Erase the entire flash of the connected device."""
    print_header("Erasing Flash")
    cmd = [sys.executable, "-m", "esptool", "--port", port, "--chip", chip, "erase-flash"]
    print_info(f"Command: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True)
        print_success("Flash erased")
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"Erase failed: {e}")
        return False


def flash_device(board, port, artifacts, baud=None, do_erase=False):
    """Flash the device. Handles both ESP8266 and ESP32."""
    cfg = BOARD_CONFIGS[board]
    if baud is None:
        baud = cfg["default_baud"]

    # ESP32-S3 always erases before flashing: the otadata partition (0xE000)
    # must be cleared so the bootloader starts fresh from the app0 slot.
    # Without this, a corrupt otadata entry causes a TG0WDT boot loop even
    # after reflashing the firmware.
    if board == "esp32":
        do_erase = True

    print_header("Flash Summary")
    print(f"{Colors.BOLD}Board:{Colors.ENDC}     {cfg['name']}")
    print(f"{Colors.BOLD}Port:{Colors.ENDC}      {port}")
    print(f"{Colors.BOLD}Baud:{Colors.ENDC}      {baud}")

    for key in ("merged_full", "merged", "firmware", "filesystem", "bootloader", "partitions"):
        if key in artifacts:
            size_kb = artifacts[key].stat().st_size / 1024
            print(f"{Colors.BOLD}{key.capitalize():<14}{Colors.ENDC} {artifacts[key].name} ({size_kb:.0f} KB)")

    if do_erase:
        if not erase_flash(port, cfg["chip"]):
            return False

    # --- Build write_flash command ---

    # ESP32: prefer merged binaries (merge_bin embeds all offsets, flash at 0x0).
    # merged_full = bootloader + partitions + app + filesystem (factory install / full reset)
    # merged      = bootloader + partitions + app only         (firmware update, keeps filesystem)
    merged_full = artifacts.get("merged_full") if board == "esp32" else None
    merged_fw   = artifacts.get("merged")      if board == "esp32" else None

    # Pick the best available merged binary.
    # If both exist, default to merged_full for a clean install. Users who only
    # want a firmware update without touching the filesystem should pass
    # --firmware with the *-merged.bin path explicitly.
    use_merged_file = merged_full or merged_fw

    cmd = [
        sys.executable, "-m", "esptool",
        "--port", port,
        "--chip", cfg["chip"],
        "-b", str(baud),
        "write-flash",
    ]

    if use_merged_file:
        label = "factory (full)" if use_merged_file == merged_full else "firmware-only"
        print_header(f"Flashing ESP32 (merged binary — {label})")
        cmd.extend(["0x0", str(use_merged_file)])
        print_info(f"Merged image ({label}) @ 0x0: {use_merged_file.name}")
    else:
        print_header(f"Flashing {cfg['name']}")

        if board == "esp32":
            # Individual components
            if "bootloader" in artifacts:
                cmd.extend([cfg["bootloader_address"], str(artifacts["bootloader"])])
                print_info(f"Bootloader     @ {cfg['bootloader_address']}")
            else:
                print_warning("Bootloader not found — flashing may fail on fresh hardware")

            if "partitions" in artifacts:
                cmd.extend([cfg["partitions_address"], str(artifacts["partitions"])])
                print_info(f"Partitions     @ {cfg['partitions_address']}")
            else:
                print_warning("Partition table not found — flashing may fail on fresh hardware")

            if "firmware" in artifacts:
                cmd.extend([cfg["firmware_address"], str(artifacts["firmware"])])
                print_info(f"Firmware       @ {cfg['firmware_address']}")

            if "filesystem" in artifacts:
                cmd.extend([cfg["filesystem_address"], str(artifacts["filesystem"])])
                print_info(f"Filesystem     @ {cfg['filesystem_address']}")
        else:
            # ESP8266
            if "firmware" in artifacts:
                cmd.extend([cfg["firmware_address"], str(artifacts["firmware"])])
                print_info(f"Firmware       @ {cfg['firmware_address']}")

            if "filesystem" in artifacts:
                cmd.extend([cfg["filesystem_address"], str(artifacts["filesystem"])])
                print_info(f"Filesystem     @ {cfg['filesystem_address']}")

    if len(cmd) <= 8:
        print_error("No files to flash!")
        return False

    print_info(f"\nCommand: {' '.join(cmd)}\n")

    try:
        subprocess.run(cmd, check=True)
        print_success("Flashing completed successfully!")
        print_info("Next steps:")
        print_info("  1. Disconnect the device from USB")
        print_info("  2. Reconnect it to the OTGW board")
        print_info("  3. Power on and connect to the web UI")
        return True
    except subprocess.CalledProcessError as e:
        print_error(f"Flashing failed: {e}")
        print_info("Troubleshooting:")
        print_info("  - Check the USB connection and cable")
        print_info("  - Verify drivers are installed (CP210x or CH340)")
        print_info("  - Try a lower baud rate:  --baud 115200")
        if board == "esp32":
            print_info("  - Hold BOOT button on the ESP32 while connecting")
        return False


# ---- Interactive mode selection --------------------------------------------

def interactive_mode_selection(board):
    """Interactive menu: flash existing artifacts, rebuild, or download."""
    artifacts = check_build_artifacts(board)

    print_header(f"OTGW-firmware Flash Tool - {BOARD_CONFIGS[board]['name']}")

    if artifacts:
        print_success("Found existing build artifacts:")
        for key in ("merged_full", "merged", "firmware", "filesystem", "bootloader", "partitions"):
            if key in artifacts:
                print(f"  {key.capitalize():<14} {artifacts[key].name}")

        print(f"\n{Colors.BOLD}What would you like to do?{Colors.ENDC}")
        print("  1. Flash existing artifacts")
        print("  2. Rebuild from source and flash")
        print("  3. Download latest release and flash")
        print("  4. Exit")

        while True:
            choice = input(f"\n{Colors.BOLD}Choice (1-4): {Colors.ENDC}").strip()
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
                print_error("Enter 1, 2, 3, or 4.")
    else:
        print_info("No existing build artifacts found in build/ directory.\n")

        print(f"{Colors.BOLD}What would you like to do?{Colors.ENDC}")
        print("  1. Build from source and flash")
        print("  2. Download latest release and flash")
        print("  3. Exit")

        while True:
            choice = input(f"\n{Colors.BOLD}Choice (1-3): {Colors.ENDC}").strip()
            if choice == "1":
                return "build", None
            elif choice == "2":
                return "download", None
            elif choice == "3":
                print_info("Exiting...")
                sys.exit(0)
            else:
                print_error("Enter 1, 2, or 3.")


# ---- Main ------------------------------------------------------------------

def main():
    check_python_version()

    if platform.system() == "Windows" and not os.environ.get("ANSICON"):
        Colors.disable()

    parser = argparse.ArgumentParser(
        description="Flash OTGW-firmware to ESP8266 (OTGW WiFi) or ESP32 (OTGW32)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Board selection:
  --board esp8266    Nodoshop OTGW WiFi (default when omitted: interactive)
  --board esp32      Nodoshop OTGW32

Flash modes:
  (default)          Interactive — shows what is available and asks what to do
  --download         Download latest GitHub release and flash
  --build            Build from source and flash (developer mode)
  --firmware/--filesystem  Manual mode: specify binary files directly

Examples:
  python3 flash_esp.py                          # interactive board + mode selection
  python3 flash_esp.py --board esp8266          # select board, then interactive
  python3 flash_esp.py --board esp32 --build    # build ESP32 firmware and flash
  python3 flash_esp.py --board esp32 --download # download latest ESP32 release
  python3 flash_esp.py --board esp8266 --firmware build/OTGW-firmware-esp8266.ino.bin
  python3 flash_esp.py --port COM5 --erase --board esp32 --download --yes
"""
    )

    board_group = parser.add_argument_group('Board Selection')
    board_group.add_argument(
        "--board",
        choices=list(BOARD_CONFIGS.keys()),
        help="Target board: esp8266 (OTGW WiFi) or esp32 (OTGW32)"
    )

    mode_group = parser.add_argument_group('Flash Mode')
    mode_group.add_argument("-d", "--download", action="store_true",
                            help="Download latest release from GitHub and flash")
    mode_group.add_argument("--build", action="store_true",
                            help="Build firmware locally and flash (developer mode)")

    conn_group = parser.add_argument_group('Connection Options')
    conn_group.add_argument("-p", "--port",
                            help="Serial port (e.g. COM5, /dev/ttyUSB0)")
    conn_group.add_argument("-b", "--baud", type=int,
                            help="Baud rate (default: board-specific: 460800 for ESP8266, 921600 for ESP32)")

    file_group = parser.add_argument_group('Manual File Options')
    file_group.add_argument("-f", "--firmware", help="Path to firmware binary (.bin)")
    file_group.add_argument("-s", "--filesystem", help="Path to filesystem binary (.littlefs.bin)")

    parser.add_argument("-e", "--erase", action="store_true",
                        help="Erase flash before flashing (recommended for first install)")
    parser.add_argument("--no-interactive", action="store_true",
                        help="Disable interactive prompts (for automation)")
    parser.add_argument("-y", "--yes", action="store_true", dest="no_interactive",
                        help="Same as --no-interactive")

    args = parser.parse_args()

    print_header("OTGW-firmware Flash Tool")
    print(f"{Colors.BOLD}Platform:{Colors.ENDC} {platform.system()} {platform.machine()}")
    print(f"{Colors.BOLD}Python:{Colors.ENDC}   {sys.version.split()[0]}\n")

    if not check_esptool():
        sys.exit(1)

    if args.download and args.build:
        print_error("Cannot combine --download and --build")
        sys.exit(1)

    # Board selection
    board = select_board(args.board if args.board else (None if not args.no_interactive else None))
    if board is None:
        print_error("No board selected")
        sys.exit(1)

    cfg = BOARD_CONFIGS[board]

    # Determine flash artifacts
    artifacts = {}
    mode = "manual"
    version_info = None

    if args.firmware or args.filesystem:
        # Manual file mode
        mode = "manual"
        if args.firmware:
            p = Path(args.firmware)
            if not p.exists():
                print_error(f"Firmware file not found: {args.firmware}")
                sys.exit(1)
            artifacts["firmware"] = p
        if args.filesystem:
            p = Path(args.filesystem)
            if not p.exists():
                print_error(f"Filesystem file not found: {args.filesystem}")
                sys.exit(1)
            artifacts["filesystem"] = p
        # Also pick up bootloader and partitions from build dir if available (ESP32)
        if board == "esp32":
            bl = config.BUILD_DIR / "esp32-bootloader.bin"
            pt = config.BUILD_DIR / "esp32-partitions.bin"
            if bl.exists():
                artifacts["bootloader"] = bl
            if pt.exists():
                artifacts["partitions"] = pt
        version_info = "Manual Selection"

    elif args.download:
        mode = "download"
        print_header(f"Download Mode - {cfg['name']}")
        release_info = get_latest_release_info()
        if not release_info:
            sys.exit(1)
        version_info = f"{release_info['name']} ({release_info['tag_name']})"
        temp_dir = Path(tempfile.mkdtemp(prefix="otgw_flash_"))
        print_info(f"Download directory: {temp_dir}")
        artifacts = download_release_assets(release_info, temp_dir, board)
        if not artifacts.get("firmware") and not artifacts.get("merged"):
            print_error(f"No {board} firmware found in release")
            sys.exit(1)

    elif args.build:
        mode = "build"
        if not build_firmware():
            sys.exit(1)
        artifacts = find_build_artifacts(board)
        if not artifacts.get("firmware") and not artifacts.get("merged"):
            print_error("Build succeeded but no firmware binary found")
            sys.exit(1)
        version_info = "Local Build"

    else:
        if args.no_interactive:
            print_error("Specify a mode (--download / --build) or files (--firmware / --filesystem) when using --no-interactive")
            sys.exit(1)

        selected_mode, existing_artifacts = interactive_mode_selection(board)

        if selected_mode == "flash_artifacts":
            mode = "artifacts"
            artifacts = existing_artifacts
            version_info = "Existing Build"

        elif selected_mode == "build":
            mode = "build"
            if not build_firmware():
                sys.exit(1)
            artifacts = find_build_artifacts(board)
            if not artifacts.get("firmware") and not artifacts.get("merged"):
                print_error("Build succeeded but no firmware binary found")
                sys.exit(1)
            version_info = "Local Build"

        elif selected_mode == "download":
            mode = "download"
            release_info = get_latest_release_info()
            if not release_info:
                sys.exit(1)
            version_info = f"{release_info['name']} ({release_info['tag_name']})"
            temp_dir = Path(tempfile.mkdtemp(prefix="otgw_flash_"))
            print_info(f"Download directory: {temp_dir}")
            artifacts = download_release_assets(release_info, temp_dir, board)
            if not artifacts.get("firmware") and not artifacts.get("merged"):
                print_error(f"No {board} firmware found in release")
                sys.exit(1)

    # Port selection
    port = args.port
    if not port:
        # Try VID/PID match first — reliable, no user interaction needed
        port = detect_board_port(board)
        if port:
            print_info(f"Auto-detected port by VID/PID: {port}")
        else:
            ports = detect_serial_ports()
            if args.no_interactive:
                if ports:
                    port = ports[0]
                    print_info(f"Auto-selected port: {port}")
                else:
                    print_error("No serial port detected and --no-interactive specified")
                    sys.exit(1)
            else:
                port = select_port(ports)

    # Baud rate
    baud = args.baud if args.baud else cfg["default_baud"]

    # Confirm before flashing (unless --no-interactive)
    if not args.no_interactive:
        print(f"\n{'=' * 60}")
        print(f"{Colors.BOLD}Ready to flash:{Colors.ENDC}")
        print(f"  Board:   {cfg['name']}")
        print(f"  Mode:    {mode.upper()}")
        if version_info:
            print(f"  Version: {version_info}")
        print(f"  Port:    {port}")
        print(f"  Baud:    {baud}")
        if args.erase:
            print(f"  {Colors.WARNING}Erase flash: YES{Colors.ENDC}")
        print(f"{'=' * 60}")

        confirm = input(f"\n{Colors.BOLD}Proceed? [Y/n]: {Colors.ENDC}").strip().lower()
        if confirm not in ("", "y", "yes"):
            print_info("Aborted.")
            sys.exit(0)

    success = flash_device(
        board=board,
        port=port,
        artifacts=artifacts,
        baud=baud,
        do_erase=args.erase,
    )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n\n{Colors.WARNING}Interrupted.{Colors.ENDC}")
        sys.exit(1)
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

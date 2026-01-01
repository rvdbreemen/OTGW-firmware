#!/usr/bin/env python3
"""
Local build script for OTGW-firmware
This script automates the build process for Windows and Mac platforms,
replicating the CI/CD workflow for local development.

Requirements:
- Python 3.x
- arduino-cli (installed automatically if not found)
- make (for running Makefile targets)

Usage:
    python build.py              # Full build (firmware + filesystem)
    python build.py --firmware   # Build firmware only
    python build.py --filesystem # Build filesystem only
    python build.py --clean      # Clean build artifacts
    python build.py --help       # Show help
"""

import argparse
import multiprocessing
import os
import platform
import shutil
import stat
import subprocess
import sys
import tarfile
import traceback
import urllib.request
import zipfile
from pathlib import Path


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


def check_make():
    """Check if make is installed"""
    print_step("Checking for make")
    try:
        result = run_command(["make", "--version"], capture_output=True, check=False)
        if result.returncode == 0:
            print_success("make is installed")
            return True
        else:
            print_warning("make not found in PATH")
            return False
    except FileNotFoundError:
        print_warning("make not found in PATH")
        return False


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
        urllib.request.urlretrieve(url, download_path)
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
        str(project_dir),
        "--filename", "version.h",
        "--githash", githash
    ]
    run_command(cmd)
    print_success("Version information updated")


def get_semver(project_dir):
    """Extract semantic version from version.h"""
    version_file = project_dir / "version.h"
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
    
    build_dir = project_dir / "build"
    
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


def build_firmware(project_dir):
    """Build firmware using make"""
    print_step("Building firmware")
    
    # Get number of CPU cores for parallel build
    try:
        num_cores = multiprocessing.cpu_count()
    except (NotImplementedError, OSError):
        num_cores = 1
    
    print_info(f"Starting compilation (this may take several minutes)...")
    print_info(f"Using {num_cores} parallel jobs")
    
    cmd = ["make", f"-j{num_cores}"]
    run_command(cmd, cwd=project_dir, show_output=True)
    print_success("Firmware build complete")


def build_filesystem(project_dir):
    """Build filesystem using make"""
    print_step("Building filesystem")
    
    print_info("Creating LittleFS filesystem image...")
    
    cmd = ["make", "filesystem"]
    run_command(cmd, cwd=project_dir, show_output=True)
    print_success("Filesystem build complete")


def consolidate_build_artifacts(project_dir):
    """Move all build artifacts from subdirectories to build root and clean up"""
    print_step("Consolidating build artifacts")
    
    build_dir = project_dir / "build"
    if not build_dir.exists():
        print_warning("Build directory not found, skipping consolidation")
        return
    
    moved = []
    
    # Find and move all binary artifacts to build root
    patterns = ["**/*.ino.bin", "**/*.ino.elf", "**/*.littlefs.bin"]
    for pattern in patterns:
        for file_path in build_dir.glob(pattern):
            # Skip files already in build root
            if file_path.parent == build_dir:
                continue
            
            # Move to build root
            dest_path = build_dir / file_path.name
            
            # Handle name conflicts
            if dest_path.exists():
                print_warning(f"File {dest_path.name} already exists, overwriting")
                dest_path.unlink()
            
            shutil.move(str(file_path), str(dest_path))
            moved.append(dest_path)
            print_info(f"Moved: {file_path.relative_to(build_dir)} -> {file_path.name}")
    
    if moved:
        print_success(f"Moved {len(moved)} artifact(s) to build root")
    
    # Remove empty subdirectories and any remaining files
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
    
    build_dir = project_dir / "build"
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
            new_name = file_path.stem + f".{semver}.littlefs.bin"
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


def clean_build(project_dir):
    """Clean build artifacts"""
    print_step("Cleaning build artifacts")
    
    cmd = ["make", "distclean"]
    run_command(cmd, cwd=project_dir)
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
  python build.py --clean          # Clean build artifacts
  python build.py --no-rename      # Build without renaming artifacts
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
        "--no-rename",
        action="store_true",
        help="Skip renaming build artifacts with version"
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
    
    # Handle clean
    if args.clean:
        clean_build(project_dir)
        return
    
    # Check for make
    if not check_make():
        print_error("make is required but not found")
        if system == "Darwin":
            print_info("Install Xcode Command Line Tools: xcode-select --install")
        elif system == "Windows":
            print_info("Install make from: http://gnuwin32.sourceforge.net/packages/make.htm")
            print_info("Or use Chocolatey: choco install make")
        sys.exit(1)
    
    # Install arduino-cli if needed and add to PATH
    if not args.no_install_cli:
        cli_install_dir = install_arduino_cli(system)
        # Add arduino-cli to PATH for subprocess calls
        if cli_install_dir:
            current_path = os.environ.get("PATH", "")
            os.environ["PATH"] = f"{cli_install_dir}{os.pathsep}{current_path}"
            print_info(f"Added {cli_install_dir} to PATH for this build session")
    
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
        build_firmware(project_dir)
    elif args.filesystem and not args.firmware:
        # Filesystem only
        build_filesystem(project_dir)
    else:
        # Full build (default)
        build_firmware(project_dir)
        build_filesystem(project_dir)
    
    # Consolidate build artifacts from subdirectories
    consolidate_build_artifacts(project_dir)
    
    # Rename artifacts with version
    if not args.no_rename:
        rename_build_artifacts(project_dir, semver)
    
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

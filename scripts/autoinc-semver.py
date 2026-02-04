#!/usr/bin/env python3
# This script is part of the autoinc-semver project.
# It increments the build number in version.h and updates timestamp and githash.

import argparse
import datetime as dt
import logging
import os
import re
import subprocess
import sys


DEFINE_RE = re.compile(r"^\s*#define\s+(\w+)\s+(.+?)\s*$")
DEFINE_RE_WITH_GROUPS = re.compile(r"^(\s*#define\s+)(\w+)(\s+)(.+?)([ \t]*)$")


def normalize_token(value):
    if value is None:
        return ""
    value = value.strip()
    if (value.startswith('"') and value.endswith('"')) or (
        value.startswith("'") and value.endswith("'")
    ):
        return value[1:-1]
    return value


def parse_int(value, key):
    try:
        return int(normalize_token(value))
    except ValueError:
        logging.error("Invalid numeric value for %s: %s", key, value)
        sys.exit(1)


def parse_version_file(path):
    """Parse the version file to extract version components."""
    logging.info("Parsing version file: %s", path)
    version_info = {}
    try:
        with open(path, "r", encoding="utf-8") as file:
            for line in file:
                match = DEFINE_RE.match(line)
                if not match:
                    continue
                key, value = match.groups()
                if key in {
                    "_VERSION_MAJOR",
                    "_VERSION_MINOR",
                    "_VERSION_PATCH",
                    "_VERSION_BUILD",
                    "_VERSION_PRERELEASE",
                }:
                    version_info[key] = value.strip()
    except FileNotFoundError:
        logging.error("Version file not found: %s", path)
        sys.exit(1)
    except Exception as exc:
        logging.error("Error reading version file %s: %s", path, exc)
        sys.exit(1)

    missing = [
        key
        for key in (
            "_VERSION_MAJOR",
            "_VERSION_MINOR",
            "_VERSION_PATCH",
            "_VERSION_BUILD",
        )
        if key not in version_info
    ]
    if missing:
        logging.error("Missing version keys in %s: %s", path, ", ".join(missing))
        sys.exit(1)
    
    # _VERSION_PRERELEASE is optional; default to empty string if missing
    if "_VERSION_PRERELEASE" not in version_info:
        version_info["_VERSION_PRERELEASE"] = ""

    logging.info("Version info: %s", version_info)
    return version_info


def resolve_githash(override, length):
    if override:
        return override[:length]
    env_sha = os.environ.get("GITHUB_SHA")
    if env_sha:
        return env_sha[:length]
    try:
        result = subprocess.check_output(
            ["git", "rev-parse", f"--short={length}", "HEAD"],
            text=True,
        )
        return result.strip()
    except Exception:
        return "unknown"


def prerelease_suffix(prerelease):
    if not prerelease:
        return ""
    lowered = prerelease.strip().lower()
    if lowered in {"none", "null", "n/a", "na"}:
        return ""
    return f"-{prerelease}"


def update_version_header(path, version_info, githash, date_str, time_str, increment, prerelease_override):
    major = parse_int(version_info["_VERSION_MAJOR"], "major")
    minor = parse_int(version_info["_VERSION_MINOR"], "minor")
    patch = parse_int(version_info["_VERSION_PATCH"], "patch")
    build = parse_int(version_info["_VERSION_BUILD"], "build") + increment
    prerelease_raw = version_info["_VERSION_PRERELEASE"]
    prerelease_value = (
        prerelease_override
        if prerelease_override is not None
        else normalize_token(prerelease_raw)
    )

    core = f"{major}.{minor}.{patch}"
    pre_suffix = prerelease_suffix(prerelease_value)

    updates = {
        "_VERSION_MAJOR": str(major),
        "_VERSION_MINOR": str(minor),
        "_VERSION_PATCH": str(patch),
        "_VERSION_BUILD": str(build),
        "_VERSION_GITHASH": f"\"{githash}\"",
        "_VERSION_DATE": f"\"{date_str}\"",
        "_VERSION_TIME": f"\"{time_str}\"",
        "_SEMVER_CORE": f"\"{core}\"",
        "_SEMVER_BUILD": f"\"{core}+{build}\"",
        "_SEMVER_GITHASH": f"\"{core}+{githash}\"",
        "_SEMVER_FULL": f"\"{core}{pre_suffix}+{githash}\"",
        "_SEMVER_NOBUILD": f"\"{core}{pre_suffix} ({date_str})\"",
        "_VERSION": f"\"{core}{pre_suffix}+{githash} ({date_str})\"",
    }
    if prerelease_override is not None:
        updates["_VERSION_PRERELEASE"] = prerelease_override

    found = set()
    output_lines = []
    with open(path, "r", encoding="utf-8", newline="") as file:
        for line in file:
            line_ending = ""
            line_body = line
            if line.endswith("\r\n"):
                line_ending = "\r\n"
                line_body = line[:-2]
            elif line.endswith("\n"):
                line_ending = "\n"
                line_body = line[:-1]

            match = DEFINE_RE_WITH_GROUPS.match(line_body)
            if match:
                prefix, key, spacer, _value, suffix = match.groups()
                if key in updates:
                    output_lines.append(
                        f"{prefix}{key}{spacer}{updates[key]}{suffix}{line_ending}"
                    )
                    found.add(key)
                    continue
            output_lines.append(line)

    missing = set(updates) - found
    if missing:
        logging.warning("Did not update keys in %s: %s", path, ", ".join(sorted(missing)))

    with open(path, "w", encoding="utf-8", newline="") as file:
        file.writelines(output_lines)

    version_info["_VERSION_BUILD"] = str(build)
    version_info["_VERSION_GITHASH"] = githash
    version_info["_VERSION_DATE"] = date_str
    version_info["_VERSION_TIME"] = time_str
    version_info["_VERSION_PRERELEASE"] = prerelease_value

    return version_info


def extract_version_from_file(filepath):
    """Extract version string from a file to check if it needs updating."""
    pre_version_text = r"((\*\*|//)\s*Version\s*:\s*v)(\d+\.\d+\.\d+)(.*)"
    version_pattern = re.compile(pre_version_text)
    
    try:
        with open(filepath, "r", encoding="utf-8", newline="") as file:
            content = file.read()
    except UnicodeDecodeError:
        try:
            with open(filepath, "r", encoding="latin-1", newline="") as file:
                content = file.read()
        except Exception:
            return None
    except Exception:
        return None
    
    match = version_pattern.search(content)
    if match:
        version = match.group(3)  # The semver part (e.g., "0.10.4")
        suffix = match.group(4).strip()  # The suffix part (e.g., "-alpha" or "")
        return version + suffix
    return None


def update_version_in_file(filepath, version_info):
    """Replace the version string in the specified file."""
    pre_version_text = r"((\*\*|//)\s*Version\s*:\s*v)(\d+\.\d+\.\d+)(.*)"
    version_pattern = re.compile(pre_version_text)
    
    major = parse_int(version_info["_VERSION_MAJOR"], "major")
    minor = parse_int(version_info["_VERSION_MINOR"], "minor")
    patch = parse_int(version_info["_VERSION_PATCH"], "patch")
    prerelease_raw = version_info["_VERSION_PRERELEASE"]
    prerelease_value = normalize_token(prerelease_raw)
    
    new_version = f"{major}.{minor}.{patch}"
    if prerelease_value and prerelease_value.lower() not in {"none", "null", "n/a", "na"}:
        new_version += f"-{prerelease_value}"

    # Try UTF-8 first, fall back to latin-1 if that fails, or skip if neither works
    try:
        with open(filepath, "r", encoding="utf-8", newline="") as file:
            content = file.read()
    except UnicodeDecodeError:
        # File is not UTF-8, try latin-1 or skip
        try:
            with open(filepath, "r", encoding="latin-1", newline="") as file:
                content = file.read()
        except Exception:
            # If we can't read it, skip it
            raise

    updated_content = version_pattern.sub(lambda m: m.group(1) + new_version, content)

    # Only write if content changed
    if updated_content != content:
        with open(filepath, "w", encoding="utf-8", newline="") as file:
            file.write(updated_content)
        return True
    return False


def should_skip_path(path, base_dir):
    """Check if a path should be skipped based on exclusion rules."""
    rel_path = os.path.relpath(path, base_dir)
    path_parts = rel_path.split(os.sep)
    
    # Directories to skip (third-party code, build artifacts, dependencies, internal)
    skip_dirs = {
        "arduino", "Arduino", "libraries", "staging", "build", 
        "node_modules", ".git", "__pycache__", ".github",
        "scripts", "docs", "hardware", "example-api", "Specification",
        "specification"
    }
    
    # Check if any part of the path is in skip_dirs
    for part in path_parts:
        if part in skip_dirs:
            return True
        # Also skip hidden directories
        if part.startswith('.') and part not in {'.'}:
            return True
    
    return False


def update_files(directory, version_info, ext_list, check_only=False):
    """Update version in files within the specified directory.
    
    Args:
        directory: Root directory to search
        version_info: Dictionary with version information from version.h
        ext_list: List of file extensions to process
        check_only: If True, only check if updates are needed without making changes
        
    Returns:
        Number of files updated (or that would be updated if check_only=True)
    """
    if not os.path.exists(directory):
        logging.error("Directory %s does not exist.", directory)
        return 0
    if not os.listdir(directory):
        logging.error("Directory %s is empty.", directory)
        return 0

    # Build the expected version string
    major = parse_int(version_info["_VERSION_MAJOR"], "major")
    minor = parse_int(version_info["_VERSION_MINOR"], "minor")
    patch = parse_int(version_info["_VERSION_PATCH"], "patch")
    prerelease_raw = version_info["_VERSION_PRERELEASE"]
    prerelease_value = normalize_token(prerelease_raw)
    
    expected_version = f"{major}.{minor}.{patch}"
    if prerelease_value and prerelease_value.lower() not in {"none", "null", "n/a", "na"}:
        expected_version += f"-{prerelease_value}"

    files_updated = 0
    files_checked = 0

    for root, dirs, files in os.walk(directory):
        # Skip excluded directories
        dirs[:] = [d for d in dirs if not should_skip_path(os.path.join(root, d), directory)]
        
        for file in files:
            filepath = os.path.join(root, file)
            
            # Skip if path should be excluded
            if should_skip_path(filepath, directory):
                continue
                
            _, ext = os.path.splitext(file)
            if ext in ext_list:
                files_checked += 1
                try:
                    current_version = extract_version_from_file(filepath)
                    if current_version and current_version != expected_version:
                        if check_only:
                            logging.info("Would update %s: %s -> %s", 
                                       os.path.relpath(filepath, directory),
                                       current_version, expected_version)
                            files_updated += 1
                        else:
                            if update_version_in_file(filepath, version_info):
                                logging.info("Updated %s: %s -> %s", 
                                           os.path.relpath(filepath, directory),
                                           current_version, expected_version)
                                files_updated += 1
                except Exception as exc:
                    logging.warning("Failed to process %s: %s", 
                                  os.path.relpath(filepath, directory), exc)
    
    logging.info("Checked %d files, updated %d files", files_checked, files_updated)
    return files_updated


def update_version_hash(path, githash):
    if not os.path.exists(path):
        return
    with open(path, "w", encoding="utf-8") as file:
        file.write(f"{githash}\n")


def git_commit_changes(directory, version):
    """Commit changes in the git repository."""
    subprocess.run(["git", "add", "."], cwd=directory, check=False)
    subprocess.run(
        ["git", "commit", "-m", f"Update version to {version}"],
        cwd=directory,
        check=False,
    )
    subprocess.run(["git", "tag", f"auto-update-version-{version}"], cwd=directory, check=False)


def main(directory, filename, git_enabled, increment, githash_override, githash_len, prerelease_override, update_all):
    directory = os.path.abspath(directory)
    os.chdir(directory)

    # Parse the current version.h to get the authoritative version
    version_info = parse_version_file(filename)
    
    now = dt.datetime.now(dt.timezone.utc)
    date_str = now.strftime("%d-%m-%Y")
    time_str = now.strftime("%H:%M:%S")
    githash = resolve_githash(githash_override, githash_len)

    # Update version.h with new build, githash, date, time
    version_info = update_version_header(
        filename,
        version_info,
        githash,
        date_str,
        time_str,
        increment,
        prerelease_override,
    )

    # Get the authoritative semver from version.h (without build number)
    major = parse_int(version_info["_VERSION_MAJOR"], "major")
    minor = parse_int(version_info["_VERSION_MINOR"], "minor")
    patch = parse_int(version_info["_VERSION_PATCH"], "patch")
    prerelease = normalize_token(version_info["_VERSION_PRERELEASE"])
    
    authoritative_semver = f"{major}.{minor}.{patch}"
    if prerelease and prerelease.lower() not in {"none", "null", "n/a", "na"}:
        authoritative_semver += f"-{prerelease}"
    
    # Update version.hash
    update_version_hash(os.path.join("data", "version.hash"), githash)

    # Check if any project files need updating (have different version than version.h)
    ext_list = [".h", ".ino", ".cpp", ".c", ".js", ".css", ".html", ".md", ".txt"]
    needs_update = False
    
    if not update_all:
        # Quick check: scan files to see if any have a different version
        for root, dirs, files in os.walk(directory):
            dirs[:] = [d for d in dirs if not should_skip_path(os.path.join(root, d), directory)]
            
            for file in files:
                filepath = os.path.join(root, file)
                if should_skip_path(filepath, directory):
                    continue
                    
                _, ext = os.path.splitext(file)
                if ext in ext_list:
                    try:
                        current_version = extract_version_from_file(filepath)
                        if current_version and current_version != authoritative_semver:
                            needs_update = True
                            logging.info("Detected version mismatch in %s: %s (expected: %s)",
                                       os.path.relpath(filepath, directory),
                                       current_version,
                                       authoritative_semver)
                            break
                    except Exception:
                        pass
            if needs_update:
                break
    
    # Update all files if requested or if version mismatch detected
    if update_all or needs_update:
        if needs_update and not update_all:
            logging.info("Version mismatch detected, automatically updating all project files")
        
        files_updated = update_files(directory, version_info, ext_list, check_only=False)
        
        if files_updated > 0:
            logging.info("Updated version strings in %d file(s)", files_updated)
        else:
            logging.info("No files needed version updates")

    if git_enabled:
        git_commit_changes(
            directory,
            f"{version_info['_VERSION_MAJOR']}.{version_info['_VERSION_MINOR']}.{version_info['_VERSION_PATCH']}+{version_info['_VERSION_BUILD']}",
        )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    parser = argparse.ArgumentParser(
        description="Increment build number and update version.h with timestamp and githash."
    )
    parser.add_argument("directory", type=str, help="Directory to update files in")
    parser.add_argument(
        "--filename",
        type=str,
        default="version.h",
        help="Filename of the version file",
    )
    parser.add_argument("--git", action="store_true", help="Enable git integration")
    parser.add_argument(
        "--increment-build",
        type=int,
        default=1,
        help="Amount to increment the build number",
    )
    parser.add_argument(
        "--githash",
        type=str,
        default=None,
        help="Override githash (full or short)",
    )
    parser.add_argument(
        "--githash-length",
        type=int,
        default=7,
        help="Length of the short githash",
    )
    parser.add_argument(
        "--prerelease",
        type=str,
        default=None,
        help="Override prerelease identifier",
    )
    parser.add_argument(
        "--update-all",
        action="store_true",
        help="Update version strings in all project files (automatically enabled on semver changes)",
    )
    args = parser.parse_args()
    main(
        args.directory,
        args.filename,
        args.git,
        args.increment_build,
        args.githash,
        args.githash_length,
        args.prerelease,
        args.update_all,
    )

#!/usr/bin/env python3
# This script is part of the autoinc-semver project.
# It increments the build number in version.h, updates timestamp and githash,
# and propagates the core/prerelease version to other source files.

import argparse
import datetime as dt
import logging
import os
import re
import subprocess
import sys


DEFINE_RE = re.compile(r"^\s*#define\s+(\w+)\s+(.+?)\s*$")
DEFINE_RE_WITH_GROUPS = re.compile(r"^(\s*#define\s+)(\w+)(\s+)(.+?)(\s*)$")


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
            "_VERSION_PRERELEASE",
        )
        if key not in version_info
    ]
    if missing:
        logging.error("Missing version keys in %s: %s", path, ", ".join(missing))
        sys.exit(1)

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
    with open(path, "r", encoding="utf-8") as file:
        for line in file:
            match = DEFINE_RE_WITH_GROUPS.match(line)
            if match:
                prefix, key, spacer, _value, suffix = match.groups()
                if key in updates:
                    output_lines.append(f"{prefix}{key}{spacer}{updates[key]}{suffix}\n")
                    found.add(key)
                    continue
            output_lines.append(line)

    missing = set(updates) - found
    if missing:
        logging.warning("Did not update keys in %s: %s", path, ", ".join(sorted(missing)))

    with open(path, "w", encoding="utf-8") as file:
        file.writelines(output_lines)

    version_info["_VERSION_BUILD"] = str(build)
    version_info["_VERSION_GITHASH"] = githash
    version_info["_VERSION_DATE"] = date_str
    version_info["_VERSION_TIME"] = time_str
    version_info["_VERSION_PRERELEASE"] = prerelease_value

    return version_info


def update_version_in_file(filepath, version_info):
    """Replace the version string in the specified file."""
    pre_version_text = r"((\*\*|//)\s*Version\s*:\s*v)(\d+\.\d+\.\d+)(.*)"
    version_pattern = re.compile(pre_version_text)
    new_version = (
        f"{version_info['MAJOR']}.{version_info['MINOR']}.{version_info['PATCH']}"
    )
    if version_info.get("PRERELEASE"):
        new_version += f"-{version_info['PRERELEASE']}"

    with open(filepath, "r", encoding="utf-8") as file:
        content = file.read()

    updated_content = version_pattern.sub(lambda m: m.group(1) + new_version, content)

    with open(filepath, "w", encoding="utf-8") as file:
        file.write(updated_content)


def update_files(directory, version_info, ext_list):
    """Update version in files within the specified directory."""
    if not os.path.exists(directory):
        logging.error("Directory %s does not exist.", directory)
        return
    if not os.listdir(directory):
        logging.error("Directory %s is empty.", directory)
        return

    for root, _dirs, files in os.walk(directory):
        for file in files:
            _, ext = os.path.splitext(file)
            if ext in ext_list:
                try:
                    update_version_in_file(os.path.join(root, file), version_info)
                except Exception as exc:
                    logging.warning("Failed to update %s: %s", file, exc)


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


def main(directory, filename, git_enabled, increment, githash_override, githash_len, prerelease_override):
    directory = os.path.abspath(directory)
    os.chdir(directory)

    version_info = parse_version_file(filename)
    now = dt.datetime.now(dt.timezone.utc)
    date_str = now.strftime("%d-%m-%Y")
    time_str = now.strftime("%H:%M:%S")
    githash = resolve_githash(githash_override, githash_len)

    version_info = update_version_header(
        filename,
        version_info,
        githash,
        date_str,
        time_str,
        increment,
        prerelease_override,
    )

    update_version_hash(os.path.join("data", "version.hash"), githash)

    ext_list = [".ino", ".h", ".c", ".cpp", ".js", ".css", ".html", ".inc", ".cfg"]
    update_files(
        directory,
        {
            "MAJOR": parse_int(version_info["_VERSION_MAJOR"], "major"),
            "MINOR": parse_int(version_info["_VERSION_MINOR"], "minor"),
            "PATCH": parse_int(version_info["_VERSION_PATCH"], "patch"),
            "PRERELEASE": prerelease_override
            if prerelease_override is not None
            else normalize_token(version_info["_VERSION_PRERELEASE"]),
        },
        ext_list,
    )

    if git_enabled:
        git_commit_changes(
            directory,
            f"{version_info['_VERSION_MAJOR']}.{version_info['_VERSION_MINOR']}.{version_info['_VERSION_PATCH']}+{version_info['_VERSION_BUILD']}",
        )


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    parser = argparse.ArgumentParser(
        description="Increment build number, update version.h, and propagate version strings."
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
    args = parser.parse_args()
    main(
        args.directory,
        args.filename,
        args.git,
        args.increment_build,
        args.githash,
        args.githash_length,
        args.prerelease,
    )

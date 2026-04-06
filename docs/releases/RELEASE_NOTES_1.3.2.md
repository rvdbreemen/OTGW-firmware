# OTGW-firmware v1.3.2 Release Notes

**Release date:** 2026-03-29
**Branch:** main (from dev)
**Compare:** [v1.3.1-fix...v1.3.2](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.1-fix...v1.3.2)

## Overview

v1.3.2 is a targeted bugfix release that resolves the persistent file explorer "Error loading file list" / file deletion failures reported by simontemplar after v1.3.1. The file listing and delete backend has been rewritten to stream directly from LittleFS, eliminating RAM-heavy sorting and shared-buffer conflicts that caused the gateway to become unresponsive.

## Bug fixes

- **File delete handler rewritten:** The `apilistfiles()` delete handler previously used the global `cMsg` buffer, which is shared with MQTT autoconfig, webhooks, and settings. Background tasks could overwrite `cMsg` between the `strlcpy` and `LittleFS.remove()` calls, causing "Failed to delete file" errors and unresponsive behavior. The handler now uses a local `deletePath[34]` buffer with proper path normalization and explicit HTTP status codes (404/200/500).

- **File listing switched to streaming:** The old approach allocated a `dirMap[]` array on the stack, performed a bubble sort, and formatted sizes server-side. This consumed significant RAM and could cause timeouts on directories with many files. The new implementation streams entries directly from `LittleFS.openDir()` as JSON, with sorting and size formatting handled entirely by the frontend (`FSexplorer.html`). Hidden files (names starting with `.`) are now filtered out server-side.

## Internal improvements

- **MQTT LWT documentation:** Added developer guide explaining MQTT Last Will and Testament behavior (#526).

## Breaking changes

No breaking changes vs v1.3.1. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical.

## Upgrade notes

- Flash both firmware and filesystem (frontend changes require filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5) to pick up the new FSexplorer.html.
- The file explorer frontend now handles sorting and size formatting — if you have a custom FSexplorer.html, update it to match.

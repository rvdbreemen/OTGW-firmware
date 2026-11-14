# OTGW-firmware v1.7.10 Release Notes

**Release date:** TBD
**Previous release:** v1.7.9
**Platform:** ESP8266 (1.x maintenance line)
**Compare:** [v1.7.9...v1.7.10](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.7.9...v1.7.10)

## Overview

v1.7.10 is a maintenance release for the 1.x (ESP8266) line with one fix in the web file handler. Everything else is
unchanged from v1.7.9.

No breaking changes versus v1.7.9.

## Bug fixes

### File paths are normalised before the access check (TASK-1935)

The web server now brings every requested file path into one normal form before it decides whether the request
needs the admin password, and it opens exactly that path. Repeated slashes are collapsed, `.` is dropped, `..` is
resolved, and a path that points above the root of the filesystem is refused.

- With an admin password set, the protected files are only served after you log in, whichever way the path is
  written.
- Without an admin password nothing changes.
- Ordinary pages, scripts and style sheets are served as before.

Recommended for every gateway that has an admin password set.

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

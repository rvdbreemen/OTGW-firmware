**Maintenance release: file paths are normalised before the access check.**

v1.7.10 is a maintenance release for the 1.x (ESP8266) line. No breaking changes versus v1.7.9.

Full release notes: [RELEASE_NOTES_1.7.10.md](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.7.10.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md) | [CHANGELOG](https://github.com/rvdbreemen/OTGW-firmware/blob/main/CHANGELOG.md)

## Fixed

- **File paths are normalised before the access check.** The web server brings every requested file path into one normal form before it decides whether the admin password is needed, and opens exactly that path. With an admin password set, protected files are only served after you log in. Without a password nothing changes. Recommended for every gateway with an admin password. (TASK-1935)

## Upgrade

Flash both firmware and filesystem. Settings are preserved.

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

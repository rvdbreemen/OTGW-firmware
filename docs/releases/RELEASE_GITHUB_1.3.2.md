v1.3.2 fixes the persistent file explorer failures ("Error loading file list", file deletion errors, unresponsive gateway) reported after v1.3.1.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.2.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md)

## Bug fixes

- **File delete reliability:** The delete handler used the global `cMsg` buffer shared with MQTT/webhooks/settings. Background tasks could overwrite it mid-operation, causing "Failed to delete file" errors and slow/unresponsive behavior. Now uses a dedicated local buffer with proper HTTP status codes.
- **File listing rewritten to streaming:** Replaced the RAM-heavy `dirMap[]` array + bubble sort with direct LittleFS streaming. Sorting and size formatting moved to the frontend. Hidden files (`.` prefix) are now filtered out.

## Improvements

- MQTT LWT developer documentation added (#526).

## Upgrade notes

- No breaking changes vs v1.3.1.
- Flash both firmware and filesystem (frontend changes require filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5).

## Thank you

Special shoutout to **simontemplar** (Discord) for persistently reporting the file explorer issue across multiple versions, providing clear reproduction steps, and confirming the fix!

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.1](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.1-fix)

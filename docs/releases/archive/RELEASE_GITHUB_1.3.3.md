v1.3.3 adds PIC-less OTGW support and fixes the dashboard showing empty values for unsupported OpenTherm message IDs.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.3.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md)

## New features

- **PIC-less OTGW support:** All PIC functions are automatically disabled when no PIC is detected at boot. Auto-recovery if the PIC appears later. REST API returns 503, UI hides PIC elements.

## Bug fixes

- **Dashboard no longer shows unsupported OT values:** Empty or zero values for message IDs the boiler doesn't support are no longer displayed. Only valid responses are tracked and shown.
- **Gateway mode detection:** Non-gateway PIC firmware now correctly shows "N/A" instead of continuously polling.
- **"Home Assistant Integration" renamed to "OTGW Connected":** Label and tooltip now accurately describe the field (OTGW online status, not HA integration status).

## Upgrade notes

- No breaking changes vs v1.3.2.
- Flash both firmware and filesystem (frontend changes require filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5).

## Thank you

Special shoutout to **chielh** (Discord) for reporting the missing DHW temperature and Max CH Water setpoint values on the dashboard, providing telnet logs, and confirming the fix!

Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.

---

Previous release: [v1.3.2](https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v1.3.2)

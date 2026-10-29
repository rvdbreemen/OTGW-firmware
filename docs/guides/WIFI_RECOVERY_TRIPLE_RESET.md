# WiFi Recovery: Force Config Portal with Triple Reset

This guide documents the reset-only recovery flow for ESP8266 boards (including Wemos D1 mini) where holding reset cannot be detected by firmware.

## Why this method exists

On ESP8266, the firmware does not run while the reset button is held. That means a "hold reset during reboot" action cannot be detected in software.

To provide reliable recovery without extra hardware buttons, OTGW-firmware supports a reset pattern trigger:

- Press reset **3 times within 10 seconds**
- On the third reset, firmware forces the WiFiManager configuration portal
- Stored WiFi credentials are kept; the portal opens even when the saved network is reachable
- Only presses of the reset button count; a power cycle resets the count

## Recovery steps

1. Power the OTGW board.
2. Press the hardware reset button 3 times quickly (all 3 resets within 10 seconds).
3. Wait for the device to boot into WiFi config mode.
4. Connect to the OTGW access point (`<hostname>-<mac>`).
5. Open the WiFiManager portal and configure the new WiFi network.

## Notes

- This trigger is only for manual recovery and should not affect normal startup.
- If resets are too slow (outside the 10-second window), forced recovery is not triggered.
- The count is also cleared when start-up completes, so press reset again before the device has finished booting.
- If you leave the portal without saving, the device restarts after 240 seconds and connects to the saved network again.
- The Web UI reset path (`/ResetWireless`, the **Reset Wireless** button on the FSexplorer page) is different: it clears the stored WiFi credentials before it reboots.

## Troubleshooting

- If the portal does not appear, repeat the triple reset with shorter intervals.
- If needed, power cycle once and retry the 3-reset sequence.
- After successful config save, the device returns to normal WiFi startup behavior.

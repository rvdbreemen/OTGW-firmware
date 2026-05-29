# WiFi Recovery: Force Config Portal with Triple Reset

This guide documents the reset-only recovery flow for ESP8266 boards (including Wemos D1 mini), where holding the reset button cannot be detected by the firmware.

## Why this method exists

On ESP8266, the firmware does not run while the reset button is held down. The CPU is held in reset, so a "hold reset during reboot" gesture cannot be detected in software. Holding the reset button therefore does nothing useful for recovery: it is not a missing feature, it is a hardware limitation.

To provide reliable recovery without an extra hardware button, OTGW-firmware uses a reset-count trigger instead. The gesture is three short presses, not one long hold.

## The exact gesture

- Press and release the hardware **reset button 3 times in a row**.
- Each press must follow the previous one within **10 seconds** (the firmware boots between presses, so wait for the board to come back up, then press again).
- On the 3rd press the firmware boots into the WiFiManager configuration portal instead of connecting to your saved network.

The trigger counts presses of the **reset button** specifically. A power-cycle (pulling and re-applying power) is reported by the chip as a different reset reason and does **not** increment the counter. Use the reset button.

If the resets are spaced more than 10 seconds apart, the counter is cleared and the sequence has to start over. This 10-second window is what keeps an ordinary single reboot from accidentally entering recovery.

## What happens on the 3rd reset

The device boots straight into the WiFiManager configuration portal:

1. It starts an access point named `<hostname>-XXXXXX`, where `<hostname>` is the configured device hostname (default `OTGW`) and `XXXXXX` is the last three bytes of the MAC address, for example `OTGW-21B4F8`.
2. Connect to that access point from a phone or laptop.
3. A captive portal page opens automatically, or browse to `http://192.168.4.1/` manually.
4. Enter your WiFi SSID and password and save. The new credentials replace the old ones, and the device reboots and connects to the new network.

The triple-reset clears the stored WiFi credentials (per ADR-043) and then opens the config portal. If the portal times out without a save, the device reboots with no stored network: it re-enters the config portal (or, on prerelease builds, the AP fallback) rather than reconnecting to the old network.

## Recovery steps

1. Power the OTGW board and wait for it to finish booting.
2. Press and release the reset button. Wait for the board to boot again.
3. Press and release the reset button a second time. Wait for it to boot again.
4. Press and release the reset button a third time, all within roughly 10 seconds of each previous press.
5. The device boots into WiFi config mode.
6. Connect to the access point (`<hostname>-XXXXXX`).
7. Open the portal and configure the new WiFi network.

## Notes

- Holding the reset button does nothing on ESP8266 (the CPU cannot sample a held reset). This is intentional behaviour, not a bug. The three-press gesture is the recognised ESP/IoT-community recovery pattern and was chosen deliberately to prevent accidental wipes.
- The trigger is only for manual recovery and does not affect a normal single reboot.
- If the resets are too slow (outside the 10-second window), forced recovery is not triggered.
- The Web UI reset path (`/ResetWireless`) and the Telnet `resetwifi` command both clear the stored WiFi credentials directly, which is a different mechanism from the triple-reset trigger.

## Troubleshooting

- If the portal does not appear, repeat the three presses with shorter intervals so each falls inside the 10-second window.
- If needed, power cycle once and retry the 3-press sequence.
- After a successful save, the device returns to normal WiFi startup behaviour.

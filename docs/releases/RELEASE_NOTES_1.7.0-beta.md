# OTGW-firmware 1.7.0 beta cycle

Beta builds for field testers on the 1.x line (`otgw-1.x.x`, ESP8266). These notes cover the 1.7.0 beta cycle as a whole and are reused across beta builds. Per-build details are in [CHANGELOG.md](CHANGELOG.md).

<!-- digest:end -->

## What the 1.7.0 beta cycle is about

Two themes, both aimed at long-running devices under sustained load (decode plus MQTT plus Web UI traffic):

1. **Heap-fragmentation crash-proofing (beta.1 to beta.6).** A field bisect traced random reboots to the largest contiguous heap block collapsing under load while total free memory still looked fine. The publish, socket and HTTP-serving paths now gate on the largest contiguous block (not just total free) and back off gracefully instead of faulting, and the main loop was returned to its field-proven `delay(1)` pacing. New diagnostic counters (`mqtt_fragskips`, `ws_fragskips`, `http_fragskips`) are exposed on telnet and in MQTT stats.

2. **RAM and heap-headroom optimization (beta.7 to beta.34).** A measured pass that reclaimed about 6.6 KB of static RAM and lifted the idle free heap by about 6 KB. The headline change moved the OpenTherm message-name table into flash; the rest were small, individually verified buffer and integer-width trims plus two String-allocation removals. The largest-contiguous-block floor under load went from about 4.9 KB back up to about 11 KB, which is roughly the headroom the firmware had at 1.3.5. No feature behaviour changed.

Plus two correctness fixes (MQTT broker ports above 32767, S0 pulses-per-kWh above 65535) and a Web UI fix (friendly labels for the runtime WiFi rows on the Debug Information tab).

## Why a beta

The optimization pass was validated on a bench (synthetic boiler-traffic replay plus MQTT and HTTP load), not on a wide range of real boilers. Field testing across different boilers, thermostats and network conditions is exactly what this beta is for.

## How to help

Flash the build, run it on your normal setup, and report findings (good and bad) in `#beta-testing`: stability over multiple days, any reboots, MQTT and Home Assistant behaviour, and the Web UI. The Debug Information tab (Advanced menu) shows the live heap and the new fragmentation counters if you want to watch headroom directly.

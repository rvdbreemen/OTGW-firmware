# Migrating from OTTHING firmware: clearing retained `otthing/*` MQTT topics

This guide is for testers who flashed OTGW32 firmware on a device that previously ran OTTHING-platform firmware (the `BOARD_SEEGEL_OTTHING` build target) and now see leftover topics under `otthing/<chipid>/*` on their broker.

## The Symptom

After flashing OTGW32 firmware on a device that previously ran OTTHING firmware, the MQTT broker tree shows two top-level branches that look like the device is publishing twice:

```
homeassistant/
otthing/
  16C6BC/
    chSetTemp1
    chMode1
    outsideTemp
    roomComp1
    status
    set
OTGW/
  value/
    otgw-1020BA16C6BC ...
```

The `otthing/<chipid>/*` entries look live — payloads are present and they sit alongside the new `OTGW/...` topics — so it appears the device is sending duplicate, outdated data on a deprecated prefix.

## The Diagnosis

The OTGW32 firmware **does not** publish anything under `otthing/`.

- The default top-level topic is `OTGW` (`src/OTGW-firmware/MQTTstuff.h:51`, `sTopTopic = "OTGW"`).
- The default unique-ID prefix is `otgw-` (`src/OTGW-firmware/MQTTstuff.h:55`).
- A grep over `src/OTGW-firmware/` confirms there is no code path in OTGW32 firmware that emits `otthing/...`.

What you are seeing on the broker is **retained MQTT messages left behind by the previous OTTHING firmware run**. Retained messages are stored by the broker indefinitely until either:

- a publish with an empty payload (and the retain flag set) overwrites them, or
- the broker is configured to drop retained state (mosquitto, by default, never does this).

This is broker-side state, not firmware-side state. Reflashing OTGW32 again does not clear it. Power-cycling the device does not clear it. The device with new firmware will simply never publish on those topics again, but the retained payloads remain visible to every subscriber until something explicitly removes them.

## How to Clean It Up

Pick whichever path matches your tooling. All three publish empty retained payloads to each `otthing/...` topic, which is the MQTT-standard way to delete a retained message.

### Option 1 — `mosquitto_pub` / `mosquitto_sub` (shell)

If the broker is mosquitto and you have `mosquitto_pub` / `mosquitto_sub` installed:

```bash
for t in $(mosquitto_sub -h <broker> -t 'otthing/#' -v -W 2 | awk '{print $1}' | sort -u); do
  mosquitto_pub -h <broker> -t "$t" -r -n
done
```

What each part does:

- `mosquitto_sub -t 'otthing/#' -v -W 2` subscribes to every topic under `otthing/`, prints `topic payload` lines for each retained message, and exits 2 seconds after the last message arrives. The `-W 2` is the timeout that lets the loop terminate; without it `mosquitto_sub` would block forever.
- `awk '{print $1}' | sort -u` extracts just the topic names and dedupes them.
- `mosquitto_pub -t "$t" -r -n` publishes an empty (`-n`) retained (`-r`) message to each topic, which tells the broker to drop the retained value.

Add `-u <user> -P <pass>` to both `mosquitto_pub` and `mosquitto_sub` if your broker requires authentication.

### Option 2 — MQTT Explorer (cross-platform GUI, recommended for non-technical testers)

[MQTT Explorer](https://mqtt-explorer.com/) is the simplest path:

1. Connect to your broker.
2. Find the `otthing` node in the topic tree on the left.
3. Right-click it and choose **Delete topic** (or the equivalent — the menu wording varies by version).

MQTT Explorer walks the subtree and publishes empty retained messages to every leaf for you. The whole `otthing/` branch disappears once it is done.

### Option 3 — Home Assistant MQTT integration

If Home Assistant is connected to your broker:

1. Open **Settings → Devices & Services → MQTT → Configure**.
2. Use the **Publish a packet** form.
3. For each `otthing/<chipid>/<leaf>` topic, publish an empty payload with **Retain** enabled.

This is the slowest option because it is one topic at a time, but it works without any extra tooling and is useful if you can identify the broker via HA but cannot reach it directly.

## Why the Firmware Does Not Do This For You

This guide is the recommended path because the cleanup is intentionally not handled by the firmware.

- The cleanup is a one-shot per device — once the broker is clean, it stays clean.
- Broker-side tooling (MQTT Explorer, mosquitto CLI, HA) already does this job well, without needing the device.
- Adding a firmware block that subscribes to a deprecated topic prefix at every boot is more permanent surface area than a one-time housekeeping issue warrants. Maintainer decision (TASK-570, 2026-05-07).

If you are coming from OTTHING firmware and run into anything else that looks like duplicate state, please flag it on Discord — but for the `otthing/*` retained topics specifically, the cleanup recipe above is the supported answer.

## Reference

- `src/OTGW-firmware/MQTTstuff.h:51` — current top-level topic prefix default (`sTopTopic = "OTGW"`).
- `src/OTGW-firmware/OTGW-firmware.h:517` — `BOARD_SEEGEL_OTTHING` build target, acknowledging the OTTHING platform port history.
- TASK-570 — backlog entry for this guide.

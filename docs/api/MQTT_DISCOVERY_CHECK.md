# MQTT HA Discovery Regression Check

Baseline for verifying the streaming Home Assistant MQTT auto-discovery pipeline
(ADR-077) after any change that touches `MQTTstuff.ino`, `mqtt_configuratie.cpp`,
or the OpenTherm message dispatch. Without this check, silent breakage of HA
integration is the most expensive failure mode in the project: users only notice
when entities stop updating in Home Assistant, and the regression may be weeks old
by then.

## Why this document exists

The streaming discovery rewrite landed in 2026-04 (commits around `02b77dc7` and
`6a10c36d`), superseding the earlier heap-pressure work. TASK-275 (heap
stability) was closed as "superseded by streaming rewrite" rather than verified,
so the original acceptance criteria never produced a clean baseline. TASK-311
(TEST-M4) fills that gap by documenting a repeatable check.

## Prerequisites

- A running OTGW firmware (ESP8266 or ESP32) with MQTT enabled and pointed at a
  broker you can subscribe to.
- `mosquitto_sub` or an equivalent client on the same LAN.
- Default settings: `MQTThaprefix=homeassistant`, `MQTTtopTopic=OTGW`.
- An active OpenTherm bus (or simulator) so the device actually publishes values.

## Baseline procedure

1. Flash a clean build of the branch under test.
2. Let the device boot and connect to MQTT. Wait ~30 seconds for the initial
   streaming discovery burst to drain.
3. Subscribe from a workstation:

   ```bash
   mosquitto_sub -h <broker-host> -t 'homeassistant/#' -v \
       | tee /tmp/otgw-discovery-$(date +%s).log
   ```

4. Trigger a device restart (`POST /api/v2/restart` or power cycle) so the drip
   pipeline republishes every discovery topic. Let it run for 2 minutes.
5. Stop the subscribe. Count topics and extract msgids.

## Expected invariants

These must hold on any branch the user considers shippable.

### Topic count

The total number of `homeassistant/` topics published during the 2-minute window
should be within ±10% of the previous known-good baseline. Record the number in
the release notes.

### Core message IDs present

Every one of the following OpenTherm message IDs MUST appear in the discovery
stream when the relevant feature is enabled. These are the msgids home users
typically bind to HA entities, and their absence silently breaks dashboards.

| MsgID | OT name                       | HA discovery topic fragment contains |
|------:|-------------------------------|--------------------------------------|
|     0 | Master/Slave status           | `msg_0` or `master_status` / `slave_status` |
|     1 | Control setpoint              | `msg_1` or `control_setpoint` |
|     5 | Application-specific fault    | `msg_5` |
|    16 | Room setpoint                 | `msg_16` or `room_setpoint` |
|    17 | Relative modulation level     | `msg_17` or `rel_mod_level` |
|    25 | Boiler water temperature      | `msg_25` or `boiler_water_temp` |
|    26 | DHW temperature               | `msg_26` |
|    28 | Return water temperature      | `msg_28` |
|    56 | DHW setpoint                  | `msg_56` |
|   126 | Master version                | `msg_126` |

A missing msgid here is a release blocker.

### SAT device + switches + selects

When SAT is enabled, discovery MUST also emit:

- `homeassistant/switch/<nodeId>_sat_enabled/config`
- `homeassistant/switch/<nodeId>_sat_simulation/config`
- `homeassistant/select/<nodeId>_sat_preset/config`
- `homeassistant/select/<nodeId>_sat_heating_mode/config`
- `homeassistant/select/<nodeId>_sat_control_mode/config`

These landed via commit `5384be2a` and are regression targets for the streaming
rewrite.

### Device info

Every entity must share the same `device.identifiers` list. Spot-check by piping
through `jq`:

```bash
grep -E 'homeassistant/.*/config' /tmp/otgw-discovery-*.log \
    | cut -d' ' -f2- \
    | jq -r '.device.identifiers[]' \
    | sort -u
```

You should see a single identifier matching `settings.mqtt.sUniqueid`.

## Pre-commit check

Before landing any change that touches the MQTT discovery path:

1. Capture the pre-change baseline using the procedure above.
2. Flash the change branch.
3. Capture the post-change log.
4. `diff` the sorted topic lists. Any disappearing topic is a regression.

## Follow-ups

- Automated regression: pair this manual procedure with a Python script that
  subscribes via `paho-mqtt`, asserts the invariants, and exits non-zero on
  mismatch. Tracked separately.
- Hardware-in-the-loop CI: out of scope for this document.

## References

- ADR-077 "Streaming MQTT HA discovery architecture" (`docs/adr/ADR-077-*.md`)
- Task TASK-275 "HA discovery heap stability" (closed as superseded)
- Task TASK-283 "HA discovery bootloop" (closed as resolved by streaming rework)
- `MQTTstuff.ino` `loopMQTTDiscovery`, `doAutoConfigureMsgid`
- `mqtt_configuratie.cpp` streaming compose functions

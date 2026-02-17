# ADR-040 Source-Specific MQTT Test Matrix

## Scope

Validate that ADR-040 works end-to-end with:

- Full WRITE/RW coverage across affected value encodings.
- Correct source-specific Home Assistant discovery behavior.
- Correct cache reset behavior on MQTT/HA restart cycles.
- Correct Web UI behavior for `mqttseparatesources`.

## Preconditions

- Firmware built from `copilot/separate-thermostat-boiler-values`.
- MQTT enabled and connected.
- Home Assistant discovery enabled (`mqtthaprefix` configured).
- `mqttseparatesources=true` unless the case explicitly says otherwise.
- Retained HA discovery topics cleared once before running full matrix.

## Test Cases

| ID | Area | Scenario | Expected Result |
| --- | --- | --- | --- |
| M01 | Toggle | Set `mqttseparatesources=false` in Web UI and save | Setting persists after refresh/reboot; no `*_thermostat/_boiler/_gateway` publishes |
| M02 | Toggle | Set `mqttseparatesources=true` in Web UI and save | Setting persists after refresh/reboot; source-specific publishes resume |
| M03 | UI | Open settings page | `mqttseparatesources` row shows one-line explanatory hint |
| P01 | f8.8 | ID 1 (`TSet`) with T and B traffic | Legacy topic updates; source-specific `TSet_thermostat` and `TSet_boiler` both publish |
| P02 | u16 | ID 116 (`BurnerStarts`) write/reset path | Legacy topic updates; source-specific suffix topic publishes for source message |
| P03 | s16 | ID 33 (`Texhaust`) read path | Legacy topic updates only (read-only: no source-specific topic) |
| P04 | s8/s8 | ID 48 boundary payload | Legacy `_value_hb/_value_lb` updates; source topics also publish when message is WRITE/RW |
| P05 | u8/u8 | ID 38 (`RelativeHumidity`) WRITE/RW | Legacy `_hb_u8/_lb_u8` updates; source-specific `_thermostat/_boiler/_gateway` topics publish |
| P06 | date | ID 21 (`Date`) RW | Legacy `_month/_day_of_month` updates; source-specific variants publish |
| P07 | daytime | ID 20 (`DayTime`) RW | Legacy `_dayofweek/_hour/_minutes` updates; source-specific variants publish |
| P08 | command | ID 4 (`Command`) RW | Legacy `_hb_u8/_lb_u8/_remote_command` updates; source-specific variants publish |
| P09 | custom write | ID 2 (`MasterConfigMemberIDcode`) WRITE | Legacy `master_configuration*` topics update; source-specific variants publish |
| P10 | RW coverage | IDs 56-63 and 96-99 sampled | Source-specific topics publish for sampled IDs |
| D01 | Discovery dedupe | First publish for an ID with two subtopics (`_hb` and `_lb`) | HA discovery messages are sent for both source-specific sensors (no dropped second sensor) |
| D02 | Discovery dedupe | Repeat same message stream | No repeated discovery flood for already-discovered source-specific sensors |
| D03 | Discovery reset | Trigger `startMQTT()` by HA offline->online cycle | Source-specific discovery cache resets and re-announces needed source sensors |
| D04 | Discovery reset | Manual MQTT reconnect | Source-specific discovery can be re-sent after reconnect |
| N01 | Negative source | Parity error / unknown source type | No source-specific topic publish |
| N02 | Overflow guards | Long topic/sensor name edge path | Graceful error log, no crash/reset |

## WRITE/RW Coverage Checklist

Use this checklist to mark that each WRITE/RW family has at least one explicit runtime validation:

- [ ] `print_f88` IDs validated
- [ ] `print_u16` IDs validated
- [ ] `print_s8s8` IDs validated where applicable
- [ ] `print_u8u8` IDs validated
- [ ] `print_date` validated
- [ ] `print_daytime` validated
- [ ] `print_command` validated
- [ ] `print_mastermemberid` validated

## MQTT/HA Verification Notes

- Verify discovery topics:
  - `homeassistant/sensor/<node_id>_<sensor>_<source>/config`
- Verify state topics:
  - `<topTopic>/value/<node_id>/<sensor>_<source>`
- Confirm no regression on legacy topics:
  - `<topTopic>/value/<node_id>/<sensor>`

## Pass Criteria

- All `M*`, `P*`, `D*`, and `N*` cases pass.
- No watchdog resets or heap-related crash behavior during test run.
- No missing source-specific discovery entities for multi-subtopic sensors.
- Legacy MQTT topic behavior remains unchanged.

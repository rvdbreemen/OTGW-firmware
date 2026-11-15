# Bench job: cloud recorder PoC (TASK-1963, TASK-1971, TASK-1972, TASK-1974, TASK-1979, TASK-1473, TASK-1991)

For the bench-tester (`otgw-bench-tester`), through the coordinator. Branch `research/cloud-recording`, never on
`dev`. Build env `esp32-combo-tlsdyn` only; the developer has built it, the bench-tester flashes it (OTA first) and
records the evidence in the tasks named per check.

## What to flash

- Build: `./build.sh --target esp32-combo-tlsdyn` on the branch head; artifact
  `build/OTGW-firmware-esp32-combo-tlsdyn-2.0.0-poc.<N>+<githash>.ino.bin`. Check that the unit reports that githash.
- Keep the `.ino.bin` of that build: `decode_all.py` needs it to read the debug section.
- Units: one board **without PSRAM** (OTGW32) and one **with PSRAM** (LOLIN S3 Mini). Save `/api/v2/settings` before
  any filesystem write.

## Collection point for the bench

The firmware checks the server certificate against GTS Root R1/R4 and ISRG Root X1/X2, so it needs a hostname with
a Cloudflare certificate. Without the Synology, use a quick tunnel on the bench laptop:

```bash
cd tools/cloud-recorder/pocketbase
docker compose up -d pocketbase            # or: docker run with pb_migrations and pb_hooks mounted, port 8090
docker exec <pocketbase-container> /pb/pocketbase superuser upsert <admin-email> '<admin-password>'
cloudflared tunnel --url http://localhost:8090   # prints https://<random>.trycloudflare.com
python3 test_client.py https://<random>.trycloudflare.com <admin-email> '<admin-password>'   # 1-9, 11 PASS
```

On each unit, upload `/cloudrec.cfg` (FSexplorer):

```text
enabled=1
host=<random>.trycloudflare.com
port=443
```

Restart the unit. Keep a telnet capture (port 23) running for every check.

## Checks

| # | Check | Pass | Task / AC |
|---|---|---|---|
| 1 | Start-up | Telnet shows `cloudrec: on, <host>:443 as otgw-<mac>, 1 send slot(s)` (no PSRAM) or `2 send slot(s)` (PSRAM) | 1974 AC 3 |
| 2 | Enrollment | Telnet `cloudrec: enrolled as otgw-<mac>`; admin UI `devices` has that uid with label, fw (`2.0.0-poc.<N>…`), board | 1971 AC 3 |
| 3 | Uploads | A `cloudrec: seq … HTTP 200` line about every 60 s; `batches` fills; most records `fmt` 5 | 1963 AC 2, 1972 AC 2 |
| 4 | Lossless on real data | `decode_all.py <base> <admin> <pw> otgw-<mac> out/ --image <the .ino.bin>`; `out/debug.log` byte-identical to the telnet capture for the same period (cut both to the first and last decoded line); `out/otframes.log` frames equal the unit's own OT log for that period; no `CRC` error | 1972 AC 2 |
| 5 | Compression on real data | Per batch: raw length (LEB128 at the start of a fmt 5 payload) against the sent bytes; report the median and the total | 1972 AC 2 |
| 6 | TLS resumption | The upload line shows `TLS resumed <n> full <m>`: after the first upload `full` stays at 1 (or rises only after a pause) and `resumed` climbs | 1974 AC 2 |
| 7 | TLS heap, no PSRAM | From the upload lines and `/api/v2/device/info`: `TLS low <heap>/<block>` and the minimum free heap, with BLE **off** and with BLE **on**; no reboot, no `CRITICAL` heap tier | 1963 AC 2-3 |
| 8 | SATTICK with SAT debug off | SAT enabled, SAT debug off: `out/sat.log` has ticks, telnet shows none | 1974 AC 1 |
| 9 | SATTICK with SAT debug on | Telnet shows SATTICK lines; `out/sat.log` has each tick once (no doubles) | 1974 AC 1 |
| 10 | Service down | Stop the PocketBase container: telnet `service unreachable; capture paused, next attempt in 5 min` (5 to 6). Keep it down for two attempts: the second says 10 to 12 min. Start it again: the queued record goes first (seq order in `batches`), then the 60 s rhythm returns; a later record may carry a `lost on the gateway` line in `decode_all.py` output | 1971 AC 3 |
| 11 | Own network down | Switch the unit's WiFi AP off for 2 minutes: capture pauses without a backoff step, uploads resume within a minute after the AP is back | 1971 AC 3 |
| 12 | Blocked | Admin UI: `blocked` on for the unit. Within about a minute telnet shows `… is blocked by the collection point; telemetry opt-in turned off`; `/cloudrec.cfg` says `enabled=0`; the PocketBase log shows no further requests from the unit, also after a restart (`cloudrec: off`) | 1972 AC 2 |
| 13 | Soak, no PSRAM | 12 h with OT traffic: no reboot, heap stable, count of `lost` notes in `decode_all.py` output | 1963 AC 3 |
| 14 | Telemetry mode | Admin UI: `mode` = `custom`, `flags` = `sat`, `mode_until` = now + 20 min. Within 15 min telnet shows `cloudrec: telemetry mode 3, log flags 0x010 (until mode_until)` and SAT debug lines appear; after `mode_until` (within a second) `log flags 0x000` and the SAT lines stop. Repeat with `full` while the user has `mqtt` debug on locally: at the end `mqtt` stays on, the rest goes off. `otgwsim`/`sensorsim` never change | 1979 AC 3 |
| 15 | Events | During checks 1-14: `out/events.log` has `mqtt_pub`, `mqtt_in` (send one command), `pic_cmd`/`response` (PIC unit), `rest` + `rest_gets` (open the web UI), `setting` (change one, secrets show as `*****`), `dallas` and `s0` where fitted, `net` (check 11), `fw` (upload a file in FSexplorer), `telnet_key` (press a key), `health` once a minute, `bridge_in` (send a line on port 25238; with SAT on, a `CS=` shows as refused) | 1473 |
| 16 | Local recording | Cloud off (`enabled=0`): web UI Advanced > Debug > Start; after 10 min Download; `decode_all.py --file <download> out/ --image <.ino.bin>` gives the four logs; with the cloud on, stop the PocketBase container: the local recording goes on (no gap in the download) while the cloud backs off; Stop and Start again; restart the unit: it resumes | 1991 |

## Report

Per check: PASS or FAIL with the telnet lines, the `test_client.py` output, the decode counts, and for check 7 the
heap numbers with BLE off and on. Attach the telnet capture and `out/` of check 4. Tick the ACs named above.

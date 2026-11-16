# Cloud recorder PoC: go/no-go bench test, step by step (Windows laptop)

The four checks that decide whether the PoC holds (full list: `cloudrec-bench-job.md`). Order: 1 ELF, 2 TLS without
PSRAM, 3 uploads and lossless on real data, 4 soak. Stop at the first FAIL and report it.

You need: the laptop with the repo, Docker Desktop running, Python 3, `cloudflared` (`winget install
Cloudflare.cloudflared`), an OTGW32 (no PSRAM) on WiFi, and the poc.8 build from `RvdB\OTGW-build-archive\cloudrec-poc8\`
(`.ino.bin` and `.elf`). Below, `<ip>` is the unit's address and `<archive>` that folder.

## 0. The repo on the research branch

```bat
cd <repo>
git fetch origin research/cloud-recording
git checkout research/cloud-recording
git pull --ff-only
```

## 1. ELF check (1 minute): are the dynamic TLS buffers in the build?

```bat
"%USERPROFILE%\.pio-tlsdyn\packages\toolchain-xtensa-esp-elf\bin\xtensa-esp-elf-nm.exe" "<archive>\OTGW-firmware-esp32-combo-tlsdyn-2.0.0-poc.8+5f760b4.elf" | findstr /C:"esp_mbedtls_free_buf" /C:"esp_tls_get_client_session"
```

PASS: two lines, each with ` T `. FAIL (nothing found): the build has the stock mbedTLS; rebuild before going on.

## 2. Collection point on the laptop (10 minutes)

```bat
cd tools\cloud-recorder\pocketbase
docker build -t otgw-pb .
mkdir pb_data
docker run -d --name otgwpb -p 8090:8090 -v "%cd%\pb_migrations:/pb/pb_migrations" -v "%cd%\pb_hooks:/pb/pb_hooks" -v "%cd%\pb_data:/pb/pb_data" otgw-pb
docker exec otgwpb /pb/pocketbase superuser upsert admin@bench.invalid "BenchAdmin-123"
```

In a second window, the tunnel (it gives a Cloudflare certificate, which the firmware checks):

```bat
cloudflared tunnel --url http://localhost:8090
```

Note the `https://<random>.trycloudflare.com` address it prints. Test the server through it:

```bat
python test_client.py https://<random>.trycloudflare.com admin@bench.invalid BenchAdmin-123
```

PASS: checks 1 to 10 and 12. Check 11 fails with a quick tunnel (it has no path rule); that is expected here.

## 3. The unit (15 minutes)

Settings backup, then flash the app only (no filesystem write, the settings stay):

```bat
curl -o settings-backup.json http://<ip>/api/v2/settings
python scripts\ota_flash.py --host <ip> --app "<archive>\OTGW-firmware-esp32-combo-tlsdyn-2.0.0-poc.8+5f760b4.ino.bin" --no-fs
```

Make the opt-in file `cloudrec.cfg` with three lines:

```text
enabled=1
host=<random>.trycloudflare.com
port=443
```

Upload it in FSexplorer (`http://<ip>/FSexplorer`, Upload, to `/`). Start a telnet capture before the restart, so the
whole start is in it (PuTTY: Session > Logging > "All session output" to `telnet-capture.txt`, host `<ip>` port 23,
connection type Telnet). Then restart the unit: open `http://<ip>/ReBoot` in the browser (admin login), or power-cycle it.

## 4. Check 7: TLS without PSRAM, BLE off (30 minutes)

In the telnet capture, within a few minutes:

- `cloudrec: on, <random>.trycloudflare.com:443 as otgw-<mac>, 1 send slot(s)`
- `cloudrec: enrolled as otgw-<mac>` (first time only), then `cloudrec: logging as otgw-<mac>`
- about once a minute: `cloudrec: seq ... B (... B sent) HTTP 200 in ... ms; TLS resumed <n> full <m>, low <heap>/<block>, min <minheap>`

PASS: `HTTP 200` lines keep coming for 30 minutes, no reboot, `full` stays at 1 (or a few) while `resumed` climbs, and
`min` stays above 5,120. Note the lowest `low` and `min` values.

Then BLE on (web UI, SAT > BLE thermometer, as you normally have it), restart, another 30 minutes, same notes.
PASS with BLE on is a bonus: with BLE the no-PSRAM heap is known to be tight (ADR-193).

## 5. Check 3 and 4: uploads and lossless on real data (15 minutes)

Leave telnet running. After at least 10 minutes:

```bat
cd tools\cloud-recorder
python decode_all.py http://127.0.0.1:8090 admin@bench.invalid BenchAdmin-123 otgw-<mac> out --image "<archive>\OTGW-firmware-esp32-combo-tlsdyn-2.0.0-poc.8+5f760b4.ino.bin"
python compare_telnet.py out\debug.log <path>\telnet-capture.txt
```

PASS: `decode_all.py` prints records, OT frames and events with no CRC error; `compare_telnet.py` ends with
`in the capture but missing or different in the recording: 0` (lines "only in the recording" are lines telnet
dropped, not recorder faults). Also look at `out\events.log`: mqtt_pub, health and rest lines should be there.

## 6. Check 13: soak (12 hours, unattended)

Leave everything running overnight (laptop awake, Docker and cloudflared on, telnet capture on). In the morning:

```bat
python decode_all.py http://127.0.0.1:8090 admin@bench.invalid BenchAdmin-123 otgw-<mac> out-soak --image "<archive>\...ino.bin"
findstr /C:"lost on the gateway" out-soak\events.log out-soak\debug.log
curl http://<ip>/api/v2/device/info
```

PASS: no reboot in the night (uptime about 12 h, reboot count unchanged), no CRC error, the `gaps` count 0 or
explained (an outage of the quick tunnel), and the `lost` notes few. Note the lowest `min` from the upload lines.

## Report

Send the PASS/FAIL per step with: the two ELF lines, the `test_client.py` result, the lowest `low`/`min` with BLE off
and on, the `decode_all.py` and `compare_telnet.py` output, and the morning `device/info`. Afterwards restore with
`enabled=0` in `cloudrec.cfg`, or flash the normal build back.

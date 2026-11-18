# OTGW collection point: PocketBase on a Synology behind a Cloudflare Tunnel

TASK-1938. This is the server side only. The proof-of-concept firmware that sends to it is TASK-1963 (branch
`research/cloud-recording`); TASK-1473 (the feature) is on hold.
The design and the cost comparison are in TASK-1862 and `docs/diagnostics/ot-stream-encoding-study.md`.

```text
gateway --HTTPS--> Cloudflare edge --tunnel (outbound from the NAS)--> cloudflared --> PocketBase :8090
                                                                     LAN only: http://<nas>:8090/_/ (admin)
```

- **No open port on the router.** `cloudflared` connects outward to Cloudflare. Through the tunnel, only the
  login and batch-upload paths reach PocketBase; everything else, the admin UI included, gets a 404.
- **The register of gateways:** `devices` holds one record per gateway. A gateway enrolls itself on first
  contact with its unique id (`uid`, `otgw-` + its MAC in hex) and a secret it generated, and logs in with
  those; a second enrollment of the same uid is refused. A new gateway may log by default. To stop one, switch
  on **blocked** on its record in the admin UI: from then on every request of that gateway (login, token
  refresh, its own record update, uploads, also with an older token) gets HTTP 403 with
  `Gateway blocked (otgw-blocked): telemetry opt-in off` (`pb_hooks/blocked.pb.js`). The gateway turns its own
  telemetry opt-in off at its next request (within about a minute) and makes no further connection. Its `label`, `fw`, `board` and `updated`
  (last login) show which gateways log.
- **Telemetry mode per gateway (TASK-1979):** a new gateway always starts in the default mode. To log more for a
  while, set on its record: `mode` = `full` (every log flag but `sattrace`), `full+trace` (all ten) or `custom`
  (the ticked `flags`); `mode_until` = when it goes back to default by itself (recommended, so it is never
  forgotten); `mode_note` = why (issue or Discord link, for you only). The gateway picks up a change within 15
  minutes (token refresh) and an expiry within a second; the extra debug lines also show on its telnet. It can set
  none of these fields itself, and the two simulation switches can never be set this way.
- **What it stores:** `batches` holds the encoded chunks. A device can create batches for itself and nothing else: no reading,
  no changing, no deleting. A resend of the same `stream` + `seq` is refused (unique index), so a gateway can
  safely send a batch again after a lost reply.
- **Retention:** every hour at minute 17, all batches older than 30 days are deleted, for every gateway. This
  is fixed, not a setting. Request logs (they hold IP addresses) are kept 7 days.
- **Who can read the data on the way:** TLS ends at the Cloudflare edge, so Cloudflare can read the payload.
  Accepted by the maintainer on 2026-11-15 (no device-side encryption).
- **Cost:** none beyond the NAS and the domain you already have. Cloudflare Tunnel is available on the free plan.

## Files

| File | Purpose |
|---|---|
| `compose.yaml` | Container Manager project: `pocketbase` (built locally) and `cloudflared` |
| `Dockerfile` | Alpine + the PocketBase release binary, checked against the release's `checksums.txt` |
| `pb_migrations/1763100000_otgw_recorder.js` | Creates `devices` and `batches`, the rules, the indexes, the trusted proxy header and the rate limits |
| `pb_migrations/1763100001_batch_build_id.js` | Stores the exact firmware build ID on new batches and preserves older batches |
| `pb_hooks/retention.pb.js` | Hourly retention cron (30 days, fixed) |
| `pb_hooks/blocked.pb.js` | 403 `otgw-blocked` for every request of a blocked gateway (required: without it a blocked gateway is not refused) |
| `test_client.py` | Acts as gateways and checks the rules, blocking and retention (Python 3, standard library only) |

## What you need

- A Synology with DSM 7.2 or later and **Container Manager** installed (Package Center).
  Check the CPU: Intel/AMD models use `PB_ARCH: amd64`, ARMv8 models use `arm64` (set it in `compose.yaml`).
- Your domain on Cloudflare DNS.
- A free Cloudflare Zero Trust organisation. Choose the **Free** plan. If the dashboard asks for a payment
  method, stop here and report it: the "never pay" rule (TASK-1862) means we pick another route then.

## Steps

### 1. Create the tunnel (Cloudflare dashboard)

1. Zero Trust > **Networks > Tunnels** > **Create a tunnel** > type **Cloudflared**, name `otgw-telemetry`
   (any name works; Cloudflare assigns the tunnel ID, a UUID).
2. Under "Install and run a connector", choose **Docker** and copy only the token, the long string after
   `--token`. Do not run the command it shows; the compose project runs the connector.
3. **Public Hostname** > Add:
   - Subdomain: `telemetry`, Domain: your domain (gives for example `telemetry.<your-domain>`)
   - Path: `^/api/(collections/(devices/(auth-with-password|auth-refresh|records(/[a-z0-9]{15})?)|batches/records)|health)$`
   - Service: type `HTTP`, URL `pocketbase:8090`
4. Save. Every other path on that hostname falls through to the tunnel's catch-all and gets a 404.

### 2. Put the files on the NAS

1. In File Station create `/docker/otgw-recorder/`, and in it the folders `pb_data`, `pb_hooks` and `pb_migrations`.
2. Copy `compose.yaml`, `Dockerfile`, both files in `pb_hooks/` and **every** file in `pb_migrations/`
   (`1763100000_otgw_recorder.js` and `1763100001_batch_build_id.js`) into the matching places.
3. Create `/docker/otgw-recorder/.env` with one line:

   ```text
   TUNNEL_TOKEN=<the token from step 1>
   ```

### 3. Start the project (Container Manager)

1. **Project** > **Create** > name `otgw-recorder`, path `/docker/otgw-recorder`, source "Use existing
   docker-compose.yml" (it picks up `compose.yaml`). Build and start.
2. Open the `pocketbase` container log. On the first start it applies the migration and prints a link of
   the form `http://0.0.0.0:8090/_/#/pbinstall/...`. Open that link with the NAS's LAN address instead of
   `0.0.0.0` and create the superuser (your admin account).
3. Without a `.env` yet, stop the `cloudflared` container (it restarts in a loop without a token) and run the
   LAN test of step 4 first. Then add `.env` and build the project again.
4. In Zero Trust > Tunnels, the tunnel should show **Healthy** with one replica on the pinned cloudflared version.
   If the replica reports an older version than `compose.yaml`, see "Pitfalls" below.

### 4. Test from your laptop

No device records to create: gateways enroll themselves. The test enrolls two made-up gateways.

```bash
python3 tools/cloud-recorder/pocketbase/test_client.py https://rec.<your-domain> <admin-email> '<admin-password>'
```

Run it twice. Against `http://<nas-ip>:8090` (LAN): checks 1 to 9 and 12 PASS, 11 FAILs by design (the admin UI is
only closed by the tunnel's path rule). Against `https://telemetry.<your-domain>`: checks 1 to 8 and 11 PASS, 9 and 12
FAIL by design (they log in as superuser, and that path gives 404 through the tunnel). Together every check passes
where it applies. Put the output in TASK-1938 (AC 2), then delete the test gateways in the admin UI.

### 5. Check the retention (AC 3)

The cron runs every hour at minute 17. Do not run `--db` against `data.db` over an SMB share while PocketBase has it
open: SQLite locking over SMB is not reliable. Test inside the container instead (Container Manager > Container >
`pocketbase` > Terminal > `sh`):

```sh
apk add --no-cache sqlite
sqlite3 /pb/pb_data/data.db "UPDATE batches SET created='2000-01-01 00:00:00.000Z' WHERE id=(SELECT id FROM batches ORDER BY created LIMIT 1); SELECT count(*) FROM batches;"
```

Then admin UI > Settings > Crons > `otgwBatchRetention` > Run, and count again: one batch fewer. The `pocketbase`
log shows `otgw retention: deleted 1 batches older than <cutoff>` (cutoff = now minus 30 days). `sqlite` is gone
again after the next build.

### 6. Back-up

Add `/docker/otgw-recorder/pb_data` to Hyper Backup. PocketBase keeps everything in that folder (SQLite).

## Pitfalls found on the first Synology run (2026-11-18)

- **Container Manager keeps its own copy of the YAML.** Editing `compose.yaml` on disk after the project exists is
  not picked up by Build. Change it in the project's YAML editor in Container Manager.
- **Cloudflare Browser Integrity Check refuses `Python-urllib`** with HTTP 403 and `error code: 1010`, at the edge,
  before the tunnel (`/_/` then also gives 403 instead of 404). `test_client.py` therefore sends its own
  `User-Agent`. Measured on `/api/health`: curl, `ESP32HTTPClient` and no User-Agent at all (what the PoC firmware
  sends) all got 200. To keep a stricter heuristic from ever locking gateways out, the zone has a configuration rule
  `set_config bic=false` for `http.host eq "telemetry.<your-domain>"` only (Rules > Configuration Rules).
- **`<nas>.local` can resolve to an IPv6 link-local address** on Windows, where curl and Python hang. Use the NAS's
  IPv4 address for the LAN test.
- **cloudflared versions** older than about a year show as "Unsupported" in the dashboard; keep the pin current.

## Run on Robert's Synology (2026-11-18)

DS with an Intel Celeron J3455 (`amd64`), Container Manager, PocketBase 0.40.6, cloudflared 2026.11.0, tunnel
`otgw-telemetry` with the path rule and a 404 catch-all, `telemetry.vandenbreemen.net` proxied. LAN test 10/11
(11 by design), tunnel test 9/11 (9 and 12 by design), retention deleted the backdated batch. Details and evidence:
TASK-1938.

## Tested locally (2026-11-14)

Built and run in Docker in the development container (with that container's proxy CA added only for the
test build). The `pocketbase_0.40.6_linux_amd64.zip` checksum check passed. The migration applied without errors, and reading the collections back showed
the rules, fields, indexes, the 14-day token, the `CF-Connecting-IP` proxy header and the three rate limits as
written. `test_client.py` against `http://127.0.0.1:8090`: checks 1 to 6 PASS; check 7 fails locally, as it should, because
only the tunnel's path rule blocks the admin UI. Extra checks: a disabled device cannot log in (403), a batch
without a token is refused (400). Retention: a batch backdated to 40 days old was deleted by the cron with
`RETENTION_DAYS=30` while a new one stayed, and `RETENTION_DAYS=0` deleted the rest.

Debug-size batches (after the payload limit went to 65,536 characters, fresh database): 25 batches of 13,301 B paced just under the limit were all accepted; a burst of 40 got 30 accepted and 10 refused with 429; a payload of 65,600 characters was refused (400) and one of exactly 65,536 accepted.

Enrollment, blocking and fixed retention (TASK-1971, 2026-11-15, fresh local PocketBase 0.40.6): `test_client.py`
checks 1 to 10 PASS (11 fails on the LAN, as it should). Against a mutant migration without the two "blocked"
rules, checks 3 and 9 FAIL, so the test sees those rules.

Blocked reply (TASK-1972): check 9 now expects 403 with "otgw-blocked" on login, upload (old token), refresh and
own update; PASS. Without `blocked.pb.js` check 9 FAILs (all 200), so the test sees the hook.

Still untested: the `arm64` build (the first Synology was Intel).

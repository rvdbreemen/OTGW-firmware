# OTGW collection point: PocketBase on a Synology behind a Cloudflare Tunnel

TASK-1938. This is the server side only. The firmware does not send anything yet (TASK-1473 is on hold).
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
  on **blocked** on its record in the admin UI: the gateway sees it at its next login and turns its own
  telemetry opt-in off; batches from a blocked gateway are refused. Its `label`, `fw`, `board` and `updated`
  (last login) show which gateways log.
- **What it stores:** `batches` holds the encoded chunks. A device can create batches for itself and nothing else: no reading,
  no changing, no deleting. A resend of the same `stream` + `seq` is refused (unique index), so a gateway can
  safely send a batch again after a lost reply.
- **Retention:** every hour at minute 17, all batches older than 30 days are deleted, for every gateway. This
  is fixed, not a setting. Request logs (they hold IP addresses) are kept 7 days.
- **Cost:** none beyond the NAS and the domain you already have. Cloudflare Tunnel is available on the free plan.

## Files

| File | Purpose |
|---|---|
| `compose.yaml` | Container Manager project: `pocketbase` (built locally) and `cloudflared` |
| `Dockerfile` | Alpine + the PocketBase release binary, checked against the release's `checksums.txt` |
| `pb_migrations/1763100000_otgw_recorder.js` | Creates `devices` and `batches`, the rules, the indexes, the trusted proxy header and the rate limits |
| `pb_hooks/retention.pb.js` | Hourly retention cron (30 days, fixed) |
| `test_client.py` | Acts as gateways and checks the rules, blocking and retention (Python 3, standard library only) |

## What you need

- A Synology with DSM 7.2 or later and **Container Manager** installed (Package Center).
  Check the CPU: Intel/AMD models use `PB_ARCH: amd64`, ARMv8 models use `arm64` (set it in `compose.yaml`).
- Your domain on Cloudflare DNS.
- A free Cloudflare Zero Trust organisation. Choose the **Free** plan. If the dashboard asks for a payment
  method, stop here and report it: the "never pay" rule (TASK-1862) means we pick another route then.

## Steps

### 1. Create the tunnel (Cloudflare dashboard)

1. Zero Trust > **Networks > Tunnels** > **Create a tunnel** > type **Cloudflared**, name `otgw-recorder`.
2. Under "Install and run a connector", choose **Docker** and copy only the token, the long string after
   `--token`. Do not run the command it shows; the compose project runs the connector.
3. **Public Hostname** > Add:
   - Subdomain: `rec`, Domain: your domain (gives for example `rec.<your-domain>`)
   - Path: `^/api/(collections/(devices/(auth-with-password|auth-refresh|records(/[a-z0-9]{15})?)|batches/records)|health)$`
   - Service: type `HTTP`, URL `pocketbase:8090`
4. Save. Every other path on that hostname falls through to the tunnel's catch-all and gets a 404.

### 2. Put the files on the NAS

1. In File Station create `/docker/otgw-recorder/`, and in it the folders `pb_data`, `pb_hooks` and `pb_migrations`.
2. Copy `compose.yaml`, `Dockerfile`, `pb_hooks/retention.pb.js` and `pb_migrations/1763100000_otgw_recorder.js`
   into the matching places.
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
3. In Zero Trust > Tunnels, `otgw-recorder` should show **Healthy**.

### 4. Test from your laptop

No device records to create: gateways enroll themselves. The test enrolls two made-up gateways.

```bash
python3 tools/cloud-recorder/pocketbase/test_client.py https://rec.<your-domain> <admin-email> '<admin-password>'
```

Checks 1 to 9 and 11 must say PASS. Check 9 logs in as superuser, which only works from the LAN, so run it once
against `http://<nas>:8090` too. Put the output in TASK-1938 (AC 2), then delete the test gateways in the admin UI.

### 5. Check the retention (AC 3)

The cron runs every hour at minute 17. To test it at once, from the LAN: run the test with
`--db /docker/otgw-recorder/pb_data/data.db` on the NAS (check 10 backdates one batch to 2000 and runs the
cron through `POST /api/crons/otgwBatchRetention`). The `pocketbase` log shows
`otgw retention: deleted N batches older than <cutoff>`.

### 6. Back-up

Add `/docker/otgw-recorder/pb_data` to Hyper Backup. PocketBase keeps everything in that folder (SQLite).

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

Still to test on the Synology: the tunnel (check 7 and the path rule), the `arm64` build if the NAS is ARM,
and Container Manager's handling of `compose.yaml`.

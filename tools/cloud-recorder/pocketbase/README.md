# OTGW collection point: PocketBase on a Synology behind a Cloudflare Tunnel

TASK-1938. This is the server side only. The firmware does not send anything yet (TASK-1473 is on hold).
The design and the cost comparison are in TASK-1862 and `docs/diagnostics/ot-stream-encoding-study.md`.

```text
gateway --HTTPS--> Cloudflare edge --tunnel (outbound from the NAS)--> cloudflared --> PocketBase :8090
                                                                     LAN only: http://<nas>:8090/_/ (admin)
```

- **No open port on the router.** `cloudflared` connects outward to Cloudflare. Through the tunnel, only the
  login and batch-upload paths reach PocketBase; everything else, the admin UI included, gets a 404.
- **What it stores:** `devices` holds one login per gateway, and only enabled devices can log in.
  `batches` holds the encoded chunks. A device can create batches for itself and nothing else: no reading,
  no changing, no deleting. A resend of the same `stream` + `seq` is refused (unique index), so a gateway can
  safely send a batch again after a lost reply.
- **Retention:** every night at 03:17 UTC, batches older than `RETENTION_DAYS` (default 30) are deleted.
- **Cost:** none beyond the NAS and the domain you already have. Cloudflare Tunnel is available on the free plan.

## Files

| File | Purpose |
|---|---|
| `compose.yaml` | Container Manager project: `pocketbase` (built locally) and `cloudflared` |
| `Dockerfile` | Alpine + the PocketBase release binary, checked against the release's `checksums.txt` |
| `pb_migrations/1763100000_otgw_recorder.js` | Creates `devices` and `batches`, the rules, the indexes, the trusted proxy header and the rate limits |
| `pb_hooks/retention.pb.js` | Nightly retention cron |
| `test_client.py` | Acts as one gateway and checks the rules (Python 3, standard library only) |

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
   - Path: `^/api/(collections/(devices/(auth-with-password|auth-refresh)|batches/records)|health)$`
   - Service: type `HTTP`, URL `pocketbase:8090`
4. Save. Every other path on that hostname falls through to the tunnel's catch-all and gets a 404.

### 2. Put the files on the NAS

1. In File Station create `/docker/otgw-recorder/`, and in it the folders `pb_data`, `pb_hooks` and `pb_migrations`.
2. Copy `compose.yaml`, `Dockerfile`, `pb_hooks/retention.pb.js` and `pb_migrations/1763100000_otgw_recorder.js`
   into the matching places.
3. Create `/docker/otgw-recorder/.env` with two lines:

   ```text
   TUNNEL_TOKEN=<the token from step 1>
   RETENTION_DAYS=30
   ```

### 3. Start the project (Container Manager)

1. **Project** > **Create** > name `otgw-recorder`, path `/docker/otgw-recorder`, source "Use existing
   docker-compose.yml" (it picks up `compose.yaml`). Build and start.
2. Open the `pocketbase` container log. On the first start it applies the migration and prints a link of
   the form `http://0.0.0.0:8090/_/#/pbinstall/...`. Open that link with the NAS's LAN address instead of
   `0.0.0.0` and create the superuser (your admin account).
3. In Zero Trust > Tunnels, `otgw-recorder` should show **Healthy**.

### 4. Create a test device

In the admin UI (`http://<nas>:8090/_/`) > collection **devices** > New record:
email `gw-test@devices.invalid`, a long random password, `enabled` on, label `test`.
Every real gateway later gets its own record the same way.

### 5. Test from your laptop

```bash
python3 tools/cloud-recorder/pocketbase/test_client.py https://rec.<your-domain> gw-test@devices.invalid '<password>'
```

All 7 checks must say PASS. Put the output in TASK-1938 (AC 2). Then check in the admin UI that
**batches** has one record from `gw-test`.

### 6. Check the retention (AC 3)

The cron runs every night at 03:17 UTC. To test it at once:

1. Set `RETENTION_DAYS=0` in `.env` and restart the project.
2. Admin UI > **Settings > Crons** > run `otgwBatchRetention` (or `POST /api/crons/otgwBatchRetention`
   with a superuser token, from the LAN).
3. The `pocketbase` log shows `otgw retention: deleted N batches older than <now>` and **batches** is empty.
4. Set `RETENTION_DAYS` back to 30 and restart.

### 7. Back-up

Add `/docker/otgw-recorder/pb_data` to Hyper Backup. PocketBase keeps everything in that folder (SQLite).

## Tested locally (2026-11-14)

Built and run in Docker in the development container (with that container's proxy CA added only for the
test build). The `pocketbase_0.40.6_linux_amd64.zip` checksum check passed. The migration applied without errors, and reading the collections back showed
the rules, fields, indexes, the 14-day token, the `CF-Connecting-IP` proxy header and the three rate limits as
written. `test_client.py` against `http://127.0.0.1:8090`: checks 1 to 6 PASS; check 7 fails locally, as it should, because
only the tunnel's path rule blocks the admin UI. Extra checks: a disabled device cannot log in (403), a batch
without a token is refused (400). Retention: a batch backdated to 40 days old was deleted by the cron with
`RETENTION_DAYS=30` while a new one stayed, and `RETENTION_DAYS=0` deleted the rest.

Still to test on the Synology: the tunnel (check 7 and the path rule), the `arm64` build if the NAS is ARM,
and Container Manager's handling of `compose.yaml`.

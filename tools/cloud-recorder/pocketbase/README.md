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
   the form `http://0.0.0.0:8090/_/#/pbinstal/...`. Open that link with the NAS's LAN address instead of
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

The cron runs at 03:17 UTC. For a quick test, set `RETENTION_DAYS=0` in `.env`, restart the project, wait
until after 03:17 UTC, and confirm that the log line `otgw retention: deleted ...` appears and the test
batch is gone. Then set it back to 30.

### 7. Back-up

Add `/docker/otgw-recorder/pb_data` to Hyper Backup. PocketBase keeps everything in that folder (SQLite).

## Not verified here

This container could not download and run PocketBase, so nothing above has run yet. The migration and hook
follow the PocketBase docs (JS migrations, rate-limit rules, trusted proxy, raw SQL). Three names come from
the reference types rather than a worked example: `authRule`, `authToken` and `@request.auth.collectionName`.
If the migration fails on the first start, the container log names the field; report it in TASK-1938.

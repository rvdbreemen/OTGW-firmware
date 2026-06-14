# ADR-134 OTA Upload Handler on AsyncWebServer onUpload (ADR-123 Phase 3, closes the OTA seam ADR-132 opened)

## Status

Proposed. Date: 2026-06-14.

This is the **OTA half of Phase 3** of the 2.0.0 concurrency-model rollout
(ADR-123), the third and final sub-phase after ADR-132 (HTTP/REST, seq9) and
ADR-133 (WebSocket, seq10). ADR-132 instantiated the single `AsyncWebServer
server(80)` and exposed it `extern` explicitly "for the WebSocket (seq10) and OTA
(seq11) phases to attach to", and it left the firmware-update server
**intentionally dark across the 865.9→865.11 seam** because the old
`OTGWUpdateServer` still bound the synchronous `WebServer` type and there was no
sync server left to attach to. This ADR (delivered by TASK-865.11 / seq11) carries
the OTA surface: the upload handler is rewritten from the synchronous
`UPLOAD_FILE_START/WRITE/END/ABORTED` lifecycle onto the AsyncWebServer
`onUpload(filename,index,data,len,final)` callback, and the `/update` route is
attached once to the shared port-80 server. This **closes the temporary OTA seam**
that ADR-132 flagged.

It **supersedes nothing.** The load-bearing flash logic is carried over unchanged:
the ADR-029 frontend XHR flow (`/update?cmd=0`/`?cmd=100`, blocking-until-flash,
health-check redirect) is preserved, and the ADR-011 hardware-watchdog feed
(I2C `0x26`/`0xA5` per write chunk) is preserved. Only the server-side argument
plumbing and the upload lifecycle dispatch change.

Status is Proposed: it is the maintainer's checkpoint. Field validation on
ESP32-S3 hardware (plain `.ino.bin` and merged-binary firmware OTA, filesystem-image
OTA, the watchdog fed through a multi-MB upload, and the flash-progress WebSocket
still flowing during the async upload) is still open (see Risks).

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-3 OTA move (sync HTTPUpload lifecycle to AsyncWebServer onUpload, /update attached once to the shared port-80 server) delivered by TASK-865.11; closes the OTA seam ADR-132 opened; supersedes nothing
    changed_via: adr-kit

## Context

The 2.0.0 firmware-update server (`OTGWUpdateServer`,
`OTGW-ModUpdateServer-esp32.h`, instantiated as `httpUpdater` in
`networkStuff.ino`) registered its `/update` routes on the **synchronous** Arduino
`WebServer` via `_server->on(path, HTTP_POST, doneHandler, uploadHandler)`. The
upload handler was driven by the synchronous `HTTPUpload` model: a callback that
the loop()-side client pump invoked with `upload.status` cycling through
`UPLOAD_FILE_START` → `UPLOAD_FILE_WRITE` (repeated) → `UPLOAD_FILE_END`, or
`UPLOAD_FILE_ABORTED` on a dropped connection. Each phase read fields off the
`HTTPUpload&` reference (`upload.buf`, `upload.currentSize`, `upload.filename`,
`upload.name`, `upload.totalSize`) and the request arguments off the server
(`_server->arg("size")`, `_server->authenticate(...)`).

ADR-132 retired that synchronous server. It moved HTTP/REST onto a single
`AsyncWebServer server(80)` served on the AsyncTCP task, deleted the per-loop
`handleClient()` drains (including the two flash-background pumps,
`handleEspFlashBackgroundTasks` / `handlePicFlashBackgroundTasks`), and removed the
`OTGWWebServer = WebServer` alias whose last consumer was this OTA server. Between
seq9 (865.9) and this task there was deliberately **no sync server to attach to**,
so `httpUpdater.setup()` was removed and OTA was dark; only the HTTP Basic Auth
credentials were kept current so this re-wire needs no settings work.

ESPAsyncWebServer exposes file uploads through a different callback shape:
`onUpload(AsyncWebServerRequest*, const String &filename, size_t index, uint8_t
*data, size_t len, bool final)`. The two models do not map one-to-one, and three
mismatches must be bridged:

1. **No lifecycle enum.** There is no `upload.status`. `index == 0` is the start of
   the upload; `final == true` is the end. The **final chunk carries both data and
   `final==true`**, so the handler must write the chunk *then* finalize, not treat
   end as a separate empty call. `index` is the **absolute byte offset** of the
   chunk, which is exactly what the merged-binary skip/limit math wants
   (`index + len` is the running total).

2. **No abort callback.** ESPAsyncWebServer has no `UPLOAD_FILE_ABORTED`
   equivalent. A client that disconnects mid-upload is observed via
   `request->onDisconnect()`. That callback also fires on a **normal** close after a
   clean upload, so it cannot blindly run abort cleanup.

3. **The callback delivers the uploaded file's name, not the form field name.** The
   sync server exposed the multipart form-field name via `upload.name`, and the old
   handler keyed filesystem-vs-firmware target detection on `upload.name ==
   "filesystem"`. The async `onUpload` filename is the **uploaded file's** name
   (`firmware.bin` / `littlefs.bin`), not the field name, so the old key is gone.

A fourth, ADR-132-wide mismatch also applies: the sync `WebServer` merged query and
body args in `arg()`, while the async server separates them
(`getParam(name, isPost)`). The OTA reads (`size`, target `cmd`) come off the query
string and must select that source explicitly.

The whole approach rests on the same premise ADR-132 and ADR-133 depend on:
**ESPAsyncWebServer runs every handler (including the `onUpload` callback) on a
single AsyncTCP service task, serialized, never on `loop()` and never
concurrently.** That serialization is what makes the per-request member state on the
`OTGWUpdateServer` instance safe (no two uploads overlap) and what forbids the
callback from blocking (no `yield`/`delay`, keep `Update.write()` and the I2C
watchdog feed short).

## Decision

Rewrite the ESP32 OTA upload handler against **`AsyncWebServer::onUpload`** and
attach `/update` to the shared port-80 `AsyncWebServer server` (ADR-132). The
synchronous `WebServer`/`HTTPUpload` model is gone from
`OTGW-ModUpdateServer-esp32.h`: no `_server->on(...)` with an upload lambda, no
`HTTPUpload&`, no `upload.status` switch, no loop-side client pump. The decision has
five concrete parts.

1. **Async upload dispatcher replaces the lifecycle enum.** `setup()` registers the
   GET form page, the POST completion handler, and the `onUpload` callback via
   `srv.on(path, HTTP_POST, doneHandler, uploadHandler)` on the async server. A
   single `_handleUpload(request, filename, index, data, len, final)` reproduces the
   old START/WRITE/END logic: `index == 0` runs `_handleUploadStart`; every non-empty
   chunk runs `_handleUploadWrite(data, len)`; `final` runs
   `_handleUploadEnd(index + len)`. Writes are gated on `_authenticated &&
   _updaterError[0] == '\0'` so an auth failure or a sticky `Update.begin()` error
   stops the stream early and the completion handler emits the 401 / error page.

2. **Target detection keys off the `cmd` query param, anchored to ADR-029.** Because
   `onUpload` does not deliver the form-field name, filesystem-vs-firmware detection
   reads the `cmd` query parameter the form already posts,
   `request->getParam("cmd", false)` (the `false` selects the query string per
   ADR-132's query-vs-body discipline), and treats `cmd == 100` as the filesystem
   image (`U_SPIFFS`) and otherwise firmware (`U_FLASH`). This is **not a new
   contract**: ADR-029 specifies the frontend posts `/update?cmd=0` (firmware) /
   `?cmd=100` (filesystem), so the server-side detection leans on an existing,
   reliably-present-at-`index==0` signal. The uploaded filename's `.littlefs.bin` /
   `filesystem.bin` suffix is a fallback for a direct `curl` upload without the form.

3. **Idempotent attach-once registration, credentials-only on transitions.** The OTA
   `/update` route is registered **exactly once** (in `setupFSexplorer()`, alongside
   the other `server.on(...)` routes and before `server.begin()`), guarded by a new
   `_routesRegistered` flag, because `AsyncWebServer::on()` *appends* handlers and the
   wiring is re-touched on every WiFi/Ethernet transition. The per-transition calls in
   `startWiFi()` (and the AP-fallback path) no longer re-register the route; they only
   call `updateCredentials()` to keep HTTP Basic Auth current. This is the OTA analog
   of ADR-133's attach-once idempotent `startWebSocket()` (`wsInitialized`) and of
   ADR-044 single-point-of-instantiation: the route moved from per-transition
   `startWiFi()` to once in `setupFSexplorer()`.

4. **Abort via `request->onDisconnect()`, guarded by `_updateFinalized`.** With no
   `UPLOAD_FILE_ABORTED`, `_handleUploadStart` registers
   `request->onDisconnect([...]{ _handleUploadAbort(); })`. Because `onDisconnect`
   fires on a clean close too, `_handleUploadEnd` sets `_updateFinalized = true` and
   `_handleUploadAbort` returns immediately when it is set (and also when
   `state.flash.bESPactive` is false, i.e. never started). A genuinely aborted
   filesystem upload remounts LittleFS (start called `LittleFS.end()`), preserving the
   old abort cleanup.

5. **The completion handler keeps the deferred-reboot contract, on the AsyncTCP
   task.** The POST done-handler funnels its response through the ADR-132 send-once
   bridge (`webBeginRequest` / `webSendP` / `webSend` / `webRequestAuth`), re-checks
   auth independently (the sync model's `_authenticated` handoff from END into the
   done lambda does not map onto the async ordering, the same two-independent-checks
   pattern as FSexplorer's `/upload`), and on success calls `requestDeferredReboot()`
   rather than `doRestart()`. The reboot is **never** fired inside the async callback:
   `doRestart()` tears down MQTT/WS/OTGWstream and must not run on the limited-stack
   AsyncTCP task while the HTTP 200 body may still be draining; `loop()` observes the
   pending flag (`isRebootPending() && !isFlashing()`) and fires
   `performDeferredReboot()`. This mirrors the ESP8266 `OTGW-ModUpdateServer-impl.h`.

**Carried over unchanged (re-plumbed args only):**

- **Merged-binary app-slot extraction** (`_mergedSkipBytes` / `_mergedWriteLimit`):
  skip `MERGED_APP_OFFSET = 0x10000`, write up to `MERGED_APP_SIZE = 0x1E0000`
  (matching `app, ota_0, 0x10000, 0x1E0000` in `partitions_otgw_esp32.csv`). The
  async absolute `index` simplifies the running-offset math.
- **Hardware-watchdog feed** (ADR-011): `Wire.beginTransmission(0x26);
  Wire.write(0xA5); Wire.endTransmission();` still fires **per write chunk**, now on
  the AsyncTCP task (short transaction, safe).
- **Target split:** `filesystem` → `Update.begin(fsSize, U_SPIFFS)` + `LittleFS.end()`
  / remount + `writeSettings`; else `U_FLASH`.
- **`state.flash.bESPactive`** gating (set at start, cleared at end/error/abort) so
  `doBackgroundTasks()` suppresses MQTT/OTGW/NTP during flash.
- **The four `logBootSignature` OTA lifecycle probes** (`pre-begin`, `post-end`,
  `post-remount` FS-only, `pre-reboot`), parity with ESP8266.
- **The ADR-029 frontend** (`updateServerHtml.h` `setIndexPage`/`setSuccessPage`
  PROGMEM pages, served via `webSendP`) is unchanged.

## Alternatives Considered

### Alternative A: Keep the synchronous WebServer OTA handler (the status quo before this task)

Leave `OTGWUpdateServer` bound to `WebServer` and the `HTTPUpload` lifecycle, and
re-introduce a sync server (or a `handleClient()` pump) just for `/update`.
**Rejected.** ADR-132 deliberately removed the synchronous server and its loop pump
for the whole firmware; re-introducing one solely for OTA would re-create a second
serving model, re-add a `handleClient()` drain to the loop (the exact poll Phase 3
exists to delete), and contradict ADR-132's Enforcement gate forbidding
`handleClient(` / `OTGWWebServer` in app files. The OTA endpoint was always scoped as
seq11 to ride the shared async server; finishing that is the planned, low-risk path.

### Alternative B: Stand up a second AsyncWebServer dedicated to OTA

Create a separate `AsyncWebServer` instance (on another port) just for the upload, to
isolate the high-risk flash write from the rest of HTTP. **Rejected.** A second
listener buys no isolation: both servers share the same heap and the same single
AsyncTCP service task, so an OTA upload and a REST request already serialize on that
one task whether they share a server or not. It would add a second port to the
firewall/docs and a second instantiation against ADR-044, for nothing. ADR-132 already
exposed `extern AsyncWebServer server` precisely so OTA attaches to the shared server.

### Alternative C: Buffer the whole upload to a temp file, then flash from loop()

Have `onUpload` stream the bytes into a LittleFS temp file and run `Update.write()`
from `loop()` afterward, keeping all flash work off the AsyncTCP task. **Rejected.**
The S3 LittleFS partition cannot hold a multi-MB firmware image plus the existing
assets, and a filesystem-image OTA would have to overwrite the very filesystem it is
buffering into. It also doubles the write wear and the flash time, and re-introduces a
loop-side pump. `Update.write()` + the short I2C watchdog feed are fast enough to run
directly in the callback; the only genuinely risky operation (the filesystem-image
`LittleFS.end()/.begin()`) is called once at start/end, not per chunk (see Risks).

### Alternative D: Do nothing this phase (leave OTA dark)

Ship Phase 3 with HTTP and the WebSocket async but firmware-over-WiFi permanently
broken. **Rejected.** OTA is the primary field-update path (testers flash alpha builds
over WiFi); leaving it dark past the seam ADR-132 explicitly scoped as temporary would
be a shipped regression versus 1.5.x, not a planned interim state.

## Consequences

**Benefits**

- The OTA seam ADR-132 opened is closed: firmware-over-WiFi works again, now on the
  shared port-80 async server with no synchronous `WebServer` and no `handleClient()`
  pump anywhere in the firmware.
- The upload is serviced on the AsyncTCP task, so a large flash no longer competes
  with OT/MQTT scheduling inside a cooperative loop turn; `state.flash.bESPactive`
  still suppresses background work for the duration.
- Route registration has one source of truth (`_routesRegistered`, registered once in
  `setupFSexplorer()`), eliminating the duplicate-`/update`-handler risk that
  `AsyncWebServer::on()`'s append semantics would otherwise create on every WiFi /
  Ethernet transition.
- The proven flash internals (merged-binary slot extraction, watchdog feed, target
  split, deferred reboot, ADR-029 frontend) are carried over verbatim; the change
  surface is the argument plumbing and the lifecycle dispatch, not the flash logic.

**Trade-offs**

- A new discipline for this handler: it runs on the limited-stack AsyncTCP task and
  must not block. `Update.write()` and the I2C feed are short, but the
  filesystem-image `LittleFS.end()/.begin()` runs from the callback and must be
  confirmed safe there (see Risks).
- The abort path is now a guarded `onDisconnect` rather than an explicit
  `UPLOAD_FILE_ABORTED` phase. The `_updateFinalized` guard is load-bearing: without
  it, the clean-close `onDisconnect` would run abort cleanup (and remount LittleFS)
  after a successful upload.
- Target detection now depends on the `?cmd=` query param (with a filename-suffix
  fallback) instead of the multipart field name. A future change to the frontend form
  action that drops `cmd` would silently route a filesystem image as firmware; the
  dependency on ADR-029's `cmd` contract is now explicit.
- OTA shares the fate of the `ESPAsyncWebServer` / `AsyncTCP` fork (ESP32Async /
  mathieucarbou, "best effort" maintenance per ADR-123) with the rest of the HTTP
  stack.

**Risks and mitigations**

- *Risk*: `LittleFS.end()` / `LittleFS.begin()` on the filesystem-image path runs from
  the limited-stack AsyncTCP callback and either overflows the stack or trips the task
  watchdog. *Mitigation*: it is called once at start and once at end (not per chunk),
  and the task has a 16 KB stack (ADR-132); but this is the riskiest operation and is
  **field-validated** on real ESP32-S3 (filesystem-image OTA succeeds: `U_SPIFFS`,
  remount, `writeSettings` restored, assets serve after reboot, per the TASK-865.11 field AC).
  Open until hardware-confirmed.
- *Risk*: the per-request member state on the `OTGWUpdateServer` instance is corrupted
  by an overlapping upload. *Mitigation*: ESPAsyncWebServer serializes all handlers on
  the single AsyncTCP task (the same premise ADR-132 / ADR-133 depend on), so two
  uploads cannot overlap; documented at the top of the header. If a future library
  change introduced concurrent handlers this ADR must be revisited.
- *Risk*: the watchdog stops being fed mid-upload and the hardware watchdog (I2C 0x26)
  resets the device during a multi-MB flash. *Mitigation*: the `0xA5` feed fires per
  write chunk inside `_handleUploadWrite`, on the same task that receives the data;
  **field-validated** through a multi-MB upload with no mid-flash reboot, and the
  flash-progress WebSocket (`sendWebSocketJSON`, ADR-133) still flowing during the
  async upload (TASK-865.11 field ACs). Open until hardware-confirmed.
- *Risk*: the deferred reboot fires before the HTTP 200 success body drains on a slow
  link, so the browser never sees confirmation. *Mitigation*: `requestDeferredReboot()`
  defers to `loop()` (never `doRestart()` in the callback), giving lwIP time to flush;
  the ADR-029 frontend additionally polls `/api/v2/health` after the 200. Field AC:
  deferred reboot fires after the HTTP 200 drains and the device boots.
- *Risk*: the merged-binary skip/limit extracts the wrong slot and bricks the device.
  *Mitigation*: the absolute `index` makes the offset math direct; `Update.begin()`
  range-checks against `MERGED_APP_SIZE`; **field-validated** with both a plain
  `.ino.bin` and a merged binary (correct slot extracted, device boots).

## Related Decisions

- **ADR-123 (2.0.0 Concurrency Model)**: master decision; this is the OTA half of its
  Phase 3 (seq11).
- **ADR-132 (HTTP stack on ESPAsyncWebServer, Phase 3 web)**: hard dependency and the
  ADR that **opened the OTA seam this ADR closes.** It instantiated the single
  `AsyncWebServer server(80)`, exposed it `extern` "for the … OTA (seq11) phase to
  attach to", and removed `httpUpdater.setup()` in `startWiFi()` while OTA was dark.
  ADR-132's body and Status are left untouched per the immutability rule; this ADR
  records the re-wire. The `webServerCompat.h` send-once bridge (`webBeginRequest`,
  `webSendP`, `webSend`, `webRequestAuth`) is reused verbatim by the OTA done-handler.
- **ADR-133 (WebSocket on AsyncWebSocket, Phase 3 WebSocket)**: sibling sub-phase
  (seq10). This ADR follows the same attach-once-idempotent pattern (its `wsInitialized`
  ↔ this `_routesRegistered`) and the same single-AsyncTCP-task serialization premise.
  The flash-progress WebSocket (`sendWebSocketJSON`) it owns must keep flowing during
  the async upload (field AC).
- **ADR-029 (Simple XHR-Based OTA Flash)**: **preserved and depended upon.** The
  frontend XHR flow, the `/update?cmd=0`/`?cmd=100` contract (now the server-side
  target-detection key), blocking-until-flash, and the `/api/v2/health` redirect are
  unchanged. The browser-side decision is untouched; only the server upload handler
  changes.
- **ADR-011 (External Hardware Watchdog)**: **preserved.** The I2C `0x26`/`0xA5` feed
  per write chunk and the `state.flash.bESPactive` background-suppression are carried
  over onto the AsyncTCP task; not superseded.
- **ADR-044 (single-point-of-instantiation for globals)**: the `/update` route is
  registered once (`_routesRegistered`) on the single shared `server`.
- **ADR-056 (protected admin endpoint security contract)**: preserved through the
  bridge; HTTP Basic Auth via `request->authenticate()` / `webRequestAuth()`, re-checked
  independently in the upload-start and completion handlers.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the constraint change that makes the
  ESP32-centric async OTA the single code path; the ESP8266 sync
  `OTGW-ModUpdateServer-impl.h` is a separate, untouched file.
- **ADR-061 (unified platform abstraction)**: `OTGW-ModUpdateServer-esp32.h` is an
  allowlisted abstraction file, so the platform-specific async includes belong there;
  no new raw `#ifdef ESP32` leaks into application code.

## References

- Task: TASK-865.11 (feat/web: rewrite the ESP32 OTA upload handler against
  AsyncWebServer onUpload), seq11 of the ADR-123 rollout; depends on TASK-865.9.
- `src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h`: `OTGWUpdateServer` rewritten:
  `setup(AsyncWebServer*)`, `srv.on(path, HTTP_POST, doneHandler, uploadHandler)`,
  `_handleUpload(request, filename, index, data, len, final)` dispatcher,
  `_isFilesystemTarget()` (cmd query param + filename-suffix fallback),
  `_parseUploadTotalSize(request)` (query `size` + content-length fallback),
  `request->onDisconnect()` abort guarded by `_updateFinalized`, `_routesRegistered`
  idempotent registration. No `_server->on(...)` upload lambda, no `HTTPUpload`, no
  `upload.status`.
- `src/OTGW-firmware/FSexplorer.ino`: `httpUpdater.setup(&server)` +
  `setIndexPage`/`setSuccessPage` + `updateCredentials` registered once in
  `setupFSexplorer()`, before `server.begin()`.
- `src/OTGW-firmware/networkStuff.ino`: `startWiFi()` (and AP-fallback) keep only
  `updateCredentials("admin", settings.sHTTPpasswd)` on WiFi/Ethernet transitions;
  the OTA re-attach is gone (route is attach-once in `setupFSexplorer()`).
- `src/OTGW-firmware/webServerCompat.h`: the ADR-132 send-once bridge reused by the
  OTA done-handler (`webBeginRequest`, `webSendP`, `webSend`, `webRequestAuth`).
- `src/OTGW-firmware/updateServerHtml.h`: `UpdateServerIndex` / `UpdateServerSuccess`
  PROGMEM pages (ADR-029), served via `webSendP`; unchanged.
- `partitions_otgw_esp32.csv`: `app, ota_0, 0x10000, 0x1E0000` (the
  `MERGED_APP_OFFSET` / `MERGED_APP_SIZE` the merged-binary extraction matches).
- ESP32Async ESPAsyncWebServer: `onUpload(request, filename, index, data, len,
  final)` semantics (single AsyncTCP task, no `yield`/`delay` in callbacks,
  `request->onDisconnect`): <https://github.com/mathieucarbou/ESPAsyncWebServer>

## Enforcement

Per ADR-080 a binding pattern-level decision wants a CI gate, and ADR-123 promised
each rollout phase adds its own enforceable boundary. This is the OTA boundary of
Phase 3. The rule maps cleanly to forbidden symbols scoped to **the OTA file only**
(`src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h`), which is exactly the task AC
("zero `HTTPUpload`/`upload.status`/`_server->on` in `OTGW-ModUpdateServer-esp32.h`")
and sidesteps the ESP8266 `OTGW-ModUpdateServer-impl.h` that still legitimately uses
the synchronous symbols. The patterns were grep-verified to have zero matches on the
rewritten file (the bare `WebServer` substring and the `UPLOAD_FILE_*` mnemonics in
the explanatory comments are deliberately **not** forbidden, to avoid self-tripping).
`handleClient` / `httpServer.` / `OTGWWebServer` are already owned by ADR-132 on the
broader app-file glob and are not re-forbidden here. Declarative and deterministic;
the maintainer ratifies (or downgrades) this block at the Proposed checkpoint.

```json
{
  "forbid_pattern": [
    {"pattern": "HTTPUpload", "path_glob": "src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h",
     "message": "Sync HTTPUpload model retired in 2.0.0 (ADR-134). Use the AsyncWebServer onUpload(filename,index,data,len,final) callback."},
    {"pattern": "upload\\.status", "path_glob": "src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h",
     "message": "No upload.status lifecycle enum on AsyncWebServer (ADR-134): index==0 is start, final==true is end; abort via request->onDisconnect()."},
    {"pattern": "_server->on\\(", "path_glob": "src/OTGW-firmware/OTGW-ModUpdateServer-esp32.h",
     "message": "OTA registers on the shared AsyncWebServer via srv.on(path, HTTP_POST, done, onUpload) (ADR-134), not the sync _server->on upload lambda."}
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```

# Native HTTPS on ESP32-S3: impact analysis and evidence-led prototype

Date: 2026-10-24. Tracking: **TASK-1349**; report: **TASK-1349.1**.

## Outcome and scope

Native HTTPS and WSS are credible on the ESP32-S3. The installed SDK already
contains the necessary TLS server implementation and cryptographic primitives.
The principal risks are internal RAM per connection, transport integration,
browser certificate trust, and recovery-image flash headroom. The ESP8266-era
memory rationale alone is insufficient to reject HTTPS on this platform.

Recommendation: build a bounded, opt-in prototype using Espressif's native
`esp_https_server`, first proving a small on-device page, then integrating the
actual application. Compare resources with an identical HTTP baseline before
deciding whether this is suitable for production. Do not port every route before
the first trusted browser handshake and geolocation experiment.

This report and backlog are planning deliverables. **No prototype HTTPS session,
real-browser geolocation result, or TLS hardware memory measurement has yet been
collected for this initiative.** A successful build is not functional proof.
Production rollout, universal certificate automation, and changing boiler-control
algorithms are outside this prototype's scope.

The decisive proof requested by the maintainer is:

> A local browser opens the application directly on the ESP32-S3 over trusted
> HTTPS, obtains a real browser geolocation, and verifies that the location is
> correct for the physical installation.

## Research basis and evidence classification

Repository inspected: `dev`, HEAD `15ce35c70` at report preparation. Source paths
below are relative to the repository. Line numbers are snapshot pointers, not
permanent identifiers; execution tasks must reopen the current source.

| Evidence | Finding | Status / source |
|---|---|---|
| Build platform | pioarduino `55.03.35`, Arduino-ESP32 3.3.5; compile database identifies IDF 5.5.1 | Verified configuration: `platformio.ini`, `compile_commands.json` |
| Web dependencies | ESPAsyncWebServer 3.11.2, AsyncTCP 3.4.10 | Verified: `platformio.ini:217-218` |
| Application server | Shared async HTTP server, with REST, files, live-log WebSocket and OTA consumers | Verified: `src/OTGW-firmware/webServerCompat.h:10-48`; C4 web/network docs |
| TLS capability | Server/client TLS 1.2, HTTPS server component, hardware AES/SHA/MPI enabled | Verified installed SDK configuration; see reproducibility below |
| TLS allocation | Internal-memory allocator; 16,384-byte maximum record length | Verified installed SDK; not a measurement of an active session |
| TCP limit | 16 active TCP control blocks | Verified installed SDK; capacity shared with other services |
| Server defaults | Native HTTPS task stack 10,240 bytes, maximum four client sockets | Verified installed `esp_https_server.h`; defaults are not the proposed configuration |
| Existing async task | 16,384-byte stack | Verified `platformio.ini:75` |
| PSRAM | Build enables runtime detection; not every supported S3 has PSRAM | Verified `platformio.ini:26,101`; available capacity must be measured per board |
| Browser URL handling | API uses page protocol/host; WebSocket selects WSS on HTTPS pages | Verified `src/OTGW-firmware/data/v2.js:84,983` |
| Existing location action | Secure-context check; immediately saves latitude then longitude as two requests, rounded to four decimals | Verified `v2.js:4896-4925`; TASK-1257 is related completed work |
| Main image | 2,085,808 bytes, alpha.487+162c3eb | Saved artifact dated 2026-10-24 11:00 local; not a build of inspected HEAD |
| Safeboot image | 931,648 bytes | Saved artifact dated 2026-10-22 07:44 local; not a new TLS build |
| TLS heap / latency / incremental flash | Not yet established for OTGW | Estimates and upstream examples only; prototype must measure |

Installed SDK files inspected on this workstation:

```text
D:/DevData/platformio/packages/framework-arduinoespressif32-libs/
  esp32s3/qio_qspi/include/sdkconfig.h
  esp32s3/include/esp_https_server/include/esp_https_server.h
  esp32s3/include/mbedtls/port/include/mbedtls/esp_config.h
  esp32s3/flags/ld_libs
```

Important configuration observations:

```text
CONFIG_ESP_HTTPS_SERVER_ENABLE=1
CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=1
CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN=16384
CONFIG_MBEDTLS_TLS_SERVER_AND_CLIENT=1
CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=1
CONFIG_MBEDTLS_HARDWARE_AES=1
CONFIG_MBEDTLS_HARDWARE_SHA=1
CONFIG_MBEDTLS_HARDWARE_MPI=1
CONFIG_LWIP_MAX_ACTIVE_TCP=16
```

TLS 1.3 is not enabled in the inspected configuration. Generic upstream support
must not be confused with features compiled into these framework libraries.
Archive the effective configuration and hashes for every measurement run.

## Firmware impact

### Transport and concurrency

The installed ESPAsyncWebServer source has `beginSecure()` behind
`ASYNC_TCP_SSL_ENABLED`, but the installed ESP32 AsyncTCP implementation supplies
no matching TLS server API. Defining that macro or changing the port to 443 is
not a working integration. Check both libraries, not only the server header:

```text
.pio/libdeps/esp32-combo/ESPAsyncWebServer/src/WebServer.cpp:120-129
.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.h
```

`webServerCompat.h` supplies shared request/response state on the assumption that
handlers are serialized on one async task. A native HTTPD port must retain a
well-defined request context and ownership contract. In particular, queued work,
body capture, chunk callbacks, disconnect cleanup and cross-task WebSocket sends
cannot inherit the old assumption without verification.

The owning components are Network and Connectivity, Integration Layer, Web
Interface, and Configuration and State. Existing weather-coordinate settings are
in scope; SAT boiler-control behaviour is not. Any later proposal to change that
behaviour must follow the separate SAT ground-truth rules.

| Surface | Required prototype work |
|---|---|
| HTTP server / lifecycle | Native secure listener, bounded sockets and handshakes, startup/reconnect/stop behaviour |
| REST / JSON | Adapt request interface, retain streaming JSON and route/auth behaviour |
| Files / UI | Stream LittleFS/gzip assets without whole-file RAM copies; preserve response semantics |
| WSS | Live-log delivery through supported server task/queue mechanisms; slow-client limits |
| Authentication | Retain Basic Auth and secret filtering; test same-origin checks including scheme and explicit port behaviour |
| Settings | Confirm before saving location; validate complete pair; verify readback and reboot persistence |
| Updates | Test filesystem upload and separate safeboot handover; avoid an implicit HTTPS-to-HTTP fallback |
| Discovery | Advertise correct HTTPS hostname/port where applicable; keep hostname/certificate identity consistent |
| Recovery | Keep WiFi provisioning and maintenance reachable; document any deliberate HTTP exception |

Existing request gates cap REST and static-file work at two in flight and bound
HTTP transmission connections separately; the WebSocket cap is three. These are
not TLS admission controls. Handshake allocations precede HTTP route admission.
TLS therefore needs early limits, timeouts and cleanup evidence. Do not globally
restore the retired heap tier machine to solve this; bound the new consumer.

### Architecture alternatives

| Option | Benefit | Cost / assessment |
|---|---|---|
| Native `esp_https_server` | Maintained SDK implementation, documented WSS examples; already available in framework | Adapter/route and task-context changes; preferred prototype candidate |
| TLS in current async transport | Could preserve more route code | Requires a demonstrated maintained integration; custom handshake/backpressure machinery is a substantial maintenance commitment |
| On-device TLS proxy | Can initially preserve existing HTTP handlers | Additional tasks, sockets, copying and forwarding buffers; experimental comparator only |
| External reverse proxy | Useful resource baseline / existing deployment option | Does not satisfy native-device proof and is not this epic's solution |

First validate that the native component links in this Arduino build. SDK
availability does not prove application integration. Retaining the existing
HTTP server alongside native HTTPS retains its task and connection costs.
Removing it later must not accidentally break other AsyncTCP consumers.

## Memory, flash and performance

### RAM budget

With the inspected configuration, two 16 KiB TLS record buffers imply about
32 KiB of record storage per established TLS connection, before connection state,
certificate parsing, crypto temporaries, TCP buffers and application responses.
Exact allocation lifecycle and peak must be measured in the selected server.

| Concurrent TLS sockets | Record-storage arithmetic | Initial planning allowance for TLS session memory |
|---:|---:|---:|
| 1 | 32 KiB | 40-60 KiB |
| 2 | 64 KiB | 80-120 KiB |
| 4 | 128 KiB | 160-240 KiB |

The allowances are engineering estimates, not measured OTGW requirements or
guaranteed upper bounds. They exclude separate server-task overhead and existing
firmware consumption. Handshake peaks may exceed them. A browser can retain a
WSS connection plus multiple HTTP/1.1 sockets; users and sockets are not equivalent.

Espressif's IDF 5.5 documentation reports about 42,196 bytes for its default
validated HTTPS **client example**, and about 22,013 bytes with dynamic-buffer
options. These figures illustrate configuration sensitivity; they are not
server-session or OTGW measurements [S2].

PSRAM is useful for other buffers but does not automatically absorb TLS: the
installed Mbed TLS allocator is internal-only. Measure internal free heap,
largest allocatable internal block and low-water mark separately from PSRAM.
Do not present total free memory as proof of adequate internal memory.

The project `include/sdkconfig.h` wrapper changes source-built NimBLE settings.
It does not rebuild precompiled Mbed TLS, lwIP or Bluetooth controller libraries.
Changing TLS allocator/buffer flags only in application headers risks a mismatch.
If tuning is necessary, rebuild the affected framework consistently and archive
the resulting configuration and library hashes. Reducing TLS input limits must
also demonstrate browser interoperability; small application responses alone
do not prove that incoming TLS records fit.

### Flash budget

Current 4 MB layout: `partitions_otgw_esp32.csv` (ADR-191).

| Image | Partition bytes | Saved image bytes | Remaining bytes |
|---|---:|---:|---:|
| Main firmware | 2,293,760 | 2,085,808 | 207,952 (~203 KiB) |
| Safeboot | 983,040 | 931,648 | 51,392 (~50 KiB) |

LittleFS occupies 786,432 bytes. Its image file size is partition size, not proof
of free filesystem capacity for certificates. Certificates and keys must not be
put into a generically downloadable file namespace.

Incremental HTTPS code size is unknown. Available framework archives do not mean
all their code is linked into the existing image. Compare clean, matched images
and link maps. Safeboot is especially constrained: main-image HTTPS does not make
the independent updater encrypted. A partition change is a separate architectural
choice with migration consequences, not an automatic fix for overflow.

### Scheduling and latency

Espressif notes that initial HTTPS session setup can take around two seconds in
its example, with much faster reuse [S1]. This is a test-planning signal, not an
OTGW latency prediction. Keep handshakes out of application control loops; use
connection reuse and explicit deadlines, then measure cold versus warm access.
Check task starvation and queue growth while normal MQTT, serial/OT traffic and
BLE scanning (where enabled) operate. HTTPS does not automatically encrypt MQTT,
telnet, PIC TCP streams or outbound weather requests.

## Suggested prototype

Use an isolated `codex/` feature branch and a dedicated bench ESP32-S3. Preserve a
known-good recovery image and the board's settings before firmware experiments.
Use `python build.py` (or its repository wrapper), not direct PlatformIO builds.
Do not silently alter board capabilities, production defaults or partition layout.

1. **Decision and baseline.** Propose the narrowly scoped HTTPS exception via
   adr-kit; relate it to ADR-003, ADR-056, ADR-123 and ADR-191. Maintainer approval
   owns ADR lifecycle. Record board identity, flash, PSRAM, exact build environment,
   clean HTTP sizes, resource traces and measurable acceptance budgets first.
2. **Identity.** Use a dedicated test CA with a device-specific leaf certificate
   (ECDSA P-256 is a candidate). Keep the CA signing key off the ESP32 and private
   keys out of git and evidence. Provision the device identity in protected storage;
   explicitly trust the CA on the test clients. Use a LAN hostname matching the
   certificate SAN, with DNS or a host mapping resolving directly to the ESP32.
   Never distribute one shared production private key in firmware.
3. **Thin vertical slice.** Start native HTTPS with two TLS client sockets as an
   initial experimental cap, one handshake at a time if feasible, and explicit
   deadlines. Serve a streamed small page with device revision and a user-triggered
   geolocation action. Capture the first genuine browser handshake/location result
   immediately as preliminary evidence. This alone does not close the epic.
4. **Real application.** Bring v2 assets and existing REST/settings operations onto
   the secure origin using a bounded adapter. Reuse existing JSON/validation logic.
   Add WSS with supported task ownership and backpressure. Verify ordinary page
   loading is usable under the chosen socket cap; tune only from measurements.
5. **Location confirmation.** Extend the existing detection UI to show coordinates,
   reported accuracy and time, state that this is the browser's location, and ask
   for confirmation of the installation site. Request a fresh high-accuracy fix
   with a finite timeout; high accuracy is a request, not a GPS guarantee. Preserve
   saved values on cancellation/denial/bad result. Handle partial save failures;
   do not silently retain latitude from one fix and longitude from another.
6. **Recovery and load.** Prove key/certificate handling across reboot, updates and
   filesystem replacement. Record any HTTP safeboot phase explicitly. Run normal
   workload, reconnect, browser and soak tests, then repeat the decisive proof on
   the final measured image and review production suitability.

A local CA is a practical prototype trust path, not automatic consumer onboarding.
A public-domain certificate is an alternative requiring ownership, renewal and
hostname provisioning. A self-signed warning bypass, `curl -k`, browser insecure
origin flag or test-runner `ignoreHTTPSErrors` cannot satisfy the trust proof.

## Functional proof and location correctness

### Setup and success conditions

Run from a physical client at the ESP32 installation. The browser locates its own
device; being on the same LAN does not establish co-location. For the baseline
reference use an independently established map/address or surveyed coordinate,
recording the method and uncertainty **before** invoking browser detection. Do
not reuse the detected fix as the reference.

For this prototype the proposed pass threshold is **100 metres** for both:

- Browser-reported `coords.accuracy` (horizontal accuracy radius in metres [S4]).
- Haversine distance from the detected fix to the independent site reference.

This threshold is a project test choice, not a browser guarantee or a requirement
from Open-Meteo. Record reference uncertainty separately; a reference too uncertain
to discriminate a 100 m error makes the result inconclusive. TASK-1349.3 locks
the threshold and budgets before testing; any maintainer-approved change must be
recorded before the run, never relaxed retrospectively to turn a failure green.

Required steps and evidence:

1. Open the device's HTTPS hostname in a real browser without a reverse proxy.
   Capture certificate issuer, SAN, fingerprint, validity, URL, remote endpoint
   and displayed firmware identity. Confirm `window.isSecureContext === true`.
2. Invoke Detect location in the actual application and grant location permission
   as the operator. No browser automation geolocation override, DevTools Sensors
   simulation, injected API, stub or prerecorded coordinate may be active.
3. Capture fix time, latitude, longitude and reported accuracy. Calculate reference
   distance independently. Report both threshold comparisons; a successful
   geolocation callback alone is insufficient.
4. Confirm the installation site in the UI. Capture HTTPS settings writes and
   readback without authentication secrets. Reboot and read back the persisted
   pair. Current four-decimal formatting permits at most 0.00005 degrees per axis
   for rounding, plus a documented tiny numeric comparison tolerance. Do not use
   exact full-precision equality as the persistence criterion.
5. Verify the subsequent Open-Meteo request uses the saved latitude and longitude
   in the correct order. This proves coordinate wiring, not forecast accuracy or
   changes in boiler-control behaviour. Keep this verification passive.
6. Repeat on at least one physical mobile browser and one desktop browser. Mark
   each result pass/fail/unavailable. One eligible real client/browser pass is
   the minimum functional proof; missing browser or board coverage remains an
   explicit production limitation.

Exercise denial, timeout/unavailable provider, poor accuracy, user cancellation,
wrong site, backend save failure, invalid coordinate ranges, zero coordinates,
plain HTTP, invalid certificate, and cross-origin admin requests. A phone may be
needed when a desktop provider cannot deliver an accurate fix. Do not substitute
IP geolocation or mocks and label it a pass. Browser provider failure can leave
the location proof failed/inconclusive even when native TLS itself works.

Geolocation requires a secure context and user permission [S3]. Installing HTTPS
does not grant permission or guarantee a particular sensor/provider quality.

### Resource and regression matrix

TASK-1349.3 must set numeric internal-memory reserve, maximum stack usage, cleanup
tolerance and responsiveness budgets from the HTTP baseline before testing. No
generic 3 KiB emergency threshold is an adequate TLS capacity argument.

| Scenario | Capture / verify |
|---|---|
| HTTP versus HTTPS idle; cold/warm page load | Binary/map delta, internal free/minimum/largest block, PSRAM separately, server and async stack watermark, handshake and page latency |
| One/two browsers, multi-tab, WSS plus polling | Socket count, queue bounds, refusals/timeouts, page usability and live stream |
| Slow/aborted/untrusted handshakes; disconnects | Admission before costly allocation, bounded cleanup, no retained-session leak |
| 100 connect/disconnect cycles | Stable recovered heap/connection counts within declared tolerance |
| 60-minute representative soak | No watchdog/panic, bounded queues and stable low-water marks; normal services continue |
| Normal MQTT and hardware traffic; BLE if enabled | Existing behaviour remains operational, with baseline differences identified |
| Network reconnect / Ethernet where fitted | HTTPS/WSS recover without stale sessions or server duplication |
| Filesystem upload / safeboot / return | Certificate handling, auth, resource peaks and explicit protocol transition |
| PSRAM and no-PSRAM boards | Separate results; an absent variant is unverified, not assumed covered |
| Chrome, Firefox, Safari latest plus two | Actual tested versions/results; unavailable combinations remain documented gaps |

Do not alter thermostat algorithms to manufacture a passing stress test. Tests
which require unavailable hardware remain pending with an exact reason.

## Evidence collection and review

Each run produces an indexed bundle under a locally chosen restricted evidence
directory. Commit a redacted summary/manifest under
`docs/research/evidence/https-1349/<run-id>/` when evidence exists. Do not commit
private keys, CA signing keys, credentials, cookies or exact personal coordinates.
Use stable local references plus artifact hashes so an authorised reviewer can
inspect originals. HAR files and screenshots require redaction before publishing.

Minimum bundle:

```text
manifest.json           run ID, timestamps, task IDs, commit, board/client IDs,
                        artifact SHA256, configurations, result and limitations
build-baseline.log      actual commands, versions, exit status, image/map sizes
build-https.log         matched secure build and its exact configuration
resources.csv          timestamp, phase, internal free/min/largest, PSRAM,
                        task watermarks, sockets, queues, latency, reset count
browser-proof.*        real URL/TLS/security state, permission and result evidence
location-check.json    reference method/uncertainty, distance and accuracy gates,
                        fix time, rounding/readback result (coordinates restricted)
negative-tests.md      expected/actual outcomes and linked raw evidence
recovery.md            provisioning, update/reboot trace and rollback/recovery
summary.md             reviewer-readable pass/fail table, gaps and go/no-go
```

Every pass cites a specific artifact, timestamp, firmware hash and actual result.
Keep unit/mock tests separate from hardware/browser proof. Missing evidence is
pending, not success. The final reviewer must be able to distinguish direct
ESP32 TLS termination from an external proxy and a real fix from a simulation.

## Backlog breakdown and dependencies

All children belong to **TASK-1349** and carry their own acceptance criteria.
Only the report task is being executed in this documentation session.

| Task | Deliverable | Depends on |
|---|---|---|
| TASK-1349.1 | This research report and evidence protocol | None |
| TASK-1349.2 | Reviewed experiment ADR and trust/recovery contract | .1 |
| TASK-1349.3 | Matched board/build/browser baseline and numeric budgets | .1 |
| TASK-1349.4 | Trusted per-device local certificate provisioning | .2, .3 |
| TASK-1349.5 | Bounded native HTTPS thin slice and measured transport decision | .4 |
| TASK-1349.6 | Actual v2 app, streamed assets, REST and auth over HTTPS | .5 |
| TASK-1349.7 | Bounded live-log WSS and reconnect behaviour | .6 |
| TASK-1349.8 | Confirmed location, safe save, readback and failure behaviour | .6 |
| TASK-1349.9 | Provisioning/update/safeboot recovery evidence | .6, .4 |
| TASK-1349.10 | Concurrency, 100 cycles, 60-minute soak and variant measurements | .7, .8, .9 |
| TASK-1349.11 | Decisive real-browser HTTPS/geolocation correctness proof | .10 |
| TASK-1349.12 | Evidence review, report update and production go/no-go | .11 |

The thin slice may gather preliminary browser proof early, but TASK-1349.11 must
repeat it against the integrated, measured image. Each implementation task also
owns its relevant regression tests and documentation; the final proof task does
not defer basic validation until the end.

The old HTTP-only decision remains an accepted historical/current constraint
until the maintainer approves its successor/exception using adr-kit. This report
does not accept a new ADR or edit immutable accepted records. Production adoption
requires an explicit decision after the prototype's evidence, not an automatic
consequence of a successful handshake.

## Source references

- [S1: Espressif IDF 5.5 HTTPS server and WSS examples](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_https_server.html)
- [S2: Espressif IDF 5.5 Mbed TLS configuration and client-example memory figures](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/mbedtls.html)
- [S3: Browser getCurrentPosition requirements and options](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation/getCurrentPosition)
- [S4: Browser accuracy radius](https://developer.mozilla.org/en-US/docs/Web/API/GeolocationCoordinates/accuracy)
- [ADR-003: HTTP-only decision](../adr/ADR-003-http-only-no-https.md)
- [ADR-056: Authentication and secret contract](../adr/ADR-056-protected-admin-endpoint-security-and-secret-handling-contract.md)
- `docs/adr/ADR-123-*`: FreeRTOS and async networking architecture.
- `docs/adr/ADR-185-*`: Source-built versus precompiled SDK configuration limits.
- `docs/adr/ADR-191-*`: Safeboot partition and update architecture.
- [C4 network component](../c4/c4-component-network.md)
- [C4 web interface component](../c4/c4-component-web-interface.md)
- [PR checklist](../guides/pr-checklist.md): use current ESP32-S3 targets; the checklist still contains historical ESP8266 wording.

## Current validation status

Research/configuration findings are verified by source inspection. Estimated TLS
memory is explicitly unmeasured. Prototype build delta, hardware HTTPS, location
correctness, soak and recovery evidence are **pending** under the tasks above.
Documentation-session checks are recorded in TASK-1349.1; they must not be read as
prototype acceptance evidence.

The documentation session also rebuilt the unchanged HTTP firmware at inspected
HEAD. The fresh main `.ino.bin` is **2,085,552 bytes** (208,208 bytes of app-slot
headroom). PlatformIO reports 118,924 bytes of static RAM and 2,085,143 bytes of
linked flash usage; these differ from the image-file size and are not runtime
free-heap measurements. Safeboot's fresh image remains 931,648 bytes. These are
HTTP baseline build observations, not an HTTPS size comparison.

Repository checks: `evaluate.py --quick` reported 85 checks, 78 passed, 7 info,
zero warnings/failures and health 100%; `tests/test_evaluate.py` passed 80 tests;
`tests/test_build.py` passed 13 tests. Local report links and the 13-record backlog
graph (one epic, 12 children, resolving dependencies, no cycles) were validated.

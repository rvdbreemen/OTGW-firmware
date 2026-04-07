---
# METADATA
Document Title: Issue #525 — Root-Cause Analysis: WiFi Unreachability After Router Reboot
Review Date: 2026-04-07 09:50:00 UTC
Versions Analysed: v1.2.0 (production) vs v1.3.0 → v1.3.5 → v1.3.6-beta (dev)
Target Issue: https://github.com/rvdbreemen/OTGW-firmware/issues/525
Reviewer: GitHub Copilot Advanced Agent
Document Type: Root-Cause Analysis
Branch: copilot/analyze-sdk-calls-issue
Status: COMPLETE
---

# Issue #525 — Root-Cause Analysis

## Symptom

Starting with v1.3.0, the OTGW-firmware becomes unreachable (ping, Web GUI, Telnet, MQTT) after any WiFi interruption (e.g. router reboot, brief outage). The device successfully re-associates at the 802.11 layer (the router's hostapd log confirms authentication + WPA key exchange), but **no DHCP exchange takes place** — the device remains with no IP address and is silently invisible on the network.

Reported and confirmed on v1.3.0, v1.3.1, v1.3.4, and v1.3.5.
v1.2.0 does **not** exhibit the problem.

---

## Version Comparison

### v1.2.0 (works correctly)

**`startWiFi()`**
- Calls `WiFi.setAutoReconnect(true)` + `WiFi.persistent(true)` after connect.
- **No** `wifi_station_dhcpc_stop()` / `wifi_station_dhcpc_start()` calls at all.
- No hostname explicitly set before `WiFi.begin()` (the SDK default is used).

**`startNTP()`**
- Calls `configTime(0, 0, ntpHost, ...)`.
- **No** hostname restore.
- **No** SDK DHCP calls.

**Reconnect mechanism:** Purely `WiFi.setAutoReconnect(true)` (SDK-level). Because the DHCP client state was never touched manually, the SDK's auto-reconnect correctly restarts DHCP on every re-association.

---

### v1.3.0 – v1.3.4 (broken — primary root cause)

Two new code paths were added as part of the hostname-fix work:

#### 1. `startWiFi()` catch-all
```cpp
WiFi.hostname(hostname);
if (!sDhcpHostnameFixed && strcmp(WiFi.hostname().c_str(), hostname) != 0) {
    wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();   // ← called while STA is CONNECTED
    sDhcpHostnameFixed = true;
}
```

#### 2. `startNTP()`
```cpp
bool hostnameWasReset = (strcmp(WiFi.hostname().c_str(), ...) != 0);
WiFi.hostname(CSTR(settings.sHostname));
if (!sDhcpHostnameFixed && hostnameWasReset && WiFi.isConnected()) {
    wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();   // ← called while STA is CONNECTED
    sDhcpHostnameFixed = true;
}
```

**The critical problem:** `wifi_station_dhcpc_start()` called while the station is already associated **immediately resets the STA IP address to 0.0.0.0**. This is an undocumented but confirmed behaviour of the ESP8266 NonOS SDK — the call is only safe when the station is *not* connected.

After `dhcpc_start()` runs while connected:
1. IP becomes 0.0.0.0; DHCP DISCOVER is sent.
2. The router responds with an OFFER; the device gets a lease. *(Appears OK.)*
3. The DHCP client is now "running" with a fresh lease.

Later, when the router reboots:
4. The 802.11 association drops.
5. SDK `setAutoReconnect(true)` fires `wifi_station_connect()` internally.
6. The device re-associates (confirmed by hostapd log).
7. **But**: `wifi_station_connect()` does **not** call `wifi_station_dhcpc_start()`. The SDK only calls `dhcpc_start()` when connecting for the first time (before any manual `dhcpc_start()` has been made). Once the firmware has called `dhcpc_start()` manually, the SDK considers DHCP to be "user-managed" and no longer automatically restarts it on reconnection.
8. The DHCP client tries to **RENEW** the previous lease (unicast REQUEST to the old server). Depending on the router, this may succeed or fail silently (especially if the router cleared its DHCP table on reboot).
9. If the RENEW fails and the lease is long (24 h is common), the client keeps waiting for hours before falling back to DISCOVER.
10. **Result:** No new IP — device is unreachable even though it is associated.

**Why v1.3.0–v1.3.4 have no recovery path:** There is no `loopWifi()` state machine in these versions. The only reconnect mechanism is `WiFi.setAutoReconnect(true)`, which, as described above, does not restart DHCP in this scenario.

---

### v1.3.5 (still broken — loopWifi added but with a new bug)

`loopWifi()` was introduced. However, the DHCP calls were placed incorrectly:

```cpp
case WIFI_RECONNECTED:
    wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();   // ← called while STA is now CONNECTED (wrong!)
    WiFi.hostname(CSTR(settings.sHostname));
    startTelnet(); startOTGWstream(); startMQTT(); startWebSocket();
    wifiState = WIFI_IDLE;
    break;
```

And in `WIFI_DISCONNECTED`, **no** `dhcpc_start()` was called before `WiFi.begin()`.

Sequence after a router reboot:
1. `WIFI_IDLE` → detects `WiFi.status() != WL_CONNECTED` → `WIFI_DISCONNECTED`
2. `WIFI_DISCONNECTED`: only calls `WiFi.begin()` — **no** `dhcpc_start()` first.
3. `WiFi.begin()` calls `wifi_station_connect()` only. DHCP client is **stopped** (from a previous `dhcpc_stop()`). No DISCOVER is sent.
4. Device re-associates at L2 but has no IP.
5. `WIFI_RECONNECTED`: calls `dhcpc_stop()` → `dhcpc_start()` **while connected** → IP resets to 0.0.0.0 again.
6. `WIFI_IDLE`: immediately detects "not connected" → starts another reconnect cycle.
7. **Infinite loop** — the device keeps cycling but never gets an IP.

---

### v1.3.6-beta after PR #537 (current dev — primary issue fixed)

PR #537 moved `dhcpc_start()` to the correct location (`WIFI_DISCONNECTED`, before `WiFi.begin()`), and removed it from `WIFI_RECONNECTED`:

```cpp
case WIFI_DISCONNECTED:
    WiFi.hostname(CSTR(settings.sHostname));
    wifi_station_dhcpc_start();   // ← safe: STA is NOT connected here
    WiFi.begin();
    wifiState = WIFI_CONNECTING;
    break;

case WIFI_RECONNECTED:
    // NO dhcpc_stop/start here — correct
    WiFi.hostname(CSTR(settings.sHostname));
    startTelnet(); startOTGWstream(); startMQTT(); startWebSocket();
    wifiState = WIFI_IDLE;
    break;
```

After a router reboot:
1. `WIFI_IDLE` → detects disconnect → `WIFI_DISCONNECTED`
2. `dhcpc_start()` while **not** connected → DHCP client resets to "fresh" state ✓
3. `WiFi.begin()` → re-associates at L2 ✓
4. DHCP DISCOVER is sent → gets new lease ✓
5. `WIFI_RECONNECTED` → services restart ✓

**The router-reboot scenario is now fixed.**

---

## Remaining Issue in Current Dev: While-Connected SDK Calls

Despite the loopWifi fix, **two call sites still invoke `wifi_station_dhcpc_stop/start` while the station is connected:**

### 1. `startWiFi()` catch-all (runs during `setup()`)

```cpp
if (!sDhcpHostnameFixed && strcmp(WiFi.hostname().c_str(), hostname) != 0) {
    wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();   // ← connected, resets IP to 0.0.0.0
    sDhcpHostnameFixed = true;
}
```

This fires once per boot if the SDK hostname didn't apply (rare on modern SDK but possible). After it runs, IP drops to 0.0.0.0 during `setup()`. `loopWifi()` then recovers the IP via `WIFI_DISCONNECTED`, but this introduces unnecessary disruption and delays before network services are fully operational.

### 2. `startNTP()` (runs during first NTP sync and every 30-min resync)

```cpp
if (!sDhcpHostnameFixed && hostnameWasReset && WiFi.isConnected()) {
    wifi_station_dhcpc_stop();
    wifi_station_dhcpc_start();   // ← connected, resets IP to 0.0.0.0
    sDhcpHostnameFixed = true;
}
```

The `sDhcpHostnameFixed` guard prevents this from firing on 30-min resyncs (only runs once). When it fires on first boot, the same IP-reset → loopWifi-recovery pattern occurs. While `loopWifi()` does recover, the disruption is unnecessary.

---

## Answer to the Core Question

> *"Validate the use of direct SDK calls is NOT causing the issue in the first place."*

**They are, and always were, the root cause.** Specifically:

| Version | SDK `dhcpc_start()` while connected? | Recovery mechanism | Result |
|---------|--------------------------------------|-------------------|--------|
| v1.2.0 | **Never** | `setAutoReconnect(true)` | ✅ Works |
| v1.3.0–v1.3.4 | Yes — in `startNTP()` / `startWiFi()` | `setAutoReconnect(true)` only | ❌ No DHCP after reconnect |
| v1.3.5 | Yes — in `startNTP()` / `startWiFi()` AND in `loopWifi` `WIFI_RECONNECTED` | `loopWifi()` (broken) | ❌ Infinite reconnect loop |
| v1.3.6-beta (post-PR #537) | Yes — in `startNTP()` / `startWiFi()` only | `loopWifi()` (fixed) | ✅ Works after router reboot, brief disruption on first boot |

The v1.2.0 code works precisely **because it never calls `wifi_station_dhcpc_start()` while connected**. The Arduino WiFi library's `WiFi.begin()` / `setAutoReconnect()` leave DHCP management entirely to the SDK, which handles it correctly. Once you call `dhcpc_start()` manually while connected, you take over ownership of the DHCP state, and the SDK no longer manages it automatically.

---

## Recommended Fix (Implemented in This PR)

**Remove all `wifi_station_dhcpc_stop/start` calls made while the STA is connected.**

The hostname goal (ensuring the DHCP server sees the correct hostname) can be achieved safely by:
1. Always setting `WiFi.hostname()` before every `WiFi.begin()` call (already done).
2. Relying on `loopWifi()` `WIFI_DISCONNECTED` to call `wifi_station_dhcpc_start()` in the correct (not-connected) state before `WiFi.begin()`.
3. Allowing the next natural DHCP exchange (renewal or reconnect-triggered DISCOVER) to propagate the correct hostname to the router.

This eliminates:
- `sDhcpHostnameFixed` — the guard variable is no longer needed.
- The `hostnameWasReset` detection block in `startNTP()` — the hostname is restored by `WiFi.hostname()` unconditionally; no SDK call is needed.
- The catch-all dhcpc block in `startWiFi()`.

**Simplified `startNTP()`:**
```cpp
void startNTP() {
    ...
    WiFi.hostname(CSTR(settings.sHostname));
    configTime(0, 0, settings.ntp.sHostname, nullptr, nullptr);
    WiFi.hostname(CSTR(settings.sHostname));   // restore if configTime() reset it
    NtpStatus = TIME_WAITFORSYNC;
}
```

**Simplified `startWiFi()` tail:**
```cpp
    WiFi.hostname(hostname);   // ensure hostname is set for next DHCP exchange
    // (loopWifi WIFI_DISCONNECTED will call dhcpc_start before WiFi.begin)
    httpUpdater.setup(&httpServer);
    ...
```

This aligns the firmware's behaviour with v1.2.0's approach: never touch the DHCP client while connected, always let the stack handle it.

---

## Impact Assessment

| Scenario | Before fix | After fix |
|----------|-----------|-----------|
| Fresh boot, SDK auto-connected | Brief IP loss during setup (startNTP fires dhcpc) | No disruption |
| Fresh boot, not yet connected | No issue | No issue |
| Router reboot (post-PR #537) | ✅ Fixed by loopWifi | ✅ Fixed by loopWifi |
| configTime() hostname reset | DHCP re-announce forced (IP disruption) | Hostname silently restored; router updates at next renewal |
| 30-min NTP resync | No issue (sDhcpHostnameFixed guards it) | No issue |

---

## Conclusion

The direct ESP8266 SDK calls (`wifi_station_dhcpc_stop()` / `wifi_station_dhcpc_start()`) called **while the station is connected** are the confirmed root cause of issue #525. They corrupt the SDK's DHCP state in a way that the Arduino WiFi auto-reconnect path does not recover from.

The PR #537 fix (`dhcpc_start()` in `WIFI_DISCONNECTED`, none in `WIFI_RECONNECTED`) correctly resolves the router-reboot scenario. The supplementary fix in this PR removes the remaining while-connected SDK calls from `startNTP()` and `startWiFi()`, completing the alignment with the v1.2.0 approach and eliminating unnecessary IP disruption on first boot.

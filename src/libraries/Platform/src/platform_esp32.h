/*
***************************************************************************
**  Program  : platform_esp32.h
**  Version  : v2.0.0-alpha.137
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  ESP32-specific includes, type aliases, and shim functions.
**  Included only by platform.h — never include this file directly.
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

// ---- Core ESP32 libraries ------------------------------------------------
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_timer.h>          // esp_timer_get_time (platformUptimeSeconds, TASK-1449)
#include <esp_heap_caps.h>      // heap_caps_malloc (platformPsramAllocOnce)
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>            // esp_wifi_get_config/set_config (platformWifiPreferStrongestAp, TASK-1397)
#include <esp_flash.h>
#include <esp_core_dump.h>       // core-dump summary for the reboot log (platformResetRegisterDump, TASK-1479)
#include <esp_partition.h>       // coredump partition read (platformCoreDumpRead, TASK-1560)
#include <hal/usb_serial_jtag_ll.h>  // platformUsbJtagPutc (TASK-1217)
#include <esp_sntp.h>            // esp_sntp_servermode_dhcp() (platformIgnoreDhcpNtp)
#include <lwip/priv/tcp_priv.h>  // tcp_active_pcbs (platformTcpActivePcbCount)
#include <lwip/tcpip.h>          // LOCK_TCPIP_CORE/UNLOCK_TCPIP_CORE (platformTcpActivePcbCount)
#include <lwip/netdb.h>          // lwip_getaddrinfo (platformResolveHost, TASK-1605)
#include <lwip/raw.h>            // raw ICMP pcb (platformGatewayProbe*, TASK-1450)
#include <lwip/prot/icmp.h>      // icmp_echo_hdr, ICMP_ECHO/ICMP_ER (platformGatewayProbe*)
#include <lwip/inet_chksum.h>    // inet_chksum (platformGatewayProbeSend)

// ---- ADR-123 async/FreeRTOS stack (TASK-865.4) ---------------------------
// Foundation headers for the 2.0.0 ESP32-S3 event-driven migration. Included
// here (the platform include aggregator) ungated so every ESP32 env compiles
// against them and PlatformIO's LDF pulls the libraries. The lib_deps pins live
// in [env:esp32].lib_deps (inherited by esp32-classic/esp32-combo via extends).
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>   // also declares AsyncWebSocket
#include <espMqttClient.h>

// ---- Platform name -------------------------------------------------------
#define PLATFORM_NAME "ESP32"

// ---- ADR-123 async-stack link-proof smoke (TASK-865.4) -------------------
// A bare #include only proves the headers compile, not that each library's
// compiled object actually LINKS. This function instantiates one class per
// async lib, which forces their out-of-line constructors (and therefore the
// libraries' .cpp objects) to be pulled in at link time — confirmable by
// grepping the .map for AsyncClient / AsyncWebSocket / espMqttClient.
//
// __attribute__((used)) defeats -flto dead-stripping: without it an uncalled
// static function (and its references) would be eliminated and prove nothing.
// The whole proof is gated on ASYNC_LINK_PROOF, set ONLY in [env:esp32]
// build_flags. esp32-classic/esp32-combo do not see that flag (they rebuild
// build_flags from the global [env]), so the smoke and its flash cost vanish
// there — keeping the ~98.4 %-full combo binary inside its 1.875 MB app slot.
// The function is never called: it exists purely so the linker resolves the
// symbols. ASYNC_LINK_PROOF is a feature flag, not a platform/board macro, so
// it does not trip the ESP-abstraction boundary check.
#if defined(ASYNC_LINK_PROOF)
// `inline` (not `static`) so the several .cpp TUs that include platform.h share
// ONE COMDAT-folded definition instead of each retaining its own copy.
inline __attribute__((used)) void platformAsyncLinkProof() {
  volatile void *sink;
  AsyncClient    asyncTcpProof;                 // AsyncTCP
  AsyncWebSocket asyncWsProof("/_linkproof");   // ESPAsyncWebServer (WebSocket)
  espMqttClient  mqttProof;                      // espMqttClient (plain, non-async)
  sink = &asyncTcpProof;
  sink = &asyncWsProof;
  sink = &mqttProof;
  (void)sink;
}
#endif

// ---- Feature flags -------------------------------------------------------
#define HAS_LLMNR           0    // ESP32 does not have LLMNR
#define MDNS_NEEDS_UPDATE   0    // ESP32 mDNS runs on a background task

// ---- Unified type aliases ------------------------------------------------
// (OTGWWebServer = WebServer removed in TASK-865.9: the HTTP server moved to
//  ESPAsyncWebServer. The sync WebServer type now survives only inside
//  OTGW-ModUpdateServer-esp32.h, which includes <WebServer.h> itself; seq11
//  ports OTA onto AsyncWebServer and retires that last consumer.)

// ---- FSInfo compatibility ------------------------------------------------
// ESP8266 uses FSInfo struct natively. Define it here for ESP32.
struct FSInfo {
  size_t totalBytes;
  size_t usedBytes;
  size_t blockSize;
  size_t pageSize;
  size_t maxOpenFiles;
  size_t maxPathLength;
};

// ---- Platform functions --------------------------------------------------

// WiFi hostname
inline void platformSetHostname(const char *hostname) {
  WiFi.setHostname(hostname);
}

inline const char* platformGetHostname() {
  return WiFi.getHostname();
}

// Disable WiFi modem power-save. ESP32 defaults to WIFI_PS_MIN_MODEM, which
// parks the radio between DTIM beacons and adds 100 ms-1 s of latency to every
// inbound packet (and drops buffered packets under burst) - the root cause of
// the "web page takes seconds to load, sometimes never" symptom on OTGW32.
// Costs a few mA extra; on a mains-powered gateway that is the right trade.
inline void platformWifiDisableSleep() {
  WiFi.setSleep(false);
}

// Join the strongest access point of the configured SSID (TASK-1397).
// Arduino-ESP32 3.3.5 defaults to WIFI_FAST_SCAN, which joins the first AP the
// scan finds on the SSID (STA.cpp:231), so a unit can sit at -84 dBm while a
// stronger AP of the same SSID is in range. WIFI_ALL_CHANNEL_SCAN scans every
// channel and WIFI_CONNECT_AP_BY_SIGNAL picks the best RSSI of the results.
// The Arduino setters only reach connect(ssid, ...) (STA.cpp:389-390). A bare
// WiFi.begin() and the SDK auto-reconnect reuse the stored driver config, so the
// same two fields are patched in it as well. Call after WiFi.mode(WIFI_STA) and
// before every WiFi.begin(); safe to repeat. Costs about 1-2 s per (re)connect.
inline void platformWifiPreferStrongestAp() {
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  wifi_config_t cfg;
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK &&
      (cfg.sta.scan_method != WIFI_ALL_CHANNEL_SCAN ||
       cfg.sta.sort_method != WIFI_CONNECT_AP_BY_SIGNAL)) {
    cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
  }
}

// TASK-1450: IPv4 default gateway of the WiFi station, in lwIP byte order (the order
// ip4_addr_set_u32() takes); 0 when there is none (static IP without a gateway).
inline uint32_t platformWifiGatewayV4() {
  return static_cast<uint32_t>(WiFi.gatewayIP());
}

// TASK-1450: drop the WiFi association without touching the stored credentials (eraseap
// false), and without the 100 ms wait of the default disconnect(). The driver reports
// reason ASSOC_LEAVE, for which arduino-esp32 3.3.5 skips its own auto-reconnect
// (STA.cpp:151), so loopWifi() sees the drop and reconnects through WiFi.begin().
inline void platformWifiForceReconnect() {
  WiFi.disconnect(false, false, 0);
}

// LittleFS info (ESP32: construct from totalBytes/usedBytes)
inline bool platformFSInfo(FSInfo &info) {
  info.totalBytes    = LittleFS.totalBytes();
  info.usedBytes     = LittleFS.usedBytes();
  info.blockSize     = 0;
  info.pageSize      = 0;
  info.maxOpenFiles  = 10;
  info.maxPathLength = 255;
  return (info.totalBytes > 0);
}

// Core/SDK version
inline const char* platformCoreVersion() {
  return ESP.getSdkVersion();
}

// SDK version
inline const char* platformSdkVersion() {
  return ESP.getSdkVersion();
}

// CPU frequency
inline uint32_t platformCpuFreqMHz() {
  return ESP.getCpuFreqMHz();
}

// MAC address
inline void platformGetMacAddress(uint8_t *mac) {
  esp_efuse_mac_get_default(mac);
}

// Chip identity (derive a 32-bit ID from the 48-bit MAC)
inline uint32_t platformChipId() {
  uint64_t mac = ESP.getEfuseMac();
  // XOR upper and lower 24 bits for a compact unique ID
  uint32_t low  = (uint32_t)(mac & 0xFFFFFF);
  uint32_t high = (uint32_t)((mac >> 24) & 0xFFFFFF);
  return low ^ high;
}

// Reset reason as a C string
inline void platformResetReason(char *buf, size_t len) {
  esp_reset_reason_t reason = esp_reset_reason();
  const char *str;
  switch (reason) {
    case ESP_RST_POWERON:  str = "Power on";        break;
    case ESP_RST_EXT:      str = "External reset";  break;
    case ESP_RST_SW:       str = "Software reset";   break;
    case ESP_RST_PANIC:    str = "Exception/panic";  break;
    case ESP_RST_INT_WDT:  str = "Interrupt watchdog"; break;
    case ESP_RST_TASK_WDT: str = "Task watchdog";   break;
    case ESP_RST_WDT:      str = "Other watchdog";  break;
    case ESP_RST_DEEPSLEEP: str = "Deep sleep wake"; break;
    case ESP_RST_BROWNOUT: str = "Brownout";         break;
    case ESP_RST_SDIO:     str = "SDIO reset";       break;
    // TASK-1438: ESP-IDF esp_system.h values 11-15. USB (0xb) is the S3's USB-Serial-JTAG peripheral:
    // the host toggling the serial lines (a tool opening the COM port) resets the chip.
    case ESP_RST_USB:        str = "USB reset (host)";  break;
    case ESP_RST_JTAG:       str = "JTAG reset";        break;
    case ESP_RST_EFUSE:      str = "eFuse error";       break;
    case ESP_RST_PWR_GLITCH: str = "Power glitch";      break;
    case ESP_RST_CPU_LOCKUP: str = "CPU lock-up";       break;
    default:               str = "Unknown";          break;
  }
  strlcpy(buf, str, len);
}

// WiFi scan: is the network at scan index `i` encrypted (not open)?
inline bool platformWiFiIsEncrypted(uint8_t i) {
  return WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
}

// No-op: the ESP32 NetworkClient has no setSync() and already streams writes
// without the ESP8266 TCP_SND_BUF temporary copy (TASK-743). Stub for link
// symmetry so the caller stays unguarded.
inline void platformWiFiClientSetSync(WiFiClient&) {
}

// Reset cause as a PIC-compatible boot-code char for the OTDirect PR: Q=
// response (P=power-on, C=cold/SW-reset, W=watchdog, B=brownout, E=external).
// OTDirect (ESP32) is the only caller; the vocabulary mirrors the PIC's reset
// codes so downstream PR-parsing stays identical to the PIC path.
inline char platformGetResetReasonChar() {
  switch (esp_reset_reason()) {
    case ESP_RST_SW:        return 'C';  // Software reset -> Cold start
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return 'W';  // Watchdog
    case ESP_RST_BROWNOUT:  return 'B';  // Brownout
    case ESP_RST_EXT:       return 'E';  // External reset
    default:                return 'P';  // Power-on or unknown
  }
}

// NTP hostname guard. ESP32's configTime() does not touch the WiFi station
// hostname, so this is a no-op. Call it before and after configTime() so the
// shared startNTP() code path stays platform-neutral.
inline void platformNtpHostnameFix(const char *hostname) {
  (void)hostname;
}

// Keep DHCP-supplied NTP servers (DHCP option 42) out of the SNTP module; we
// configure our own server in startNTP(). With CONFIG_LWIP_DHCP_GET_NTP_SRV=y
// lwIP requests option 42 and passes every offered server to
// dhcp_set_ntp_servers(), but SNTP only takes it when the flag
// sntp_set_servers_from_dhcp is 1. On this framework that flag starts at 0 and
// nothing in the image sets it to 1 (TASK-1052, checked in the source and in
// the linked ELFs), so this call pins the default rather than changing it. It
// keeps the reject in place if a future framework or library flips the default.
//
// esp_sntp_servermode_dhcp() hands the work to the lwIP thread through
// tcpip_callback(), which asserts when the TCP/IP stack is not up yet
// (CONFIG_LWIP_ESP_LWIP_ASSERT=y). The guard skips the call in that case;
// skipping is safe because the flag already holds the reject value. In our boot
// path the stack comes up in WiFi.mode() (wifiLowLevelInit() -> Network.begin()
// -> esp_netif_init()), so startWiFi() calls this right after WiFi.mode().
inline void platformIgnoreDhcpNtp() {
  if (sys_thread_tcpip(LWIP_CORE_IS_TCPIP_INITIALIZED)) {
    esp_sntp_servermode_dhcp(0);
  }
}

// Heap information
inline uint32_t platformFreeHeap() {
  return ESP.getFreeHeap();
}

inline uint32_t platformMaxFreeBlock() {
  return ESP.getMaxAllocHeap();
}

// Heap fragmentation as percentage (0 = none, 100 = worst). ESP32 has no
// native fragmentation API, so we compute it the same way ESP8266 core does:
// 100 - (maxBlock * 100 / totalFree). 0 when heap is empty.
inline uint8_t platformHeapFragmentation() {
  const uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap == 0) return 0;
  const uint32_t maxBlk = ESP.getMaxAllocHeap();
  return (uint8_t)(100U - ((maxBlk * 100U) / freeHeap));
}

// Windowed heap low-water mark (TASK-1339 diagnostics). The IDF heap can track
// minimum_free_bytes from a chosen moment instead of from boot
// (heap_caps_monitor_local_minimum_free_size_start/_stop, esp_heap_caps.h:258).
// That monitor is process-wide: while it runs, heap_caps_get_minimum_free_size()
// returns the minimum since the window started, for every caller. So the shim
// keeps the since-boot minimum from before the window in platformHeapWinBootMin(),
// and platformMinFreeHeap() returns min(that, window minimum) while a window is
// open. That sum-of-region-minima can sit slightly above IDF's per-region merge
// at stop; after stop the native value is exact again.
// The state lives in function-local statics of inline functions, so every
// translation unit that includes this header shares one copy.
inline uint32_t& platformHeapWinBootMin() { static uint32_t v = 0xFFFFFFFFu; return v; }
inline volatile bool& platformHeapWinActiveRef() { static volatile bool a = false; return a; }
inline bool platformHeapWindowActive() { return platformHeapWinActiveRef(); }

// Minimum free internal heap since the window started (0 when no window is open).
inline uint32_t platformHeapWindowMin() {
  if (!platformHeapWindowActive()) return 0;
  return (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
}

// Close the window. The IDF merges the window minimum into the since-boot
// minimum per region, so platformMinFreeHeap() stays a true low-water mark.
inline void platformHeapWindowStop() {
  if (!platformHeapWindowActive()) return;
  heap_caps_monitor_local_minimum_free_size_stop();
  platformHeapWinActiveRef() = false;
}

// Open a window, or restart it when one is open (stop, then start: a start
// while monitoring runs is not documented in esp_heap_caps.h; the IDF 5.5.1
// code then keeps the older saved minimum and returns ESP_OK, but we do not
// rely on that and pay one extra free + malloc per restart). The start does one small
// malloc (one size_t per heap region) and asserts on failure, so it is skipped
// when the largest free block is under 256 B. Returns true when a window is open.
inline bool platformHeapWindowStart() {
  const bool wasActive = platformHeapWindowActive();
  uint32_t m = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  uint32_t& bootMin = platformHeapWinBootMin();
  if (m < bootMin) bootMin = m;
  // Mark active before the start, clear it after the stop: a reader on another
  // task then always sees min(bootMin, native), which is never too high.
  platformHeapWinActiveRef() = true;
  if (wasActive) heap_caps_monitor_local_minimum_free_size_stop();
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < 256 ||
      heap_caps_monitor_local_minimum_free_size_start() != ESP_OK) {
    platformHeapWinActiveRef() = false;
    return false;
  }
  return true;
}

// Minimum observed free heap since boot. ESP32 tracks this natively in the
// heap allocator via heap_caps_get_minimum_free_size — no user-space
// watermark needed, except while a diagnostic window is open (see above).
inline uint32_t platformMinFreeHeap() {
  uint32_t m = ESP.getMinFreeHeap();
  if (platformHeapWindowActive()) {
    uint32_t b = platformHeapWinBootMin();
    if (b < m) m = b;
  }
  return m;
}

// One block from PSRAM for a buffer that is allocated once at boot and never freed
// (TASK-1296 graph history). Returns nullptr on a board without PSRAM or when the
// allocation fails; internal DRAM is never used, so the caller must have a fallback.
inline void* platformPsramAllocOnce(size_t bytes) {
  if (!psramFound()) return nullptr;
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

// No-op on ESP32: the heap allocator tracks min-free internally.
inline void platformUpdateMinFreeHeap() {
  // intentionally empty
}

// TASK-1017: count of active lwIP TCP PCBs (established/handshaking TCP
// connections), for load-test observability of connection-level pressure
// (TASK-879 class: PCB pool exhaustion under flood). tcp_active_pcbs is an
// internal lwIP linked list (lwip/priv/tcp_priv.h) normally only walked from
// the tcpip thread; this build has CONFIG_LWIP_TCPIP_CORE_LOCKING=y (verified
// in the arduino-esp32 sdkconfig), so LOCK_TCPIP_CORE()/UNLOCK_TCPIP_CORE()
// is a real mutex safe to take from any task (the documented pattern for
// touching lwIP internals from application code). Sample from the 1 Hz loop
// task only (helperStuff.ino sampleHeapWatermark) — never from a per-request
// hot path, since a live per-response walk would (a) break the REST chunked
// emit's determinism contract (byte-identical JSON across re-run windows,
// see DeviceInfoSnap) and (b) add lock contention to the async_tcp task.
inline uint16_t platformTcpActivePcbCount() {
  uint16_t count = 0;
  LOCK_TCPIP_CORE();
  for (struct tcp_pcb *pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
    count++;
  }
  UNLOCK_TCPIP_CORE();
  return count;
}

// Only response-owned waiters are excluded, matching both the live PCB and its
// AsyncTCP owner under the core lock; address reuse cannot hide another sender.
struct PlatformTcpIdleConnection {
  const tcp_pcb* pcb;
  const void* owner;
};
inline uint16_t platformTcpTxHoldingOnPort(uint16_t localPort,
                                         const PlatformTcpIdleConnection* idle,
                                         uint8_t idleCount) {
  uint16_t count = 0;
  LOCK_TCPIP_CORE();
  for (struct tcp_pcb *pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
    if (pcb->local_port != localPort) continue;
    if (pcb->unsent != nullptr || pcb->unacked != nullptr) { count++; continue; }
    if (pcb->state != ESTABLISHED) continue;
    bool waiting = false;
    for (uint8_t i = 0; i < idleCount; ++i) {
      if (pcb == idle[i].pcb && pcb->callback_arg == idle[i].owner) { waiting = true; break; }
    }
    if (!waiting) count++;
  }
  UNLOCK_TCPIP_CORE();
  return count;
}

// TASK-1605: resolve a hostname to an IPv4 address without Network/WiFi.hostByName().
// arduino-esp32 3.3.5 NetworkManager::hostByName() (NetworkManager.cpp:78-82) calls
// dns_clear_cache() without the lwIP core lock whenever its static "have IPv4/IPv6" flags
// change, which is always the first call after boot. dns_clear_cache() runs the found-callback
// of every pending DNS entry (dns.c:414), so a pending SNTP lookup of the NTP server then runs
// sntp_dns_found() -> sys_untimeout() on the caller's task and trips the core-lock assert
// (bench .63, beta.74, internet blocked: a panic every 5 min). NetworkClient::connect(host, ..)
// and HTTPClient's connect go through that same hostByName() (NetworkClient.cpp:310-315).
//
// lwip_getaddrinfo() is safe from any task: netconn_gethostbyname() runs the lookup through
// tcpip_send_msg_wait_sem(), which takes LOCK_TCPIP_CORE() (tcpip.c:446-448, this build has
// CONFIG_LWIP_TCPIP_CORE_LOCKING=y), and it never clears the cache. Never call this with the
// core lock held: the lock is not recursive.
//
// IPv4 only (AF_INET): every outbound target of this firmware is IPv4. A literal address
// (IPAddress::fromString) returns at once without DNS.
//
// Wait bound: lwip_getaddrinfo() has no timeout parameter and blocks the caller until the lwIP
// DNS timer gives up. Per configured DNS server that is DNS_MAX_RETRIES (4, lwIP opt.h default,
// not overridden) tries with waits of 1+1+2+3 DNS_TMR_INTERVAL (1000 ms) = 7 s (dns.c
// dns_check_entry), then the next non-zero server. CONFIG_LWIP_DNS_MAX_SERVERS=3 and no fallback
// server (CONFIG_LWIP_FALLBACK_DNS_SERVER_SUPPORT unset), so at most 3 x 7 s = 21 s, 7 s with
// one DHCP DNS server. A DNS server that answers (also NXDOMAIN) returns at once. A tighter hard
// bound would need a separate resolver task, which this shim does not have; callers that must
// not block run it on their own task.
inline bool platformResolveHost(const char *name, IPAddress &out) {
  out = IPAddress(static_cast<uint32_t>(0));
  if (name == nullptr || name[0] == '\0') return false;
  if (out.fromString(name)) return true;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  int err = lwip_getaddrinfo(name, nullptr, &hints, &res);
  if (err != 0 || res == nullptr) return false;
  const struct sockaddr_in *sin = reinterpret_cast<const struct sockaddr_in *>(res->ai_addr);
  out = IPAddress(sin->sin_addr.s_addr);
  lwip_freeaddrinfo(res);
  return static_cast<uint32_t>(out) != 0;
}

// TASK-1450 probe begin
// ICMP echo probe of the WiFi default gateway (dead-link check in networkStuff.ino).
// One raw ICMP pcb that exists only while a probe runs (Begin .. End, about 6 s).
//
// Thread safety: this build has CONFIG_LWIP_TCPIP_CORE_LOCKING=y and
// CONFIG_LWIP_CHECK_THREAD_SAFETY=y, so raw_new/raw_recv/raw_sendto/raw_remove assert that
// the caller holds the core lock (raw.c LWIP_ASSERT_CORE_LOCKED). Every lwIP call below runs
// between LOCK_TCPIP_CORE() and UNLOCK_TCPIP_CORE(). The lock is not recursive: call these
// only from the loop task, never with the lock held.
//
// The receive callback runs on the tcpip task, which holds the core lock while it processes
// input (ip4_input asserts it, ip4.c:504). It only reads the packet and sets a bit in
// replyMask: no logging, no allocation, no Arduino call. It never eats a packet (returns 0),
// so every ICMP packet, our own echo replies included, still reaches lwIP's ICMP code
// (icmp.c handles ICMP_ER as a no-op, and echo requests to the gateway keep being answered),
// and it never moves p->payload (raw.c asserts that for packets that are not eaten).
// target, seqBase and replyMask are written by Begin/End under the core lock and by the
// callback under that same lock, so the mutex orders them.
#define PLATFORM_GW_PROBE_ECHO_ID   0x4F54   // "OT"; other pingers use their own id
#define PLATFORM_GW_PROBE_ECHO_DATA 16       // payload bytes per echo request
#define PLATFORM_GW_PROBE_MAX_SEQ   8        // replies tracked per probe (bits in replyMask)

struct PlatformGatewayProbe {
  struct raw_pcb   *pcb;
  uint32_t          target;      // gateway, lwIP byte order
  uint16_t          seqBase;     // seq of the first echo of this probe
  volatile uint8_t  replyMask;   // bit n: echo (seqBase + n) answered by target
};

// Function-local static: one instance across every translation unit that includes this header.
inline PlatformGatewayProbe &platformGatewayProbeState() {
  static PlatformGatewayProbe s = { nullptr, 0, 0, 0 };
  return s;
}

// tcpip task, core lock held. Flags only.
inline u8_t platformGatewayProbeRecv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
  (void)pcb;
  PlatformGatewayProbe *s = static_cast<PlatformGatewayProbe *>(arg);
  if (s == nullptr || p == nullptr || addr == nullptr) return 0;
  if (ip_addr_get_ip4_u32(addr) != s->target) return 0;
  const uint16_t hlen = static_cast<uint16_t>((pbuf_get_at(p, 0) & 0x0F) * 4);
  struct icmp_echo_hdr hdr;
  if (pbuf_copy_partial(p, &hdr, sizeof(hdr), hlen) != sizeof(hdr)) return 0;
  if (ICMPH_TYPE(&hdr) != ICMP_ER || hdr.id != PP_HTONS(PLATFORM_GW_PROBE_ECHO_ID)) return 0;
  const uint16_t n = static_cast<uint16_t>(lwip_ntohs(hdr.seqno) - s->seqBase);
  if (n < PLATFORM_GW_PROBE_MAX_SEQ) s->replyMask = static_cast<uint8_t>(s->replyMask | (1u << n));
  return 0;
}

// Opens the probe pcb for target (lwIP byte order). Echoes are numbered from seqBase.
// False when a probe is already open or the pcb could not be allocated.
inline bool platformGatewayProbeBegin(uint32_t target, uint16_t seqBase) {
  PlatformGatewayProbe &s = platformGatewayProbeState();
  bool ok = false;
  LOCK_TCPIP_CORE();
  if (s.pcb == nullptr) {
    s.target    = target;
    s.seqBase   = seqBase;
    s.replyMask = 0;
    s.pcb = raw_new(IP_PROTO_ICMP);
    if (s.pcb != nullptr) {
      raw_recv(s.pcb, platformGatewayProbeRecv, &s);
      ok = true;
    }
  }
  UNLOCK_TCPIP_CORE();
  return ok;
}

// Sends echo request seq to the probe target. False only when the probe is not open or the
// echo buffer could not be allocated. A send that the stack or the WiFi driver refuses
// returns true: an unanswered echo is exactly what a dead link looks like.
inline bool platformGatewayProbeSend(uint16_t seq) {
  PlatformGatewayProbe &s = platformGatewayProbeState();
  const uint16_t len = static_cast<uint16_t>(sizeof(struct icmp_echo_hdr) + PLATFORM_GW_PROBE_ECHO_DATA);
  bool ok = false;
  LOCK_TCPIP_CORE();
  if (s.pcb != nullptr) {
    struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
    if (p != nullptr) {
      struct icmp_echo_hdr *echo = static_cast<struct icmp_echo_hdr *>(p->payload);
      ICMPH_TYPE_SET(echo, ICMP_ECHO);
      ICMPH_CODE_SET(echo, 0);
      echo->id     = PP_HTONS(PLATFORM_GW_PROBE_ECHO_ID);
      echo->seqno  = lwip_htons(seq);
      echo->chksum = 0;
      uint8_t *data = reinterpret_cast<uint8_t *>(echo) + sizeof(struct icmp_echo_hdr);
      for (uint16_t i = 0; i < PLATFORM_GW_PROBE_ECHO_DATA; i++) data[i] = static_cast<uint8_t>(i);
      echo->chksum = inet_chksum(echo, len);
      ip_addr_t dst;
      ip_addr_set_ip4_u32_val(dst, s.target);
      (void)raw_sendto(s.pcb, p, &dst);
      pbuf_free(p);
      ok = true;
    }
  }
  UNLOCK_TCPIP_CORE();
  return ok;
}

// Closes the probe pcb and returns the reply mask (0 when nothing answered or no probe was
// open). After raw_remove() under the lock the callback can no longer run for this probe.
inline uint8_t platformGatewayProbeEnd() {
  PlatformGatewayProbe &s = platformGatewayProbeState();
  uint8_t mask = 0;
  LOCK_TCPIP_CORE();
  if (s.pcb != nullptr) {
    raw_remove(s.pcb);
    s.pcb = nullptr;
    mask = s.replyMask;
  }
  s.replyMask = 0;
  UNLOCK_TCPIP_CORE();
  return mask;
}
// TASK-1450 probe end

// Exception cause from the last reset. ESP32 does not expose a direct
// numeric exccause via esp_reset_reason(); instead the reset_reason enum
// conveys the class of fault. We map panic/watchdog to non-zero sentinel
// values so the boot signature still flags abnormal reboots.
inline uint32_t platformExccause() {
  switch (esp_reset_reason()) {
    case ESP_RST_PANIC:    return 1U;  // software panic / exception
    case ESP_RST_INT_WDT:  return 2U;  // interrupt watchdog
    case ESP_RST_TASK_WDT: return 3U;  // task watchdog
    case ESP_RST_WDT:      return 4U;  // other watchdog
    case ESP_RST_BROWNOUT: return 5U;  // brownout
    default:               return 0U;  // clean boot / power-on / external / SW
  }
}

// Flash information
inline uint32_t platformFlashChipRealSize() {
  return ESP.getFlashChipSize();   // ESP32 has no "real" vs "configured" distinction
}

inline uint32_t platformFlashChipSize() {
  return ESP.getFlashChipSize();
}

inline uint32_t platformFlashChipSpeed() {
  return ESP.getFlashChipSpeed();
}

inline uint32_t platformFlashChipId() {
  // Read JEDEC flash chip ID via ESP-IDF esp_flash API (ESP32 / ESP32-S3)
  uint32_t id = 0;
  esp_flash_read_id(NULL, &id);  // NULL = default/main flash chip
  return id;
}

inline uint8_t platformFlashChipMode() {
  return 4;  // Not available on ESP32; return index for "Unknown" in flashMode[]
}

inline uint32_t platformSketchSize() {
  return ESP.getSketchSize();
}

inline uint32_t platformFreeSketchSpace() {
  return ESP.getFreeSketchSpace();
}

// Restart
inline void platformRestart() {
  ESP.restart();
}

// Mute the IDF/ROM console on UART0 and keep it on USB (TASK-972, TASK-1217).
// On PIC-carrying boards the primary IDF console (CONFIG_ESP_CONSOLE_UART_NUM=0)
// is the same UART0 that OTGWSerial drives into the PIC RX line (GPIO43), at
// the PIC's 9600 baud. Any console text sent there reaches the PIC: the PIC
// application answers stray lines with SE, and a single byte during the PIC
// bootloader's STX window aborts a PIC firmware flash.
//
// Console output takes three paths, each redirected here:
//  - ets_printf, Arduino log_e and the Arduino ESP_LOGx macros go through the
//    ROM putc channels: putc1 (UART0) is silenced, putc2 writes to the
//    USB-Serial-JTAG FIFO.
//  - IDF component logs (esp_log, e.g. the WiFi driver) go through the esp_log
//    vprintf hook, pointed at the same USB writer.
//  - plain printf goes through the VFS console, which writes UART0 and USB
//    together and cannot be split; stdout/stderr go to /dev/null, so printf
//    output is lost on USB as well. This build registers no /dev/usbserjtag.
//
// uartSetDebug(NULL) comes first. Arduino resets both putc channels to NULL
// whenever the UART that owns the log output is closed (uartEnd(0) ->
// uartSetDebug -> ets_install_putc2(NULL)), and OTGWSerial.end() closes UART0.
// With no UART owning the log, a later OTGWSerial.end() leaves putc2 alone.
extern "C" void ets_install_putc1(void (*p)(char c));
extern "C" void ets_install_putc2(void (*p)(char c));
inline void platformNullRomPutc(char) {}
// Non-blocking write to the USB-Serial-JTAG TX FIFO (64 bytes, sent per USB
// frame while a host reads the port). With no host, or a host that does not
// read, the FIFO stays full: the first character then waits at most 2 ms, and
// later characters are dropped without waiting until the FIFO drains again.
inline void platformUsbJtagPutc(char c) {
  static bool stalled = false;
  if (!usb_serial_jtag_ll_txfifo_writable()) {
    if (stalled) return;
    uint32_t t0 = micros();
    while (!usb_serial_jtag_ll_txfifo_writable()) {
      if (micros() - t0 > 2000) { stalled = true; return; }
    }
  }
  stalled = false;
  usb_serial_jtag_ll_write_txfifo(reinterpret_cast<const uint8_t *>(&c), 1);
  if (c == '\n') usb_serial_jtag_ll_txfifo_flush();
}
inline int platformUsbJtagLogVprintf(const char *fmt, va_list args) {
  char buf[256];
  int n = vsnprintf(buf, sizeof(buf), fmt, args);
  for (const char *p = buf; *p; p++) platformUsbJtagPutc(*p);
  return n;
}
inline void platformMuteUart0Console() {
  uartSetDebug(NULL);
  ets_install_putc1(platformNullRomPutc);
  ets_install_putc2(platformUsbJtagPutc);
  esp_log_set_vprintf(platformUsbJtagLogVprintf);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
}

// One WS2812-style RGB pixel through the core's RMT driver (TASK-1221).
// rgbOrder selects the LOLIN S3 Mini's RGB wiring; false = the common GRB.
inline void platformPixelWrite(uint8_t pin, bool rgbOrder, uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWriteOrdered(pin, rgbOrder ? LED_COLOR_ORDER_RGB : LED_COLOR_ORDER_GRB, r, g, b);
}

// Hardware random
inline uint32_t platformHardwareRandom() {
  return esp_random();
}

// RTC user memory — emulated via NVS Preferences on ESP32.
// WARNING: Unlike ESP8266 RTC memory (battery-backed SRAM, unlimited writes),
// NVS on ESP32 is flash-backed with finite erase/write cycles (10K-100K per
// sector). Only call platformRtcWrite() infrequently (boot, reset, deep sleep).
// Never use in a loop or on a timer. For high-frequency volatile storage on
// ESP32, consider RTC_DATA_ATTR variables instead.
inline bool platformRtcRead(uint32_t slot, uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", true)) return false;  // read-only
  char key[12];
  snprintf_P(key, sizeof(key), PSTR("s%u"), (unsigned)slot);
  size_t got = prefs.getBytes(key, data, len);
  prefs.end();
  return (got == len);
}

inline bool platformRtcWrite(uint32_t slot, const uint32_t *data, size_t len) {
  Preferences prefs;
  if (!prefs.begin("otgw_rtc", false)) return false;  // read-write
  char key[12];
  snprintf_P(key, sizeof(key), PSTR("s%u"), (unsigned)slot);
  prefs.putBytes(key, data, len);
  prefs.end();
  return true;
}

// Reset info
inline bool platformIsExternalReset() {
  return (esp_reset_reason() == ESP_RST_EXT);
}

// Seconds since boot from the 64-bit boot clock (TASK-1449, port of 1.x TASK-1289): counts setup()
// time and loop stalls too. Not micros(): that is 32-bit and wraps after ~71.6 minutes.
inline uint32_t platformUptimeSeconds() {
  return (uint32_t)(esp_timer_get_time() / 1000000LL);
}

inline uint32_t platformResetCode() {
  return (uint32_t)esp_reset_reason();
}

// TASK-1447: true when the chip restarted through esp_restart() (ESP_RST_SW), as doRestart() and the
// config-portal timeout do. False after power-on, panic, watchdog, brownout or an external reset.
inline bool platformResetWasSoftware() {
  return esp_reset_reason() == ESP_RST_SW;
}

// Reboot-log detail line for the reset that just happened (TASK-1479).
// The ESP32 reset info has no register dump; the crash is in the ESP-IDF core dump
// (coredump partition, ELF format, written by the panic handler, also on a task-watchdog
// panic). When a dump is present that was not reported before, buf gets
// "Core dump: <task> pc=0x.. bt=0x.. ..\r\n" (up to 16 backtrace PCs, as many as fit; the reboot log gives
// it REBOOT_LOG_LINE_MAX, room for all 16, TASK-1538; a trailing '!'
// marks a corrupted backtrace). Only the summary is read, not the whole dump. The dump
// stays in flash for an offline esp-coredump decode; a fingerprint in NVS keeps the same
// dump from being reported on every boot. The current reset reason is not checked, because
// an OTA after a crash boots with a software reset while the dump is still unreported.
// A panic or watchdog reset without a readable dump gives "Core dump: none (reset N)".
inline void platformResetRegisterDump(char *buf, size_t bufLen) {
  if (buf == nullptr || bufLen == 0) return;
  buf[0] = '\0';

  esp_core_dump_summary_t sum;
  size_t addr = 0, size = 0;
  bool haveDump = (esp_core_dump_image_check() == ESP_OK) &&
                  (esp_core_dump_image_get(&addr, &size) == ESP_OK) &&
                  (esp_core_dump_get_summary(&sum) == ESP_OK);
  if (haveDump) {
    uint32_t depth = sum.exc_bt_info.depth;
    if (depth > 16) depth = 16;
    uint32_t fp = (uint32_t)size ^ sum.exc_pc ^ (depth ? sum.exc_bt_info.bt[0] : 0U) ^
                  ((uint32_t)sum.app_elf_sha256[0] << 24) ^ ((uint32_t)sum.app_elf_sha256[1] << 16) ^
                  ((uint32_t)sum.app_elf_sha256[2] << 8) ^ (uint32_t)sum.app_elf_sha256[3];
    if (fp == 0) fp = 1;  // 0 is the "nothing reported yet" value
    uint32_t reported = 0;
    Preferences prefs;
    if (prefs.begin("otgw_cd", false)) {
      reported = prefs.getUInt("fp", 0);
      if (reported != fp) prefs.putUInt("fp", fp);
      prefs.end();
    }
    if (reported == fp) return;  // this dump is already in the reboot log

    sum.exc_task[sizeof(sum.exc_task) - 1] = '\0';
    sum.app_elf_sha256[sizeof(sum.app_elf_sha256) - 1] = 0;   // hex string of the crashing app's ELF
    int n = snprintf_P(buf, bufLen, PSTR("Core dump: %s pc=0x%08lx elf=%.8s bt="),
                       sum.exc_task, (unsigned long)sum.exc_pc, (const char *)sum.app_elf_sha256);
    if (n < 0 || (size_t)n >= bufLen) { buf[0] = '\0'; return; }
    size_t pos = (size_t)n;
    for (uint32_t i = 0; i < depth; i++) {
      if (pos + 11 + 4 > bufLen) break;  // " 0x12345678" + "!\r\n" + NUL must still fit
      n = snprintf_P(buf + pos, bufLen - pos, PSTR("%s0x%08lx"), i ? " " : "",
                     (unsigned long)sum.exc_bt_info.bt[i]);
      if (n < 0) break;
      pos += (size_t)n;
    }
    if (sum.exc_bt_info.corrupted && pos + 1 + 3 <= bufLen) buf[pos++] = '!';
    if (pos + 3 <= bufLen) { buf[pos++] = '\r'; buf[pos++] = '\n'; buf[pos] = '\0'; }
    return;
  }

  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
      reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
    snprintf_P(buf, bufLen, PSTR("Core dump: none (reset %d)\r\n"), (int)reason);
  }
}

// Exception cause — not available on ESP32 via esp_reset_reason() API
inline void platformResetExceptionInfo(char *buf, size_t bufLen) {
  buf[0] = '\0';
}

// TASK-1560: the stored core dump as a byte range, for GET /api/v2/device/coredump.
// platformCoreDumpSize() returns the length of the valid dump image (esp_core_dump_image_check()
// verifies its checksum, esp_core_dump_image_get() reports the length actually written, which is
// less than the 64 KB partition), or 0 when no valid dump is stored.
inline size_t platformCoreDumpSize() {
  size_t addr = 0, size = 0;
  if (esp_core_dump_image_check() != ESP_OK) return 0;
  if (esp_core_dump_image_get(&addr, &size) != ESP_OK) return 0;
  return size;
}

// Copy up to len bytes of the dump image, starting at byte offset, into dst. Returns the number of
// bytes copied (0 at or past the end of the image, or on a flash read error). The partition is found
// by type and subtype, no offset is hard-coded; every call is a bounded flash read, nothing is cached.
inline size_t platformCoreDumpRead(size_t offset, uint8_t* dst, size_t len) {
  size_t addr = 0, size = 0;
  if (dst == nullptr || esp_core_dump_image_get(&addr, &size) != ESP_OK) return 0;
  if (offset >= size) return 0;
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                         ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (part == nullptr || addr < part->address) return 0;
  size_t base = addr - part->address;
  if (base >= part->size || size > part->size - base) return 0;
  if (len > size - offset) len = size - offset;
  if (esp_partition_read(part, base + offset, dst, len) != ESP_OK) return 0;
  return len;
}

// TASK-1560: SHA-256 (lowercase hex) of the ELF of the app that wrote the stored dump, from the dump's
// own summary, not from the running build (the stored dump is usually from an older build). esp-coredump
// compares exactly this string with the sha256 of the .elf it is given. The IDF keeps only
// CONFIG_APP_RETRIEVE_LEN_ELF_SHA characters (9 by default), so dst may hold a prefix. Returns true and
// fills dst (NUL-terminated, at most len-1 characters) when the summary has hex digits; otherwise false
// and dst is empty.
inline bool platformCoreDumpAppSha(char* dst, size_t len) {
  if (dst == nullptr || len == 0) return false;
  dst[0] = '\0';
  if (len < 2) return false;
  esp_core_dump_summary_t sum;
  if (esp_core_dump_image_check() != ESP_OK || esp_core_dump_get_summary(&sum) != ESP_OK) return false;
  size_t n = 0;
  for (; n < sizeof(sum.app_elf_sha256) && n + 1 < len; n++) {
    char c = (char)sum.app_elf_sha256[n];
    if (c >= 'A' && c <= 'F') c = (char)(c + ('a' - 'A'));
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) break;
    dst[n] = c;
  }
  dst[n] = '\0';
  return n > 0;
}

// platformRestartDHCP() removed in TASK-432 port. Although the ESP32
// esp_netif_dhcpc_stop/start API differs from ESP8266's SDK calls, the
// underlying ownership-takeover concern still applies: once user code
// manages DHCP explicitly, autonomous SDK reassociation may not restart
// it correctly. The ESP8266 fix removed the call sites in networkStuff.ino;
// this header drops the function for symmetry across platforms.

// Serial error checks (ESP32 HardwareSerial does not expose overrun/rx error)
inline bool platformSerialHasOverrun(HardwareSerial &serial) {
  (void)serial;
  return false;
}

inline bool platformSerialHasRxError(HardwareSerial &serial) {
  (void)serial;
  return false;
}

// ---- Settings no-op fingerprint (TASK-564) -------------------------------
// ESP32 retains a full-struct byte snapshot (memcmp) rather than a CRC sentinel:
// DRAM is plentiful and a byte-exact compare avoids CRC collisions, and the
// ESP8266 SDK crc32() is not part of the ESP32 Arduino API. The buffer is
// allocated once (settings size is fixed) and reused. Not re-entrant by
// contract: updateSetting() captures at entry and compares once before return.
// Fail-safe: if allocation fails, "unchanged" returns false so the caller
// treats the settings as dirty and writes them.
struct _PlatformNoopSnap { uint8_t *buf; size_t len; };
inline _PlatformNoopSnap &_platformSettingsNoopSnap() {
  static _PlatformNoopSnap s{nullptr, 0};
  return s;
}

inline void platformSettingsNoopCapture(const void *data, size_t len) {
  _PlatformNoopSnap &s = _platformSettingsNoopSnap();
  if (s.len != len) {
    free(s.buf);
    s.buf = static_cast<uint8_t *>(malloc(len));
    s.len = s.buf ? len : 0;
  }
  if (s.buf) memcpy(s.buf, data, len);
}

inline bool platformSettingsNoopUnchanged(const void *data, size_t len) {
  _PlatformNoopSnap &s = _platformSettingsNoopSnap();
  return s.buf && s.len == len && memcmp(s.buf, data, len) == 0;
}

// ---- ADR-123 concurrency primitives (TASK-865.5) -------------------------
// FreeRTOS-backed mutex and value-copy queue shims. Application code uses the
// opaque PlatformMutex / PlatformQueue handles and the platformXxx() calls
// below, never the raw FreeRTOS API — so the OTGWState mutex and the OT-frame
// producer/consumer queue stay platform-neutral. On ESP32 these map directly
// onto the IDF FreeRTOS objects (freertos/FreeRTOS.h is pulled in by Arduino.h).
//
// Phase 1 (this task) creates ONE mutex and ONE queue in setup() (ADR-044) and
// runs the consumer cooperatively from loop(); seq6 lifts the producer into a
// FreeRTOS task. The shims already provide the real synchronisation so that
// lift needs no further primitive work.
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

using PlatformMutex = SemaphoreHandle_t;
using PlatformQueue = QueueHandle_t;

// Create a non-recursive mutex. Returns nullptr on allocation failure; callers
// must treat that as fatal-at-setup (a missing mutex means no cross-task
// protection). Non-recursive by design: a single call chain must never take it
// twice (would self-deadlock even single-threaded).
inline PlatformMutex platformMutexCreate() {
  return xSemaphoreCreateMutex();
}

// Take the mutex, blocking up to timeoutMs. timeoutMs == 0 means block forever
// (portMAX_DELAY). Returns true if acquired. A nullptr handle returns true
// (no-op lock) so a failed-create path degrades to "unprotected" rather than
// "always blocked".
inline bool platformMutexLock(PlatformMutex m, uint32_t timeoutMs = 0) {
  if (m == nullptr) return true;
  TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  return xSemaphoreTake(m, ticks) == pdTRUE;
}

inline void platformMutexUnlock(PlatformMutex m) {
  if (m == nullptr) return;
  xSemaphoreGive(m);
}

// Recursive mutex: the owning task may take it again inside the same call chain (the PIC command queue, TASK-1569:
// handleCommandQueue -> pmChainTick -> addCommandToQueue). Same nullptr and timeout rules as the plain mutex above;
// timeoutMs == 0 blocks forever.
inline PlatformMutex platformRecursiveMutexCreate() {
  return xSemaphoreCreateRecursiveMutex();
}

inline bool platformRecursiveMutexLock(PlatformMutex m, uint32_t timeoutMs = 0) {
  if (m == nullptr) return true;
  TickType_t ticks = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  return xSemaphoreTakeRecursive(m, ticks) == pdTRUE;
}

inline void platformRecursiveMutexUnlock(PlatformMutex m) {
  if (m == nullptr) return;
  xSemaphoreGiveRecursive(m);
}

// Create a value-copy queue of `length` items, each `itemSize` bytes. The queue
// copies by value on send/receive (no shared pointers, no mutex needed for the
// item flow). Returns nullptr on allocation failure.
inline PlatformQueue platformQueueCreate(size_t length, size_t itemSize) {
  return xQueueCreate((UBaseType_t)length, (UBaseType_t)itemSize);
}

// Non-blocking send (copy `item` into the queue). Returns false if the queue is
// full or the handle is null. Callers size the queue so it cannot fill in
// normal operation and treat a false result as a diagnostic drop, never a
// fall-back to direct processing (which would reorder the FIFO).
inline bool platformQueueSend(PlatformQueue q, const void *item) {
  if (q == nullptr) return false;
  return xQueueSend(q, item, 0) == pdTRUE;
}

// Non-blocking send to the FRONT of the queue (copy `item` ahead of every item
// already queued). Returns false if the queue is full or the handle is null.
// Used to restore FIFO order after a flow-control short write: the popped chunk
// is put back at the head so it is the next thing written, never reordered
// behind chunks that were enqueued after it (TASK-865.15 TX-ordering invariant).
inline bool platformQueueSendToFront(PlatformQueue q, const void *item) {
  if (q == nullptr) return false;
  return xQueueSendToFront(q, item, 0) == pdTRUE;
}

// Sentinel for platformQueueReceive: block indefinitely until an item arrives
// (FreeRTOS portMAX_DELAY). An event-consumer task that idles with zero CPU
// between events (e.g. the webhook sender task, TASK-865.13) passes this instead
// of a finite poll. pdMS_TO_TICKS(UINT32_MAX) would overflow to a garbage tick
// count, so the sentinel maps straight to portMAX_DELAY rather than converting.
#define PLATFORM_QUEUE_WAIT_FOREVER 0xFFFFFFFFUL

// Receive one item (copy out). timeoutMs == 0 polls without blocking;
// timeoutMs == PLATFORM_QUEUE_WAIT_FOREVER blocks indefinitely. Returns true if
// an item was dequeued.
inline bool platformQueueReceive(PlatformQueue q, void *item, uint32_t timeoutMs = 0) {
  if (q == nullptr) return false;
  TickType_t ticks;
  if (timeoutMs == 0) {
    ticks = 0;
  } else if (timeoutMs == PLATFORM_QUEUE_WAIT_FOREVER) {
    ticks = portMAX_DELAY;
  } else {
    ticks = pdMS_TO_TICKS(timeoutMs);
  }
  return xQueueReceive(q, item, ticks) == pdTRUE;
}

// ---- ADR-123 dedicated-task primitives (TASK-865.6) ----------------------
// The PIC UART moves onto its own FreeRTOS task pinned to the application core.
// Application code uses these shims and the opaque PlatformTask handle, never
// the raw xTaskCreatePinnedToCore / vTaskDelay symbols, so the dedicated-task
// machinery stays behind the platform abstraction (no raw FreeRTOS in *.ino).
using PlatformTask = TaskHandle_t;

// Pin the task to the application core. APP_CPU_NUM is core 1 on a dual-core
// ESP32-S3; the protocol core (0) is left for the WiFi/BT stack and AsyncTCP.
// stackBytes is in BYTES (xTaskCreate wants words for some ports but the IDF
// Arduino wrapper takes a byte count). priority 1 keeps it just above the idle
// task; the loop()/Arduino task runs at priority 1 too, so the PIC task does
// not pre-empt it indefinitely (both yield via the task delay below).
// Returns nullptr on creation failure.
inline PlatformTask platformTaskCreatePinned(void (*fn)(void *),
                                             const char *name,
                                             uint32_t stackBytes,
                                             void *arg,
                                             unsigned int priority) {
  TaskHandle_t h = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stackBytes, arg,
                                          (UBaseType_t)priority, &h, APP_CPU_NUM);
  return (ok == pdPASS) ? h : nullptr;
}

// Block the calling task for ms milliseconds. This is the cooperative yield
// point inside the PIC task loop: it MUST block (vTaskDelay) between drains so
// it never busy-polls available() and starves the core / trips the TWDT. A 0 ms
// delay still yields one tick (vTaskDelay(0) is a no-op, so clamp to >=1 tick).
inline void platformTaskDelay(uint32_t ms) {
  TickType_t ticks = pdMS_TO_TICKS(ms);
  if (ticks == 0) ticks = 1;
  vTaskDelay(ticks);
}

// End the calling task and free its stack (TASK-1604: one-shot worker tasks such as the
// weather fetch). Must be the last statement of the task body; it does not return.
inline void platformTaskExit() {
  vTaskDelete(nullptr);
}

/***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
****************************************************************************
*/

#endif // PLATFORM_ESP32_H

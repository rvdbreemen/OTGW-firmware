# TASK-2026: why a starved .61 may miss the 5-minute group-key rekey (source research)

Date: 2026-11-19. Author: 5e (Claude). Status: research, no code change. A fix is a separate task for Robert.

## Question

09 sees that .61 (no PSRAM, BLE on), held starved by the 'H' hook (free < 8 KB, largest block < 4 KB), does not answer
the AP's group-key rekey (MikroTik wlan2 `group-key-update=5m`): the AP kicks it with "group key exchange timeout"
every 5 min. Which allocations does the rekey need, and is there a cheap reservation that guarantees it?

## Sources and pin

- Precompiled Arduino libs: `framework-arduinoespressif32-libs/versions.txt`: `esp-idf: release/v5.5 9bb7aa84fe`.
  The combo uses `board_build.arduino.memory_type = qio_qspi` (platformio.ini:26), so the config is
  `esp32s3/qio_qspi/include/sdkconfig.h`.
- IDF source: the five files cited below were downloaded from `espressif/esp-idf` at `9bb7aa84fe` and are
  byte-identical to the local `framework-espidf` 5.5.1 copy (`cmp`, 2026-11-19). Line numbers are at that commit.
- The WiFi driver itself (libnet80211 / libpp, where data frames are received, copied and dropped) is a closed
  binary. Anything inside it is marked "not visible in source".

## The config the firmware runs with (qio_qspi sdkconfig.h)

| Setting | Value | Meaning on .61 (no PSRAM) |
|---|---|---|
| `CONFIG_SPIRAM` / `SPIRAM_TRY_ALLOCATE_WIFI_LWIP` | 1 / 1 | libs built for PSRAM; WiFi allocs try PSRAM first, then internal |
| `SPIRAM_IGNORE_NOTFOUND` | not set | Dynamic TX is not selectable (Kconfig:82, `depends on !(SPIRAM_TRY_ALLOCATE_WIFI_LWIP && !SPIRAM_IGNORE_NOTFOUND)`) |
| `ESP_WIFI_STATIC_RX_BUFFER_NUM` | 8 | ~1.6 KB each (Kconfig:26), allocated at `esp_wifi_init`, never freed: the hardware RX DMA buffers |
| `ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` | 32 | a COUNT limit; each received data frame is copied into a heap buffer of the frame's size (Kconfig:42-52) |
| `ESP_WIFI_STATIC_TX_BUFFER` / `_NUM` | y / 8 | ~1.6 KB each, preallocated; every TX data frame is copied into one |
| `ESP_WIFI_CACHE_TX_BUFFER_NUM` | 16 | queue for TX frames when no static TX buffer is free (Kconfig:105) |
| `ESP_WIFI_STATIC_RX_MGMT_BUFFER` / `RX_MGMT_BUF_NUM_DEF` | y / 5 | management frames only (Kconfig:145, :156); EAPOL is NOT a management frame |
| `MBEDTLS_INTERNAL_MEM_ALLOC` | y | supplicant crypto contexts come from internal RAM |
| `WPA_DEBUG_PRINT` / `LOG_DEFAULT_LEVEL` | off / 1 (error) | no supplicant debug output is available from these libs |

So about 16 x 1.6 KB = ~25 KB of RX/TX frame buffers is already reserved permanently in internal RAM on .61.

## What the group-key handshake allocates (open-source part)

EAPOL-Key frames are 802.11 *data* frames (ethertype 0x888E), not management frames.

1. RX of message 1/2: the driver copies the frame (about 150-200 B) into a dynamic RX buffer
   (Kconfig:52). The allocator it uses is inside the blob (not visible in source); the adapter table offers
   `wifi_malloc` = SPIRAM-preferred, internal fallback (`esp32s3/esp_adapter.c:74`) and internal-DMA wrappers
   (`:412-426`). Without PSRAM all of these land in internal RAM.
2. `wpa_sm_rx_eapol` (`src/rsn_supp/wpa.c:1848`): MIC check through mbedTLS HMAC. `hmac_vector`
   `mbedtls_md_setup(&md_ctx, md_info, 1)` (`esp_supplicant/src/crypto/crypto_mbedtls.c:325`) allocates the hash
   context plus the HMAC ipad/opad (2 x 64 B), internal (`MBEDTLS_INTERNAL_MEM_ALLOC`). A few hundred bytes.
3. Key-data decrypt (AES key unwrap): done IN PLACE, the malloc is commented out (`wpa.c:1767-1776`,
   "replaced by xxx to remove malloc"). The AES context is `os_malloc(sizeof(*aes))` (`crypto_mbedtls.c:407`), small.
4. GTK install: into the driver (blob, not visible).
5. Reply 2/2: `wpa_supplicant_send_2_of_2` -> `wpa_sm_alloc_eapol` (`wpa.c:1581`) -> `os_malloc(*msg_len +
   sizeof(struct l2_ethhdr))` (`esp_wpas_glue.c:26`): 14 + 4 + 95 = about 113 B. Sent with
   `esp_wifi_internal_tx(WIFI_IF_STA, ...)` (`esp_wpas_glue.c:71`), which copies it into one of the 8 static TX
   buffers (or the 16-deep cache queue): no heap allocation for the frame itself.

## Conclusion

- Every heap allocation of the rekey that is visible in source is small: about 113 B for the reply, a few hundred
  bytes for the mbedTLS HMAC and AES contexts, and a frame-sized RX copy (~200 B). The 'H' hold keeps free >= 4 KB
  and the largest block around 2 KB, so **the source does not show any rekey allocation failing in that state**.
  "Starved heap -> rekey malloc fails" is NOT supported by the source.
- Competing explanations, all inside the closed driver (not visible in source, no citation possible):
  a. the 32 dynamic RX buffers are a count limit: if lwIP does not drain its 32-deep recvmbox
     (`CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=32`) because the loop/async_tcp tasks are slow under heap pressure, new data
     frames (EAPOL included) are dropped while beacons (static mgmt buffers) still arrive: the link looks alive;
  b. all 8 static TX buffers + 16 cache entries busy, so the 2/2 reply is not sent in time;
  c. the driver may refuse an RX copy below some internal free-heap threshold of its own (speculation, not
     visible in source).
- **Cheap guaranteed reservation: none found.** The supplicant has no pool to reserve. A larger
  `DYNAMIC_RX_BUFFER_NUM` raises a limit, it reserves nothing. Any sdkconfig change needs `custom_sdkconfig` and the
  hybrid compile (slow, and it broke on the TLS PoC). An app-side reserve block that is freed when the heap is low
  gets no warning before message 1/2 arrives. A longer `group-key-update` on the router only hides it, and does
  nothing for field users. The effective lever is the one TASK-2026 already pulls: do not let the heap stay starved.

## Discriminating bench checks for 09 (non-blocking, .61 already on 391)

1. Baseline: does an UNstarved .61 on the same AP pass several 5-minute rekeys? If it fails there too, the heap is
   not the cause.
2. The disconnect reason the firmware logs (`networkStuff.ino:91-95`, name via `WiFi.disconnectReasonName`):
   16 = GROUP_KEY_UPDATE_TIMEOUT, or something else (e.g. a beacon timeout).
3. Hold variants with the hook: free low but largest block high, and the other way round, to see which one
   triggers the miss.
4. `esp_wifi_statis_dump(WIFI_STATIS_BUFFER | WIFI_STATIS_RXTX)` (`esp_wifi.h:1395`) before and during the
   hold would show the driver's buffer and drop counters. CAUTION: it prints through ESP_LOG to UART0 (the PIC
   link on a Classic; on OTGW32 the USB console), and with `LOG_DEFAULT_LEVEL=1` it may print nothing. It needs a
   prerelease hook to call it; not added.

## Not verified

- Anything inside libnet80211 / libpp (RX copy allocator, drop rules, thresholds).
- The real frame sizes on .61's AP (estimated from the EAPOL-Key layout, not captured).

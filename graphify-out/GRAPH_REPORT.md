# Graph Report - .  (2026-07-26)

## Corpus Check
- 76 files · ~190,902 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1628 nodes · 3220 edges · 93 communities (89 shown, 4 thin omitted)
- Extraction: 93% EXTRACTED · 7% INFERRED · 0% AMBIGUOUS · INFERRED: 216 edges (avg confidence: 0.73)
- Token cost: 592,087 input · 0 output

## Community Hubs (Navigation)
- OpenTherm Protocol Library
- Async Telnet Transport
- Web UI Core (index.js)
- SAT Dashboard Logic
- V2 Dashboard Shell
- Debug & Update HTML Helpers
- Chunked JSON Emit
- HA Discovery Types
- Board & Hardware Types
- Telnet Core State
- PIC Firmware Upgrade Events
- PIC Upgrade Transfer
- Web UI Log Viewer
- V2 SAT Fetch & Charts
- Async WebServer Compat
- SAT MQTT Publish
- Settings Sections
- Telnet Client API
- Network & PIC Settings
- OTGW Serial Debug
- Sync Telnet Transport
- Gateway Mode UI Controls
- HA Discovery Topics
- Resizable Table UI
- V2 Theme & Downloads
- ESP32 OTA Update Server
- BLE Sensor UI
- HA Discovery Streaming
- MQTT Settings Section
- Firmware Page UI
- SAT Switch Discovery
- OpenTherm Data Model
- Device Info UI
- Main Page Init
- Heap Diagnostics Counters
- SimpleTelnet Documentation
- V2 PIC & Status Fetch
- V2 BLE & FS Explorer
- V2 Settings Forms
- REST Performance Metrics
- PIC Flash UI Flow
- MQTT JSON Writer
- PIC Info Structures
- IP Octet Inputs
- OT Log WebSocket
- V2 Onboarding Flow
- V2 BLE Discovery Cards
- OT Frame Status Flags
- OT Message Lookup
- PIC Settings Cache
- V2 Monitor Log
- OTA Upload Handlers
- ECharts Theming
- SAT Appliance Overrides
- V2 PIC File Manager
- Discovery Verify Counters
- Filesystem Info Shim
- Telnet Negotiation Bytes
- Telnet Negotiation Flow
- MQTT Discovery Verification
- Web Server Setup & Auth
- PIC Firmware Typing
- Platform Mutex Shims
- Safe Timers
- Webhook Job Types
- Webhook Settings
- FSexplorer UI Page
- OpenTherm Library Docs
- Platform Queue Shims
- OT Command Queue
- OT Override Entries
- Board Name Helpers
- Sequential Asset Loader
- Telnet Printf Helpers
- Telnet Input Filtering
- Download Icon Asset
- SAT Slider Widget
- Web Request Body
- Serial Error Shims
- Platform Noop Snapshot
- FSexplorer Icon Asset
- Refresh Icon Asset
- Settings Icon Asset
- System Update Icon
- Theme Toggle Script
- Update Icon Asset
- OT TX Message
- Platform Task Shim
- Null Log Shim
- Telnet Negotiation Config

## God Nodes (most connected - your core abstractions)
1. `index.html (classic Web UI shell)` - 271 edges
2. `v2.html (v2 dashboard shell)` - 216 edges
3. `OpenTherm` - 73 edges
4. `SimpleTelnetCore` - 56 edges
5. `OTGWUpgrade` - 51 edges
6. `init()` - 46 edges
7. `OTGWSerial` - 42 edges
8. `HaDiscoveryContext` - 37 edges
9. `AsyncSimpleTelnet` - 37 edges
10. `OTGWUpdateServer` - 34 edges

## Surprising Connections (you probably didn't know these)
- `index.html (classic Web UI shell)` --indirect_call--> `clearStoredData()`  [INFERRED]
  src/OTGW-firmware/data/index.html → src/OTGW-firmware/data/index.js
- `index.html (classic Web UI shell)` --indirect_call--> `formatGatewayModeDisplayValue()`  [INFERRED]
  src/OTGW-firmware/data/index.html → src/OTGW-firmware/data/index.js
- `index.html (classic Web UI shell)` --indirect_call--> `initMainPage()`  [INFERRED]
  src/OTGW-firmware/data/index.html → src/OTGW-firmware/data/index.js
- `index.html (classic Web UI shell)` --indirect_call--> `restoreDataFromLocalStorage()`  [INFERRED]
  src/OTGW-firmware/data/index.html → src/OTGW-firmware/data/index.js
- `index.html (classic Web UI shell)` --indirect_call--> `saveDataToLocalStorage()`  [INFERRED]
  src/OTGW-firmware/data/index.html → src/OTGW-firmware/data/index.js

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Sequential asset loading to respect the device serve gate** — src_otgw_firmware_data_index_sequential_asset_loader, src_otgw_firmware_data_v2_sequential_loader, src_otgw_firmware_data_v2_fontface_sequential, concept_web_file_max_inflight [EXTRACTED 0.95]
- **SimpleTelnet dual-transport architecture on a shared core** — concept_simpletelnet_sync, concept_asyncsimpletelnet, concept_simpletelnetcore [EXTRACTED 1.00]
- **OTGW Web UI page family sharing the design-token theme model** — src_otgw_firmware_data_index, src_otgw_firmware_data_v2, src_otgw_firmware_data_fsexplorer, src_otgw_firmware_data_design [INFERRED 0.85]

## Communities (93 total, 4 thin omitted)

### Community 0 - "OpenTherm Protocol Library"
Cohesion: 0.06
Nodes (82): gptimer_alarm_event_data_t, gptimer_handle_t, OpenThermMessageID, OpenThermMessageType, OpenThermRxStatus, OpenThermSmartPower, OpenThermTxStatus, function (+74 more)

### Community 1 - "Async Telnet Transport"
Cohesion: 0.06
Nodes (55): AsyncClient, AsyncServer, Ctx, SemaphoreHandle_t, SIMPLETELNET_RX_BUF_LEN, SIMPLETELNET_TX_BUF_LEN, AsyncSimpleTelnet, _attachClient (+47 more)

### Community 2 - "Web UI Core (index.js)"
Cohesion: 0.05
Nodes (55): ADR-0091, ADR-0113, ADR-0116, ADR-0123, ADR-0132, design.html (design system component reference page), index.html (classic Web UI shell), availableFirmwareFiles (+47 more)

### Community 3 - "SAT Dashboard Logic"
Cohesion: 0.08
Nodes (60): addClass(), addMarkerAtClick(), adjustTarget(), _approxChanged(), buildCurrentPointData(), buildCurveOption(), calcHeatingCurve(), checkWeatherNeedsSetup() (+52 more)

### Community 4 - "V2 Dashboard Shell"
Cohesion: 0.07
Nodes (56): ADR-0056, ADR-0147, ADR-0151, ADR-0155, v2.html (v2 dashboard shell), connDetailRow(), connRecency(), dirBadge() (+48 more)

### Community 5 - "Debug & Update HTML Helpers"
Cohesion: 0.05
Nodes (42): catch, delayMs, formId, onReady, onStatus, options, prefix, WiFiClient (+34 more)

### Community 6 - "Chunked JSON Emit"
Cohesion: 0.08
Nodes (25): K, RestEmitFn, Print, RestChunkWindow, _cap, _out, _pos, _start (+17 more)

### Community 7 - "HA Discovery Types"
Cohesion: 0.06
Nodes (36): HaBinaryPayload, HaDeviceClass, HaEntityCat, HaIcon, HaStateClass, HaUnit, PGM_P, MqttHaBinSensorCfg (+28 more)

### Community 8 - "Board & Hardware Types"
Cohesion: 0.07
Nodes (16): ComboPinMap, activeButton(), activeI2cScl(), activeI2cSda(), activeLed1(), activeLed2(), activePicRst(), comboActivePinMap() (+8 more)

### Community 10 - "Telnet Core State"
Cohesion: 0.05
Nodes (38): SimpleTelnetCore, _attemptIp, _clientActive, clientIP, connectedCount, _extractIP, _feed, getIP (+30 more)

### Community 11 - "PIC Firmware Upgrade Events"
Cohesion: 0.06
Nodes (34): OTGWFirmwareReport, OTGWUpgradeFinished, OTGWUpgradeProgress, OTGWFirmwareReport, OTGWProcessor, OTGWUpgradeFinished, OTGWUpgradeProgress, OTGWSerial (+26 more)

### Community 12 - "PIC Upgrade Transfer"
Cohesion: 0.07
Nodes (32): OTGWTransferData, File, OTGWUpgrade, buffer, bufpos, checksum, cmdcode, codemem (+24 more)

### Community 13 - "Web UI Log Viewer"
Cohesion: 0.11
Nodes (32): addLogLine(), calculateOptimalMaxLines(), checkFileRotation(), clearStoredData(), downloadLog(), enqueueLogLine(), estimateMemoryUsage(), forceDownloadBlob() (+24 more)

### Community 14 - "V2 SAT Fetch & Charts"
Cohesion: 0.13
Nodes (31): arcPath(), fetchSatMarkers(), fetchSatPage(), fetchSatPageWeather(), fetchWithRetry(), polar(), renderB(), renderSatPage() (+23 more)

### Community 15 - "Async WebServer Compat"
Cohesion: 0.11
Nodes (23): AsyncResponseStream, AsyncWebServerResponse, HTTPMethod, strlcpy_P(), hasSpareAppOtaSlot(), argCompat(), bodyCompat(), __FlashStringHelper (+15 more)

### Community 16 - "SAT MQTT Publish"
Cohesion: 0.13
Nodes (28): platformHardwareRandom(), __FlashStringHelper, publishIfChangedB(), publishIfChangedBStr(), publishIfChangedF(), publishIfChangedI(), publishIfChangedS(), publishJsonAttrIfChanged() (+20 more)

### Community 17 - "Settings Sections"
Cohesion: 0.07
Nodes (29): DeviceSection, NTPSection, OutputsSection, PICBootSection, S0Section, SATSection, SensorsSection, OTGWSettings (+21 more)

### Community 18 - "Telnet Client API"
Cohesion: 0.08
Nodes (11): IPAddress, SimpleTelnetCallback, SimpleTelnetCore<MAX_CLIENTS>::_extractIP(), SimpleTelnetCore<MAX_CLIENTS>::_handleCharInput(), SimpleTelnetCore<MAX_CLIENTS>::_handleLineInput(), SimpleTelnetCore<MAX_CLIENTS>::onConnect(), SimpleTelnetCore<MAX_CLIENTS>::onConnectionAttempt(), SimpleTelnetCore<MAX_CLIENTS>::onDisconnect() (+3 more)

### Community 19 - "Network & PIC Settings"
Cohesion: 0.07
Nodes (28): FlashSection, HardwareSection, NetworkSection, OTBusState, PICSection, PicSettingsSection, SATRuntimeSection, MQTTRuntimeSection (+20 more)

### Community 20 - "OTGW Serial Debug"
Cohesion: 0.16
Nodes (23): OTGWDebugFunction, OTGWError, byte, OTGWSerial::registerDebugFunc(), startUpgrade, eraseCode, finishUpgrade, fwCommand (+15 more)

### Community 21 - "Sync Telnet Transport"
Cohesion: 0.08
Nodes (25): MAX_CLIENTS, WiFiClient, SimpleTelnet, _acceptNewClients, available, begin, _checkKeepAlive, _clients (+17 more)

### Community 22 - "Gateway Mode UI Controls"
Cohesion: 0.14
Nodes (26): applyOTGWSimulationState(), applyParsedGatewayMode(), applyPSmodeState(), closeInlineSensorLabelEditor(), exitFlashMode(), fetchDallasLabels(), fetchWithRetry(), formatGatewayModeDisplayValue() (+18 more)

### Community 23 - "HA Discovery Topics"
Cohesion: 0.16
Nodes (25): buildBinSensorDiscoveryTopic(), buildSensorDiscoveryTopic(), clearTopologyDiscoveryForOTId(), composeSensorPayload(), HaDevice, HaDeviceClass, HaEntityCat, HaIcon (+17 more)

### Community 24 - "Resizable Table UI"
Cohesion: 0.17
Nodes (25): applyStoredTableColWidths(), applyTableColWidths(), ensureResizableTableColgroup(), escapeHtml, fitTableColumnsToContent(), getResizableTable(), getResizableTableConfig(), initOtSupportColResizers() (+17 more)

### Community 25 - "V2 Theme & Downloads"
Cohesion: 0.13
Nodes (25): applyFrame(), applyTheme(), closeViewMenu(), connectWs(), downloadBlob(), downloadLog(), exportCsv(), exportPng() (+17 more)

### Community 26 - "ESP32 OTA Update Server"
Cohesion: 0.10
Nodes (16): OTGWUpdateServer, _authenticated, _password, _routesRegistered, _serial_output, _server, _serverIndex, _serverSuccess (+8 more)

### Community 27 - "BLE Sensor UI"
Cohesion: 0.16
Nodes (21): bleForget(), bleIsValidMac(), bleNameConfirmedMismatchJs(), blePost(), bleRescan(), bleRowFor(), bleSaveLabel(), bleSaveNamePrefix() (+13 more)

### Community 28 - "HA Discovery Streaming"
Cohesion: 0.49
Nodes (20): ComposeFn, platformFreeHeap(), composeBinSensorPayload(), haSourcePrefix(), measureMallocPublish(), streamButtonDiscovery(), streamClimateDiscovery(), streamDallasSensorDiscovery() (+12 more)

### Community 29 - "MQTT Settings Section"
Cohesion: 0.10
Nodes (20): MQTTSettingsSection, bDiscoveryAutoVerify, bEnable, bHaRebootDetect, bLastPublishedLegacy, bLegacyMode, bLegacyPort25238Enabled, bOnChangePublishing (+12 more)

### Community 30 - "Firmware Page UI"
Cohesion: 0.20
Nodes (19): deviceinfoPage(), disconnectOTLogWebSocket(), enterFlashMode(), firmwarePage(), refreshCrashLogInfo(), refreshSatPvBoostBadge(), refreshWebhookPage(), renderCrashLogInfo() (+11 more)

### Community 31 - "SAT Switch Discovery"
Cohesion: 0.11
Nodes (19): streamSatSwitchDiscovery(), HaDevice, HaDiscoveryContext, device, haPrefix, hostname, isFirstEntity, legacyMode (+11 more)

### Community 32 - "OpenTherm Data Model"
Cohesion: 0.11
Nodes (18): byte, OpenthermData_t, bAnswerOverride, bGatewaySubstituted, buf, f88, id, len (+10 more)

### Community 33 - "Device Info UI"
Cohesion: 0.15
Nodes (17): applyOTDirectAvailability(), applyPICAvailability(), buildWifiScanPanel(), formatDeviceInfoLabel(), formatDeviceInfoValue(), getHttpPasswordPlaceholderLength(), getOriginalPasswordPrefill(), getSettingsGroupId() (+9 more)

### Community 34 - "Main Page Init"
Cohesion: 0.16
Nodes (17): applyTheme(), checkFSMismatch(), closeAdvDropdown(), ensureWebkitScrollbarStyles(), handleOTLogResize(), initMainPage(), isPasswordPlaceholderField(), renderSharedPageNavShell() (+9 more)

### Community 35 - "Heap Diagnostics Counters"
Cohesion: 0.12
Nodes (17): HeapDiagSection, aMaxBlockBucket, iDripActiveBurstSkipCount, iDripCooldownSkipCount, iDripSlowModeCount, iEnteredCriticalCount, iEnteredLowCount, iEnteredWarningCount (+9 more)

### Community 36 - "SimpleTelnet Documentation"
Cohesion: 0.16
Nodes (16): AsyncSimpleTelnet<N> event-driven transport (AsyncTCP, ESP32-only), ESPTelnet library (Lennart Hennigs), No-String / const char* callback design (no heap per keystroke), SimpleTelnet<N> synchronous transport (WiFiServer/WiFiClient), SimpleTelnetCore shared protocol core (RFC 854 negotiation), TelnetStream library (Juraj Andrassy), SimpleTelnet API.md (full API reference), SimpleTelnet BACKLOG.md (+8 more)

### Community 37 - "V2 PIC & Status Fetch"
Cohesion: 0.16
Nodes (16): applyPicTabVisibility(), checkPicUpdate(), fetchDebug(), fetchPicSettings(), fetchSystemStatus(), maybePromptSimulation(), picModeText(), picUpdatePoll() (+8 more)

### Community 38 - "V2 BLE & FS Explorer"
Cohesion: 0.20
Nodes (16): bleAction(), bleToast(), clearBleRoster(), fetchBle(), fetchFsList(), fsDelete(), fsDoUpload(), fsFmtBytes() (+8 more)

### Community 39 - "V2 Settings Forms"
Cohesion: 0.17
Nodes (16): catById(), catFor(), curVal(), discardSettings(), fieldDirty(), hintFor(), humanizeKey(), isPwd() (+8 more)

### Community 40 - "REST Performance Metrics"
Cohesion: 0.13
Nodes (15): RestPerfTarget, RestPerfSample, iLastChunkCount, iLastRenderMs, iLastSendMs, iLastTotalMs, iMaxTotalMs, iSampleCount (+7 more)

### Community 41 - "PIC Flash UI Flow"
Cohesion: 0.26
Nodes (15): clearLogBuffer(), handleFlashCompletion(), handleFlashError(), handleFlashMessage(), parseFirmwareInfo(), performFlash(), pollFlashStatus(), pollPICRefresh() (+7 more)

### Community 42 - "MQTT JSON Writer"
Cohesion: 0.20
Nodes (8): Mode, writeFriendlyName(), MqttJsonWriter, buf, byteCount, cap, mode, ok

### Community 43 - "PIC Info Structures"
Cohesion: 0.15
Nodes (12): byte, HardwareSerial, PicInfo, blockwrite, cfgbase, codesize, confsize, datasize (+4 more)

### Community 44 - "IP Octet Inputs"
Cohesion: 0.19
Nodes (13): collapseOctetGroupsForSave(), getOrCreateSettingsGroup(), ipGetOctetInputs(), ipJoinOctetsToIp(), ipMakeOctetGroup(), ipMarkFixedIPChanged(), ipNormalizeOctetValue(), ipPrefillFromDHCP() (+5 more)

### Community 45 - "OT Log WebSocket"
Cohesion: 0.19
Nodes (13): debouncedSave(), getOTLogDisplayState(), initOTLogWebSocket(), parseLogLine(), persistOTLogBufferForUnload(), resetWSWatchdog(), saveDataToLocalStorage(), scheduleOTLogWebSocketInit() (+5 more)

### Community 46 - "V2 Onboarding Flow"
Cohesion: 0.33
Nodes (11): advAction(), fetchSettings(), maybeShowOnboarding(), maybeShowSatOnboarding(), satOnbNeeded(), satPageStart(), satPageStop(), saveSettings() (+3 more)

### Community 47 - "V2 BLE Discovery Cards"
Cohesion: 0.27
Nodes (11): bleDiscoverDismiss(), discoverSeenSet(), dropDiscoverCard(), getDiscoverStack(), goToSensorRoster(), markDiscoverSeen(), pollDallasDiscovery(), pollDiscovery() (+3 more)

### Community 48 - "OT Frame Status Flags"
Cohesion: 0.18
Nodes (7): OTFrameMsg, len, line, source, suppressOutput, OTPublishGate, _saved

### Community 49 - "OT Message Lookup"
Cohesion: 0.20
Nodes (10): OTmsgcmd_t, OTtype_t, OTlookup_t, bSlaveEchoesValue, friendlyname, id, label, msgcmd (+2 more)

### Community 50 - "PIC Settings Cache"
Cohesion: 0.27
Nodes (10): getPICSettingFromCache(), getPICSettingsCache(), getPICSettingsStorageKey(), isPageVisible(), isPICSettingDiscovered(), refreshPICsettings(), savePICSettingToCache(), setPICValueWithBreaks() (+2 more)

### Community 51 - "V2 Monitor Log"
Cohesion: 0.27
Nodes (10): activeDesign(), eventLineClass(), isMonitorLogVisible(), onWsMessage(), pushLog(), pushTicker(), rawFromLine(), renderLog() (+2 more)

### Community 52 - "OTA Upload Handlers"
Cohesion: 0.39
Nodes (3): platformFreeSketchSpace(), AsyncWebServerRequest, String

### Community 53 - "ECharts Theming"
Cohesion: 0.31
Nodes (8): otgwChartTheme(), registerOtgwThemes(), v(), fmt(), pressClass(), renderC(), renderCGrid(), renderTicker()

### Community 54 - "SAT Appliance Overrides"
Cohesion: 0.28
Nodes (9): applyApplianceToggle(), effectiveSource(), fetchSatStatus(), ovrMark(), renderA(), setMark(), setSource(), setTile() (+1 more)

### Community 55 - "V2 PIC File Manager"
Cohesion: 0.31
Nodes (9): fetchPic(), fetchPicFiles(), newerThanDevice(), picDelete(), picIcon(), picRefresh(), pollPicFlash(), renderPicFiles() (+1 more)

### Community 56 - "Discovery Verify Counters"
Cohesion: 0.22
Nodes (9): DiscoverySection, eLastOutcome, iLastMissingCount, iLastOrphanCount, iLastVerifyEpoch, iPublishedTopicCount, iRepublishTriggeredCount, iVerifyRunCount (+1 more)

### Community 57 - "Filesystem Info Shim"
Cohesion: 0.25
Nodes (8): FSInfo, blockSize, maxOpenFiles, maxPathLength, pageSize, totalBytes, usedBytes, platformFSInfo()

### Community 58 - "Telnet Negotiation Bytes"
Cohesion: 0.25
Nodes (8): SimpleTelnetCore<MAX_CLIENTS>::_filterByte(), SimpleTelnetCore<MAX_CLIENTS>::_negReply(), _maybeEcho, _negOnDo, _negOnDont, _negOnWill, _negOnWont, _sendToClient

### Community 59 - "Telnet Negotiation Flow"
Cohesion: 0.36
Nodes (8): SimpleTelnetCore<MAX_CLIENTS>::_maybeEcho(), SimpleTelnetCore<MAX_CLIENTS>::_negOnDo(), SimpleTelnetCore<MAX_CLIENTS>::_negOnDont(), SimpleTelnetCore<MAX_CLIENTS>::_negOnWill(), SimpleTelnetCore<MAX_CLIENTS>::_startNegotiation(), _negReply, _setUs, _usState

### Community 60 - "MQTT Discovery Verification"
Cohesion: 0.32
Nodes (4): endDiscoveryVerification(), startDiscoveryVerification(), tickDiscoveryVerification(), time

### Community 61 - "Web Server Setup & Auth"
Cohesion: 0.29
Nodes (6): AsyncWebServer, AsyncWebServerRequest, PGM_P, webBeginRequest(), webRequestAuth(), webSendP()

### Community 62 - "PIC Firmware Typing"
Cohesion: 0.38
Nodes (7): OTGWFirmware, OTGWProcessor, String, firmwareToString, firmwareType, processor, processorToString

### Community 63 - "Platform Mutex Shims"
Cohesion: 0.33
Nodes (6): PlatformMutex, platformMutexCreate(), platformMutexLock(), platformMutexUnlock(), OTStateLock, locked

### Community 64 - "Safe Timers"
Cohesion: 0.33
Nodes (4): __Due__(), byte, __Once__(), __TimeLeft__()

### Community 65 - "Webhook Job Types"
Cohesion: 0.29
Nodes (6): WebhookJob, hasPayload, sContentType, sPayloadExpanded, stateOn, sURL

### Community 66 - "Webhook Settings"
Cohesion: 0.29
Nodes (7): WebhookSection, bEnabled, iTriggerBit, sContentType, sPayload, sURLoff, sURLon

### Community 67 - "FSexplorer UI Page"
Cohesion: 0.33
Nodes (4): FSexplorer.html (LittleFS file explorer UI), loadFileList(), Protected files list (undeletable UI assets), Pre-paint theme script (data-theme before first frame)

### Community 68 - "OpenTherm Library Docs"
Cohesion: 0.40
Nodes (5): OpenTherm Adapter (7-15V level shifting hardware), OpenTherm protocol (v2.2, interrupt-driven two-wire boiler bus), OpenTherm CMakeLists.txt (ESP-IDF component registration), OpenTherm keywords.txt (Arduino IDE highlighting), OpenTherm Arduino Library README

### Community 69 - "Platform Queue Shims"
Cohesion: 0.40
Nodes (5): PlatformQueue, platformQueueCreate(), platformQueueReceive(), platformQueueSend(), platformQueueSendToFront()

### Community 70 - "OT Command Queue"
Cohesion: 0.40
Nodes (5): OT_cmd_t, cmd, cmdlen, due, retrycnt

### Community 71 - "OT Override Entries"
Cohesion: 0.40
Nodes (5): OTOverrideEntry_t, id, kind, lastSeen, value

### Community 72 - "Board Name Helpers"
Cohesion: 0.40
Nodes (5): boardName(), __FlashStringHelper, hardwareModeName(), hardwareTypeName(), networkModeName()

### Community 73 - "Sequential Asset Loader"
Cohesion: 0.83
Nodes (4): WEB_FILE_MAX_INFLIGHT=2 file-serve gate (ADR-147/ADR-165), Sequential asset loader with retry (TASK-960), loadFontsSequentially() (FontFace API, one at a time), v2 sequential CSS/JS/font loader (TASK-978/989)

### Community 74 - "Telnet Printf Helpers"
Cohesion: 0.50
Nodes (4): PGM_P, _hasActiveClient, SimpleTelnetCore<MAX_CLIENTS>::printf(), SimpleTelnetCore<MAX_CLIENTS>::printf_P()

### Community 75 - "Telnet Input Filtering"
Cohesion: 0.50
Nodes (4): _filterByte, _handleCharInput, _handleLineInput, SimpleTelnetCore<MAX_CLIENTS>::_feed()

### Community 76 - "Download Icon Asset"
Cohesion: 0.50
Nodes (4): Download/save-to-device UI action, FSexplorer file manager UI, Download-to-storage icon (PNG), OTGW Web UI LittleFS assets

### Community 77 - "SAT Slider Widget"
Cohesion: 0.83
Nodes (3): bind(), pct(), sync()

### Community 78 - "Web Request Body"
Cohesion: 0.50
Nodes (4): WebRequestBody, data, len, owner

### Community 79 - "Serial Error Shims"
Cohesion: 0.67
Nodes (3): HardwareSerial, platformSerialHasOverrun(), platformSerialHasRxError()

### Community 80 - "Platform Noop Snapshot"
Cohesion: 0.67
Nodes (3): _PlatformNoopSnap, buf, len

### Community 81 - "FSexplorer Icon Asset"
Cohesion: 0.67
Nodes (3): FSexplorer icon (small PNG, magnifying-glass/file-explorer glyph), LittleFS web asset bundle (src/OTGW-firmware/data/), FSexplorer file manager web UI (LittleFS browser)

### Community 82 - "Refresh Icon Asset"
Cohesion: 0.67
Nodes (3): Refresh/reload circular-arrows icon (refresh-page-option.png), Page/data refresh UI action, Web UI static asset shipped in LittleFS image

### Community 83 - "Settings Icon Asset"
Cohesion: 0.67
Nodes (3): settings.png gear icon asset, Settings page of the OTGW Web UI, OTGW Web UI (LittleFS assets)

### Community 84 - "System Update Icon"
Cohesion: 0.67
Nodes (3): Firmware/filesystem update UI concept, System update icon (device with download arrow), Web UI LittleFS asset set (data/)

### Community 86 - "Update Icon Asset"
Cohesion: 0.67
Nodes (3): Firmware /update OTA page (OTGW-ModUpdateServer), update.png — clock/history icon with clockwise refresh arrow, Web UI LittleFS asset set (data/)

### Community 87 - "OT TX Message"
Cohesion: 0.67
Nodes (3): OTTxMsg, bytes, len

## Ambiguous Edges - Review These
- `Download-to-storage icon (PNG)` → `FSexplorer file manager UI`  [AMBIGUOUS]
  src/OTGW-firmware/data/download-to-storage-drive.png · relation: references

## Knowledge Gaps
- **422 isolated node(s):** `bConnected`, `iLastConnectedMs`, `bEnable`, `bSecure`, `sBroker` (+417 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **What is the exact relationship between `Download-to-storage icon (PNG)` and `FSexplorer file manager UI`?**
  _Edge tagged AMBIGUOUS (relation: references) - confidence is low._
- **Why does `OTGWSerial` connect `PIC Firmware Upgrade Events` to `Board & Hardware Types`, `PIC Info Structures`, `PIC Upgrade Transfer`, `OTGW Serial Debug`, `PIC Firmware Typing`?**
  _High betweenness centrality (0.073) - this node is a cross-community bridge._
- **Why does `index.html (classic Web UI shell)` connect `Web UI Core (index.js)` to `Device Info UI`, `Main Page Init`, `PIC Flash UI Flow`, `Sequential Asset Loader`, `IP Octet Inputs`, `Web UI Log Viewer`, `OT Log WebSocket`, `PIC Settings Cache`, `Gateway Mode UI Controls`, `Resizable Table UI`, `BLE Sensor UI`, `Firmware Page UI`?**
  _High betweenness centrality (0.067) - this node is a cross-community bridge._
- **Why does `SimpleTelnetCore` connect `Telnet Core State` to `Async Telnet Transport`, `Telnet Negotiation Config`, `Telnet Printf Helpers`, `Telnet Input Filtering`, `Telnet Client API`, `Sync Telnet Transport`, `Telnet Negotiation Bytes`, `Telnet Negotiation Flow`?**
  _High betweenness centrality (0.052) - this node is a cross-community bridge._
- **Are the 6 inferred relationships involving `index.html (classic Web UI shell)` (e.g. with `design.html (design system component reference page)` and `clearStoredData()`) actually correct?**
  _`index.html (classic Web UI shell)` has 6 INFERRED edges - model-reasoned connections that need verification._
- **What connects `bConnected`, `iLastConnectedMs`, `bEnable` to the rest of the system?**
  _422 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `OpenTherm Protocol Library` be split into smaller, more focused modules?**
  _Cohesion score 0.05581395348837209 - nodes in this community are weakly interconnected._
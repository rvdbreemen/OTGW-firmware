---
id: TASK-865.11
title: >-
  feat(web): rewrite the ESP32 OTA upload handler against AsyncWebServer
  onUpload
status: To Do
assignee: []
created_date: '2026-06-13 05:55'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.9
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 3; OTA; depends seq9 = TASK-865.9)
The OTA/firmware-update server is a self-contained high-risk seam. Today OTGWUpdateServer (OTGW-ModUpdateServer-esp32.h, instantiated httpUpdater in networkStuff.ino) registers GET/POST + a synchronous upload callback on WebServer via _server->on(path,HTTP_POST,done,upload) (67-124); its UPLOAD_FILE_START/WRITE/END/ABORTED lifecycle (113-121) is the sync HTTPUpload model. Rewrite against AsyncWebServer onUpload(filename,index,data,len,final). Depends on seq9 (the AsyncWebServer(80) instance + removal of the handleClient() upload-pump in the flash-background helpers). See other-projects/EMS-ESP32-dev for an async OTA onUpload reference.

## Carry over UNCHANGED (load-bearing flash logic; re-plumb args only)
- merged-binary skip/limit: _mergedSkipBytes/_mergedWriteLimit extract the app slot (skip MERGED_APP_OFFSET=0x10000, write up to MERGED_APP_SIZE=0x1E0000) (135-253). async gives (data,index,len,final); index = absolute offset, simplifies skip math.
- hardware watchdog feed I2C 0x26/0xA5 per write-chunk (229-231) must still fire per chunk on the AsyncTCP task.
- target detect: filename=="filesystem" -> Update.begin(fsSize,U_SPIFFS) + LittleFS.end()/remount + writeSettings; else U_FLASH (183-222,276-306).
- deferred reboot: requestDeferredReboot from the done-handler, fired by loop() (96-109); do NOT doRestart() inside the async callback.
- auth + the four logBootSignature OTA lifecycle probes preserved.

## Model change
No more handleClient() polling for uploads (the handleEspFlashBackgroundTasks/handlePicFlashBackgroundTasks pumps OTGW-firmware.ino:738/748 are removed by seq9). Confirm the flash-progress WebSocket (sendWebSocketJSON) still flows during an async upload. GET /update form (send_P 78) + POST result page (82-109) -> request->send_P/request->send; setIndexPage/setSuccessPage PROGMEM pointers (updateServerHtml.h) carry over.

## Concurrency note
Async upload callback runs on the AsyncTCP limited-stack task and must not block. Update.write() + I2C watchdog write are short; LittleFS .end()/.begin() for the filesystem-image path are riskier - validate they complete safely from the callback context.

## Acceptance Criteria
- build: esp32 + esp32-classic exit 0 with the rewritten OTA handler on AsyncWebServer (grep per-env SUCCESS); zero _server->on/HTTPUpload/upload.status/handleClient in OTGW-ModUpdateServer-esp32.h; handler uses onUpload(filename,index,data,len,final).
- evaluator: evaluate.py --quick no new failures.
- field: firmware OTA via /update succeeds on real ESP32-S3 with a plain .ino.bin AND a merged binary (merged-app skip/limit extracts the correct slot, device boots, deferred reboot fires after the HTTP 200 drains).
- field: filesystem-image (LittleFS) OTA succeeds (U_SPIFFS, remount, writeSettings restored, assets serve after reboot).
- field: hardware watchdog (I2C 0x26) fed throughout a multi-MB upload (no mid-flash reboot); flash-progress WebSocket updates still display during the async upload. (Per CLAUDE.md: never flash PIC over WiFi; ESP OTA only.)
<!-- SECTION:DESCRIPTION:END -->

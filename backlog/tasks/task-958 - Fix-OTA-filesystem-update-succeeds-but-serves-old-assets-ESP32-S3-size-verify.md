---
id: TASK-958
title: >-
  Fix OTA filesystem update: succeeds but serves old assets (ESP32-S3
  size/verify)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 17:05'
updated_date: '2026-06-30 18:15'
labels: []
dependencies: []
ordinal: 170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTA fs update (POST /update?cmd=100, *.littlefs.bin) reports success + reboots but serves OLD web assets; USB esptool flash of the same littlefs.bin to 0x270000 works. Partition layout is consistent (not the cause). Root cause: OTGW-ModUpdateServer-esp32.h _handleUploadStart sizes Update.begin from LittleFS.totalBytes() (FS bookkeeping) not the uploaded image size, the too-large guard compares against the same (multipart contentLength fallback), and _handleUploadEnd never verifies the write took. Fix: resolve spiffs via esp_partition (authoritative size/offset), size Update.begin to the image (?size only), add post-write version.hash read-back so a non-effective write errors loudly. ADR-139 ETag cache (max-age 60) can briefly mask success -> hard-refresh in verify.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Phase 0: reproduced on .39 with telnet [OTA] capture + curl version.hash before/after; exact failure pinned
- [ ] #2 Update.begin sized to the uploaded image; over-size check bounds against esp_partition spiffs->size not LittleFS.totalBytes()
- [x] #3 Post-write verification: version.hash read back after remount; mismatch -> clear error, not bare success
- [x] #4 OTA fs upload of a fresh littlefs.bin to .39 flips served version.hash + hard-refreshed UI shows new assets; truncated-image negative test rejected
- [x] #5 esp32 + esp32-classic + esp32-combo build green; evaluate --quick clean; prerelease bumped; ADR-029/134 amended via adr-kit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PHASE 0 DONE (live repro on OTGW32 @.39, telnet + curl): the OTA fs WRITE WORKS — a full upload flipped served version.hash a46e95a->03591d5 (telnet FS githash confirmed, served ETag became 03591d5). Original size/partial-write hypothesis DISPROVEN (LittleFS.totalBytes()==partition==image==1572864). ROOT CAUSE = browser cache: assets served Cache-Control: public, max-age=60 + ETag=git-hash (ADR-139, FSexplorer.ino:91); the post-OTA reload window.location.href='/' (updateServerHtml.h:316) does NOT cache-bust, so the browser serves cached old assets for 60s without revalidating. Confirmed live: curl -D - .39/index.js -> ETag "03591d5" + Cache-Control: public, max-age=60. Secondary: client may not get the final 200 (curl saw HTTP=100; reboot races drain). Plan revised accordingly: Cache-Control no-cache (always-revalidate) + cache-bust the reload + assert full write; amend ADR-139.

AC#2 SUPERSEDED by Phase 0 evidence: Update.begin sizing was NOT the bug. LittleFS.totalBytes()==partition==image==1572864 (verified), and a complete OTA correctly replaces the fs. No size/esp_partition change needed; left as-is deliberately. The real fix is the cache policy. AC#3 done: _handleUploadEnd now checks /version.hash exists after remount (truncated write that Update.end(true) finalized -> clear error, not silent success). AC#4 done: OTA of fresh littlefs (03591d5) to .39 flipped served version.hash a46e95a->03591d5; with Cache-Control no-cache the new assets show without hard-refresh (stale-ETag GET -> 200 fresh, matching -> 304); truncated image -> version.hash-missing error. AC#5 done: esp32+esp32-classic+esp32-combo all SUCCESS (alpha.297), evaluate --quick 98.7%/Failed 0, prerelease bumped; ADR-029/134 unchanged (write path correct) and ADR-139 cache-portion AMENDED by new ADR-163 (no-cache, lint PASS) instead.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Re-diagnosed and fixed 'OTA filesystem update succeeds but serves old assets' (2.0.0 ESP32-S3). Phase 0 bench repro on OTGW32 @.39 DISPROVED the size/partial-write hypothesis: a complete OTA fs upload correctly replaces the filesystem (served version.hash a46e95a->03591d5, telnet FS githash + served ETag confirmed). Root cause is browser caching: assets were served Cache-Control: public, max-age=60 (ADR-139), so the post-reboot reload (window.location.href='/') served cached old assets for up to 60s without revalidating. Fix: (1) FSexplorer.ino serveVersionedAsset Cache-Control public,max-age=60 -> no-cache (always-revalidate via the existing ETag; 304 path is ungated by the ADR-147 file gate, verified 24/24 304 on an 8-asset concurrent burst, zero 503); (2) updateServerHtml.h post-OTA reload cache-bust (?_ota=Date.now()); (3) OTGW-ModUpdateServer-esp32.h _handleUploadEnd verifies /version.hash exists after remount so a truncated write errors loudly. ADR-163 authored (Amends ADR-139, lint PASS). Verified live on .39 (alpha.297): no-cache headers, stale-ETag->200/matching->304, v1<->v2 UI switch clean (classic->v2->classic, 0 JS errors). Builds esp32/esp32-classic/esp32-combo green; evaluate 98.7%.
<!-- SECTION:FINAL_SUMMARY:END -->

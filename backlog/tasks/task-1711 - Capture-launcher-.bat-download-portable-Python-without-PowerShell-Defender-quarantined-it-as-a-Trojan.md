---
id: TASK-1711
title: >-
  Capture launcher (.bat): download portable Python without PowerShell (Defender
  quarantined it as a Trojan)
status: In Progress
assignee: []
created_date: '2026-11-07T20:28:00Z'
updated_date: '2026-11-07T20:29:00Z'
labels:
  - capture-tool
  - windows
dependencies: []
ordinal: 822000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs (Discord #alpha-testing, 2026-11-07 19:45) got capture-mqtt-debug.bat quarantined by Windows Defender: 'Trojan, Severe'. The portable-Python bootstrap ran an inline PowerShell command with -ExecutionPolicy Bypass and Net.WebClient.DownloadFile, then started the downloaded python.exe: the download-and-run pattern AMSI inspects. Replace it with curl.exe, certutil -hashfile and tar.exe from System32 (Windows 10 1803+), keep the SHA-256 check before unpacking.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 no PowerShell, ExecutionPolicy or WebClient in the launcher
- [x] #2 download with System32 curl.exe, SHA-256 with certutil -hashfile before tar.exe unpacks
- [x] #3 a checksum mismatch deletes the zip, unpacks nothing and exits 1
- [ ] #4 without curl.exe or tar.exe the launcher points to python.org and exits 1
- [ ] #5 the tester's Defender no longer quarantines the launcher
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 evidence. Red first: new tests/test_capture_launcher_bat.py, 4 of 6 failed on the old launcher (a no PowerShell, b curl/certutil/tar, c checksum before unpack, e order); 6/6 OK after. Real runs on Windows 11 (PATH=C:\Windows\System32 only, LOCALAPPDATA and TEMP in a scratch dir): first run downloaded python-3.12.10-embed-amd64.zip, checksum OK, unpacked, ran otgw_capture.py --help, exit 0, 4 s, zip removed; second run used the cache (--discover found 4 gateways, exit 0). Wrong pinned SHA (copy): 'SHA-256 mismatch ... got 4acbed6d...' (the real hash, so the certutil parse is right), zip deleted, nothing unpacked, exit 1. MpCmdRun -Scan (signature 1.463.176.0) finds no threat in the old or the new launcher, so the tester's detection was behavioural or cloud: AC 5 can only be confirmed by the tester. Not tested: the no-curl/tar branch (Windows older than 10 1803), win32 and arm64 downloads (checksums unchanged). test_build TestDistributionZipComplete OK, test_capture_launcher.py OK, evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->

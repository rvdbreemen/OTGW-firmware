/*
***************************************************************************
**  Program  : satMarkers.ino
**  Version  : v2.0.0-beta.400
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  RAM copy of the heating-curve calibration markers (/sat_markers.json, TASK-586) for
**  /api/v2/sat/markers, written to flash on the loop task (TASK-1911).
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

// --- satMarkers begin
// TASK-1911: the RAM copy of /sat_markers.json (TASK-586 heating-curve calibration markers), the TASK-1912 dallasLabels
// pattern. GET/POST/DELETE /api/v2/sat/markers work on the copy under a mutex (503 when busy, fetchWithRetry retries),
// so no LittleFS call runs on async_tcp. The loop task writes the file when the copy changed (temp file +
// fsCommitTempFile, so a power cut keeps the old file), never while flashing, and doRestart() flushes it. The flush
// holds the lock only to snapshot the copy; the flash write runs without it, and a generation counter keeps the copy
// dirty when an edit arrives during the write. FSexplorer upload/delete of the file asks the loop task to reload it;
// the last action wins. The copy is the file's own text, "[{...},{...}]", at most SAT_MARKERS_MAX_BYTES: 20 markers of
// at most ~110 B. Both buffers come from PSRAM when the board has it, otherwise from the internal heap (2 x 2561 B,
// allocated once at boot).
static const size_t  SAT_MARKERS_MAX_BYTES = 2560;           // = the FS-OTA keep-list cap of /sat_markers.json
static const int     SAT_MARKERS_MAX       = 20;
static const char    kSatMarkersFile[]     = "/sat_markers.json";
static const char    kSatMarkersTmp[]      = "/sat_markers.tmp";
static char*         s_smBuf      = nullptr;                 // the copy
static char*         s_smFlushBuf = nullptr;                 // snapshot the flush writes outside the lock
static uint32_t      s_smGen = 0;                            // counts changes of the copy
static bool          s_smReloadReq = false;                  // FSexplorer changed the file: reload on the loop task
static uint32_t      s_smReloadGen = 0;                      // s_smGen when that request was made
static bool          s_smDirty = false;                      // the copy differs from the file
static uint32_t      s_smRetryMs = 0;                        // a failed write waits before the next try
static PlatformMutex s_smMutex = nullptr;

static char* satMarkersAllocBuf() {
  void* p = platformPsramAllocOnce(SAT_MARKERS_MAX_BYTES + 1);
  if (p == nullptr) p = malloc(SAT_MARKERS_MAX_BYTES + 1);
  return static_cast<char*>(p);
}

// REST (async_tcp): false when busy or when the copy could not be allocated at boot.
bool satMarkersLock(uint32_t waitMs) {
  if (s_smMutex == nullptr || s_smBuf == nullptr) return false;
  return platformMutexLock(s_smMutex, waitMs);
}
void satMarkersUnlock() { platformMutexUnlock(s_smMutex); }
const char* satMarkersJsonLocked() { return s_smBuf; }

// The next flat object {...} at or after p; *end points past its '}'. nullptr when there is none.
static const char* satMarkersNextObj(const char* p, const char** end) {
  p = strchr(p, '{');
  if (p == nullptr) return nullptr;
  int depth = 0;
  for (const char* q = p; *q; ++q) {
    if (*q == '{') depth++;
    else if (*q == '}' && --depth == 0) { *end = q + 1; return p; }
  }
  return nullptr;
}

static int satMarkersObjId(const char* obj) {
  const char* idp = strstr(obj, "\"id\":");
  if (idp == nullptr) return 0;
  idp += 5;
  while (*idp == ' ') idp++;
  return atoi(idp);
}

// Caller holds the lock.
void satMarkersScanLocked(int& count, int& maxId) {
  count = 0; maxId = 0;
  const char* end = nullptr;
  for (const char* o = satMarkersNextObj(s_smBuf, &end); o != nullptr; o = satMarkersNextObj(end, &end)) {
    count++;
    const int id = satMarkersObjId(o);
    if (id > maxId) maxId = id;
  }
}

static void satMarkersChangedLocked() {
  s_smGen++;
  s_smDirty = true;
  s_smRetryMs = 0;
}

// Caller holds the lock. Appends entry (a flat object). 0 ok, 1 full (count), 2 no room (bytes).
int satMarkersAppendLocked(const char* entry, int count) {
  if (count >= SAT_MARKERS_MAX) return 1;
  const size_t len = strlen(s_smBuf), add = strlen(entry) + (count > 0 ? 1 : 0);
  if (len < 2 || s_smBuf[len - 1] != ']' || len + add > SAT_MARKERS_MAX_BYTES) return 2;
  char* tail = s_smBuf + len - 1;                             // the closing ']'
  if (count > 0) *tail++ = ',';
  strcpy(tail, entry);
  strcat(tail, "]");
  satMarkersChangedLocked();
  return 0;
}

// Caller holds the lock. Removes the marker with id and one separating comma. false when not found.
bool satMarkersDeleteLocked(int id) {
  const char* end = nullptr;
  for (const char* o = satMarkersNextObj(s_smBuf, &end); o != nullptr; o = satMarkersNextObj(end, &end)) {
    if (satMarkersObjId(o) != id) continue;
    char* from = const_cast<char*>(o);
    char* to = const_cast<char*>(end);
    if (*to == ',') to++;                                     // first or middle: drop the comma after it
    else if (from > s_smBuf + 1 && from[-1] == ',') from--;   // last: drop the comma before it
    memmove(from, to, strlen(to) + 1);
    satMarkersChangedLocked();
    return true;
  }
  return false;
}

// Read the file into buf (SAT_MARKERS_MAX_BYTES + 1); "[]" when it is missing, empty, too large or not an array.
static void readSatMarkersFile(char* buf) {
  strlcpy(buf, "[]", SAT_MARKERS_MAX_BYTES + 1);
  if (!LittleFSmounted) return;
  File f = LittleFS.open(kSatMarkersFile, "r");
  if (!f) return;
  const size_t size = f.size();
  if (size == 0 || size > SAT_MARKERS_MAX_BYTES) {
    DebugTf(PSTR("SAT markers: %s has %u bytes, not loaded (max %u)\r\n"), kSatMarkersFile, (unsigned)size,
            (unsigned)SAT_MARKERS_MAX_BYTES);
    f.close();
    return;
  }
  const size_t n = f.read(reinterpret_cast<uint8_t*>(buf), SAT_MARKERS_MAX_BYTES);
  f.close();
  buf[n] = '\0';
  if (n != size || buf[0] != '[' || buf[n - 1] != ']') strlcpy(buf, "[]", SAT_MARKERS_MAX_BYTES + 1);
}

// Setup, loop task: allocate the copy and read the file once, before the web server starts.
void loadSatMarkers() {
  if (s_smMutex == nullptr) s_smMutex = platformMutexCreate();
  if (s_smBuf == nullptr) s_smBuf = satMarkersAllocBuf();
  if (s_smFlushBuf == nullptr) s_smFlushBuf = satMarkersAllocBuf();
  if (s_smBuf == nullptr || s_smFlushBuf == nullptr) {
    DebugTln(F("SAT markers: no memory for the RAM copy, the markers API answers 503"));
    s_smBuf = nullptr;                                        // satMarkersLock() refuses without a copy
    return;
  }
  platformMutexLock(s_smMutex);
  readSatMarkersFile(s_smBuf);
  s_smDirty = false;
  platformMutexUnlock(s_smMutex);
}

static bool writeSatMarkersFile(const char* json, size_t len) {
  if (!LittleFSmounted) return false;
  File f = LittleFS.open(kSatMarkersTmp, "w");
  if (f) {
    f.write(reinterpret_cast<const uint8_t*>(json), len);
    f.close();
  }
  const bool ok = fsCommitTempFile(kSatMarkersTmp, kSatMarkersFile, len);
  if (!ok) DebugTln(F("SAT markers: write failed, the old file stays; retry in 30 s"));
  return ok;
}

// Loop task (and doRestart()): write the copy when it changed; the lock covers the snapshot, not the flash write.
void flushSatMarkers() {
  if (s_smBuf == nullptr) return;
  platformMutexLock(s_smMutex);
  if (!s_smDirty) { platformMutexUnlock(s_smMutex); return; }
  const size_t len = strlen(s_smBuf);
  memcpy(s_smFlushBuf, s_smBuf, len + 1);
  const uint32_t gen = s_smGen;
  platformMutexUnlock(s_smMutex);

  const bool ok = writeSatMarkersFile(s_smFlushBuf, len);

  platformMutexLock(s_smMutex);
  if (!ok) s_smRetryMs = millis();
  else if (s_smGen == gen) s_smDirty = false;               // an edit during the write keeps the copy dirty
  platformMutexUnlock(s_smMutex);
}

// Any task (FSexplorer runs on async_tcp): path was uploaded or deleted. Only the markers file asks for a reload.
void satMarkersFileChanged(const char* path) {
  if (path == nullptr) return;
  while (*path == '/') path++;
  if (strcmp(path, kSatMarkersFile + 1) != 0) return;
  __atomic_store_n(&s_smReloadGen, __atomic_load_n(&s_smGen, __ATOMIC_ACQUIRE), __ATOMIC_RELAXED);
  __atomic_store_n(&s_smReloadReq, true, __ATOMIC_RELEASE);
}

// Loop task: take the file over, read outside the lock, unless an edit came after the request.
static void reloadSatMarkers() {
  const uint32_t gen = __atomic_load_n(&s_smReloadGen, __ATOMIC_RELAXED);
  readSatMarkersFile(s_smFlushBuf);
  platformMutexLock(s_smMutex);
  if (s_smGen == gen) {
    memcpy(s_smBuf, s_smFlushBuf, strlen(s_smFlushBuf) + 1);
    s_smDirty = false;
    s_smRetryMs = 0;
  }
  platformMutexUnlock(s_smMutex);
}

// Loop task, from loopLittleFSStatus() (so never while flashing).
void loopSatMarkers() {
  if (s_smBuf == nullptr) return;
  if (__atomic_exchange_n(&s_smReloadReq, false, __ATOMIC_ACQ_REL)) reloadSatMarkers();
  if (!__atomic_load_n(&s_smDirty, __ATOMIC_ACQUIRE)) return;   // a hint only; the flush checks again under the lock
  if (s_smRetryMs != 0 && (uint32_t)(millis() - s_smRetryMs) < 30000UL) return;
  flushSatMarkers();
}
// --- satMarkers end

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

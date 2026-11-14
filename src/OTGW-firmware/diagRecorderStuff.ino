/*
***************************************************************************
**  Program  : diagRecorderStuff.ino
**  Version  : v2.0.0-beta.238
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Local diagnostic recorder, glue (TASK-1665, ADR-213): the platform environment
**  (clock, heap, PSRAM, lock), start/stop/resume and the REST capture helper. The
**  record format, ring and redaction are in diagRecorderStuff.h (host-tested). The
**  other capture points are one-line calls in processOT(), sendPICSerial(),
**  handleOTDirectCommand(), sendMQTTData(), handleMQTTcallback(), processAPI(),
**  weatherApplyResult(), satBLEUpdateState(), updateSetting() and the NTP sync;
**  each is `if (diagRec.active) diagRec.recXxx(...)`, one volatile read when idle.
**
**  Lock contract (ADR-090): diagRecMutex is taken without waiting by every capture
**  point; a busy lock drops the record and counts it (Gap record). start() and
**  stop() wait up to 50 ms. Nothing is called while the lock is held.
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
*/

DiagRecorder diagRec;
static DiagEnv       diagEnv;
static PlatformMutex diagRecMutex = nullptr;

static uint32_t diagEnvMillis()  { return millis(); }
static uint32_t diagEnvEpoch()   { const time_t t = time(nullptr); return (t > 946684800) ? (uint32_t)t : 0u; }   // 0 until NTP set the clock
static uint32_t diagEnvFreeHeap(){ return platformFreeHeap(); }
static void*    diagEnvAlloc(size_t n)       { return malloc(n); }
static void     diagEnvFree(void* p)         { free(p); }
static void*    diagEnvPsram(size_t n)       { return platformPsramAllocOnce(n); }
static bool     diagEnvTryLock()             { return platformMutexTryLock(diagRecMutex); }
static bool     diagEnvLockWait(uint32_t ms) { return platformMutexLock(diagRecMutex, ms ? ms : 1u); }
static void     diagEnvUnlock()              { platformMutexUnlock(diagRecMutex); }

// Once from setup(), before any capture point can run. No ring is allocated here (ADR-213 decision 3).
void diagRecorderInit()
{
  if (diagRecMutex == nullptr) diagRecMutex = platformMutexCreate();
  diagEnv.millis = diagEnvMillis;
  diagEnv.epoch = diagEnvEpoch;
  diagEnv.freeHeap = diagEnvFreeHeap;
  diagEnv.allocHeap = diagEnvAlloc;
  diagEnv.freeHeapBlock = diagEnvFree;
  diagEnv.allocPsramOnce = diagEnvPsram;
  diagEnv.tryLock = diagEnvTryLock;
  diagEnv.lockWait = diagEnvLockWait;
  diagEnv.unlock = diagEnvUnlock;
  diagEnv.hasPsram = platformHasPsram();
}

// Manual start (the REST route of TASK-1666 calls this). A refusal is not an error: the status
// reports diagStartReasonText(diagRec.lastStart). Without the lock there is no safe recorder.
DiagStartResult diagRecorderStart()
{
  if (diagRecMutex == nullptr) return DIAG_REFUSED_ALLOC;
  const DiagStartResult r = diagRec.start(diagEnv);
  if (r == DIAG_START_OK) updateSetting("DiagRecordingOn", "true");   // first Setting record of the recording
  return r;
}

bool diagRecorderStop()
{
  updateSetting("DiagRecordingOn", "false");                          // last Setting record of the recording
  return diagRec.stop();
}

// From setup() after the reboot log is written: a recording that was on when the unit rebooted
// resumes as a new recording whose first record is the Reboot record. The same start rule as a
// manual start applies; a refusal leaves the flag set and the reason in diagRec.lastStart.
void diagRecorderBootResume()
{
  if (!settings.diag.bRecordingOn || diagRecMutex == nullptr) return;
  // platformResetReason(): "Exception/panic", "Interrupt watchdog", "Task watchdog", "Other watchdog", "CPU lock-up"
  const bool crashed = strstr_P(lastReset, PSTR("panic")) != nullptr || strstr_P(lastReset, PSTR("atchdog")) != nullptr ||
                       strstr_P(lastReset, PSTR("lock-up")) != nullptr;
  char summary[160];
  char details[REBOOT_LOG_LINE_MAX];
  const char* crash = "";
  if (crashed && readLatestCrashLog(summary, sizeof(summary), details, sizeof(details))) crash = summary;
  const DiagStartResult r = diagRec.resume(diagEnv, state.uptime.iRebootCount, lastReset, crash);
  DebugTf(PSTR("Diag recorder: resume %s%s%s\r\n"), (r == DIAG_START_OK) ? "started" : "refused",
          (r == DIAG_START_OK) ? "" : ": ", diagStartReasonText(r));
}

// REST capture helper: one record per non-GET /api/v2 request that reached its handler. The body of
// the settings route is left out (updateSetting() records each changed value, redacted), as is the
// body of any route whose name looks like a secret holder.
void diagRecordRestIn(uint8_t routeIdx, const char* segment, const char* method, const char* body)
{
  if (!diagRec.active) return;
  uint8_t m = 5;
  if (strcmp_P(method, PSTR("POST")) == 0) m = 1;
  else if (strcmp_P(method, PSTR("PUT")) == 0) m = 2;
  else if (strcmp_P(method, PSTR("PATCH")) == 0) m = 3;
  else if (strcmp_P(method, PSTR("DELETE")) == 0) m = 4;
  const bool skipBody = (body == nullptr) || strcasecmp_P(segment, PSTR("settings")) == 0 ||
                        diagNameIsSecret(segment, strlen(segment));
  diagRec.recRestIn(routeIdx, m, skipBody ? "" : body, skipBody ? 0u : strlen(body));
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

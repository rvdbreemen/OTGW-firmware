/*
***************************************************************************
**  Program  : fsWriter.ino
**  Version  : v2.0.0-beta.361
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Small state-file writes off the loop task (TASK-1989). A LittleFS write of
**  a 256 B file costs 10-20 ms, but about every 6th one 250-410 ms (block
**  allocation / metadata commit), measured on .61 (beta.342). On the loop task
**  each of those is a [loop-stall]; from a task on the protocol core the loop
**  saw none (09, 2026-11-16, 40 writes per mode). The loop hands the file
**  contents to one FIFO queue; one writer task writes them in order.
**
**  Scope: the SAT PID state, the SAT energy total and the estimated energy,
**  every write and remove of those files (one FIFO keeps the order, so a
**  queued older write can never land after a newer one). Not queued: the DHW
**  meter (it verifies by reading back) and settings.ini (large and streamed).
**
**  Drain: doRestart() and the start of a filesystem OTA wait until the queue
**  is empty. While a filesystem image is written or LittleFS is not mounted,
**  the writer holds the next job. Never drained, so a queued write is lost:
**  a watchdog reset, a panic, a brownout or a power cut (the same as a write
**  that had not started yet on the loop task), and the config-portal timeout
**  restart in setup(), which runs before anything is queued.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#include "fsWriter.h"

static PlatformQueue     s_fswQueue     = nullptr;
// Two counters with one writer each (submitted: the submitting task, done: the writer task), so no lock is
// needed: pending = submitted - done.
static volatile uint32_t s_fswSubmitted = 0;
static volatile uint32_t s_fswDone      = 0;

// One job: write data to path ("w", replaces the file) or remove path. Runs on the writer task, or on the
// caller when there is no queue (before fsWriterBegin(), host tests).
void fsWriterService(const FsWriteJob& job)
{
  if (job.op == FSW_OP_REMOVE) {
    LittleFS.remove(job.path);
    return;
  }
  File f = LittleFS.open(job.path, "w");
  if (!f) return;
  f.write((const uint8_t*)job.data, job.len);
  f.close();
}

static void fsWriterTask(void*)
{
  FsWriteJob job;
  for (;;) {
    if (!platformQueueReceive(s_fswQueue, &job, PLATFORM_QUEUE_WAIT_FOREVER)) continue;
    // A filesystem image is being written, or LittleFS is not mounted: hold this job until it is back.
    while (::state.flash.bFsImageWriting || !LittleFSmounted) platformTaskDelay(50);
    fsWriterService(job);
    s_fswDone = s_fswDone + 1;
  }
}

// Start the queue and the writer task (setup(), after LittleFS is mounted, before initSAT()).
void fsWriterBegin()
{
  if (s_fswQueue != nullptr) return;
  s_fswQueue = platformQueueCreate(FSW_QUEUE_DEPTH, sizeof(FsWriteJob));
  if (s_fswQueue == nullptr) return;   // no queue: fsWriterSubmit() writes on the caller, as before
  if (!platformTaskCreatePinnedCore(fsWriterTask, "fswriter", 4096, nullptr, 1, PLATFORM_CORE_PROTOCOL)) {
    s_fswQueue = nullptr;              // no consumer: never leave jobs in a queue nobody reads
  }
}

uint32_t fsWriterPending()
{
  return s_fswSubmitted - s_fswDone;
}

// Wait until every queued job is written, at most timeoutMs. Returns true when the queue is empty.
bool fsWriterDrain(uint32_t timeoutMs)
{
  const uint32_t t0 = millis();
  while (fsWriterPending() != 0) {
    if ((uint32_t)(millis() - t0) >= timeoutMs) return false;
    platformTaskDelay(5);
  }
  return true;
}

static bool fsWriterQueue(uint8_t op, const char* path, const char* data, size_t len)
{
  // LittleFS is not mounted and no image write will bring it back: write nothing, as the direct open() did before
  // (it failed and the save returned). Queueing would only fill the queue behind a writer that waits forever.
  if (!LittleFSmounted && !::state.flash.bFsImageWriting) return false;
  FsWriteJob job;
  job.op = op;
  strlcpy(job.path, path, sizeof(job.path));
  if (len > sizeof(job.data)) len = sizeof(job.data);
  job.len = (uint16_t)len;
  if (len) memcpy(job.data, data, len);
  if (s_fswQueue == nullptr) { fsWriterService(job); return true; }
  if (platformQueueSend(s_fswQueue, &job)) { s_fswSubmitted = s_fswSubmitted + 1; return true; }
  // Queue full: wait for it to empty, then queue again, so the order holds. Writing on the caller instead would
  // put this newer write before the queued older ones.
  fsWriterDrain(FSW_FULL_WAIT_MS);
  if (platformQueueSend(s_fswQueue, &job)) { s_fswSubmitted = s_fswSubmitted + 1; return true; }
  DebugTf(PSTR("[fswriter] queue still full after %lu ms, %s not written\r\n"), (unsigned long)FSW_FULL_WAIT_MS, path);
  return false;
}

bool fsWriterSubmit(const char* path, const char* data, size_t len)
{
  return fsWriterQueue(FSW_OP_WRITE, path, data, len);
}

bool fsWriterRemove(const char* path)
{
  return fsWriterQueue(FSW_OP_REMOVE, path, nullptr, 0);
}

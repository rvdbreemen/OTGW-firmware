/*
***************************************************************************
**  Program  : fsWriter.ino
**  Version  : v2.0.0-beta.365
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
**  Drain: doRestart() waits until the queue is empty. The start of a
**  filesystem OTA pauses the writer, waits until it is between jobs and writes
**  the rest of the queue on the caller (fsWriterPauseAndFlush(), TASK-2007), so
**  no job runs between the copy of the files and LittleFS.end(); a job queued
**  after that is held through the image write. At the remount the writer is
**  paused again (fsWriterPause()) until the heap copies are restored, so the
**  held job lands after them instead of being overwritten by them (bench 09,
**  .63, beta.364: the remount cleared the image flag ~300 ms before the
**  restore, and the restore overwrote the newer save). While
**  a filesystem image is written or LittleFS is not mounted, the writer holds
**  the next job. Never drained, so a queued write is lost:
**  a watchdog reset, a panic, a brownout or a power cut (the same as a write
**  that had not started yet on the loop task), and the config-portal timeout
**  restart in setup(), which runs before anything is queued.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#include "fsWriter.h"
#include <atomic>

static PlatformQueue         s_fswQueue     = nullptr;
// pending = submitted - done. submitted is counted BEFORE the queue send (and undone when the send fails), so
// fsWriterPending() never reads 0 while a job is on its way into the queue (TASK-2007 F2). done is written by
// the writer task, or by the caller of fsWriterPauseAndFlush() while the writer is paused between jobs.
static std::atomic<uint32_t> s_fswSubmitted{0};
static std::atomic<uint32_t> s_fswDone{0};
// TASK-2007 F1: paused is set by the start of a filesystem OTA; busy is set by the writer while it takes and
// writes a job. Both are sequentially consistent atomics: the writer sets busy and then reads paused, the
// pauser sets paused and then reads busy, so one of them always sees the other (no job slips past a pause).
static std::atomic<bool>     s_fswPaused{false};
static std::atomic<bool>     s_fswBusy{false};

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

// One pass of the writer task: take the next job and write it, unless the writer is paused, a filesystem image
// is being written or LittleFS is not mounted (the job then stays in the queue). The queue is read without
// waiting, so a job is never taken out of the queue while a pause is being set. Returns true when it wrote a job.
bool fsWriterStep()
{
  s_fswBusy = true;
  if (s_fswPaused || ::state.flash.bFsImageWriting || !LittleFSmounted) { s_fswBusy = false; return false; }
  FsWriteJob job;
  if (!platformQueueReceive(s_fswQueue, &job, 0)) { s_fswBusy = false; return false; }
  fsWriterService(job);
  s_fswDone++;
  s_fswBusy = false;
  return true;
}

static void fsWriterTask(void*)
{
  for (;;) {
    if (!fsWriterStep()) platformTaskDelay(20);   // nothing to write, or held: look again in 20 ms
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

// Start of a filesystem OTA (TASK-2007 F1): pause the writer, wait until it is between jobs, then write every
// queued job on the caller. Returns with the writer paused (false when the writer stayed busy for timeoutMs; the
// queue is then left as it is). The caller sets state.flash.bFsImageWriting before fsWriterResume(), so the
// writer keeps holding until the remount.
bool fsWriterPauseAndFlush(uint32_t timeoutMs)
{
  s_fswPaused = true;
  if (s_fswQueue == nullptr) return true;   // no queue: every write already ran on the caller
  const uint32_t t0 = millis();
  while (s_fswBusy) {
    if ((uint32_t)(millis() - t0) >= timeoutMs) return false;
    platformTaskDelay(1);
  }
  FsWriteJob job;
  while (platformQueueReceive(s_fswQueue, &job, 0)) {
    fsWriterService(job);
    s_fswDone++;
  }
  return true;
}

// Pause without writing anything (TASK-2007): the remount after a filesystem image, until the restore is done. The
// writer finishes the job it may be in; it takes no new one until fsWriterResume().
void fsWriterPause()
{
  s_fswPaused = true;
}

void fsWriterResume()
{
  s_fswPaused = false;
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
  // TASK-2007 F3: a job that does not fit is refused and logged, never cut (a cut JSON file loads as garbage).
  if (len > FSW_DATA_LEN || strlen(path) >= FSW_PATH_LEN) {
    DebugTf(PSTR("[fswriter] %s refused: %u B data, path %u chars (max %u B, %u chars)\r\n"), path, (unsigned)len,
            (unsigned)strlen(path), (unsigned)FSW_DATA_LEN, (unsigned)(FSW_PATH_LEN - 1));
    return false;
  }
  FsWriteJob job;
  job.op = op;
  strlcpy(job.path, path, sizeof(job.path));
  job.len = (uint16_t)len;
  if (len) memcpy(job.data, data, len);
  if (s_fswQueue == nullptr) { fsWriterService(job); return true; }
  s_fswSubmitted++;   // before the send (TASK-2007 F2)
  if (platformQueueSend(s_fswQueue, &job)) return true;
  s_fswSubmitted--;   // not queued: the drain below must not wait for this job
  // Queue full: wait for it to empty, then queue again, so the order holds. Writing on the caller instead would
  // put this newer write before the queued older ones.
  fsWriterDrain(FSW_FULL_WAIT_MS);
  s_fswSubmitted++;
  if (platformQueueSend(s_fswQueue, &job)) return true;
  s_fswSubmitted--;
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

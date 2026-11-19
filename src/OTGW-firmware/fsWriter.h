/*
***************************************************************************
**  Program  : fsWriter.h
**  Version  : v2.0.0-poc.20
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Queued small state-file writes off the loop task (TASK-1989). See fsWriter.ino.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>
#include <stddef.h>

enum : uint8_t { FSW_OP_WRITE = 1, FSW_OP_REMOVE = 2 };

static const size_t   FSW_PATH_LEN     = 32;    // the longest path today: "/sat_energy_estimate.json" (25)
static const size_t   FSW_DATA_LEN     = 256;   // the PID state buffer (satSavePidState)
static const size_t   FSW_QUEUE_DEPTH  = 3;     // one slot per state file (PID, energy, estimated energy); TASK-2007: was 6, ~870 B of heap back
static const uint32_t FSW_FULL_WAIT_MS = 1000;

struct FsWriteJob {
  uint8_t  op;
  uint16_t len;
  char     path[FSW_PATH_LEN];
  char     data[FSW_DATA_LEN];
};

void     fsWriterBegin();
bool     fsWriterSubmit(const char* path, const char* data, size_t len);
bool     fsWriterRemove(const char* path);
bool     fsWriterDrain(uint32_t timeoutMs);
uint32_t fsWriterPending();
void     fsWriterService(const FsWriteJob& job);
bool     fsWriterStep();                              // one pass of the writer task (TASK-2007)
bool     fsWriterPauseAndFlush(uint32_t timeoutMs);   // start of a filesystem OTA (TASK-2007)
void     fsWriterPause();                             // remount after a filesystem OTA, until the restore (TASK-2007)
void     fsWriterResume();
bool     fsWriterRunning();                           // the writer task runs (TASK-1995)
bool     fsWriterWriteFailed();                       // the last write job failed (open or short write; TASK-2020)

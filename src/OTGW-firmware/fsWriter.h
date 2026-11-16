/*
***************************************************************************
**  Program  : fsWriter.h
**  Version  : v2.0.0-beta.357
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
static const size_t   FSW_QUEUE_DEPTH  = 6;     // the 5-min PID save and two hourly energy saves never get close
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

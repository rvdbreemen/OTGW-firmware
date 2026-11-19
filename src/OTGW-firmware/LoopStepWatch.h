/*
***************************************************************************
**  Program  : LoopStepWatch.h
**  Version  : v2.0.0-beta.387
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  The slowest step of one loop() pass, for the [loop-stall] line (TASK-1989).
**  The loop's connProbe() call sites (CPROBE_* in MQTTstuff.h) cut a pass into
**  steps; each probe closes the step that ended there. Pure helper: no Arduino
**  or firmware state, so a host test compiles this header as it is.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdint.h>

struct LoopStepWatch {
  uint32_t lastMs   = 0;   // time of the previous probe
  uint8_t  lastTag  = 0;   // tag of the previous probe (0 = none yet)
  uint32_t slowMs   = 0;   // slowest step of the pass so far
  uint8_t  slowFrom = 0;   // the probe that opened it
  uint8_t  slowTo   = 0;   // the probe that closed it
};

// A probe with tag at nowMs. loopTopTag starts a new pass: it closes the previous pass's last step first, so the step that
// ran from the last probe of a pass to the next loop top still counts for that pass, then it clears the record.
static inline void loopStepProbe(LoopStepWatch& w, uint8_t tag, uint32_t nowMs, uint8_t loopTopTag)
{
  if (tag == loopTopTag) {
    w.slowMs = 0; w.slowFrom = 0; w.slowTo = 0;
  } else if (w.lastTag != 0) {
    const uint32_t dt = nowMs - w.lastMs;   // millis() wrap safe
    if (dt > w.slowMs) { w.slowMs = dt; w.slowFrom = w.lastTag; w.slowTo = tag; }
  }
  w.lastMs = nowMs;
  w.lastTag = tag;
}

// The slowest step of the pass that just ended, read at the top of the next pass before its loop-top probe: the open step
// from the last probe to nowMs counts too (closed as loopTopTag).
static inline void loopStepSlowest(const LoopStepWatch& w, uint32_t nowMs, uint8_t loopTopTag,
                                   uint8_t* from, uint8_t* to, uint32_t* ms)
{
  *from = w.slowFrom; *to = w.slowTo; *ms = w.slowMs;
  if (w.lastTag != 0 && w.lastTag != loopTopTag) {
    const uint32_t tail = nowMs - w.lastMs;
    if (tail > *ms) { *from = w.lastTag; *to = loopTopTag; *ms = tail; }
  }
}

/*
 ***************************************************************************
 **  Program  : dhwWaterMeter
 **  Version  : v1.7.5
 **
 **  Copyright (c) 2026 Robert van Breemen
 **
 **  TERMS OF USE: MIT License. See bottom of file.
 ***************************************************************************
 Cumulative water volume in the DHW circuit, integrated from OpenTherm
 MsgID 19 (DHWFlowRate, litres/minute).

 Home Assistant's Energy dashboard needs a cumulative volume, and MQTT
 discovery cannot create the integration/template helpers that would build
 one host-side, so the gateway keeps the total itself and publishes it as a
 discoverable entity (pseudo-ID 243).

 Two properties of the bus shape this code:

 - The gateway does not poll MsgID 19. Frames appear only when the thermostat
   requests that id, so the interval between samples is a property of each
   installation and gaps are normal. An interval longer than the clamp is
   therefore treated as a hole in the measurement, not as flowing water:
   silence never adds volume, however high the last reading was.
 - Each sample is applied to the interval that preceded it (right-hand
   rectangle rule). At the start of a draw this under-counts the ramp by
   roughly one sample interval; at the end it stops counting immediately.
   No sample history is kept, so this stays a two-variable integrator.

 The total lives in RAM only. Home Assistant reads it as total_increasing,
 which treats the drop after a reboot as a meter reset and keeps the
 long-run sum, so the Energy dashboard survives what the gateway forgets.
 Persisting it would mean a flash write per draw for no gain.
 */

// An interval longer than this between two MsgID 19 frames is a measurement
// gap, not water. One minute matches the firmware's own 60s publish cadence.
static const uint32_t DHW_METER_MAX_GAP_MS = 60000UL;

float           dhwWaterTotalL  = 0.0f;   // litres since boot (not persisted)
static uint32_t dhwMeterLastMs  = 0;
static bool     dhwMeterSeeded  = false;
// Whether this boot has already announced the discovery config. File scope, not
// a function-local static, so the broker-restart path can clear it: a restarted
// broker drops the retained config, and the entity must be announced again.
static bool     dhwMeterAnnounced = false;

//===========================================================================================
// Fold one MsgID 19 reading into the running total.
// flowLitresPerMin: the value just decoded. nowMs: millis() at decode time.
//===========================================================================================
void updateDHWWaterMeter(float flowLitresPerMin, uint32_t nowMs)
{
  if (!dhwMeterSeeded) {
    // First reading after boot has no interval behind it to integrate over.
    dhwMeterLastMs = nowMs;
    dhwMeterSeeded = true;
    return;
  }

  const uint32_t dtMs = nowMs - dhwMeterLastMs;   // unsigned math: wraps correctly
  dhwMeterLastMs = nowMs;

  if (dtMs > DHW_METER_MAX_GAP_MS) return;        // gap: never counted as water
  if (flowLitresPerMin <= 0.0f) return;           // nothing flowing, nothing to add

  dhwWaterTotalL += flowLitresPerMin * ((float)dtMs / 60000.0f);
}

//===========================================================================================
// True once a MsgID 19 frame has ever decoded on this boot. Callers use this to
// stay silent on installations whose thermostat never requests that id: without
// it every gateway would grow a water meter entity pinned at 0.0 L.
//===========================================================================================
bool dhwWaterMeterHasData()
{
  return dhwMeterSeeded;
}

//===========================================================================================
// Discovery-announce latch. dhwWaterMeterNeedsAnnounce() reports whether the
// config still has to go out; forgetDHWWaterMeterAnnounce() re-arms it after a
// broker restart has thrown the retained configs away.
//===========================================================================================
bool dhwWaterMeterNeedsAnnounce()
{
  return !dhwMeterAnnounced;
}

void markDHWWaterMeterAnnounced()
{
  dhwMeterAnnounced = true;
}

void forgetDHWWaterMeterAnnounce()
{
  dhwMeterAnnounced = false;
}

//===========================================================================================
// Test seam: drop the accumulated total and forget the last sample time.
//===========================================================================================
void resetDHWWaterMeter()
{
  dhwWaterTotalL = 0.0f;
  dhwMeterLastMs = 0;
  dhwMeterSeeded = false;
  dhwMeterAnnounced = false;
}

 /***************************************************************************
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ****************************************************************************
 */

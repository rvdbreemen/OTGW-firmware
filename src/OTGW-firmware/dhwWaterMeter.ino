/*
***************************************************************************
**  Program  : dhwWaterMeter.ino
**  Version  : v2.0.0-alpha.406
**
**  Copyright (c) 2026 Robert van den Breemen
**
**  TERMS OF USE: GNU GPLv3. See bottom of file.
***************************************************************************
 Cumulative water volume in the DHW circuit, integrated from OpenTherm MsgID 19
 (DHWFlowRate, litres/minute), for the Home Assistant Energy dashboard (ADR-176).
 Published as dhw_water_total under the faux discovery id OTGWdhwmeterid (241).

 The accumulation method and the gap cap are the ones the 1.x line ships in its
 own dhwWaterMeter.ino, so both firmwares report the same total for the same boiler:

 - Frames for MsgID 19 appear when a master asks for that id: the thermostat on the
   PIC path, and also the OT-Direct schedule (every 10 s). The interval between two
   samples is therefore a property of the installation. An interval longer than
   DHW_METER_MAX_GAP_MS is a hole in the measurement: silence never adds volume.
 - Each sample is applied to the interval that preceded it (right-hand rectangle
   rule). Litres are elapsed time times flow, so a second sample of the same
   exchange only splits an interval in two; it never adds a volume of its own.

 Only boiler Read-Ack frames feed the total: print_f88() calls updateDHWWaterMeter()
 for a boiler Read-Ack (B frame) and says which frames it leaves out, and why. The
 summary line of a PIC in PS=1 mode does not count (ADR-181): it repeats stored
 values without their age, so a PIC gateway in PS=1 mode reports no total, as on
 the 1.x line.

 Unlike the 1.x line, this line keeps the total across a reboot: it lives in its
 own LittleFS file (DHW_METER_FILE, never settings.ini), is restored at boot by
 loadDHWWaterMeter(), and is written under the rule in dhwWaterMeterSaveDue().
 It also has a user reset (ADR-176): POST /api/v2/otgw/reset_water_total and the
 MQTT command set/<node>/otgw/reset_water_total. And it does not count a flow an
 f8.8 frame cannot carry (DHW_METER_MAX_FLOW_LPM); no MsgID 19 frame carries one,
 so that bound only guards the function's contract.

 The total is loop-task state. Every function here runs on the loop task, called
 from processOT() (through print_f88()), the 60 s task, setup(), doRestart() or
 loop(), except queueDHWWaterMeterReset(). The REST handler calls that one on the
 async_tcp task, the MQTT callback from inside doBackgroundTasks() (which re-enters
 through delayms()), and it only raises a flag that handlePendingDHWWaterMeterReset()
 consumes in loop().
 */

// An interval longer than this between two MsgID 19 samples is a measurement gap,
// not water. The 1.x line uses the same 60000 ms.
static const uint32_t DHW_METER_MAX_GAP_MS = 60000UL;

// The largest flow a MsgID 19 frame can carry: f8.8 ends at 127.996 L/min. A reading
// above this, or one that is not a number, cannot come off the bus, and
// updateDHWWaterMeter() does not count it. The PS=1 summary, where a malformed field
// could carry one, does not feed the meter (ADR-181).
static const float    DHW_METER_MAX_FLOW_LPM = 128.0f;

// Write-rate rule (ADR-176): write once DHW_METER_SAVE_DELTA_L litres are unsaved,
// or once DHW_METER_SAVE_INTERVAL_MS has passed since the last write with any
// unsaved water at all, and at once after a reset the file does not hold yet.
// Evaluated from the 60 s task, so at most one write a minute, and while writes
// succeed none while no water is drawn; a failed write stays due. A user reset
// writes outside this rule, once per reset that finds something to zero.
static const double   DHW_METER_SAVE_DELTA_L     = 10.0;
static const uint32_t DHW_METER_SAVE_INTERVAL_MS = 15UL * 60UL * 1000UL;

// The largest total loadDHWWaterMeter() accepts from the file. The accumulator adds at
// most DHW_METER_MAX_FLOW_LPM, 128 L/min, which is about 6.7e7 L a year, so 1e10 L is
// more than a century of it. A restored total therefore has at most 11 integer digits.
static const double   DHW_METER_MAX_LITRES       = 1e10;

static const char DHW_METER_FILE[] PROGMEM = "/dhw_water.json";

// double, not float: the total is persisted and keeps growing across reboots. At
// 2^18 L (about six years of a household's hot water) a float's step is already
// 1/32 L, the size of a one-second sample, so it would round away real water.
double          dhwWaterTotalL  = 0.0;    // litres, restored from DHW_METER_FILE at boot
static uint32_t dhwMeterLastMs  = 0;      // millis() of the previous MsgID 19 sample
static bool     dhwMeterSeeded  = false;  // a MsgID 19 sample was taken on this boot
static double   dhwMeterSavedL  = 0.0;    // the total as last written to (or read from) the file
static uint32_t dhwMeterSavedMs = 0;      // millis() of that write, 0 = boot

//===========================================================================================
// Fold one MsgID 19 reading into the running total.
// flowLitresPerMin: the value just decoded. nowMs: millis() at decode time.
//===========================================================================================
void updateDHWWaterMeter(float flowLitresPerMin, uint32_t nowMs)
{
  if (!dhwMeterSeeded) {
    // The first reading after boot has no interval behind it to integrate over.
    dhwMeterLastMs = nowMs;
    dhwMeterSeeded = true;
    return;
  }

  const uint32_t dtMs = nowMs - dhwMeterLastMs;   // unsigned math: wraps correctly
  dhwMeterLastMs = nowMs;

  if (dtMs > DHW_METER_MAX_GAP_MS) return;        // gap: never counted as water
  // Nothing flowing adds nothing, and neither does a reading f8.8 cannot carry: NaN fails
  // the first test, a flow above DHW_METER_MAX_FLOW_LPM (infinity too) the second.
  if (!(flowLitresPerMin > 0.0f) || flowLitresPerMin > DHW_METER_MAX_FLOW_LPM) return;

  dhwWaterTotalL += (double)flowLitresPerMin * (double)dtMs / 60000.0;
}

//===========================================================================================
// True once a MsgID 19 sample has been taken on this boot. The 60 s state publish waits
// for it (TASK-1123 AC#2), so a gateway whose bus never carries that id publishes no
// value. A total restored from the file does not count. The discovery config does not
// wait: it is queued at boot like the other faux ids (ADR-176).
//===========================================================================================
bool dhwWaterMeterHasData()
{
  return dhwMeterSeeded;
}

//===========================================================================================
// The write-rate rule. nowMs: millis() of the 60 s tick that asks.
//===========================================================================================
bool dhwWaterMeterSaveDue(uint32_t nowMs)
{
  const double unsaved = dhwWaterTotalL - dhwMeterSavedL;
  if (unsaved == 0.0) return false;                               // the file holds the total: no flash write
  if (unsaved < 0.0) return true;                                 // a reset whose file write failed
  if (unsaved >= DHW_METER_SAVE_DELTA_L) return true;             // bounds what a power cut can lose
  return (nowMs - dhwMeterSavedMs) >= DHW_METER_SAVE_INTERVAL_MS; // the remainder of a draw
}

// LittleFS commits a file opened with "w" on close, so a power cut during the write
// leaves the previous version in place. A failed commit, for example on a full
// filesystem, is not reported: print() reports the bytes it put into a stdio buffer,
// and close() returns nothing. So the file is read back, and only a verified write moves
// the saved mark. After a failed one the next tick and the restart flush write again.
static bool writeDHWWaterMeterFile(uint32_t nowMs)
{
  if (!LittleFSmounted || isFlashing()) return false;
  char buf[48];
  snprintf_P(buf, sizeof(buf), PSTR("{\"litres\":%.3f}"), dhwWaterTotalL);
  File f = LittleFS.open(FPSTR(DHW_METER_FILE), "w");
  if (!f) return false;
  f.print(buf);
  f.close();

  char back[sizeof(buf)];
  File r = LittleFS.open(FPSTR(DHW_METER_FILE), "r");
  if (!r) return false;
  const size_t len = r.readBytes(back, sizeof(back) - 1);
  r.close();
  back[len] = '\0';
  if (strcmp(back, buf) != 0) return false;

  dhwMeterSavedL  = dhwWaterTotalL;
  dhwMeterSavedMs = nowMs;
  return true;
}

//===========================================================================================
// Called from the 60 s task.
//===========================================================================================
void saveDHWWaterMeterIfDue(uint32_t nowMs)
{
  if (dhwWaterMeterSaveDue(nowMs)) writeDHWWaterMeterFile(nowMs);
}

//===========================================================================================
// Called from doRestart(): an orderly restart keeps every litre, and a reset whose file
// write failed. Also writes when the file is gone while the total is not zero: a
// filesystem OTA replaces the partition with an image that does not carry it, and
// restores only the settings.
//===========================================================================================
void flushDHWWaterMeter()
{
  if (!LittleFSmounted || isFlashing()) return;
  const bool unsaved  = (dhwWaterTotalL != dhwMeterSavedL);
  const bool fileGone = (dhwWaterTotalL > 0.0) && !LittleFS.exists(FPSTR(DHW_METER_FILE));
  if (unsaved || fileGone) writeDHWWaterMeterFile(millis());
}

//===========================================================================================
// Called from setup(), before any OT frame is decoded. A missing or unreadable file, or a
// value that is not a number from 0 to DHW_METER_MAX_LITRES, leaves the total at 0.
//===========================================================================================
void loadDHWWaterMeter()
{
  if (!LittleFSmounted) return;
  File f = LittleFS.open(FPSTR(DHW_METER_FILE), "r");
  if (!f) return;
  char buf[48];
  const size_t len = f.readBytes(buf, sizeof(buf) - 1);
  f.close();
  buf[len] = '\0';

  static const char key[] PROGMEM = "\"litres\":";
  const char *p = strstr_P(buf, key);
  if (p == nullptr) return;
  p += strlen_P(key);
  char *end = nullptr;
  const double litres = strtod(p, &end);
  if (end == p || isnan(litres) || litres < 0.0 || litres > DHW_METER_MAX_LITRES) return;

  dhwWaterTotalL = litres;
  dhwMeterSavedL = litres;
  DebugTf(PSTR("DHW water total restored: %.1f L\r\n"), litres);
}

// Raised by queueDHWWaterMeterReset(), cleared by handlePendingDHWWaterMeterReset().
// One producer side (the REST handler and the MQTT callback) and one consumer, all on
// core 1: a byte store cannot tear, and volatile keeps loop() from caching the flag.
static volatile bool dhwMeterResetPending = false;

//===========================================================================================
// The user reset (ADR-176): POST /api/v2/otgw/reset_water_total (restAPI.ino, async_tcp
// task) and set/<node>/otgw/reset_water_total (MQTTstuff.ino, inside doBackgroundTasks(),
// which re-enters through delayms()). Both only ask; loop() does the reset between
// frames, the pattern of queueMQTTRepublishAll() (TASK-1176).
//===========================================================================================
void queueDHWWaterMeterReset()
{
  dhwMeterResetPending = true;
}

//===========================================================================================
// Called from loop(), before drainOTFrameQueue(). Zeroes the total in RAM and in
// DHW_METER_FILE, then publishes 0, also before the first MsgID 19 sample, so Home
// Assistant records a clean meter reset.
// - The reset waits while the file cannot be written, so RAM, file and the published 0
//   change together or not at all. Both tests are needed: a filesystem upload unmounts
//   LittleFS (LittleFS.end()) without clearing LittleFSmounted, so isFlashing() covers
//   it, and a failed LittleFS health probe (GET /api/v2/health on a full filesystem)
//   clears LittleFSmounted until a later probe succeeds.
// - A reset that finds the total and the saved mark both at 0 has nothing to write (a
//   missing file also restores 0), so repeated resets cost no flash; it still publishes 0.
// - A write that fails without an error (the read-back differs) stays due: the 60 s task
//   and the restart flush write the 0 until one succeeds (dhwWaterMeterSaveDue()).
// - sendDHWWaterTotal() publishes only while MQTT can publish; nothing retries the 0.
// The sample clock is kept, so the next MsgID 19 sample adds its interval to 0 as usual.
//===========================================================================================
void handlePendingDHWWaterMeterReset()
{
  if (!dhwMeterResetPending) return;
  if (!LittleFSmounted || isFlashing()) return;   // the file cannot be written: the reset waits
  dhwMeterResetPending = false;   // clear first; a request raised after this line is served next pass
  if (dhwWaterTotalL != 0.0 || dhwMeterSavedL != 0.0) {   // RAM or file not at 0 yet
    dhwWaterTotalL = 0.0;
    writeDHWWaterMeterFile(millis());
  }
  sendDHWWaterTotal();
  DebugTln(F("DHW water total reset to 0 L"));
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

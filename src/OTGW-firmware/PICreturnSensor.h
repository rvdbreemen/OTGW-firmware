/*
***************************************************************************
**  Program  : PICreturnSensor.h
**  Version  : v2.0.0-alpha.569
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Pure helpers for the once-a-minute return water refresh (TASK-1314).
**  A PIC with firmware 6.6 or later prints the external temperature sensor
**  with "PR=E" ("PR: E=34.56", "PR: E=-" when no valid reading). When the
**  sensor function is "R" (return water, "PR: D=R") that value is the return
**  water temperature (MsgID 28). No Arduino or firmware state in here, so a
**  host test compiles this header as it is.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <stdlib.h>
#include <string.h>

// True when the PIC firmware version "major.minor[...]" is at least major.minor.
// Numeric compare (6.10 is newer than 6.6). Empty or non-numeric text is false.
static inline bool picVersionAtLeast(const char* ver, int major, int minor)
{
  if (ver == nullptr || ver[0] < '0' || ver[0] > '9') return false;
  char* end = nullptr;
  const long maj = strtol(ver, &end, 10);
  if (end == ver || *end != '.') return false;
  const char* minStart = end + 1;
  if (minStart[0] < '0' || minStart[0] > '9') return false;
  const long mnr = strtol(minStart, &end, 10);
  if (maj != major) return maj > major;
  return mnr >= minor;
}

enum PicSensorEResult : unsigned char {
  PIC_SENSOR_E_VALID,       // out holds the temperature
  PIC_SENSOR_E_NO_READING,  // "-": no sensor or no valid reading
  PIC_SENSOR_E_INVALID      // not a number, trailing text, or outside the plausible range
};

// Parses the value of a "PR: E=<value>" reply (the text after "E=").
static inline PicSensorEResult picParseSensorE(const char* value, float& out)
{
  if (value == nullptr || value[0] == '\0') return PIC_SENSOR_E_INVALID;
  if (value[0] == '-' && value[1] == '\0') return PIC_SENSOR_E_NO_READING;
  char* end = nullptr;
  const double v = strtod(value, &end);
  if (end == value || *end != '\0') return PIC_SENSOR_E_INVALID;
  if (!(v >= -40.0 && v <= 130.0)) return PIC_SENSOR_E_INVALID;  // also rejects NaN
  out = static_cast<float>(v);
  return PIC_SENSOR_E_VALID;
}

// Gate for sending "PR=E": PIC path, not flashing, not in PS=1 mode (the summary
// carries MsgID 28 itself), PIC firmware 6.6 or later, sensor function "R".
static inline bool picTretPollGate(bool picPath, bool flashing, bool psMode,
                                   const char* picFwVersion, char sensorFunction)
{
  if (!picPath || flashing || psMode) return false;
  if (!picVersionAtLeast(picFwVersion, 6, 6)) return false;
  return sensorFunction == 'R';
}

/*
***************************************************************************
**  Program  : test/host/sat_pid_shim/Arduino.h
**
**  Stand-in for <Arduino.h> so test_sat_pid_derivative.cpp can include the
**  real SATtypes.h on the host (TASK-1195). Platform surface only: the
**  types and macros the SAT headers and SATpid.ino need to compile. Nothing
**  of the SAT controller is reimplemented here.
**
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
***************************************************************************
*/
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

#define PROGMEM
#define PSTR(s) (s)
#define F(s)    (s)

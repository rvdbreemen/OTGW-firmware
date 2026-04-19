/*
***************************************************************************
**  Program  : debugStuff.h
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**  Met dank aan Willem Aandewiel en Erik
**
**  Debug-output MACHINERY interface (ADR-079 split):
**    - Debug* macros (telnet-routed print/println/printf)
**    - DebugT* macros (same with timestamp + function(line) prefix via _debugBOL)
**    - DebugFlush macro
**    - Forward declarations for the two helper functions implemented in
**      debugStuff.ino.
**
**  This file is PURE INTERFACE: only macros and function decls, no bodies.
**  Bodies live in debugStuff.ino to follow the project-wide stuff.h/.ino
**  sibling convention (see MQTTstuff.h/.ino, helperStuff.h/.ino,
**  networkStuff.h/.ino, OTGW-Core.h/.ino).
**
**  Runtime per-subsystem trace toggles (state.debug.bXxx) live in the data
**  sibling: Debugtypes.h. Module-specific conditional macros (OTDebug*,
**  MQTTDebug*, RESTDebug*, SensorDebug*, SATDebug*, OTDDebug*) are defined
**  locally per .ino file, gated on those flags.
**
**  OTGW uses the hardware Serial interface for the PIC link, so debug
**  output never goes to Serial -- only to the telnet client via
**  debugTelnet (SimpleTelnet<1>, declared in OTGW-firmware.h, defined in
**  networkStuff.ino).
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include "platform.h"

// ============================================================================
// Types (ADR-081 — merged from former Debugtypes.h)
// ============================================================================
//
// DebugSection lives here (inside debugStuff.h) because debug is a component
// that owns both machinery and a state-struct, and per ADR-081 types merge
// into <Component>stuff.h when a stuff.h sibling exists.
// Runtime per-subsystem trace toggles; accessed as state.debug.<field>.

struct DebugSection {          // state.debug — Runtime diagnostic output flags
  bool     bOTmsg                 = true;   // was bDebugOTmsg — OpenTherm message trace
  bool     bRestAPI               = false;  // was bDebugRestAPI — REST API request trace
  bool     bMQTT                  = false;  // was bDebugMQTT — MQTT publish/receive trace
  bool     bMQTTGate              = false;  // MQTT gate decisions: interval/change-based publish logic
  bool     bSensors               = false;  // was bDebugSensors — Dallas sensor scan trace
  bool     bNTP                   = true;   // NTP time sync telemetry (on by default for diagnostics)
  bool     bSensorSim             = false;  // was bDebugSensorSimulation
  bool     bOTGWSimulation        = false;  // was bDebugOTGWSimulation
  bool     bSAT                   = true;   // SAT control loop + cycle + HCR trace (default on)
  bool     bOTDirect              = true;   // OTDirect frame handling + PI loop trace (default on)
  uint32_t iOTGWSimulationIntervalMs = 750;
  uint32_t iOTGWSimulationNextDueMs  = 0;
};

// ============================================================================
// Interface (macros + function declarations)
// ============================================================================

/*---- start macros -------------------------------------------------------*/

#define Debug(...)      ({ debugTelnet.print(__VA_ARGS__);    })
#define Debugln(...)    ({ debugTelnet.println(__VA_ARGS__);  })
#define Debugf(...)     ({ _debugPrintf_P(__VA_ARGS__);       })

#define DebugFlush()    ({ debugTelnet.flush(); })

#define DebugT(...)     ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debug(__VA_ARGS__);                 \
                        })
#define DebugTln(...)   ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugln(__VA_ARGS__);               \
                        })
#define DebugTf(...)    ({ _debugBOL(__FUNCTION__, __LINE__);  \
                           Debugf(__VA_ARGS__);                \
                        })

/*---- end macros ---------------------------------------------------------*/

// Forward declarations. Implementations live in debugStuff.ino so that only
// a single translation unit instantiates the function bodies -- preventing
// the multi-definition linker-error class when any future standalone .cpp
// picks up this header.
void _debugPrintf_P(PGM_P fmt, ...);
void _debugBOL(const char *fn, int line);

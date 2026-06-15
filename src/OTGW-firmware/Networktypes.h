/*
***************************************************************************
**  Program  : Networktypes.h
**  Version  : v2.0.0-alpha.198
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**
**  Per-component types header for network transport (ADR-079). Bundles
**  the runtime-state struct and the persisted Ethernet configuration
**  because they are conceptually one component (network).
**  Contents:
**    - OTGWNetworkMode enum (WiFi / Ethernet / AP_FALLBACK prerelease)
**    - NetworkSection (state.net — active network transport state)
**    - EthernetSection (settings.eth — persisted static-IP configuration)
**
**  Requires HAS_ETH_CAPABLE / _VERSION_PRERELEASE from boards.h; include
**  AFTER boards.h.
**
**  TERMS OF USE: MIT License. See OTGW-firmware.h for the full notice.
***************************************************************************
*/

#pragma once

#include <Arduino.h>

//===================[ Network Transport — WiFi vs Ethernet ]===================
enum OTGWNetworkMode : uint8_t {
  NET_WIFI        = 0,   // Using WiFi (default)
  NET_ETHERNET    = 1,   // Using wired Ethernet (W5500)
#if defined(_VERSION_PRERELEASE)
  NET_AP_FALLBACK = 2,   // BETA ONLY — SoftAP when WiFi unavailable
#endif
};

// NetworkSection is always present; Ethernet fields only on capable hardware.
struct NetworkSection {        // state.net — active network transport state
#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
  OTGWNetworkMode eMode        = NET_WIFI;
  bool bEthernetLink           = false;   // physical link up on W5500
#endif
#if defined(_VERSION_PRERELEASE)
  // *** BETA ONLY — AP fallback flag ***
  // Guarded by _VERSION_PRERELEASE. If this define is absent (production release),
  // this field does not exist and AP fallback cannot activate under any circumstance.
  bool bAPFallback             = false;   // true while SoftAP fallback is active
  char sAPSSID[32]             = "";      // SSID of the active fallback AP
#endif
};

#if defined(HAS_ETH_CAPABLE) && HAS_ETH_CAPABLE
struct EthernetSection {
  bool bStaticIP       = false;             // false=DHCP (default), true=use static IP
  char sIPaddress[16]  = "0.0.0.0";        // Static IP address
  char sGateway[16]    = "0.0.0.0";        // Gateway
  char sSubnet[16]     = "255.255.255.0";  // Subnet mask
  char sDNS[16]        = "0.0.0.0";        // DNS server (0.0.0.0 = use gateway)
};
#endif

// WifiSection — persisted static IP for the WiFi STA interface.
// All fields default to empty string; empty sStaticIp means DHCP (the OS
// keeps DHCP mode unchanged and the other fields are ignored).
struct WifiSection {
  char sStaticIp[16] = "";   // e.g. "192.168.1.100" (empty = DHCP)
  char sSubnet[16]   = "";   // e.g. "255.255.255.0"
  char sGateway[16]  = "";   // e.g. "192.168.1.1"
  char sDns1[16]     = "";   // e.g. "8.8.8.8"
  char sDns2[16]     = "";   // e.g. "8.8.4.4"
};

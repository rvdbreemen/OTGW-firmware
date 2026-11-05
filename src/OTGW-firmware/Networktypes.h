/*
***************************************************************************
**  Program  : Networktypes.h
**  Version  : v2.0.0-beta.95
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
**  TERMS OF USE: GNU GPLv3. See OTGW-firmware.h for the full notice.
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
  bool bNoPortal     = false; // TASK-1645: saved WiFi unreachable at boot -> no portal/AP, keep retrying
};

// TASK-1446 types begin
// WiFi station link record (TASK-1446 banner, TASK-1444 device/info; GH #682).
// Filled by the WiFi event callbacks in networkStuff.ino, read as one copy under
// its lock (wifiLinkSnapshot()). One outage runs from the first disconnect event
// after a connect to the next connect; the driver repeats the event while it
// retries, so iDropEvents can be far above 1 for one outage.
enum WifiSelfCause : uint8_t {   // who caused the last drop
  WIFI_SELF_NONE      = 0,       // the network (a field outage)
  WIFI_SELF_ETHERNET  = 1,       // switchToEthernet(): disconnect + WIFI_OFF
  WIFI_SELF_AP_RETRY  = 2,       // BETA AP fallback: reconnect attempt timed out
  WIFI_SELF_DEAD_LINK = 3,       // dead-link check: forced reconnect (TASK-1450)
};

struct WifiLinkRecord {
  bool          bLinkUp           = false;  // associated (connected event seen, no disconnect since)
  bool          bJoinSeen         = false;  // joinedBssid/iJoinedChannel are filled
  uint8_t       joinedBssid[6]    = {0};    // AP of the last connect
  uint8_t       iJoinedChannel    = 0;
  uint32_t      iDisconnectEvents = 0;      // every disconnect event since boot, self-caused included
  uint32_t      iOutages          = 0;      // field outages since boot; self-caused drops are not counted
  bool          bDropValid        = false;  // the drop fields below describe a drop (field or self)
  uint8_t       iDropFirstReason  = 0;      // wifi_err_reason_t of the first event of the last drop
  uint8_t       iDropLastReason   = 0;      // ... of its last event
  uint8_t       dropBssid[6]      = {0};    // AP that was left
  uint8_t       iDropChannel      = 0;      // channel that was left
  uint32_t      iDropEvents       = 0;      // disconnect events in the last drop
  uint32_t      iDropStartMs      = 0;      // millis() of its first event
  uint32_t      iDropDurationMs   = 0;      // set on the next connect; 0 while the drop lasts
  WifiSelfCause eDropSelf         = WIFI_SELF_NONE;
};
// TASK-1446 types end

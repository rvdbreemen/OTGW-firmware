/* 
***************************************************************************  
**  Program  : helperStuff
**  Version  : v1.7.0-beta.13
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <Arduino.h>  // for type definitions

template <typename T> void PROGMEM_readAnything (const T * sce, T& dest)
{
  memcpy_P (&dest, sce, sizeof (T));
}

//===========================================================================================
// Get High Resolution Timestamp for Logs
//===========================================================================================
const char* getOTLogTimestamp() {
  static char timestamp[16]; // "HH:MM:SS.mmmmmm"

  // Cache timezone object and time decomposition once per second.
  // Same pattern as _debugBOL() — avoids recreating the timezone object
  // and running ZonedDateTime conversion on every OT message (~10/sec).
  static TimeZone cachedTz;
  static time_t   lastTzUpdate = 0;
  static bool     tzInitialized = false;
  static time_t   lastCachedSec = 0;
  static char     cachedHMS[9] = "00:00:00"; // "HH:MM:SS"

  timeval now;
  gettimeofday(&now, nullptr);
  time_t now_sec = now.tv_sec;

  // Refresh timezone object every 5 minutes
  if (now_sec > 0 && (!tzInitialized || now_sec - lastTzUpdate > 300)) {
    TimeZone newTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
    if (!newTz.isError()) {
      cachedTz = newTz;
      lastTzUpdate = now_sec;
      tzInitialized = true;
    }
  }
  if (!tzInitialized) {
    cachedTz = TimeZone::forTimeOffset(TimeOffset::forMinutes(0));
    tzInitialized = true;
  }

  // Refresh HH:MM:SS once per second
  if (now_sec != lastCachedSec) {
    ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now_sec, cachedTz);
    snprintf_P(cachedHMS, sizeof(cachedHMS), PSTR("%02d:%02d:%02d"),
               myTime.hour(), myTime.minute(), myTime.second());
    lastCachedSec = now_sec;
  }

  snprintf_P(timestamp, sizeof(timestamp), PSTR("%s.%06lu"),
             cachedHMS, (unsigned long)(now.tv_usec));
  return timestamp;
}

//===========================================================================================
// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}



//===========================================================================================
boolean isValidIP(IPAddress ip)
{
 /* Works as follows:
   *  example: 
   *  127.0.0.1 
   *   1 => 127||0||0||1 = 128>0 = true 
   *   2 => !(false || false) = true
   *   3 => !(false || false || false || false ) = true
   *   4 => !(true && true && true && true) = false
   *   5 => !(false) = true
   *   true && true & true && false && true = false ==> correct, this is an invalid addres
   *   
   *   0.0.0.0
   *   1 => 0||0||0||0 = 0>0 = false 
   *   2 => !(true || true) = false
   *   3 => !(false || false || false || false) = true
   *   4 => !(true && true && true && tfalse) = true
   *   5 => !(false) = true
   *   false && false && true && true && true = false ==> correct, this is an invalid addres
   *   
   *   192.168.0.1 
   *   1 => 192||168||0||1 =233>0 = true 
   *   2 => !(false || false) = true
   *   3 +> !(false || false || false || false) = true
   *   4 => !(false && false && true && true) = true
   *   5 => !(false) = true
   *   true & true & true && true && true = true ==> correct, this is a valid address
   *   
   *   255.255.255.255
   *   1 => 255||255||255||255 =255>0 = true 
   *   2 => !(false || false ) = true
   *   3 => !(true && true && true && true) = false  (all four octets are 255 => global broadcast)
   *   true && true && false = false ==> correct, this is an invalid address
   *   
   *   0.123.12.1       => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.0.0.0         => true && false && true && true && true = false  ==> correct, this is an invalid address 
   *   10.255.0.1       => true && true && true && true && true = true    ==> correct, this is a valid address 
   *   150.150.255.150  => true && true && true && true && true = true    ==> correct, this is a valid address 
   *   
   *   123.21.1.99      => true && true && true && true && true = true    ==> correct, this is a valid address 
   *   1.1.1.1          => true && true && true && true && true = true    ==> correct, this is a valid address 
   *   
   *   Some references on valid ip addresses: 
   *   - https://www.rfc-editor.org/rfc/rfc3986 (IPv4 octet range 0-255)
   *   - https://www.rfc-editor.org/rfc/rfc1123 (dotted-decimal host syntax)
   *   
   */
  boolean _isValidIP = false;
  _isValidIP = ((ip[0] || ip[1] || ip[2] || ip[3])>0);                             // if any bits are set, then it is not 0.0.0.0
  _isValidIP &= !((ip[0]==0) || (ip[3]==0));                                       // if either the first or last is a 0, then it is invalid
  _isValidIP &= !(ip[0]==255 && ip[1]==255 && ip[2]==255 && ip[3]==255);             // only reject global broadcast address 255.255.255.255
  _isValidIP &= !(ip[0]==127 && ip[1]==0 && ip[2]==0 && ip[3]==1);                 // if not 127.0.0.0 then it might be valid
  _isValidIP &= !(ip[0]>=224);                                                     // if ip[0] >=224 then reserved space  
  
  // DebugTf( PSTR("%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
  // if (_isValidIP) 
  //   Debugln(F(" = Valid IP")); 
  // else 
  //   Debugln(F(" = Invalid IP!"));
    
  return _isValidIP;
  
} //  isValidIP()


uint32_t updateRebootCount()
{
  //simple function to keep track of the number of reboots 
  //return: number of reboots (if it goes as planned)
  uint32_t _reboot = 0;
  #define REBOOTCNT_FILE "/reboot_count.txt"
  if (LittleFS.begin()) {
    //start with opening the file
    File fh = LittleFS.open(REBOOTCNT_FILE, "r");
    if (fh) {
      //read from file
      if (fh.available()){
        //read the first line — static char avoids heap allocation (ADR-004)
        char cntBuf[12] = {0};
        uint8_t n = fh.readBytesUntil('\n', cntBuf, sizeof(cntBuf) - 1);
        cntBuf[n] = '\0';
        _reboot = (uint32_t)strtoul(cntBuf, nullptr, 10);
      }
    }
    fh.close();
    //increment reboot counter
    _reboot++;
    //write back the reboot counter
    fh = LittleFS.open(REBOOTCNT_FILE, "w");
    if (fh) {
      //write to _reboot to file
      fh.println(_reboot);
    }
    fh.close();
  }
  DebugTf(PSTR("Reboot count = [%d]\r\n"), _reboot);
  return _reboot;
}

// Maximum length for filesystem probe paths
#define FS_PROBE_PATH_MAX 32

bool updateLittleFSStatus(const char *probePath)
{
  // Default probe path stored in PROGMEM
  static const char defaultPath[] PROGMEM = "/.health";
  bool useDefault = (probePath == nullptr);
  
  LittleFSmounted = LittleFS.info(LittleFSinfo);
  if (!LittleFSmounted) {
    return false;
  }
  
  // Handle PROGMEM string for default path or use provided path
  char pathBuffer[FS_PROBE_PATH_MAX];
  const char *path;
  if (useDefault) {
    strncpy_P(pathBuffer, defaultPath, sizeof(pathBuffer) - 1);
    pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    path = pathBuffer;
  } else {
    path = probePath;
  }
  
  File probe = LittleFS.open(path, "w");
  if (probe) {
    size_t written = probe.println(F("ok"));
    if (written == 0) {
      // Write failed (e.g. disk full or filesystem error)
      LittleFSmounted = false;
    } else {
      probe.flush();
    }
    probe.close();
  } else {
    LittleFSmounted = false;
  }
  return LittleFSmounted;
}

// PROGMEM overload for updateLittleFSStatus
bool updateLittleFSStatus(const __FlashStringHelper *probePath)
{
  char pathBuffer[FS_PROBE_PATH_MAX];
  PGM_P p = reinterpret_cast<PGM_P>(probePath);
  strncpy_P(pathBuffer, p, sizeof(pathBuffer) - 1);
  pathBuffer[sizeof(pathBuffer) - 1] = '\0';
  return updateLittleFSStatus(pathBuffer);
}

static bool isRebootLogSummaryLine(const char* line)
{
  return (line != nullptr && strstr(line, " - reboot cause: ") != nullptr);
}

static bool isRebootLogDetailLine(const char* line)
{
  return (line != nullptr &&
          (strncmp_P(line, PSTR("ESP register contents:"), 22) == 0 ||
           strncmp_P(line, PSTR("External Reason:"), 16) == 0));
}

static void trimRebootLogLine(char* line)
{
  if (line == nullptr) return;

  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
    line[--len] = '\0';
  }
}

bool readLatestCrashLog(char* summary, size_t summarySize, char* details, size_t detailsSize)
{
  static const char rebootLogFile[] = "/reboot_log.txt";
  char pendingSummary[160] = {0};
  char line[160] = {0};

  if (summary == nullptr || summarySize == 0 || details == nullptr || detailsSize == 0) {
    return false;
  }

  summary[0] = '\0';
  details[0] = '\0';

  // Use the existing LittleFSmounted flag rather than calling LittleFS.begin() again.
  // Calling begin() during or after OTA (when LittleFS.end() was invoked) is unsafe (K3).
  if (!LittleFSmounted) {
    return false;
  }

  File fh = LittleFS.open(rebootLogFile, "r");
  if (!fh) {
    return false;
  }

  while (fh.available()) {
    size_t n = fh.readBytesUntil('\n', line, sizeof(line) - 1);
    line[n] = '\0';
    trimRebootLogLine(line);

    if (line[0] == '\0') {
      continue;
    }

    if (isRebootLogSummaryLine(line)) {
      strlcpy(pendingSummary, line, sizeof(pendingSummary));
      continue;
    }

    if (pendingSummary[0] != '\0' && isRebootLogDetailLine(line)) {
      strlcpy(summary, pendingSummary, summarySize);
      strlcpy(details, line, detailsSize);
      fh.close();
      return true;
    }

    pendingSummary[0] = '\0';
  }

  fh.close();
  return false;
}

bool updateRebootLog(String text)
{
  #define REBOOTLOG_FILE "/reboot_log.txt"
  #define TEMPLOG_FILE "/reboot_log.t.txt"
  #define LOG_LINES 20
  #define LOG_LINE_LENGTH 140

  char log_line[LOG_LINE_LENGTH] = {0};
  char log_line_regs[120] = {0};  // "ESP register contents: epc1=0x12345678, ..." = 116 chars max
  char log_line_excpt[64]  = {0}; // "- Access to invalid address (29)" = 32 chars max
  uint32_t errorCode = -1;

  //waitforNTPsync();
  loopNTP(); // make sure time is up to date (improved error logging)

  struct	rst_info	*rtc_info	=	system_get_rst_info();
  
  if (rtc_info == NULL) {
    DebugTf(PSTR("no reset info available:	%x\r\n"),	errorCode);
  } else {

    DebugTf(PSTR("reset reason:	%x\r\n"),	rtc_info->reason);
    errorCode = rtc_info->reason;
    // Rst cause No.    Cause                     GPIO state
    //--------------    -------------------       -------------
    // 0                Power reboot              Changed
    // 1                Hardware WDT reset        Changed
    // 2                Fatal exception           Unchanged
    // 3                Software watchdog reset   Unchanged
    // 4                Software reset            Unchanged
    // 5                Deep-sleep                Changed
    // 6                Hardware reset            Changed

    if	(rtc_info->reason	==	REASON_WDT_RST	|| rtc_info->reason	==	REASON_EXCEPTION_RST	|| rtc_info->reason	==	REASON_SOFT_WDT_RST)	{

      //The	address	of	the	last	crash	is	printed,	which	is	used	to	debug	garbled	output
      snprintf_P(log_line_regs, LOG_LINE_LENGTH, PSTR("ESP register contents: epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x\r\n"), rtc_info->epc1, rtc_info->epc2, rtc_info->epc3, rtc_info->excvaddr, rtc_info->depc);
      Debugf(log_line_regs);
    }

    if (rtc_info->reason == REASON_EXT_SYS_RST) {
      //external reset, so try to fetch the reset reason from the tiny watchdog and print that
      { char wdReason[64]; initWatchDog(wdReason, sizeof(wdReason));
        snprintf_P(log_line_regs, LOG_LINE_LENGTH, PSTR("External Reason: External Watchdog reason: %s\r\n"), wdReason);
        Debugf(log_line_regs); }      
    }

    if	(rtc_info->reason	==	REASON_EXCEPTION_RST)	{

      // Fatal exception No.    Description             Possible Causes
      // -------------------    --------------          -------------------
      //  0                     Invalid command         1. Damaged BIN binaries
      //                                                2. Wild pointers
      //
      //  6                     Division by zero        Division by zero
      //
      //  9                     Unaligned read/write    1. Unaligned read/write Cache addresses
      //                        operation addresses     2. Wild pointers
      //
      //  28/29                 Access to invalid       1. Access to Cache after it is turned off
      //                        address                 2. Wild pointers
      //
      // more reasons can be found in the "corebits.h" file of the Extensa SDK

      switch(rtc_info->exccause) {
        case 0:   snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Invalid command (0)")); break;
        case 6:   snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Division by zero (6)")); break;
        case 9:   snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Unaligned read/write operation addresses (9)")); break;
        case 28:  snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Access to invalid address (28)")); break;
        case 29:  snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Access to invalid address (29)")); break;
        default:  snprintf_P(log_line_excpt, LOG_LINE_LENGTH, PSTR("- Other (not specified) (%d)"), rtc_info->exccause); break;
      }

      Debugf(PSTR("Fatal exception (%d): %s\r\n"),	rtc_info->exccause, log_line_excpt);
    }
  }

  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
  snprintf_P(log_line, LOG_LINE_LENGTH, PSTR("%d-%02d-%02d %02d:%02d:%02d - reboot cause: %s (%x) %s\r\n"), myTime.year(),  myTime.month(), myTime.day(), myTime.hour(), myTime.minute(), myTime.second(), CSTR(text), errorCode, log_line_excpt);

  if (LittleFS.begin()) {
    //start with opening the file
    File outfh = LittleFS.open(TEMPLOG_FILE, "w");

    if (outfh) {
      //write to _reboot to file
      outfh.print(log_line);

      if (strlen(log_line_regs)>2) {
        outfh.print(log_line_regs);
      }

      File infh = LittleFS.open(REBOOTLOG_FILE, "r");
      
      int i = 1;
      if (infh) {
        //read from file
        while (infh.available() && (i < LOG_LINES)){
          //read the next line — char buffer avoids heap allocation (ADR-004)
          char line[LOG_LINE_LENGTH] = {0};
          int n = infh.readBytesUntil('\n', line, sizeof(line) - 1);
          line[n] = '\0';
          // Filter out empty or very short lines (< 3 chars) to keep log file clean
          if (n > 3) {
            outfh.print(line);
          }
          i++;
        }
        infh.close();
      }
      outfh.close();
      
      if (LittleFS.exists(REBOOTLOG_FILE)) {
        LittleFS.remove(REBOOTLOG_FILE);
      }
      
      LittleFS.rename(TEMPLOG_FILE, REBOOTLOG_FILE);

      return true; // succesfully logged
    }
  }
  
  return false; // logging unsuccesfull
}


// ---------------------------------------------------------------------------
// Reboot-process instrumentation helpers (TASK-396).
// ---------------------------------------------------------------------------
// Three concerns, one block:
//   1. Heap watermark — smallest freeHeap seen since boot, for slow-leak detection.
//   2. Deferred-reboot mechanism — move the actual reboot out of HTTP callback
//      context so the response can flush before service cleanup begins.
//   3. Flash-config sanity check — warn if hardware does not match the 4M2M DIO
//      build assumption (PUYA chips, NodeMCU/Wemos variants, flash-mode mismatch).
//
// All log output uses the "[reboot]" or "[flash]" prefix so the lifecycle is
// greppable across the telnet log. After debugTelnet.stop() runs inside
// prepareForReboot(), subsequent Debug* calls silently drop — we flush before
// that point so the last-seen state is always visible to an external logger.

// Heap watermark. Updated from loop() via rebootHeapWatermarkTick().
static uint32_t g_minFreeHeap = 0xFFFFFFFFUL;

// Deferred reboot pending-flag. Set by requestDeferredReboot() (typically
// from an HTTP or timer callback), observed by the main loop which fires
// performDeferredReboot() on the next tick when !isFlashing().
static volatile bool g_rebootPending = false;
static const char  *g_rebootReason   = "deferred reboot";
static uint32_t     g_rebootRequestMs = 0;

void rebootHeapWatermarkTick() {
  uint32_t h = ESP.getFreeHeap();
  if (h < g_minFreeHeap) g_minFreeHeap = h;
}

uint32_t getMinFreeHeap() {
  return (g_minFreeHeap == 0xFFFFFFFFUL) ? ESP.getFreeHeap() : g_minFreeHeap;
}

bool isRebootPending() {
  return g_rebootPending;
}

void requestDeferredReboot(const char *reason) {
  if (g_rebootPending) {
    DebugTf(PSTR("[reboot] request IGNORED (already pending): was=\"%s\" new=\"%s\"\r\n"),
            g_rebootReason ? g_rebootReason : "?",
            reason ? reason : "?");
    return;
  }
  g_rebootReason = reason ? reason : "deferred reboot";
  g_rebootRequestMs = millis();
  DebugTf(PSTR("[reboot] deferred request: \"%s\" heap=%u minHeap=%u maxBlk=%u frag=%u flashing=%d\r\n"),
          g_rebootReason,
          (unsigned)ESP.getFreeHeap(),
          (unsigned)getMinFreeHeap(),
          (unsigned)ESP.getMaxFreeBlockSize(),
          (unsigned)ESP.getHeapFragmentation(),
          (int)state.flash.bESPactive);
  g_rebootPending = true;
}

void performDeferredReboot() {
  uint32_t deferredMs = millis() - g_rebootRequestMs;
  DebugTf(PSTR("[reboot] performing deferred reboot after %lums defer: \"%s\"\r\n"),
          (unsigned long)deferredMs, g_rebootReason);
  logBootSignature("[reboot] pre-doRestart");
  DebugFlush();
  doRestart(g_rebootReason);
  // doRestart() never returns. Guard against compiler "unreachable" on hosts
  // without [[noreturn]] inference through the call boundary.
  while (true) { delay(1000); }
}

// Warn at boot if the flash chip's real-size or mode doesn't match the 4M2M
// DIO build assumption. Fires at most three WARN lines; silent on matching
// hardware. Strictly informational — the firmware continues to run regardless.
// Catches: wrong partition config (4M1M image on 4M2M board or vice versa),
// PUYA chip with misdetected size, non-DIO flash mode (QIO/QOUT can change
// behaviour around OTA writes on some boards).
void maybeWarnFlashMismatch() {
  const uint32_t real   = ESP.getFlashChipRealSize();
  const uint32_t mapped = ESP.getFlashChipSize();
  const uint8_t  mode   = ESP.getFlashChipMode();  // FM_QIO=0 FM_QOUT=1 FM_DIO=2 FM_DOUT=3 FM_UNKNOWN=255
  if (real != mapped) {
    DebugTf(PSTR("[flash] WARN: real size %u != mapped size %u — wrong board config (expected 4M2M)\r\n"),
            (unsigned)real, (unsigned)mapped);
  }
  if (real < 4UL * 1024UL * 1024UL) {
    DebugTf(PSTR("[flash] WARN: real size %u < 4 MB — this build requires 4M2M (4 MB flash, 2 MB FS)\r\n"),
            (unsigned)real);
  }
  if (mode != 2 /* FM_DIO */ && mode != 0 /* FM_QIO */) {
    DebugTf(PSTR("[flash] WARN: flash mode %u (expected DIO=2 or QIO=0) — OTA writes may misbehave\r\n"),
            (unsigned)mode);
  }
}

// Explicit service cleanup required before ESP.restart() on Arduino Core 3.1.0+.
// PR esp8266/Arduino#8598 removed the implicit WiFiClient/WiFiUDP::stopAll() that
// used to run as part of the Update path. Without manual cleanup, TCP sockets
// linger in lwIP and the WiFi SDK state persists through the soft reset. The
// next boot ends up in a half-state: WiFi is still associated (leftover from
// before the restart) but telnet/HTTP/MQTT fail to bind their ports.
// Reported 2026-04-22 by andrebrait (Discord #beta-testing): force WiFi
// disconnect via AP -> services initialise -> alive. Root cause: Arduino
// Core 3.1.0 breaking change, not an OTGW-firmware regression.
static void prepareForReboot() {
  // Focus on TCP-based services — those are the ones whose lingering lwIP
  // state in Core 3.1.0+ blocks clean reboot. mDNS and LLMNR are UDP
  // responders whose stale state is harmless across a reset, so we do not
  // touch them (also avoids API compatibility issues across Core versions
  // where ESP8266mDNS::end() and LLMNRResponder::end() are not uniformly
  // exposed).
  const uint32_t tStart = millis();
  DebugTf(PSTR("[reboot] prepareForReboot begin, heap=%u maxBlk=%u\r\n"),
          (unsigned)ESP.getFreeHeap(),
          (unsigned)ESP.getMaxFreeBlockSize());

  uint32_t t = millis();
  doMqttDisconnect();     // clean disconnect to broker (file-static wrapper, see MQTTstuff.ino)
  DebugTf(PSTR("[reboot]   mqtt disconnect: %lums\r\n"), (unsigned long)(millis() - t));

  t = millis();
  doWebSocketClose();     // close all WebSocket clients (wrapper, see webSocketStuff.ino)
  DebugTf(PSTR("[reboot]   ws close: %lums\r\n"), (unsigned long)(millis() - t));

  // Final cleanup log before closing the OTGW TCP stream. Telnet logging stays
  // up through the restart call so the final pre-restart line can still reach
  // the operator.
  DebugTf(PSTR("[reboot]   stopping otgwstream, total=%lums heap=%u\r\n"),
          (unsigned long)(millis() - tStart),
          (unsigned)ESP.getFreeHeap());
  DebugFlush();

  // Keep debugTelnet running so the final reboot log is still visible.
  OTGWstream.stop();      // port 25238 OTGW stream

  // IMPORTANT: do NOT call WiFi.disconnect() here. On ESP8266 Arduino with
  // WiFi.persistent(true) (which networkStuff.ino startWiFi() sets, and is
  // the default), WiFi.disconnect() writes an EMPTY station_config to flash
  // NVRAM — wiping the stored SSID and password. The device then boots into
  // the captive portal with no credentials. Reference:
  // ESP8266WiFiSTA.cpp::disconnect() writes wifi_station_set_config(&conf)
  // with conf.ssid = 0 / conf.password = 0 when _persistent is true.
  // This was observed 2026-04-23 — reboot caused WiFi creds to be lost.
  //
  // We intentionally keep WiFi up through this cleanup phase so the TCP FINs
  // from the close/stop calls above can reach their peers before the restart
  // fires.
}

// One-line boot/runtime signature with platform, hardware, heap, and reset
// info. Dev (1.4.x) is ESP8266-only and has no platform abstraction layer, so
// this calls the ESP APIs directly (ESP.getHeapFragmentation() is native on
// ESP8266). Pass a short phase label (e.g. "boot:", "[OTA] pre-begin") so the
// lifecycle is greppable across the telnet log. Ported from 2.0.0 TASK-394
// Phase 2; output format kept identical so logs are cross-branch comparable.
// TASK-396 extensions: minHeap watermark + exccause (raw exception cause from
// ESP.getResetInfoPtr()->exccause; 0 on normal reboot, 1-9 on crash/WDT).
void logBootSignature(const char *phase) {
  rst_info *rst = ESP.getResetInfoPtr();
  const uint32_t exccause = (rst != nullptr) ? rst->exccause : 0;
  DebugTf(PSTR("%s core=%s sdk=%s cpu=%u flashId=0x%08X flashReal=%u flashMap=%u flashSpeed=%u sketch=%u freeSketch=%u md5=%s heap=%u maxBlk=%u frag=%u minHeap=%u exccause=%u reset=[%s]\r\n"),
          phase ? phase : "boot:",
          ESP.getCoreVersion().c_str(),
          ESP.getSdkVersion(),
          (unsigned)ESP.getCpuFreqMHz(),
          (unsigned)ESP.getFlashChipId(),
          (unsigned)ESP.getFlashChipRealSize(),
          (unsigned)ESP.getFlashChipSize(),
          (unsigned)ESP.getFlashChipSpeed(),
          (unsigned)ESP.getSketchSize(),
          (unsigned)ESP.getFreeSketchSpace(),
          ESP.getSketchMD5().c_str(),
          (unsigned)ESP.getFreeHeap(),
          (unsigned)ESP.getMaxFreeBlockSize(),
          (unsigned)ESP.getHeapFragmentation(),
          (unsigned)getMinFreeHeap(),
          (unsigned)exccause,
          ESP.getResetReason().c_str());
}

// Central reboot wrapper. Every intentional reboot path in the firmware should
// go through this — direct ESP.restart()/ESP.reset() calls in application code
// are a bug (one documented exception: networkStuff.ino WiFi-portal timeout,
// where services are not yet up and cleanup would be a no-op).
// TASK-396: richly instrumented so every reboot leaves a breadcrumb trail in
// the telnet log. Each phase logs its duration and a heap snapshot, except the
// final "ESP.restart() now" line which is emitted before the restart request so
// it can still reach telnet.
void doRestart(const char* str) {
  const uint32_t tStart = millis();
  DebugTf(PSTR("[reboot] doRestart(\"%s\") begin, heap=%u minHeap=%u maxBlk=%u frag=%u\r\n"),
          str ? str : "(null)",
          (unsigned)ESP.getFreeHeap(),
          (unsigned)getMinFreeHeap(),
          (unsigned)ESP.getMaxFreeBlockSize(),
          (unsigned)ESP.getHeapFragmentation());

  uint32_t t = millis();
  flushSettings();        // persist any pending settings before reboot
  DebugTf(PSTR("[reboot]   flushSettings: %lums\r\n"), (unsigned long)(millis() - t));

  prepareForReboot();     // graceful shutdown: MQTT LWT, WS close frames, TCP FINs
  // Telnet stays up through the final pre-restart log. DebugFlush() is
  // defensive so the message is on the wire before the restart request.

  delay(2000);            // let TCP FINs + WiFi disassoc propagate (~1-2s RTT budget)

  // Finish with the proven v1.2.0-style soft restart sequence after the newer
  // cleanup/logging steps above.
  DebugTf(PSTR("[reboot]   calling ESP.restart() after %lums total\r\n"),
          (unsigned long)(millis() - tStart));
  DebugFlush();
  ESP.restart();
  delay(5000);  // Safety tail retained from v1.2.0; should not normally return.
}

String upTime() 
{
  char    calcUptime[20];

  snprintf_P(calcUptime, sizeof(calcUptime), PSTR("%d(d)-%02d:%02d(H:m)")
                                          , int((state.uptime.iSeconds / (60 * 60 * 24)) % 365)
                                          , int((state.uptime.iSeconds / (60 * 60)) % 24)
                                          , int((state.uptime.iSeconds / (60)) % 60));

  return calcUptime;

} // upTime()

bool yearChanged(){
  static int16_t lastyear = -1;
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
  int16_t thisyear = myTime.year();
  bool _ret = (lastyear != thisyear); //year changed
  if (_ret) {
    //year changed
    lastyear = thisyear;
  }
  return _ret;
}

bool dayChanged(){
  static int8_t lastday = -1;
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
  int8_t thisday = myTime.day();
  bool _ret = (lastday != thisday);
  if (_ret) {
    //day changed
    lastday = thisday;
  }
  return _ret;
}

bool minuteChanged(){
  static int8_t lastminute = -1;
  TimeZone myTz =  timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
  int8_t thisminute = myTime.minute();
  bool _ret = (lastminute != thisminute);
  if (_ret){
    lastminute = thisminute;
  }
  return _ret;
}

bool hourChanged(){
  static int8_t lasthour = -1;
  TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(time(nullptr), myTz);
  int8_t thishour = myTime.hour();
  bool changed = (lasthour != thishour);
  if (changed) {
    lasthour = thishour;
  }
  return changed;
}

// Path to the LittleFS file containing the build git hash (used by checklittlefshash and getFilesystemHash)
#define GITHASH_FILE "/version.hash"

/*
  check if the version githash is in the littlefs as version.hash

*/
bool checklittlefshash(){
  char _githash[16] = {0};  // git short hash is 7 chars; 16 is ample (ADR-004)
  if (LittleFS.begin()) {
    //start with opening the file
    File fh = LittleFS.open(GITHASH_FILE, "r");
    if (fh) {
      //read from file
      if (fh.available()){
        uint8_t n = fh.readBytesUntil('\n', _githash, sizeof(_githash) - 1);
        _githash[n] = '\0';
        // Trim trailing \r (readBytesUntil drops \n but keeps \r on Windows-style lines)
        if (n > 0 && _githash[n-1] == '\r') _githash[n-1] = '\0';
      }
      fh.close();
    }
    bool match = (strcasecmp(_githash, _VERSION_GITHASH)==0);
    if (!match) {
      DebugTf(PSTR("FS githash = [%s] | FW githash = [%s]\r\n"), _githash, _VERSION_GITHASH);
      DebugTf(PSTR("WARNING: Firmware version (%s) does not match filesystem version (%s)\r\n"),
              _VERSION_GITHASH, _githash);
      DebugTln(F("This may cause compatibility issues. Flash matching filesystem version."));
      state.statusMessage = StatusMessage::LittleFSMismatch;
    } else if (state.statusMessage == StatusMessage::LittleFSMismatch) {
      state.statusMessage = StatusMessage::None;
    }
    return match;
  }
  return false;
}

const __FlashStringHelper* getStatusMessageText()
{
  switch (state.statusMessage) {
    case StatusMessage::LittleFSMismatch:
      return F("Flash your littleFS with matching version!");
    case StatusMessage::PSModeActive:
      return F("PS=1 mode; decoded summary updates active.");
    case StatusMessage::None:
    default:
      return F("");
  }
}

/*
  Get filesystem version hash from /version.hash file
  Returns empty string if file not found or error
*/
const char* getFilesystemHash(){
  // Static char cache — avoids permanent heap String (ADR-004)
  static char _githash[16] = {0};

  // Return cached value if available
  if (_githash[0] != '\0') return _githash;

  if (LittleFS.begin()) {
    if (LittleFS.exists(GITHASH_FILE)) {
      File fh = LittleFS.open(GITHASH_FILE, "r");
      if (fh) {
        if (fh.available()){
          uint8_t n = fh.readBytesUntil('\n', _githash, sizeof(_githash) - 1);
          _githash[n] = '\0';
          if (n > 0 && _githash[n-1] == '\r') _githash[n-1] = '\0';
        }
        fh.close();
      }
    }
  }
  return _githash;
}


String strHTTPmethod(HTTPMethod method)
{
  switch (method)
  {
    case HTTPMethod::HTTP_GET:
      return "GET";
    case HTTPMethod::HTTP_POST:
      return "POST";
    case HTTPMethod::HTTP_PUT:
      return "PUT";
    case HTTPMethod::HTTP_PATCH:
      return "PATCH";
    case HTTPMethod::HTTP_DELETE:
      return "DELETE";
    case HTTPMethod::HTTP_OPTIONS:
      return "OPTIONS";
    case HTTPMethod::HTTP_HEAD:
      return "HEAD";
    default:
      return "";
  }
}

/*
  RSSI signal quality to percentage quad function
  https://www.intuitibits.com/2016/03/23/dbm-to-percent-conversion/#comment-9
  https://gist.github.com/senseisimple/002cdba344de92748695a371cef0176a
*/

int signal_quality_perc_quad(int rssi) {
  const int perfect_rssi = -50;
  const int worst_rssi = -85;
  int nominal_rssi = perfect_rssi - worst_rssi;
  int signal_quality = ceil(100 * nominal_rssi * nominal_rssi - (perfect_rssi - rssi) * (15 * nominal_rssi + 62 * (perfect_rssi - rssi))) / (nominal_rssi * nominal_rssi);
  if (signal_quality > 100) {
      signal_quality = 100;
  } else if (signal_quality < 1) {
      signal_quality = 0;
  }
  return signal_quality;
}



/*
  dBm to Quality statement TL:DR 
  --> https://support.randomsolutions.nl/827069-Best-dBm-Values-for-Wifi
  --> https://www.metageek.com/training/resources/wifi-signal-strength-basics/
  TL;DR strings on quality is based on this
*/
String dBmtoQuality(int dBm)
{
  String _ret="Amazing";
  if (dBm<=-67) { _ret = "Very good";}
  if (dBm<=-70) { _ret = "Okay";}
  if (dBm<=-80) { _ret = "Not good enough";}
  if (dBm<=-90) { _ret = "Unusable";}
  //if (dBm<=-30) { _ret = "Amazing";}

  return (_ret);
}//dBmtoQuality

// Replace all occurrences of token with replacement, guarding buffer size
bool replaceAll(char *buffer, const size_t bufSize, const char *token, const char *replacement) {
  if (!buffer || !token || !replacement) return false;
  const size_t tokenLen = strlen(token);
  const size_t replLen = strlen(replacement);
  if (tokenLen == 0) return true;

  char *pos = strstr(buffer, token);
  while (pos) {
    const size_t tailLen = strlen(pos + tokenLen);
    const size_t required = (pos - buffer) + replLen + tailLen + 1;
    if (required > bufSize) return false;
    memmove(pos + replLen, pos + tokenLen, tailLen + 1);
    memcpy(pos, replacement, replLen);
    pos = strstr(pos + replLen, token);
  }
  return true;
}

//===========================================================================================
// Heap Monitoring and Backpressure Management
//===========================================================================================

// Heap thresholds for different severity levels
// Rationale: ESP8266 typically has ~40KB RAM after core libraries.
// Tuned on tester log data (Crashevans, v1.4.0-beta+0d6942a, debug_2a.txt)
// combined with the burst-reduction fixes from TASK-338/339/340/342. Each
// tier is sized to cover a specific allocation risk:
// - CRITICAL (1.5KB): leaves just one lwIP pbuf (~1.5KB) worth of headroom.
//                     Eronder = near-certain crash zone.
// - WARNING  (3KB):   2x pbuf + streaming chunk. Also the floor for
//                     accepting new WebSocket clients (see webSocketStuff.ino).
// - LOW      (5KB):   sits below the expected in-burst dip floor after the
//                     1.4.1 burst-reduction fixes. Throttling fires only on
//                     abnormal pressure (longer uptime, fragmentation,
//                     extra WS clients), not on routine bursts.
// - HEALTHY (>=5KB):  sufficient for steady-state operation.
#define HEAP_CRITICAL_THRESHOLD   1536   // Critical: Stop all non-essential operations
#define HEAP_WARNING_THRESHOLD    3072   // Warning: Start throttling messages
#define HEAP_LOW_THRESHOLD        5120   // Low: Begin reducing message frequency
// HEAP_LOW_RESTORE_THRESHOLD (6144) is declared in OTGW-firmware.h so it is
// visible to MQTTstuff.ino, which is concatenated before this file (TASK-553).

// Throttling state
static uint32_t lastWebSocketSendMs = 0;
static uint32_t lastMQTTPublishMs = 0;
static uint32_t lastWebSocketWarningMs = 0;
static uint32_t lastMQTTWarningMs = 0;
static uint32_t webSocketDropCount = 0;
static uint32_t mqttDropCount = 0;
// Set by emergencyHeapRecovery() when it stops OTGWstream under CRITICAL without
// restarting it (restart allocates). serviceDeferredStreamRearm() re-arms the
// listener once heap is HEALTHY again. Avoids allocating during the crisis.
static bool sOTGWstreamRearmPending = false;

// Minimum intervals when heap is under pressure (milliseconds)
#define WEBSOCKET_THROTTLE_MS_WARNING  50   // 50ms = max 20 msg/sec when heap is low
#define WEBSOCKET_THROTTLE_MS_CRITICAL 200  // 200ms = max 5 msg/sec when heap is critical
#define MQTT_THROTTLE_MS_WARNING       100  // 100ms = max 10 msg/sec when heap is low
#define MQTT_THROTTLE_MS_CRITICAL      500  // 500ms = max 2 msg/sec when heap is critical

// Diagnostic logging intervals (milliseconds)
#define WARNING_LOG_INTERVAL_MS        10000  // Log warnings every 10 seconds
#define EMERGENCY_RECOVERY_INTERVAL_MS 30000  // Attempt recovery max once per 30 seconds

// HeapHealthLevel enum defined in OTGW-firmware.h

//===========================================================================================
// Check current heap health level
//
// Primary signal is ESP.getFreeHeap(). When freeHeap is already in LOW tier,
// we additionally consult ESP.getMaxFreeBlockSize() so that fragmentation
// promotes the level by one tier. Rationale: umm_malloc has no compaction,
// so a 1.2KB discovery payload can fail when maxBlock<1.2KB even though
// total free looks ok. Promoting early lets the publish gate start throttling
// BEFORE the next allocation silently fails.
//
// Perf note: getMaxFreeBlockSize() walks the full free list. We only call it
// outside the HEALTHY path, so the common case stays cheap.
//===========================================================================================
constexpr uint32_t HEAP_FRAG_PROMOTE_MAXBLOCK = 1536;   // maxBlock below this while freeHeap in LOW -> promote to WARNING (matched to CRITICAL)
HeapHealthLevel getHeapHealth() {
  static HeapHealthLevel lastLevel = HEAP_HEALTHY;
  uint32_t freeHeap = ESP.getFreeHeap();

  HeapHealthLevel level;
  if (freeHeap < HEAP_CRITICAL_THRESHOLD) {
    level = HEAP_CRITICAL;
  } else if (freeHeap < HEAP_WARNING_THRESHOLD) {
    level = HEAP_WARNING;
  } else if (freeHeap < HEAP_LOW_THRESHOLD) {
    // Fragmentation check: if contiguous block is already small, promote
    // one tier so callers back off before the next alloc fails.
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    if (maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK) {
      level = HEAP_WARNING;
    } else {
      level = HEAP_LOW;
    }
  } else {
    level = HEAP_HEALTHY;
  }

  // Track tier-entry transitions for cumulative diagnostics (TASK-346).
  // Only count when moving INTO a stricter tier than the previous call;
  // recovery back to HEALTHY is not counted (focus is on pressure events).
  if (level != lastLevel && level > lastLevel) {
    if (level == HEAP_LOW)      state.heapdiag.iEnteredLowCount++;
    if (level == HEAP_WARNING)  state.heapdiag.iEnteredWarningCount++;
    if (level == HEAP_CRITICAL) state.heapdiag.iEnteredCriticalCount++;
  }
  lastLevel = level;
  return level;
}

//===========================================================================================
// Return heap fragmentation as a percentage (0 = no fragmentation, 100 = max)
// Defined as: 100 * (1 - maxBlock / freeHeap). Observability only — not a gate.
//===========================================================================================
uint8_t getHeapFragmentation() {
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap == 0) return 100;
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  if (maxBlock >= freeHeap) return 0;
  return (uint8_t)(100UL - (100UL * maxBlock / freeHeap));
}

//===========================================================================================
// Check if we can send a WebSocket message (with backpressure)
//===========================================================================================
bool canSendWebSocket() {
  HeapHealthLevel heapLevel = getHeapHealth();
  uint32_t now = millis();

  // Fragmentation pre-flight: free heap can pass the tier gate while the largest
  // contiguous block is too small for the next lwIP/WebSocket allocation, which
  // would return NULL and fault (StoreProhibited). Skip gracefully instead.
  // Only walk the free list when not HEALTHY (cost control, like getHeapHealth).
  if (heapLevel != HEAP_HEALTHY && ESP.getMaxFreeBlockSize() < MQTT_PUBLISH_MIN_MAXBLOCK) {
    state.heapdiag.iWsMaxBlockSkips++;
    if ((uint32_t)(now - lastWebSocketWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-FRAG: skip WebSocket (maxBlock=%u < %u, heap=%u, skips=%u)\r\n"),
              ESP.getMaxFreeBlockSize(), (unsigned)MQTT_PUBLISH_MIN_MAXBLOCK,
              ESP.getFreeHeap(), (unsigned)state.heapdiag.iWsMaxBlockSkips);
      lastWebSocketWarningMs = now;
    }
    return false;
  }

  // Critical: block WebSocket messages completely
  if (heapLevel == HEAP_CRITICAL) {
    webSocketDropCount++;
    state.heapdiag.iWsDropsTotal++;
    // Log warning periodically (use unsigned arithmetic for rollover safety)
    if ((uint32_t)(now - lastWebSocketWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-CRITICAL: Blocking WebSocket (dropped %u msgs, heap=%u, maxBlock=%u bytes)\r\n"),
              webSocketDropCount, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
      lastWebSocketWarningMs = now;
    }
    return false;
  }
  
  // Warning: aggressive throttling
  if (heapLevel == HEAP_WARNING) {
    // Use unsigned arithmetic to handle millis() rollover correctly
    if ((uint32_t)(now - lastWebSocketSendMs) < WEBSOCKET_THROTTLE_MS_CRITICAL) {
      webSocketDropCount++;
      state.heapdiag.iWsDropsTotal++;
      return false;
    }
  }
  
  // Low: moderate throttling
  if (heapLevel == HEAP_LOW) {
    // Use unsigned arithmetic to handle millis() rollover correctly
    if ((uint32_t)(now - lastWebSocketSendMs) < WEBSOCKET_THROTTLE_MS_WARNING) {
      webSocketDropCount++;
      state.heapdiag.iWsDropsTotal++;
      return false;
    }
  }
  
  // Update last send time
  lastWebSocketSendMs = now;
  
  // Log warning if we're dropping messages (use unsigned arithmetic for rollover safety)
  if (webSocketDropCount > 0 && (uint32_t)(now - lastWebSocketWarningMs) > WARNING_LOG_INTERVAL_MS) {
    DebugTf(PSTR("WebSocket throttled: dropped %u msgs (heap=%u, maxBlock=%u bytes)\r\n"),
            webSocketDropCount, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
    lastWebSocketWarningMs = now;
    webSocketDropCount = 0; // reset counter after reporting
  }
  
  return true;
}

//===========================================================================================
// Check if we can publish an MQTT message (with backpressure)
//===========================================================================================
bool canPublishMQTT() {
  HeapHealthLevel heapLevel = getHeapHealth();
  uint32_t now = millis();

  // Fragmentation pre-flight: free heap can pass the tier gate while the largest
  // contiguous block is too small for the PubSubClient/lwIP publish allocation,
  // which would return NULL and fault (StoreProhibited). Skip gracefully instead.
  // Only walk the free list when not HEALTHY (cost control, like getHeapHealth).
  if (heapLevel != HEAP_HEALTHY && ESP.getMaxFreeBlockSize() < MQTT_PUBLISH_MIN_MAXBLOCK) {
    state.heapdiag.iMqttMaxBlockSkips++;
    if ((uint32_t)(now - lastMQTTWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-FRAG: skip MQTT (maxBlock=%u < %u, heap=%u, skips=%u)\r\n"),
              ESP.getMaxFreeBlockSize(), (unsigned)MQTT_PUBLISH_MIN_MAXBLOCK,
              ESP.getFreeHeap(), (unsigned)state.heapdiag.iMqttMaxBlockSkips);
      lastMQTTWarningMs = now;
    }
    return false;
  }

  // Critical: block MQTT messages completely
  if (heapLevel == HEAP_CRITICAL) {
    mqttDropCount++;
    state.heapdiag.iMqttDropsTotal++;
    // Log warning periodically (use unsigned arithmetic for rollover safety)
    if ((uint32_t)(now - lastMQTTWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-CRITICAL: Blocking MQTT (dropped %u msgs, heap=%u, maxBlock=%u bytes)\r\n"),
              mqttDropCount, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
      lastMQTTWarningMs = now;
    }
    return false;
  }
  
  // Warning: aggressive throttling
  if (heapLevel == HEAP_WARNING) {
    // Use unsigned arithmetic to handle millis() rollover correctly
    if ((uint32_t)(now - lastMQTTPublishMs) < MQTT_THROTTLE_MS_CRITICAL) {
      mqttDropCount++;
      state.heapdiag.iMqttDropsTotal++;
      return false;
    }
  }
  
  // Low: moderate throttling
  if (heapLevel == HEAP_LOW) {
    // Use unsigned arithmetic to handle millis() rollover correctly
    if ((uint32_t)(now - lastMQTTPublishMs) < MQTT_THROTTLE_MS_WARNING) {
      mqttDropCount++;
      state.heapdiag.iMqttDropsTotal++;
      return false;
    }
  }
  
  // Update last publish time
  lastMQTTPublishMs = now;
  
  // Log warning if we're dropping messages (use unsigned arithmetic for rollover safety)
  if (mqttDropCount > 0 && (uint32_t)(now - lastMQTTWarningMs) > WARNING_LOG_INTERVAL_MS) {
    DebugTf(PSTR("MQTT throttled: dropped %u msgs (heap=%u, maxBlock=%u bytes)\r\n"),
            mqttDropCount, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
    lastMQTTWarningMs = now;
    mqttDropCount = 0; // reset counter after reporting
  }
  
  return true;
}

//===========================================================================================
// Check if it is safe to serve an HTTP request (maxBlock fragmentation gate)
//
// HTTP request serving (ESP8266WebServer + the ~17KB web UI) is the dominant heap
// fragmenter on this device (field A/B, TASK-841): under sustained browser/dashboard
// load the largest contiguous block collapses and a later publish/socket allocation
// returns NULL and faults (ROM memcpy to a NULL dest). Skipping handleClient() while
// fragmented lets the heap coalesce; the browser/HA simply retries. ONLY the steady-state
// main-loop serve is gated — the OTA/PIC flash-upload handlers keep serving (they must
// finish the in-progress flash). getMaxFreeBlockSize() is only walked when not HEALTHY.
//===========================================================================================
bool canServeHttp() {
  if (getHeapHealth() == HEAP_HEALTHY) return true;
  if (ESP.getMaxFreeBlockSize() < HTTP_SERVE_MIN_MAXBLOCK) {
    state.heapdiag.iHttpFragSkips++;
    static uint32_t lastHttpFragWarnMs = 0;
    uint32_t now = millis();
    if ((uint32_t)(now - lastHttpFragWarnMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-FRAG: skip HTTP serve (maxBlock=%u < %u, heap=%u, skips=%u)\r\n"),
              ESP.getMaxFreeBlockSize(), (unsigned)HTTP_SERVE_MIN_MAXBLOCK,
              ESP.getFreeHeap(), (unsigned)state.heapdiag.iHttpFragSkips);
      lastHttpFragWarnMs = now;
    }
    return false;
  }
  return true;
}

//===========================================================================================
// Get heap statistics for debugging
//===========================================================================================
void logHeapStats() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  HeapHealthLevel level = getHeapHealth();
  
  const char* levelStr = "UNKNOWN";
  switch (level) {
    case HEAP_HEALTHY:  levelStr = "HEALTHY"; break;
    case HEAP_LOW:      levelStr = "LOW"; break;
    case HEAP_WARNING:  levelStr = "WARNING"; break;
    case HEAP_CRITICAL: levelStr = "CRITICAL"; break;
  }
  
  DebugTf(PSTR("Heap: %u bytes free, %u max block, level=%s, WS_drops=%u, MQTT_drops=%u, WS_fragskips=%u, MQTT_fragskips=%u, HTTP_fragskips=%u\r\n"),
          freeHeap, maxBlock, levelStr,
          (uint32_t)state.heapdiag.iWsDropsTotal,
          (uint32_t)state.heapdiag.iMqttDropsTotal,
          (uint32_t)state.heapdiag.iWsMaxBlockSkips,
          (uint32_t)state.heapdiag.iMqttMaxBlockSkips,
          (uint32_t)state.heapdiag.iHttpFragSkips);
}

//===========================================================================================
// Emergency heap recovery - called when heap is critically low.
// ADR-079: performs three concrete recovery actions in order:
//   1. Drop all WebSocket clients (browsers reconnect via graph.js).
//   2. Stop OTGWstream port 25238 (re-armed later once heap recovers; restart
//      allocates a listen socket and must not run while heap is CRITICAL).
//   3. Clear MQTT discovery pending bitmap (JIT path repopulates on next status burst).
// Telnet is intentionally NOT dropped (operators need live diagnostics).
// MQTT is intentionally NOT disconnected (reconnect cost exceeds heap recovered).
// 30-second rate-limit caps disruption frequency.
//===========================================================================================
void emergencyHeapRecovery() {
  static uint32_t lastRecoveryMs = 0;
  uint32_t now = millis();

  // Only attempt recovery once per interval to avoid thrashing
  // Use unsigned arithmetic to handle millis() rollover correctly
  if ((uint32_t)(now - lastRecoveryMs) < EMERGENCY_RECOVERY_INTERVAL_MS) {
    return;
  }
  lastRecoveryMs = now;

  uint32_t heapBefore = ESP.getFreeHeap();
  uint8_t actions = 0;

  // Action 1: drop all WebSocket clients (~2-4 KB lwIP buffer per client).
  // Wrapper lives in webSocketStuff.ino (same pattern as doWebSocketClose).
  if (hasWebSocketClients()) {
    doWebSocketDisconnectAll();
    actions |= 0x01;
  }

  // Action 2: drop OTGWstream port 25238 clients to free their lwIP buffers.
  // STOP ONLY — do NOT restart here. startOTGWstream() -> WiFiServer::begin()
  // ALLOCATES a listen socket, which is self-defeating (and a candidate fault
  // site) while heap is CRITICAL and the largest block is ~300 B. The listener
  // is re-armed later by serviceDeferredStreamRearm() once heap is HEALTHY again.
  OTGWstream.stop();
  if (settings.mqtt.bLegacyPort25238Enabled) sOTGWstreamRearmPending = true;
  actions |= 0x02;

  // Action 3: clear MQTT discovery pending bitmap to stop drip allocations
  // during the critical window. JIT path (ADR-073) re-arms on next status burst.
  clearMQTTConfigPending();
  actions |= 0x04;

  uint32_t heapAfter = ESP.getFreeHeap();
  DebugTf(PSTR("[heap-recovery] before=%u after=%u delta=%+ld actions=0x%02X\r\n"),
          (unsigned)heapBefore, (unsigned)heapAfter,
          (long)((int32_t)heapAfter - (int32_t)heapBefore),
          (unsigned)actions);
}

//===========================================================================================
// Re-arm the OTGWstream listener that emergencyHeapRecovery() stopped under CRITICAL.
// Deferred until heap is HEALTHY so the WiFiServer::begin() allocation has room to
// succeed. Called every doBackgroundTasks() tick; cheap no-op when nothing pending.
//===========================================================================================
void serviceDeferredStreamRearm() {
  if (!sOTGWstreamRearmPending) return;
  if (getHeapHealth() != HEAP_HEALTHY) return;   // wait for real headroom before allocating
  sOTGWstreamRearmPending = false;
  applyLegacyPort25238Setting();                 // honours the enabled setting; re-binds the listener
  DebugTln(F("[heap-recovery] OTGWstream listener re-armed after heap recovery"));
}

//===[ blink status led ]===
void setLed(uint8_t led, uint8_t status){
  pinMode(led, OUTPUT);
  digitalWrite(led, status);
}

void blinkLEDms(uint32_t delay){
  //blink the statusled, when time passed
  DECLARE_TIMER_MS(timerBlink, delay);
  if (DUE(timerBlink)) {
    blinkLEDnow();
  }
}

void blinkLED(uint8_t led, int nr, uint32_t waittime_ms){
    for (int i = nr; i>0; i--){
      blinkLEDnow(led);
      delayms(waittime_ms);
      blinkLEDnow(led);
      delayms(waittime_ms);
    }
}

void blinkLEDnow(uint8_t led = LED1){
  pinMode(led, OUTPUT);
  if (settings.bLEDblink) {
    digitalWrite(led, !digitalRead(led));
  } else setLed(led, OFF);

}

//===[ cooperative delay running background tasks in ms ]===
// Previously used DECLARE_TIMER_MS, which expands to static locals: init-once
// semantics made the wait collapse to 0 ms on the first call and freeze at the
// first interval ever passed on later calls. Use a local timestamp so each call
// honours its own delay_ms parameter and millis() rollover is handled by the
// unsigned subtraction.
void delayms(unsigned long delay_ms)
{
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < delay_ms) {
    doBackgroundTasks();
  }
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

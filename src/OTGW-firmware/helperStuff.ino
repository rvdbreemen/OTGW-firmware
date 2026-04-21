/* 
***************************************************************************  
**  Program  : helperStuff
**  Version  : v2.0.0-beta
**
**  Copyright (c) 2021-2026 Robert van den Breemen
**     based on Framework ESP8266 from Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include <Arduino.h>  // for type definitions
// PROGMEM_readAnything template is defined in helperStuff.h (included via OTGW-firmware.h)

//===========================================================================================
// Get High Resolution Timestamp for Logs
//===========================================================================================
const char* getOTLogTimestamp() {
  static char timestamp[16]; // "HH:MM:SS.mmmmmm"
  timeval now;
  gettimeofday(&now, nullptr);
  // Default to UTC if not initialized, but typically settings.ntp.sTimezone is valid
  // Recreating the timezone object is what _debugBOL does, so we follow that pattern
  // assuming timezoneManager is available.
  TimeZone myTz = timezoneManager.createForZoneName(CSTR(settings.ntp.sTimezone));
  if (myTz.isError()) {
    // Fallback if generic name failed
    myTz = TimeZone::forTimeOffset(TimeOffset::forMinutes(0)); 
  }
  
  ZonedDateTime myTime = ZonedDateTime::forUnixSeconds64(now.tv_sec, myTz);

  // 6 digit subsecond resolution from microseconds (0..999999)
  const unsigned long subSeconds = (unsigned long)(now.tv_usec);

  snprintf_P(timestamp, sizeof(timestamp), PSTR("%02d:%02d:%02d.%06lu"),
           myTime.hour(), myTime.minute(), myTime.second(), subSeconds);

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
  
  LittleFSmounted = platformFSInfo(LittleFSinfo);
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

  errorCode = platformResetCode();
  DebugTf(PSTR("reset reason: %x\r\n"), errorCode);

  platformResetRegisterDump(log_line_regs, sizeof(log_line_regs));
  if (log_line_regs[0] != '\0') Debugf(log_line_regs);

  if (platformIsExternalReset()) {
    char wdReason[64]; initWatchDog(wdReason, sizeof(wdReason));
    snprintf_P(log_line_regs, LOG_LINE_LENGTH, PSTR("External Reason: External Watchdog reason: %s\r\n"), wdReason);
    Debugf(log_line_regs);
  }

  platformResetExceptionInfo(log_line_excpt, sizeof(log_line_excpt));
  if (log_line_excpt[0] != '\0') {
    Debugf(PSTR("Fatal exception: %s\r\n"), log_line_excpt);
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


void doRestart(const char* str) {
  DebugTln(str);
  flushSettings();  // Persist any pending settings before reboot
  delay(2000);  // Enough time for messages to be sent.
  platformRestart();
  delay(5000);  // Enough time to ensure we don't return.
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
    DebugTf(PSTR("Check githash = [%s]\r\n"), _githash);
    DebugTf(PSTR("FS githash = [%s] | FW githash = [%s]\r\n"), _githash, _VERSION_GITHASH);
    bool match = (strcasecmp(_githash, _VERSION_GITHASH)==0);
    if (!match) {
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
// Tuned on tester log data (CrashEvans, v1.4.x beta, debug_2a.txt)
// combined with the burst-reduction fixes. Each tier is sized to cover
// a specific allocation risk:
// - CRITICAL (1.5KB): leaves just one lwIP pbuf (~1.5KB) worth of headroom.
//                     Below this = near-certain crash zone.
// - WARNING  (3KB):   2x pbuf + streaming chunk. Also the floor for
//                     accepting new WebSocket clients (see webSocketStuff.ino).
// - LOW      (5KB):   sits below the expected in-burst dip floor after the
//                     burst-reduction fixes. Throttling fires only on
//                     abnormal pressure (longer uptime, fragmentation,
//                     extra WS clients), not on routine bursts.
// - HEALTHY (>=5KB):  sufficient for steady-state operation.
#define HEAP_CRITICAL_THRESHOLD   1536   // Critical: Stop all non-essential operations
#define HEAP_WARNING_THRESHOLD    3072   // Warning: Start throttling messages
#define HEAP_LOW_THRESHOLD        5120   // Low: Begin reducing message frequency

// Throttling state
static uint32_t lastWebSocketSendMs = 0;
static uint32_t lastMQTTPublishMs = 0;
static uint32_t lastWebSocketWarningMs = 0;
static uint32_t lastMQTTWarningMs = 0;
static uint32_t webSocketDropCount = 0;
static uint32_t mqttDropCount = 0;

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
// we additionally consult platformMaxFreeBlock() so that fragmentation
// promotes the level by one tier. Rationale: umm_malloc has no compaction,
// so a 1.2KB discovery payload can fail when maxBlock<1.2KB even though
// total free looks ok. Promoting early lets the publish gate start throttling
// BEFORE the next allocation silently fails.
//
// Perf note: getMaxFreeBlockSize() walks the full free list. We only call it
// outside the HEALTHY path, so the common case stays cheap.
//===========================================================================================
#define HEAP_FRAG_PROMOTE_MAXBLOCK  1536   // maxBlock below this while freeHeap in LOW → promote to WARNING (matched to CRITICAL)
HeapHealthLevel getHeapHealth() {
  static HeapHealthLevel lastLevel = HEAP_HEALTHY;
  uint32_t freeHeap = platformFreeHeap();


  HeapHealthLevel level;
  if (freeHeap < HEAP_CRITICAL_THRESHOLD) {
    level = HEAP_CRITICAL;
  } else if (freeHeap < HEAP_WARNING_THRESHOLD) {
    level = HEAP_WARNING;
  } else if (freeHeap < HEAP_LOW_THRESHOLD) {
    // Fragmentation check: if contiguous block is already small, promote
    // one tier so callers back off before the next alloc fails.
    uint32_t maxBlock = platformMaxFreeBlock();
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
  uint32_t maxBlock = platformMaxFreeBlock();
  if (maxBlock >= freeHeap) return 0;
  return (uint8_t)(100UL - (100UL * maxBlock / freeHeap));
}

//===========================================================================================
// Check if we can send a WebSocket message (with backpressure)
//===========================================================================================
bool canSendWebSocket() {
  HeapHealthLevel heapLevel = getHeapHealth();
  uint32_t now = millis();
  
  // Critical: block WebSocket messages completely
  if (heapLevel == HEAP_CRITICAL) {
    webSocketDropCount++;
    state.heapdiag.iWsDropsTotal++;
    // Log warning periodically (use unsigned arithmetic for rollover safety)
    if ((uint32_t)(now - lastWebSocketWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-CRITICAL: Blocking WebSocket (dropped %u msgs, heap=%u bytes)\r\n"), 
              webSocketDropCount, platformFreeHeap());
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
    DebugTf(PSTR("WebSocket throttled: dropped %u msgs (heap=%u bytes)\r\n"), 
            webSocketDropCount, platformFreeHeap());
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
  
  // Critical: block MQTT messages completely
  if (heapLevel == HEAP_CRITICAL) {
    mqttDropCount++;
    state.heapdiag.iMqttDropsTotal++;
    // Log warning periodically (use unsigned arithmetic for rollover safety)
    if ((uint32_t)(now - lastMQTTWarningMs) > WARNING_LOG_INTERVAL_MS) {
      DebugTf(PSTR("HEAP-CRITICAL: Blocking MQTT (dropped %u msgs, heap=%u bytes)\r\n"), 
              mqttDropCount, platformFreeHeap());
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
    DebugTf(PSTR("MQTT throttled: dropped %u msgs (heap=%u bytes)\r\n"), 
            mqttDropCount, platformFreeHeap());
    lastMQTTWarningMs = now;
    mqttDropCount = 0; // reset counter after reporting
  }
  
  return true;
}

//===========================================================================================
// Get heap statistics for debugging
//===========================================================================================
void logHeapStats() {
  uint32_t freeHeap = platformFreeHeap();
  uint32_t maxBlock = platformMaxFreeBlock();
  HeapHealthLevel level = getHeapHealth();
  
  const char* levelStr = "UNKNOWN";
  switch (level) {
    case HEAP_HEALTHY:  levelStr = "HEALTHY"; break;
    case HEAP_LOW:      levelStr = "LOW"; break;
    case HEAP_WARNING:  levelStr = "WARNING"; break;
    case HEAP_CRITICAL: levelStr = "CRITICAL"; break;
  }
  
  DebugTf(PSTR("Heap: %u bytes free, %u max block, level=%s, WS_drops=%u, MQTT_drops=%u\r\n"),
          freeHeap, maxBlock, levelStr, webSocketDropCount, mqttDropCount);
}

//===========================================================================================
// Emergency heap recovery - called when heap is critically low
// This function tries to free up memory by clearing non-essential buffers
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
  
  uint32_t heapBefore = platformFreeHeap();
  DebugTf(PSTR("Emergency heap recovery starting (heap=%u bytes)\r\n"), heapBefore);
  
  // Yield to allow ESP8266 to do housekeeping
  yield();
  delay(10);
  
  uint32_t heapAfter = platformFreeHeap();
  // Calculate recovered bytes safely (handle case where heap decreased)
  // Use int32_t to avoid overflow and allow negative values
  int32_t recovered = (int32_t)heapAfter - (int32_t)heapBefore;
  DebugTf(PSTR("Emergency heap recovery complete (heap=%u bytes, recovered=%ld bytes)\r\n"), 
          heapAfter, (long)recovered);
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

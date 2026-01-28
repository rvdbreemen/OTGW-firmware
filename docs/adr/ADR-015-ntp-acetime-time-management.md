# ADR-015: NTP and AceTime for Time Management

**Status:** Accepted  
**Date:** 2020-01-01 (AceTime adoption)  
**Updated:** 2026-01-28 (Documentation)

## Context

The OTGW-firmware needs accurate time for:
- Timestamping OpenTherm messages (WebSocket, logs)
- Scheduling time-based operations
- MQTT message timestamps
- Display in Web UI
- Optional: Sending time to boiler (via PIC firmware)

**Requirements:**
- Accurate time synchronization over network
- Timezone support (DST transitions)
- Configurable timezone (user's location)
- Low memory footprint
- Persistent across reboots
- Human-readable timestamps

**Constraints:**
- No RTC (Real-Time Clock) hardware
- ESP8266 has no battery backup
- Time lost on power cycle
- Must sync over WiFi

## Decision

**Use NTP (Network Time Protocol) for synchronization combined with AceTime library for timezone handling.**

**Architecture:**
- **NTP sync:** ESP8266 Arduino Core's `configTime()` function
- **Timezone library:** AceTime (not ezTime or TimeLib)
- **Sync interval:** Every 30 minutes
- **NTP server:** Configurable (default: pool.ntp.org)
- **Timezone:** User-configurable via Web UI (IANA format, e.g., "Europe/Amsterdam")
- **Storage:** Timezone setting persisted in LittleFS
- **Fallback:** Continue operation if NTP fails (timestamps may be wrong)

**Key components:**
```cpp
#include <AceTime.h>
#include <time.h>  // ESP8266 time functions

// NTP configuration
configTime(timezone_offset, dst_offset, ntp_server);

// AceTime for timezone
ace_time::ExtendedZoneManager zoneManager;
ace_time::TimeZone tz = zoneManager.createForZoneName(settingNTPtimezone);
```

## Alternatives Considered

### Alternative 1: ezTime Library
**Pros:**
- Popular Arduino time library
- Simple API
- Built-in NTP sync
- Timezone support

**Cons:**
- Larger memory footprint (~8KB)
- String-heavy (uses String class)
- Less accurate timezone database
- Discontinued/unmaintained

**Why not chosen:** Memory overhead too large for ESP8266. AceTime is more efficient.

### Alternative 2: TimeLib (Paul Stoffregen)
**Pros:**
- Lightweight
- Well-tested
- Standard Arduino library

**Cons:**
- No timezone support (UTC only)
- No built-in NTP
- Manual DST handling required
- No IANA timezone database

**Why not chosen:** Timezone support is essential. Manual DST rules are error-prone.

### Alternative 3: ESP8266 Built-in Time Only
**Pros:**
- No external library
- Minimal memory
- Direct hardware access

**Cons:**
- No timezone database
- Manual DST transitions
- Limited formatting options
- No IANA timezone names

**Why not chosen:** Lack of timezone database makes this impractical for international users.

### Alternative 4: External RTC Module (DS3231)
**Pros:**
- Battery-backed time keeping
- Survives power loss
- Very accurate (±2ppm)

**Cons:**
- Requires additional hardware
- Cost increase (~$5)
- Uses GPIO pins (I2C)
- Still needs NTP for initial sync
- Overkill for application

**Why not chosen:** Hardware changes not feasible. NTP provides sufficient accuracy.

### Alternative 5: GPS Time Sync
**Pros:**
- Extremely accurate
- No internet dependency
- Works anywhere

**Cons:**
- Requires GPS module
- Significant cost increase
- Antenna needed
- Indoor reception poor
- Power consumption
- Complexity

**Why not chosen:** Cost and complexity far exceed requirements.

## Consequences

### Positive
- **Accurate time:** NTP provides millisecond accuracy
- **Timezone support:** AceTime handles 600+ timezones with DST
- **Low memory:** AceTime uses ~4KB RAM (acceptable)
- **IANA database:** Standard timezone names (Europe/Amsterdam, America/New_York)
- **Automatic DST:** No manual rule updates needed
- **User-friendly:** Timezone selected by name in Web UI
- **Persistent:** Timezone setting survives reboot
- **Optional PIC sync:** Can send time to boiler thermostat

### Negative
- **Network dependency:** Requires internet/NTP server access for sync
  - Mitigation: Continues operation with last known time
  - Mitigation: 30-minute resync interval keeps time accurate
- **Boot delay:** Must wait for NTP sync (can take 1-5 seconds)
  - Mitigation: Non-blocking sync, system continues booting
- **Power loss:** Time lost if power fails before NTP sync
  - Accepted: Time accuracy not critical for device boot
- **Memory usage:** AceTime adds ~4KB RAM
  - Accepted: Worth it for proper timezone support
- **Timezone data size:** ~40KB in flash for timezone database
  - Accepted: Flash space is less constrained than RAM

### Risks & Mitigation
- **NTP server unreachable:** Cannot sync time
  - **Mitigation:** Multiple NTP servers configured (pool.ntp.org is pool of servers)
  - **Mitigation:** Device continues with last known time
  - **Mitigation:** Retry on next 30-minute interval
- **Timezone name typo:** Invalid timezone in configuration
  - **Mitigation:** Web UI validates timezone name
  - **Mitigation:** Fallback to UTC if invalid
- **DST transition bugs:** AceTime may have bugs in DST rules
  - **Mitigation:** AceTime actively maintained, updates available
- **Time zones change:** Political changes to timezone rules
  - **Mitigation:** AceTime database can be updated with firmware update

## Implementation Details

**NTP synchronization:**
```cpp
void startNTP() {
  if (settingNTPenable) {
    // Configure NTP with timezone offset
    configTime(0, 0, settingNTPhostname);  // UTC, let AceTime handle timezone
    
    DebugTf(PSTR("NTP sync started with server: %s\r\n"), settingNTPhostname);
  }
}

// In loop - check sync every 30 minutes
DECLARE_TIMER_MIN(ntpTimer, 30);
if (DUE(ntpTimer) && settingNTPenable) {
  startNTP();  // Resync
}
```

**AceTime timezone handling:**
```cpp
#include <AceTime.h>
using namespace ace_time;

ExtendedZoneManager zoneManager;

void setupTimezone() {
  // Create timezone from IANA name
  TimeZone tz = zoneManager.createForZoneName(settingNTPtimezone);
  
  if (tz.isError()) {
    DebugTf(PSTR("Invalid timezone: %s, using UTC\r\n"), settingNTPtimezone);
    tz = TimeZone::forUtc();
  }
  
  // Set timezone for time functions
  setTimeZone(tz);
}
```

**Timestamp generation:**
```cpp
char* formatTime(const char* format) {
  static char buffer[32];
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);  // Converts to local timezone
  
  strftime(buffer, sizeof(buffer), format, timeinfo);
  return buffer;
}

// Usage
DebugTf(PSTR("Current time: %s\r\n"), formatTime("%Y-%m-%d %H:%M:%S"));
// Output: "Current time: 2026-01-28 14:23:45"
```

**WebSocket timestamps (microsecond precision):**
```cpp
void sendOTMessage(const char* message) {
  char timestamp[32];
  
  // Get current time
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  // Get microseconds
  uint32_t micros_part = micros() % 1000000;
  
  // Format: HH:MM:SS.mmmmmm
  snprintf_P(timestamp, sizeof(timestamp),
    PSTR("%02d:%02d:%02d.%06lu"),
    timeinfo->tm_hour,
    timeinfo->tm_min,
    timeinfo->tm_sec,
    micros_part);
  
  // Send to WebSocket
  char fullMessage[256];
  snprintf_P(fullMessage, sizeof(fullMessage),
    PSTR("%s %s"), timestamp, message);
  
  webSocket.broadcastTXT(fullMessage);
}
```

**Settings configuration:**
```cpp
// In settings.json
{
  "NTPenable": true,
  "NTPhostname": "pool.ntp.org",
  "NTPtimezone": "Europe/Amsterdam",
  "NTPsendtime": false  // Send time to PIC/thermostat
}
```

**Web UI timezone selector:**
```html
<select id="timezone">
  <option value="Europe/Amsterdam">Europe/Amsterdam (CET/CEST)</option>
  <option value="America/New_York">America/New_York (EST/EDT)</option>
  <option value="Asia/Tokyo">Asia/Tokyo (JST)</option>
  <!-- 600+ timezones supported -->
</select>
```

## Time-Related Features

**1. WebSocket message timestamps:**
- Format: `HH:MM:SS.mmmmmm` (microsecond precision)
- Timezone: User's configured timezone
- Use: Real-time OpenTherm message log

**2. MQTT timestamps:**
- ISO 8601 format in MQTT messages
- Use: InfluxDB, Grafana integration

**3. REST API time endpoint:**
```
GET /api/v1/time
{
  "epoch": 1706451825,
  "utc": "2026-01-28T13:23:45Z",
  "local": "2026-01-28T14:23:45+01:00",
  "timezone": "Europe/Amsterdam",
  "ntp_synced": true
}
```

**4. Optional PIC time sync:**
- Send current time to PIC firmware (once per minute)
- Allows thermostat to display correct time
- Configurable: `settingNTPsendtime` boolean

## NTP Configuration Options

**Default NTP server:**
```
pool.ntp.org  # DNS round-robin to nearest NTP server
```

**Alternative servers:**
- `time.nist.gov` - US NIST
- `time.google.com` - Google NTP
- `time.cloudflare.com` - Cloudflare NTP
- Local NTP server on network

**Sync strategy:**
- Initial sync: On WiFi connection
- Periodic resync: Every 30 minutes
- On demand: Manual trigger via REST API

## Related Decisions
- ADR-007: Timer-Based Task Scheduling (30-minute NTP timer)
- ADR-008: LittleFS for Configuration Persistence (timezone setting storage)
- ADR-005: WebSocket for Real-Time Streaming (timestamp format)

## References
- AceTime library: https://github.com/bxparks/AceTime
- ESP8266 time functions: https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#time
- Implementation: `helperStuff.ino` (timestamp functions)
- Settings: `settingStuff.ino` (NTP configuration)
- IANA timezone database: https://www.iana.org/time-zones
- Debug output: `Debug.h` (timezone-aware timestamps)

# Plan: OTGW32 Hardware Support — 2 Build Targets with Runtime Feature Detection

## Context

The OTGW firmware needs to support two Nodoshop hardware variants in a single codebase:
- **PIC-based OTGW** (ESP8266): PIC16F co-processor on UART handles the OT bus
- **OT-direct OTGW32** (ESP32-S3): ESP32 drives OpenTherm directly via GPIO interrupts, no PIC

**Decision:** 2 build targets — ESP8266 (PIC) and ESP32-S3 (OT-direct). Running `python build.py` with no arguments builds **both** binaries by default. The hypothetical ESP32+PIC combination is dropped since no such hardware exists.

**Base branch:** `dev-version-1.4.0-fix`

Within each build, the firmware auto-detects what hardware features are actually present at runtime (PIC presence, OT bus liveness, OLED, Ethernet, sensors).

## Design Principles

- **MCU is orthogonal to OTGW hardware** — ESP8266/ESP32 is the processor; PIC/OT-direct is the OpenTherm interface. Don't conflate them.
- **Compile-time for electrical constraints** — GPIO pin maps and code inclusion (`HAS_PIC`, `HAS_DIRECT_OT`) are compile-time because the boards are physically different.
- **Runtime for everything detectable** — PIC presence, OT bus liveness, OLED, Ethernet, sensors are all probed at boot.
- **Frame bridge pattern** — Convert 32-bit GPIO OT frames to the same text format the PIC uses (`B%08lX`, `T%08lX`), then feed into the existing `processOT()` parser. This reuses the entire MQTT/REST/WebSocket stack unchanged.

## Runtime Detection Per Build

### ESP8266 build (PIC):
- `detectPIC()` → PIC found or not (already implemented and proven)
- I2C scan for watchdog at 0x26 → confirms Nodoshop board identity
- `isPICEnabled()` gates all PIC features at runtime

### ESP32-S3 OTGW32 build (OT-direct):
- Enable step-up converter (GPIO 10), verify OT master input settles to idle → `isOTDirectEnabled()`
- I2C scan on GPIO 17/18 for OLED at 0x3C → `state.hw.bOLEDPresent`
- SPI probe for W5500 Ethernet → `state.hw.bEthernetPresent`
- Send OT status request to boiler → `state.otgw.bOnline`
- Dallas 1-Wire sensor scan → same as ESP8266

| Feature | ESP8266 build | ESP32-S3 OTGW32 build |
|---|---|---|
| PIC presence | `detectPIC()` ✓ | N/A (no PIC) |
| PIC firmware version | Banner parsing ✓ | N/A |
| I2C watchdog (0x26) | I2C scan ✓ | N/A (different I2C bus) |
| OT bus online | Via PIC status ✓ | OT frame probe ✓ |
| OLED display | N/A | I2C scan at 0x3C ✓ |
| W5500 Ethernet | N/A | SPI probe ✓ |
| Dallas 1-Wire sensors | Sensor scan ✓ | Sensor scan ✓ |

## Implementation Plan

### Step 1: Add `BOARD_NODOSHOP_OTGW32` to `boards.h`

Add a third board section with OTGW32 pin definitions and feature flags:

```cpp
#elif defined(BOARD_NODOSHOP_OTGW32)
// Nodoshop OTGW32 — ESP32-S3, direct GPIO OpenTherm
// NOTE: Pin numbers below are from the OT-Thing NODO ESP32-S3 variant (hwdef.h).
// Verify against actual Nodoshop OTGW32 hardware before deploying.

#define HAS_PIC           0
#define HAS_DIRECT_OT     1
#define HAS_OLED_CAPABLE  1   // probed at runtime via I2C
#define HAS_ETH_CAPABLE   1   // probed at runtime via SPI

// OpenTherm GPIO pins
#define PIN_OT_MASTER_IN  21
#define PIN_OT_MASTER_OUT 1
#define PIN_OT_SLAVE_IN   6
#define PIN_OT_SLAVE_OUT  7
#define PIN_STEPUP_ENABLE 10
#define PIN_BYPASS_RELAY  47

// I2C (OLED)
#define PIN_I2C_SCL       17
#define PIN_I2C_SDA       18

// LEDs, button, 1-wire
#define PIN_STATUS_LED    8
#define PIN_OT_RED_LED    2
#define PIN_OT_GREEN_LED  48
#define PIN_BUTTON        9
#define PIN_1WIRE         4
```

Add feature flags to the existing ESP8266 board:
```cpp
// BOARD_NODOSHOP_ESP8266:
#define HAS_PIC           1
#define HAS_DIRECT_OT     0
```

Remove or deprecate `BOARD_NODOSHOP_ESP32` (no real hardware exists). The auto-detect fallback (`#elif defined(ESP32) → BOARD_NODOSHOP_ESP32`) changes to default to `BOARD_NODOSHOP_OTGW32` instead.

**File:** `src/OTGW-firmware/boards.h`

### Step 2: State model and guard functions

Add hardware mode enum and detection state to `OTGW-firmware.h`:

```cpp
enum OTGWHardwareMode : uint8_t {
  HW_MODE_UNKNOWN = 0,
  HW_MODE_PIC,
  HW_MODE_OT_DIRECT,
  HW_MODE_DEGRADED
};

struct HardwareSection {
  OTGWHardwareMode eMode = HW_MODE_UNKNOWN;
  bool bOLEDPresent    = false;
  bool bEthernetPresent = false;
};
```

Add to `OTGWState`:
```cpp
HardwareSection hw;   // state.hw.eMode, state.hw.bOLEDPresent, etc.
```

Update guard functions:
```cpp
inline bool isPICEnabled() {
#if HAS_PIC
  return state.hw.eMode == HW_MODE_PIC;
#else
  return false;  // compiled out on OTGW32
#endif
}

inline bool isOTDirectEnabled() {
#if HAS_DIRECT_OT
  return state.hw.eMode == HW_MODE_OT_DIRECT;
#else
  return false;  // compiled out on PIC boards
#endif
}
```

Existing `state.pic.bAvailable` is set from `state.hw.eMode == HW_MODE_PIC` for backward compat.

**File:** `src/OTGW-firmware/OTGW-firmware.h`

### Step 3: Gate OTGWSerial and PIC code behind `HAS_PIC`

```cpp
#if HAS_PIC
OTGWSerial OTGWSerial(PIN_PIC_RST, PIN_LED2);
#endif
```

Wrap `detectPIC()`, `handleOTGW()`, PIC firmware upgrade code in `#if HAS_PIC`. All existing `isPICEnabled()` runtime guards stay — when `HAS_PIC=0`, they return `false` and the compiler eliminates the dead branches.

**Files:** `src/OTGW-firmware/OTGW-firmware.h`, `src/OTGW-firmware/OTGW-Core.ino`

### Step 4: Gate I2C watchdog behind board-specific code

The external watchdog at 0x26 is PIC-board-specific. On OTGW32, replace with ESP32's built-in task watchdog:

```cpp
#if HAS_PIC
  // existing I2C watchdog code (0x26)
  initWatchDog(); feedWatchDog(); WatchDogEnabled();
#else
  // ESP32 TWDT (Task Watchdog Timer)
  esp_task_wdt_init(timeout, true);
#endif
```

**File:** `src/OTGW-firmware/OTGW-Core.ino`

### Step 5: Create `OTDirect.ino` — direct GPIO OpenTherm driver

New file, only compiled when `HAS_DIRECT_OT=1`. ~200 lines.

**Detection at init:**
```
initOTDirect():
  1. Enable step-up converter (GPIO 10 HIGH)
  2. Wait 50ms for voltage stabilization
  3. Attach opentherm_library to master/slave GPIO pins
  4. Read OT master input — if stable idle level, hardware is present
  5. Set state.hw.eMode = HW_MODE_OT_DIRECT (or HW_MODE_DEGRADED)
```

**Frame bridge to existing parser:**
When a frame arrives from boiler via GPIO interrupt:
```cpp
void onMasterResponse(unsigned long frame, OpenThermResponseStatus status) {
  if (status == OpenThermResponseStatus::SUCCESS) {
    char buf[10];
    snprintf(buf, sizeof(buf), "B%08lX", frame);
    processOT(buf, 9);  // Feed into existing parser unchanged
  }
}
```

When a frame arrives from thermostat:
```cpp
void onSlaveRequest(unsigned long frame, OpenThermResponseStatus status) {
  if (status == OpenThermResponseStatus::SUCCESS) {
    char buf[10];
    snprintf(buf, sizeof(buf), "T%08lX", frame);
    processOT(buf, 9);
  }
}
```

**Command translation** (for `addOTWGcmdtoqueue`):
```
"TT=20.0" → buildRequest(WRITE_DATA, TSet, encode_f88(20.0)) → send to boiler
"CS=60.0" → buildRequest(WRITE_DATA, TSet, encode_f88(60.0))
"HW=1"    → set DHW enable flag in next status request
```

**Master request scheduler** — periodic OT polls (status every 1s, temps every 10s, etc.).

**File:** `src/OTGW-firmware/OTDirect.ino` (new)

### Step 6: Wire into main loop

```cpp
// setup():
#if HAS_PIC
  detectPIC();
  if (state.hw.eMode == HW_MODE_PIC) {
    // existing PIC init path
  }
#endif
#if HAS_DIRECT_OT
  initOTDirect();  // includes hardware detection
#endif

// doBackgroundTasks():
#if HAS_PIC
  if (isPICEnabled()) handleOTGW();
#endif
#if HAS_DIRECT_OT
  if (isOTDirectEnabled()) loopOTDirect();
#endif
```

**File:** `src/OTGW-firmware/OTGW-firmware.ino`

### Step 7: Runtime peripheral detection (OTGW32 only)

After OT-direct init, probe optional peripherals:

```cpp
#if HAS_OLED_CAPABLE
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.beginTransmission(0x3C);
  state.hw.bOLEDPresent = (Wire.endTransmission() == 0);
#endif

#if HAS_ETH_CAPABLE
  // SPI init + Ethernet.begin() with timeout
  state.hw.bEthernetPresent = ...;
#endif
```

These enable/disable display and Ethernet modules at runtime. The OLED and Ethernet code only execute if their hardware is actually present.

**File:** `src/OTGW-firmware/OTGW-firmware.ino`

### Step 8: Build system — PlatformIO + build.py

**PlatformIO** — add OTGW32 environment:
```ini
[env:otgw32]
platform = espressif32@6.9.0
board = esp32-s3-devkitm-1
build_flags =
  ${env.build_flags}
  -DBOARD_NODOSHOP_OTGW32
lib_deps =
  ${env.lib_deps}
  opentherm_library
```

**build.py** — default builds both targets:
- `python build.py` → builds ESP8266 (PIC) **and** ESP32-S3 (OTGW32)
- `python build.py --target esp8266` → ESP8266 only
- `python build.py --target otgw32` → OTGW32 only

The existing `--target` flag (added in v1.4.0) already supports this pattern. Update the target definitions table to replace `esp32` with `otgw32`, and change the default from `esp8266` to `all`.

Output artifacts:
```
build/
  ├── OTGW-firmware-esp8266-1.4.0.bin      # ESP8266 PIC firmware
  ├── OTGW-filesystem-esp8266-1.4.0.bin    # ESP8266 filesystem
  ├── OTGW-firmware-otgw32-1.4.0.bin       # ESP32-S3 OT-direct firmware
  └── OTGW-filesystem-otgw32-1.4.0.bin     # ESP32-S3 filesystem
```

**Files:** `platformio.ini`, `build.py`

## Summary: What's Compile-Time vs Runtime

| Decision | How it's made | Why |
|---|---|---|
| ESP8266 vs ESP32 | **Compile-time** | Different CPU architectures |
| GPIO pin assignments | **Compile-time** | Physical electrical conflicts between boards |
| PIC code inclusion | **Compile-time** (`HAS_PIC`) | No PIC on OTGW32, no OTGWSerial needed |
| OT-direct code inclusion | **Compile-time** (`HAS_DIRECT_OT`) | No OT GPIOs on PIC boards |
| PIC actually present | **Runtime** (`detectPIC()`) | PIC could be missing/broken |
| OT bus actually working | **Runtime** (frame probe) | Boiler may be disconnected |
| OLED display present | **Runtime** (I2C scan) | Optional peripheral on OTGW32 |
| Ethernet present | **Runtime** (SPI probe) | Optional peripheral on OTGW32 |
| Dallas sensors present | **Runtime** (1-Wire scan) | Optional on all boards |
| I2C watchdog present | **Runtime** (I2C scan) | Confirms PIC board identity |

## Verification

1. **Default build**: `python build.py` — produces both ESP8266 and OTGW32 binaries
2. **ESP8266 build**: `pio run -e esp8266` — compiles, `detectPIC()` works, no regression from current v1.4.0
3. **ESP32-S3 OTGW32 build**: `pio run -e otgw32` — compiles, OTGWSerial excluded, `initOTDirect()` probes hardware
4. **OTGW32 + boiler**: OT frames flow through `processOT()`, MQTT publishes, WebSocket streams
5. **OTGW32 + no boiler**: `state.hw.eMode = HW_MODE_OT_DIRECT` but `state.otgw.bOnline = false`
6. **OTGW32 + OLED**: Display activates automatically via I2C scan
7. **OTGW32 + no OLED**: Display code skipped, no errors
8. **`python evaluate.py`**: PROGMEM and safety checks pass for both builds

## Files Modified/Created

| File | Action |
|---|---|
| `src/OTGW-firmware/boards.h` | Add `BOARD_NODOSHOP_OTGW32` with pins + `HAS_PIC`/`HAS_DIRECT_OT` flags; remove/deprecate `BOARD_NODOSHOP_ESP32` |
| `src/OTGW-firmware/OTGW-firmware.h` | Add `OTGWHardwareMode` enum, `HardwareSection`, gate OTGWSerial behind `HAS_PIC`, add `isOTDirectEnabled()` |
| `src/OTGW-firmware/OTGW-Core.ino` | Gate `detectPIC`/`handleOTGW`/watchdog behind `HAS_PIC` |
| `src/OTGW-firmware/OTGW-firmware.ino` | Detection sequence in `setup()`, conditional init/loop paths |
| `src/OTGW-firmware/OTDirect.ino` | **NEW** — GPIO OT driver, frame bridge, command translation, master scheduler |
| `src/OTGW-firmware/settingStuff.ino` | OT mode setting (master/repeater/bypass) for OTGW32 |
| `platformio.ini` | Replace `[env:esp32]` with `[env:otgw32]` targeting ESP32-S3 (`esp32-s3-devkitm-1`) + opentherm_library |
| `build.py` | Change default target to build both ESP8266 + OTGW32; replace `esp32` target with `otgw32` |
| `docs/adr/ADR-063-otgw32-hardware-support.md` | **NEW** — ADR documenting the approach |

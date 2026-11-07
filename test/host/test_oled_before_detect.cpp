// TASK-1678 host harness, built by test_oled_before_detect.py.
// Fakes the I2C bus and the board pin map; the OLED code under test is the
// real code sliced from OLED.ino and OTGW-firmware.ino (generated/oled_before_detect.inc).
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <vector>

// ---- board pin map (values from boards.h, combo board) ---------------------
#define HAS_RUNTIME_HW_DETECT 1
#define PIN_I2C_SDA          18   // OT-Direct bus
#define PIN_I2C_SCL          17
#define PIN_CLASSIC_I2C_SDA  35   // Classic S3 Mini bus
#define PIN_CLASSIC_I2C_SCL  36
#define PIN_CLASSIC_BUTTON   18   // D3 hole: the same GPIO as SDA on the OT-Direct bus
#define PIN_BUTTON           0

enum ComboPinMap { COMBO_MAP_OTGW32, COMBO_MAP_CLASSIC, COMBO_MAP_CLASSIC_PRO };
enum HwMode { HW_MODE_PIC, HW_MODE_OT_DIRECT };

struct { struct { bool bOLEDPresent = false; HwMode eMode = HW_MODE_PIC; } hw; } state;
struct { int iBoardMode = 0; } settings;

static ComboPinMap comboActivePinMap() {
  return state.hw.eMode == HW_MODE_OT_DIRECT ? COMBO_MAP_OTGW32 : COMBO_MAP_CLASSIC;
}
static int activeI2cSda() { return comboActivePinMap() == COMBO_MAP_CLASSIC ? PIN_CLASSIC_I2C_SDA : PIN_I2C_SDA; }
static int activeI2cScl() { return comboActivePinMap() == COMBO_MAP_CLASSIC ? PIN_CLASSIC_I2C_SCL : PIN_I2C_SCL; }
static int activeButton() { return comboActivePinMap() == COMBO_MAP_CLASSIC ? PIN_CLASSIC_BUTTON : PIN_BUTTON; }
#define BUTTON activeButton()
#define OLED_BUTTON_PIN BUTTON
static bool isOTDirectEnabled() { return state.hw.eMode == HW_MODE_OT_DIRECT; }

// ---- Arduino bits -----------------------------------------------------------
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 5
#define F(s) (s)
#define PSTR(s) (s)
static uint32_t millis() { return 1000; }
static void DebugTln(const char*) {}
static void DebugTf(const char*, ...) {}
#define log_e(...) do { std::printf("   log_e: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

// ---- fake I2C bus -----------------------------------------------------------
static int  gBusSda = -1, gBusScl = -1;
static int  gDispSda = -1, gDispScl = -1;     // where the display answers (-1 = no display)
static uint8_t gTxAddr = 0;
static std::vector<int> gPinModeOnBus;          // pinMode() calls that hit a live I2C pin
struct FakeWire {
  void begin(int sda, int scl) { gBusSda = sda; gBusScl = scl; }
  void end() { gBusSda = gBusScl = -1; }
  void beginTransmission(uint8_t a) { gTxAddr = a; }
  uint8_t endTransmission() {
    return (gBusSda >= 0 && gBusSda == gDispSda && gBusScl == gDispScl && gTxAddr == 0x3C) ? 0 : 2;
  }
} Wire;
static void pinMode(int pin, int) {
  if (pin >= 0 && (pin == gBusSda || pin == gBusScl)) gPinModeOnBus.push_back(pin);
}
static int digitalRead(int) { return HIGH; }

// ---- fake SSD1306Ascii ----------------------------------------------------------
static const int Adafruit128x64 = 0, System5x7 = 0;
struct FakeOled {
  void begin(const int*, uint8_t) {}
  void setFont(int) {}
  void displayRemap(bool) {}
  void clear() {}
  void set1X() {}
  void set2X() {}
  void setCol(uint8_t) {}
  void print(const char*) {}
  void println(const char*) {}
  void ssd1306WriteCmd(uint8_t) {}
} oledDisplay;
static void oledRow(uint8_t) {}
static void oledDrawFlame(uint8_t, uint8_t) {}

static constexpr uint8_t OLED_ADDR = 0x3C;
static bool     oledPresent = false;
static uint8_t  oledPage = 0;
static uint32_t oledLastActivity = 0;
static uint8_t  oledLastRenderedPage = 0xFF;
static uint8_t  oledLastBtnLevel = HIGH;
static bool     oledButtonAttached = false;

#include "generated/oled_before_detect.inc"

// ---- scenarios --------------------------------------------------------------
static int gFail = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) gFail++;
}

// setup() order: Wire.begin(activeI2cSda/Scl) for the pre-detection map, then the boot OLED call.
static void boot(int boardMode, int dispSda, int dispScl) {
  state = {}; settings = {}; settings.iBoardMode = boardMode;
  oledPresent = false; oledButtonAttached = false; gPinModeOnBus.clear();
  gDispSda = dispSda; gDispScl = dispScl;
  Wire.end(); Wire.begin(activeI2cSda(), activeI2cScl());
  bootOledCall();
}

int main() {
  std::printf("A: board without a PIC, auto mode, display on the OT-Direct bus (tjfs report)\n");
  boot(0, PIN_I2C_SDA, PIN_I2C_SCL);
  check(oledPresent, "display found before board detection (setup portal is not dark)");
  check(gBusSda == PIN_I2C_SDA && gBusScl == PIN_I2C_SCL, "I2C left on the OT-Direct pins the display answers on");
  check(gPinModeOnBus.empty(), "no pinMode() on a live I2C pin (the Classic button GPIO 18 is SDA here)");

  std::printf("B: Classic board, display on the Classic bus\n");
  boot(0, PIN_CLASSIC_I2C_SDA, PIN_CLASSIC_I2C_SCL);
  check(oledPresent, "display found");
  check(gBusSda == PIN_CLASSIC_I2C_SDA, "I2C stays on the Classic pins");
  check(oledButtonAttached || gPinModeOnBus.empty(), "button handling unchanged on the Classic bus");

  std::printf("C: Classic board without a display\n");
  boot(0, -1, -1);
  check(!oledPresent, "no display reported");
  check(gBusSda == PIN_CLASSIC_I2C_SDA && gBusScl == PIN_CLASSIC_I2C_SCL,
        "I2C back on the Classic pins (0x26 watchdog bus) after the extra look");

  std::printf("D: Classic mode forced by the user, no display on the Classic bus\n");
  boot(1, PIN_I2C_SDA, PIN_I2C_SCL);
  check(!oledPresent, "forced mode: no look on the other bus");
  check(gBusSda == PIN_CLASSIC_I2C_SDA, "I2C stays on the Classic pins");

  std::printf(gFail ? "FAIL: %d check(s) failed\n" : "PASS: all checks\n", gFail);
  return gFail ? 1 : 0;
}

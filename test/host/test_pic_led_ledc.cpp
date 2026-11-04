// test/host/test_pic_led_ledc.cpp  (TASK-1218)
//
// The PIC-flash progress LEDs on the ESP32-S3 sit on pins attached to LEDC;
// arduino-esp32 3.x refuses digitalWrite on them ("IO n is not set as GPIO").
// test_pic_led_ledc.py slices the REAL code (setLed + LEDC statics from
// OTGW-firmware.ino, picProgressLedWrite and the LED prelude of fwupgradestart
// from OTGW-Core.ino, OTGWSerial::SetLED from the library) and this file
// supplies only the platform doubles: a pin table that behaves like
// arduino-esp32 (digitalWrite on an LEDC-attached pin is refused and counted)
// and a stub of the OTGWSerial class members the sliced method uses.
// Build with -DPLAIN_GPIO for a board without LEDC LEDs (HAS_LEDC_LED == 0).

#include <cstdio>
#include <cstdint>
#include <cstring>

#define F(s) (s)
#define DebugTln(...) ((void)0)
#define LOW  0
#define HIGH 1
#define OUTPUT 1
#define ON  LOW
#define OFF HIGH
#define LED1 16
#define LED2 4
#define LED_BRIGHTNESS 5

// ---- pin table ------------------------------------------------------------
static bool pinLedc[64];
static int  refused = 0;          // digitalWrite on an LEDC pin
static int  ledcWrites = 0;       // ledcWrite calls
static int  gpioWrites = 0;       // accepted digitalWrite calls
static int  lastLedcDuty[64];
static int  lastGpioLevel[64];

static void pinMode(int, int) {}
static void digitalWrite(int pin, int val) {
  if (pinLedc[pin]) { refused++; return; }   // "[E] __digitalWrite(): IO n is not set as GPIO"
  gpioWrites++; lastGpioLevel[pin] = val;
}
static void ledcAttachChannel(int pin, int, int, int) { pinLedc[pin] = true; lastLedcDuty[pin] = -1; }
static void ledcOutputInvert(int, bool) {}
static void ledcWrite(int pin, int duty) {
  if (!pinLedc[pin]) { refused++; return; }
  ledcWrites++; lastLedcDuty[pin] = duty;
}

#define LED1_LEDC_CHANNEL 0
#define LED2_LEDC_CHANNEL 1
#define LED_FREQ_HZ 5000
#define LED_RESOLUTION 8

// ---- real LED driver, sliced from OTGW-firmware.ino ----------------------------
#ifdef PLAIN_GPIO
#include "led_plain.inc"
#else
#include "led_ledc.inc"
#endif

// ---- library class: members used by the sliced SetLED --------------------------
typedef void OTGWLedWrite(int pin, bool on);
struct OTGWSerialStub {
  int _led = -1;
  OTGWLedWrite *_ledFunc = nullptr;
  void registerLedCallback(OTGWLedWrite *f) { _ledFunc = f; }
  void SetLED(int state);
} OTGWSerial;
#include "led_lib.inc"

// ---- real callback and the LED prelude of fwupgradestart -----------------------
#include "led_core_cb.inc"
static void flashPrelude() {
#include "led_core_prelude.inc"
}

static void reset() {
  std::memset(pinLedc, 0, sizeof pinLedc);
  refused = ledcWrites = gpioWrites = 0;
  for (int i = 0; i < 64; i++) { lastLedcDuty[i] = -1; lastGpioLevel[i] = -1; }
}

static int fails = 0;
static void verdict(const char *name, bool ok, const char *what) {
  std::printf("CASE %s %s  (%s: refused=%d ledcWrites=%d gpioWrites=%d)\n", name, ok ? "pass" : "FAIL", what,
              refused, ledcWrites, gpioWrites);
  if (!ok) fails++;
}

int main() {
#ifdef PLAIN_GPIO
  // Plain GPIO LEDs: nothing is LEDC-attached, the level goes out through digitalWrite.
  reset();
  OTGWSerial = OTGWSerialStub(); OTGWSerial._led = LED2;
  flashPrelude();
  bool p1 = lastGpioLevel[LED1] == LOW && refused == 0;
  OTGWSerial.SetLED(1);
  bool p2 = lastGpioLevel[LED2] == LOW && refused == 0;
  OTGWSerial.SetLED(0);
  bool p3 = lastGpioLevel[LED2] == HIGH && refused == 0 && ledcWrites == 0;
  verdict("P1", p1 && p2 && p3, "plain GPIO: prelude, SetLED(1), SetLED(0) via digitalWrite, active LOW");
#else
  // LEDC LEDs: both pins are LEDC-attached (what _ensureLEDsInit does at boot).
  reset();
  OTGWSerial = OTGWSerialStub(); OTGWSerial._led = LED2;
  _ensureLEDsInit();
  flashPrelude();                              // fwupgradestart: LED1 on
  verdict("L1", refused == 0 && lastLedcDuty[LED1] == LED_BRIGHTNESS, "LED1 on at flash start");
  reset(); _ledsInitDone = false; _ensureLEDsInit(); refused = ledcWrites = 0;
  flashPrelude();                              // registers the callback
  OTGWSerial.SetLED(1);                        // upgradeEvent(STX)
  bool on = lastLedcDuty[LED2] == LED_BRIGHTNESS;
  OTGWSerial.SetLED(0);                        // upgradeEvent(ETX)
  bool off = lastLedcDuty[LED2] == 0;
  verdict("L2", refused == 0 && on && off, "LED2 progress on/off during flash");
#endif
  std::printf("%s\n", fails ? "RESULT FAIL" : "RESULT PASS");
  return fails ? 1 : 0;
}

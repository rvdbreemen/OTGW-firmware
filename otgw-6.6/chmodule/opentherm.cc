/* IN_MODULE should be defined for modules */
#define IN_MODULE

#include <cassert>
#include <sstream>
#include <iostream>
#include <iomanip>

#include <gpsim/packages.h>
#include <gpsim/stimuli.h>
#include <gpsim/processor.h>

#include "config.h"
#include "opentherm.h"

const char *StatusCounters[] = {
    "Messages sent",
    "Messages received",
    "Stop bit errors",
    "Parity errors",
    "Data bit errors",
    "Wrong direction",
    "Invalid messages",
    "Missing response",
    "DataID mismatch",
    "Multiple responses"
};

static const unsigned short garbagedata[] = {
    1473,972,1228,973,972,972,973,972,
    972,973,1470,1228,1467,1474,1969,972,
    972,973,1228,972,973,973,972,972,973,
    972,1002,943,973,972,0
};

// Attribute class that allows the user to dynamically connect or disconnect
// the thermostat
class ModeAttribute : public Value {
   Thermostat *ot;

public:
   explicit ModeAttribute(Thermostat *_ot)
     : Value("mode", "thermostat mode (on/off/opentherm)"), ot(_ot)
   {
       assert(ot);
   }

   void set(Value *v) {
       if (typeid(*v) == typeid(String)) {
	   char buff[20];
	   v->get(buff, sizeof(buff));
	   set(buff);
       } else {
	   throw TypeMismatch("set ", "ModeAttribute", v->showType());
       }
   }

   // Inform the thermostat object of any change
   void set(const char *buffer, int len = 0) {
       if (buffer) {
	   if (strcasecmp("off", buffer) == 0) {
	       ot->SetMode(kModeOff);
	   } else if (strcasecmp("on", buffer) == 0) {
	       ot->SetMode(kModeOn);
	   } else {
	       ot->SetMode(kModeOpentherm);
	   }
       }
   }

   std::string toString() override {
       switch (ot->GetMode()) {
	default:
	   return "ot";
	case kModeOff:
	   return "off";
	case kModeOn:
	   return "on";
       }
   }
};

// Attribute class that allows the user to dynamically connect or disconnect
// the thermostat
class PowerAttribute : public Value {
   Thermostat *ot;

public:
   explicit PowerAttribute(Thermostat *_ot)
     : Value("power", "thermostat power level (low/medium/high)"), ot(_ot)
   {
       assert(ot);
   }

   void set(Value *v) {
       if (typeid(*v) == typeid(String)) {
	   char buff[20];
	   v->get(buff, sizeof(buff));
	   set(buff);
       } else {
	   throw TypeMismatch("set ", "ModeAttribute", v->showType());
       }
   }

   // Inform the thermostat object of any change
   void set(const char *buffer, int len = 0) {
       if (buffer) {
	   if (strcasecmp("medium", buffer) == 0) {
	       ot->SetPower(kPowerMedium);
	   } else if (strcasecmp("high", buffer) == 0) {
	       ot->SetPower(kPowerHigh);
	   } else {
	       ot->SetPower(kPowerLow);
	   }
       }
   }

   std::string toString() override {
       switch (ot->GetPower()) {
	default:
	   return "low";
	case kPowerMedium:
	   return "medium";
	case kPowerHigh:
	   return "high";
       }
   }
};

// Attribute class that allows the user to initiate an Opentherm message
class TransmitAttribute : public Integer {
   Thermostat *ot;

public:
   explicit TransmitAttribute(Thermostat *_ot):
   Integer("tx", 0, "Opentherm Transmit Register"),
   ot(_ot) {}

   void set(gint64 msg) override {
       msg &= 0xffffffff;
       if (ot) {
	   ot->AddMessage(msg);
       }
       Integer::set(msg);
   }

   std::string toString() override {
       return Integer::toString("0x%08" PRINTF_INT64_MODIFIER "X");
   }
};

// Attribute class that allows the user to verify a received Opentherm message
class ReceiveAttribute : public Integer {
   Opentherm *ot;

public:
   explicit ReceiveAttribute(Opentherm *_ot):
   Integer("rx", 0, "Opentherm Receive Register"),
   ot(_ot) {}

   // Ignore attempts to load the register by the user
   void set(gint64 msg) override {
       std::cout << "Receive buffer is read only\n";
   }

   // Report the received message as a hexadecimal number
   std::string toString() override {
       return Integer::toString("0x%08" PRINTF_INT64_MODIFIER "X");
   }

   // Alternative method for internal callers because they can't use set()
   void ReceivedMessage(gint64 msg) {
       Integer::set(msg);
   }
};

// Attribute class for setting up responses for the Boiler object
class RespondAttribute : public Integer {
   Boiler *ot;

public:
   explicit RespondAttribute(Boiler *_ot)
     : Integer("response", 0, "Set response message"), ot(_ot)
   {
   }

   void set(gint64 msg) override {
       msg &= 0xffffffff;
       if (ot) {
	   ot->SetResponse(msg);
       }
       Integer::set(msg);
   }
};

class ReportAttribute : public String {
   Opentherm *ot;

public:
   ReportAttribute(Opentherm *_ot)
     : String("report", "", "Generate a summary report"), ot(_ot)
   {
   }

   void set(Value *v) {
       if (typeid(*v) == typeid(String)) {
	   char buff[20];
	   v->get(buff, sizeof(buff));
	   set(buff);
       }
   }

   void set(const char *buffer, int len = 0) override {
       if (buffer) {
	   if (strncmp(buffer, "reset", len) == 0) {
	       ot->ResetCounters();
	   }
       }
   }

   std::string toString() override {
       return ot->SummaryReport();
   }
};

class TSPAttribute : public String {
   Boiler *ot;

public:
   TSPAttribute(Boiler *_ot)
     : String("TSP", "", "Configure TSPs"), ot(_ot)
   {
   }

   void set(Value *v) {
       if (typeid(*v) == typeid(String)) {
	   char buff[1300];
	   v->get(buff, sizeof(buff));
	   set(buff);
       }
   }

   void set(const char *buffer, int len = 0) override {
       if (buffer && *buffer) {
           guint8 id, cnt = 0;
           guint8 list[256];
           char *s, *p;
           const std::string usage = " string format: <id>[: <value>, ...]";
           id = strtol(buffer, &s, 10);
           if (!*s) {
               ot->SetupTSP(id);
               return;
           }
           if (*s != ':') {
               throw Error(usage);
           }
           while (*++s) {
               list[cnt++] = strtol(s, &p, 10);
               if (p == s) {
                   throw Error(usage);
               }
               if (!cnt) {
                   throw Error(" too many values (max: 255)");
               }
               s = p;
               if (!*s) {
                   ot->SetupTSP(id, cnt, list);
                   return;
               }
               if (*s != ',') {
                   throw Error(usage);
               }
           }
       }
   }

   std::string toString() override {
       return "";
   }
};

class StringAttribute : public String {
   Boiler *ot;

public:
   StringAttribute(Boiler *_ot)
     : String("string", "", "Configure string"), ot(_ot)
   {
   }

   void set(Value *v) {
       if (typeid(*v) == typeid(String)) {
	   char buff[256];
	   v->get(buff, sizeof(buff));
	   set(buff);
       }
   }

   void set(const char *buffer, int len = 0) override {
       if (buffer && *buffer) {
           guint8 id, cnt = 0;
           char str[256];
           char *s, *p;
           const std::string usage = " string format: <id>: <value>";
           id = strtol(buffer, &s, 10);
           if (!*s) {
               ot->SetupStr(id);
               return;
           }
           if (*s++ != ':') {
               throw Error(usage);
           }
           while (isspace(*s)) s++;
           len = strlen(s);
           if (len > 255) { 
               throw Error(" string too long (max: 255)");
           }
           strcpy(str, s);
           ot->SetupStr(id, len, str);
       }
   }

   std::string toString() override {
       return "";
   }
};

// I/O pin class for an Opentherm input. The pin will be connected to a PIC
// digital output pin. The PIC output levels are inverted.
class OTInput : public IO_bi_directional_pu {
private:
   Opentherm *ot;

public:
   OTInput(Opentherm *_ot, const char *opt_name = nullptr)
     : IO_bi_directional_pu(opt_name), ot(_ot)
   {
       bDrivenState = true;
       update_direction(0, true);  // Make the RX pin an input.
       bPullUp = true;
       Zpullup = 4.7e3;
   }

   // Report any level change to the Opentherm object, which will pass it on
   // to the Receiver object
   void setDrivenState(bool new_dstate) override {
       bool diff = new_dstate ^ bDrivenState;
       // std::cout << "OTInput: " << bDrivenState << " => " << new_dstate << "\n";
       
       if (ot && diff) {
	   bDrivenState = new_dstate;
	   IOPIN::setDrivenState(new_dstate);
	   ot->RxEdgeEvent(bDrivenState);
       }
   }
};

// I/O pin class for an Opentherm output. The pin will be connected to a PIC
// analog input pin. The voltage levels differ for a thermostat and boiler.
class OTOutput : public IO_bi_directional {
public:
   Opentherm *ot;

   OTOutput(Opentherm *_ot, const char *opt_name = nullptr)
     : IO_bi_directional(opt_name), ot(_ot)
   {
       set_is_analog(true);
       bDriving = true;
       update_direction(1, true);
   }

   void putState(double new_Vth) override {
       // printf("%lld: putState(%g)\n", get_cycles().get(), new_Vth);
       IO_bi_directional::putState(new_Vth);
   }
};

// Transmitter class for sending an Opentherm message
class Transmitter : public TriggerObject {
   guint32 msg, cnt;
   const unsigned short *garbageptr = nullptr;
   bool bit, invert;

public:
   OTOutput *pin;
   Opentherm *ot;

   Transmitter(Opentherm *_ot, OTOutput *_pin)
     : ot(_ot), pin(_pin), cnt(0), invert(false)
   {
       assert(ot);
       assert(pin);
   }

   void SetInvert(bool state) {
       invert = state;
       // printf("SetInvert: pin->putState(%g)\n", ot->GetVoltage(bit != invert));
       pin->putState(ot->GetVoltage(bit != invert));
   }

   bool GetInvert() {
       return invert;
   }

   bool SendGarbage() {
       // Some devices send a secondary protocol
       if (cnt) return false;
       garbageptr = garbagedata;
       callback();
       return true;
   }

   bool SendMessage(guint32 _msg) {
       if (cnt) return false;
       msg = _msg;
       cnt = 1;
       if (!garbageptr) callback();
       return true;
   }

   void callback() override {
       unsigned duration;
       // Always toggle the line
       bit = !bit;
       // printf("callback: pin->putState(%g)\n", ot->GetVoltage(bit != invert));
       pin->putState(ot->GetVoltage(bit != invert));

       if (garbageptr) {
           if (bit != invert) {
               duration = 24;
           } else if (*garbageptr) {
               duration = *garbageptr++;
           } else {
               garbageptr = nullptr;
               if (!cnt) return;
               // Idle interval between garbage and the OT message
               duration = 13271;
           }
       } else if (cnt == 1) {
           // Start bit
           // assert(bit == true);
           duration = 500;
           cnt++;
       } else if (cnt >= 68) {
           // Finished
           // assert(bit == false);
           cnt = 0;
           return;
       } else {
           // calculate when to send 2 equal half-bits
           guint64 dbl = msg ^ ((guint64)msg << 1 ^ 0x100000001ULL);
           if (dbl >> (33 - cnt / 2) & 1) {
               duration = 1000;
               cnt += 2;
           } else {
               duration = 500;
               cnt++;
           }
       }
       get_cycles().set_break_delta(duration, this);
   }
};

// Receiver class for receiving an Opentherm message
class Receiver : public TriggerObject {
   Opentherm *ot;
   OTInput *pin;
   guint64 last_time;
   guint32 message;
   int count;
   bool parity, invert;

public:
   explicit Receiver(Opentherm *_ot, OTInput *_pin) 
     : ot(_ot), pin(_pin), last_time(0), count(0), invert(false)
   {
       // Need to be associated with an Opentherm object
       assert(ot);
   }

   // Handle a level change on the input pin
   void RxEdgeEvent(bool level) {
       guint64 now = get_cycles().get(), next_time;
       // Logical levels are reversed (1 = low, 0 = high) in normal mode
       // On top of that the line may be sending in inverted mode
       bool bit = (level == invert);
       // printf("%d: cycles = %ld (%d)\n", bit, now, count);
       // printf("%s (%lld): level = %d, bit = %d\n", ot->GetDevice(), now, level, bit);
       if (last_time == 0) {
	   // printf("%s (%lld): level = %d, bit = %d\n", ot->GetDevice(), now, level, bit);
	   // New event
	   if (bit) {
	       // Line went active
	       // Set timer for 5 ms
	       last_time = now;
	       next_time = get_cycles().get(5e-3);
	       get_cycles().set_break(next_time, this);
	   }
       } else {
	   double delta = (now - last_time) / get_cycles().instruction_cps();
	   if (count == 0) {
	       if (!bit && delta < 100e-6) {
		   // Spike, most likely during initialization of the OTGW
		   get_cycles().clear_break(this);
		   last_time = 0;
	       } else {
		   count = 33;
		   last_time = now;
		   message = 0x12345678;
		   parity = false;
	       }
	   } else {
	       if (delta < 750e-6) {
		   // Ignore inter-bit transition
	       } else {
		   get_cycles().clear_break(this);
		   if (--count) {
		       message <<= 1;
		       // Falling edge represents a bit value of 1
		       if (!bit) {
			   message |= 1;
			   parity = !parity;
		       }
		       // printf("%08X\n", message);
		       // Next bit must happen within 2ms
		       next_time = get_cycles().get(2e-3);
		       get_cycles().set_break(next_time, this);
		       last_time = now;
		   } else if (bit) {
		       // Stop bit must be 1. A rising edge is wrong
		       ot->IncrCounter(kCounterStopBit);
		   } else if (parity) {
		       // Parity must be even
		       ot->IncrCounter(kCounterParity);
		   } else {
		       // Valid message received
		       ot->NewRxMessage(message);
		   }
		   if (count == 0) last_time = 0;
	       }
	   }
       }
   }

   bool GetState() {
       return pin->getState() == invert;
   }

   bool GetInvert() {
       return invert;
   }

   // Timer expiry handler
   void callback() override {
       if (count == 0) {
	   // The line has been high (according to the current mode) for 5 ms
	   // Switch invert/normal receive mode
	   invert = !invert;
	   ot->ReceiveMode(invert);
	   // printf("%s (%lld): Invert = %d\n", ot->GetDevice(), get_cycles().get(), invert);
       } else {
	   // Expected bit transition did not happen
	   ot->IncrCounter(kCounterCorrupt);
	   // printf("%s (%lld): Error 03\n", ot->GetDevice(), get_cycles().get());
	   count = 0;
	   // printf("Pin: %d\n", ot->m_recv->GetState());
	   if (ot->m_recv->GetState()) {
	       // Line is active
	       guint64 next_time = get_cycles().get(3e-3);
	       get_cycles().set_break(next_time, this);
	   }
       }
       last_time = 0;
   }
};

// Base class for both types of Opentherm devices (thermostat and boiler)
Opentherm::Opentherm(const char *_new_name, const char *_desc) : Module(_new_name, _desc) {
    m_rxbuffer = new ReceiveAttribute(this);
    addSymbol(m_rxbuffer);
    m_report = new ReportAttribute(this);
    addSymbol(m_report);

    ResetCounters();
}

Opentherm::~Opentherm() {
    removeSymbol(m_rxbuffer);
    delete m_rxbuffer;
    removeSymbol(m_report);
    delete m_report;
    removeSymbol(m_v_low);
    delete m_v_low;
    removeSymbol(m_v_high);
    delete m_v_high;
}

bool Opentherm::SendMessage(guint32 msg) {
    if (m_xmit->SendMessage(msg)) {
	IncrCounter(kCounterSent);
	return true;
    } else {
	return false;
    }
}

void Opentherm::LogicalLevels(double v_low, double v_high) {
    m_v_low = new Float("Vlow", v_low, "Logical low level");
    m_v_high = new Float("Vhigh", v_high, "Logical high level");
    addSymbol(m_v_low);
    addSymbol(m_v_high);
    m_tx->set_Zth(100.0);
    m_tx->putState(v_low);
}

double Opentherm::GetVoltage(bool active) {
    return active ? m_v_high->getVal() : m_v_low->getVal();
}

void Opentherm::RxEdgeEvent(bool bit) {
    // Pass on the input level change to the Receiver object
    m_recv->RxEdgeEvent(bit);
}

void Opentherm::CreatePinMap() {
    // An Opentherm device has 2 connections
    create_pkg(2);
    // Set up the input on pin 1
    m_rx = new OTInput(this, "in");
    assign_pin(1, m_rx);
    addSymbol(m_rx);
    // Set up the output on pin 2
    m_tx = new OTOutput(this, "out");
    assign_pin(2, m_tx);
    addSymbol(m_tx);
    // Create the transmitter and receiver objects
    m_xmit = new Transmitter(this, m_tx);
    m_recv = new Receiver(this, m_rx);
}

void Opentherm::NewRxMessage(unsigned msg) {
    m_rxbuffer->ReceivedMessage(msg);
    IncrCounter(kCounterReceived);
    // Spare bits should be 0
    if (msg & 0x0f000000) IncrCounter(kCounterInvalid);
}

void Opentherm::IncrCounter(enum StatusCounters num) {
    counter[num]++;
}

void Opentherm::ResetCounters() {
    for (int i = 0; i < kCounterCount; i++) {
	counter[i] = 0;
    }
}

unsigned Opentherm::SetParity(unsigned message) {
    unsigned parity = message ^ (message >> 1);
    parity = parity ^ (parity >> 2);
    parity = parity ^ (parity >> 4);
    parity = parity ^ (parity >> 8);
    parity = parity ^ (parity >> 16);
    if (parity & 1) {
	// Correct the parity
	message ^= 0x80000000;
    }
    return message;
}

std::string Opentherm::SummaryReport(int count) {
    std::ostringstream rc;
    for (int i = 0; i < count; i++) {
	rc << std::left << std::setw(20) << StatusCounters[i] << std::right << std::setw(8) << counter[i] << "\n";
    }
    return rc.str();
}

// Class for the thermostat-specific behavior
Thermostat::Thermostat(const char *name) : Opentherm(name, "Thermostat") {
    // Constructor
    CreatePinMap();
    // Attributes for user interaction
    m_txbuffer = new TransmitAttribute(this);
    addSymbol(m_txbuffer);
    m_mode = new ModeAttribute(this);
    addSymbol(m_mode);
    m_interval = new Float("interval", 0.2, "Time between sending messages in seconds");
    addSymbol(m_interval);
    m_roomtemp = new Float("temperature", 20.42, "Room temperature");
    addSymbol(m_roomtemp);
    m_setpoint = new Setpoint(20.5);
    addSymbol(m_setpoint);
    m_power = new PowerAttribute(this);
    addSymbol(m_power);
    m_garbage = new Boolean("garbage", false, "Dual protocol");
    addSymbol(m_garbage);

    LogicalLevels(0.75, 1.85);
}

Thermostat::~Thermostat() {
    // Destructor
    removeSymbol(m_txbuffer);
    delete m_txbuffer;
    removeSymbol(m_mode);
    delete m_mode;
    removeSymbol(m_interval);
    delete m_interval;
    removeSymbol(m_roomtemp);
    delete m_roomtemp;
    removeSymbol(m_setpoint);
    delete m_setpoint;
    removeSymbol(m_power);
    delete m_power;
    removeSymbol(m_garbage);
    delete m_garbage;
}

const char *Thermostat::GetDevice() {
    return "Thermostat";
}

void Thermostat::AddMessage(unsigned msg) {
    if (request_count >= 64) {
	std::cout << "Request buffer is full\n";
	return;
    }
    request[request_count] = msg;
    if (request_count++ == 0) {
	// Schedule the first message after 0.5 s, or 1.2 s after power up.
	// Immediately after power up instruction_cps() is not yet reliable.
	guint64 next_time = std::max((guint64)1200000, get_cycles().get(0.5));
	get_cycles().set_break(next_time, this);
    }
}

void Thermostat::NewRxMessage(unsigned msg) {
    // printf("Thermostat (%d): %08X\n", pointer, msg);
    Opentherm::NewRxMessage(msg);
    if (!(msg & 0x40000000)) IncrCounter(kCounterDirection);
    if (msgid >= 0) {
	// Already received a response
	IncrCounter(kCounterUnexpected);
    }
    msgid = msg >> 16 & 0xff;
    if (msgid != (request[pointer] >> 16 & 0xff)) {
	// Response does not match the request
	// printf("Request %08X, response %08X\n", request[pointer], msg);
	IncrCounter(kCounterMismatch);
    }
    switch (msgid) {
     case 9:
	if ((msg >> 28 & 7) == kReadAck) {
	    if (override && (msg + 64 & 0xff80) == override) {
		m_setpoint->override((double)override / 256);
	    } else {
		override = msg + 64 & 0xff80;
	    }
	}
	break;
    }
    // Stop the timer
    get_cycles().clear_break(this);
    // Get the user defined interval between messages, considering that the
    // master unit must wait at least 100ms from the end of a previous
    // conversation before initiating a new conversation
    guint64 next_time = get_cycles().get(std::max(0.1, m_interval->getVal()));
    // The master must communicate at least every 1 sec
    guint64 max_time = msg_time + (guint64)(get_cycles().instruction_cps());
    // Set up a timer for sending the next message
    get_cycles().set_break(std::min(next_time, max_time), this);
}

void Thermostat::SetMode(enum ThermostatModes newMode) {
    switch (newMode) {
     case kModeOpentherm:
	m_tx->putState(m_v_low->getVal());
	m_xmit->SetInvert(false);
	if (request_count) {
	    get_cycles().clear_break(this);
	    request_count = 0;
	    pointer = 0;
	}
	break;
     case kModeOff:
	m_tx->putState(2.795);
	break;
     case kModeOn:
	m_tx->putState(0.05);
	break;
     default:
	return;
    }
    mode = newMode;
}

enum ThermostatModes Thermostat::GetMode() {
    return mode;
}

void Thermostat::SetPower(enum PowerLevels newLevel) {
    switch (newLevel) {
     case kPowerLow:
     case kPowerMedium:
     case kPowerHigh:
	SmartPower(newLevel);
	break;
    }
}

enum PowerLevels Thermostat::GetPower() {
    enum PowerLevels level;
    if (!m_recv->GetInvert()) {
	level = kPowerLow;
    } else if (m_xmit->GetInvert()) {
	level = kPowerHigh;
    } else {
	level = kPowerMedium;
    }
    return level;
}

bool Thermostat::SendMessage(unsigned msg) {
    msgid = -1;
    if (mode != kModeOpentherm) return false;
    int id = msg >> 16 & 0xff;
    switch (id) {
     case 16: // Room setpoint
	msg = SetParity((msg & 0xffff0000) | (unsigned)(256 * m_setpoint->getVal()));
	break;
     case 24: // Room temperature
	msg = SetParity((msg & 0xffff0000) | (unsigned)(256 * m_roomtemp->getVal()));
	break;
    }
    return Opentherm::SendMessage(msg);
}

void Thermostat::ScheduleMessage(double delay) {
    powerstate = kPowerIdle;
    // Wait a maximum of 800 ms for a response
    guint64 future_time = get_cycles().get(std::max(0.8, delay));
    get_cycles().set_break(future_time, this);
}

bool Thermostat::SmartPower(enum PowerLevels newLevel) {
    enum PowerLevels oldLevel = GetPower();
    if (newLevel == oldLevel) return false;
    // Stop the running timer
    if (request_count) get_cycles().clear_break(this);
    // Must wait at least 20 ms after the last message (which takes 34 ms)
    guint64 next_time = msg_time + (guint64)(0.054 * get_cycles().instruction_cps());
    if (get_cycles().get() < next_time) {
	switch (newLevel) {
	 default:
	    powerstate = kPowerPendingLow;
	    break;
	 case kPowerMedium:
	    powerstate = kPowerPendingMedium;
	    break;
	 case kPowerHigh:
	    powerstate = kPowerPendingHigh;
	    break;
	}
	get_cycles().set_break(next_time, this);
	return true;
    }
    switch (newLevel) {
     case kPowerLow:
	if (oldLevel == kPowerMedium) {
	    // Make the transmit line high for 15 ms
	    printf("%lld: Low -> High\n", get_cycles().get());
	    m_xmit->SetInvert(true);
	    powerstate = kPowerPendingLow;
	    next_time = get_cycles().get(15e-3);
	    get_cycles().set_break(next_time, this);
	} else {
	    // Make the transmit line low
	    printf("%lld: High -> Low\n", get_cycles().get());
	    m_xmit->SetInvert(false);
	    ScheduleMessage(20e-3);
	}
	break;
     case kPowerMedium:
	if (oldLevel == kPowerLow) {
	    // Make the transmit line high
	    printf("%lld: Low -> High\n", get_cycles().get());
	    m_xmit->SetInvert(true);
	    powerstate = kPowerWaitMedium;
	    next_time = get_cycles().get(10e-3);
	    get_cycles().set_break(next_time, this);
	} else {
	    // Need to switch back to Low Power first
	    printf("%lld: High -> Low\n", get_cycles().get());
	    m_xmit->SetInvert(false);
	    powerstate = kPowerPendingMedium;
	    next_time = get_cycles().get(20e-3);
	    get_cycles().set_break(next_time, this);
	}
	break;
     case kPowerHigh:
	// Make the transmit line high
	printf("%lld: Low -> High\n", get_cycles().get());
	m_xmit->SetInvert(true);
	if (oldLevel == kPowerLow) {
	    powerstate = kPowerWaitHigh;
	    next_time = get_cycles().get(10e-3);
	    get_cycles().set_break(next_time, this);
	} else {
	    // Switching from Medium to High requires no acknowledgement
	    // Just wait at least 20 ms before sending the next message
            ScheduleMessage(20e-3);
	}
	break;
    }
    return true;
}

std::string Thermostat::SummaryReport() {
    return "Thermostat report\n" + std::string(28, '=') + "\n" + Opentherm::SummaryReport(kCounterCount);
}

void Thermostat::callback() {
    guint64 future_time;
    switch (powerstate) {
     case kPowerIdle:
	// Check if the expected response was received
	if (msgid < 0 && counter[kCounterSent]) {
	    // No response received
	    IncrCounter(kCounterMissing);
	}
	if (msgid == (request[pointer] >> 16 & 0xff)) {
	    // Move on to the next message
	    if (++pointer >= request_count) pointer = 0;
	}
	// Save the start time of sending the message
	msg_time = get_cycles().get();
	// printf("%lld Sending: %08X\n", msg_time, request[pointer]);
	if (*m_garbage) m_xmit->SendGarbage();
	SendMessage(request[pointer]);
	// Set a timer for sending the next message
	ScheduleMessage();
	break;
     case kPowerPendingLow:
	SmartPower(kPowerLow);
	break;
     case kPowerPendingMedium:
	SmartPower(kPowerMedium);
	break;
     case kPowerPendingHigh:
	SmartPower(kPowerHigh);
	break;
     case kPowerWaitMedium:
	// Switch back to low output, irrespective of acknowledge by the slave
	printf("%lld: High -> Low\n", get_cycles().get());
	m_xmit->SetInvert(false);
	// The next message may be sent in 20 ms
	ScheduleMessage(20e-3);
	break;
     case kPowerWaitHigh:
	if (!m_rx->getState()) {
	    // Slave has acknowledged the High Power request
	    // The next message may be sent in 10 ms (20 ms in total)
	    ScheduleMessage(10e-3);
	} else {
	    // Slave failed to acknowledge the Hig Power request
	    printf("%lld: High -> Low\n", get_cycles().get());
	    m_xmit->SetInvert(false);
	    // The next message may be sent in 20 ms
	    ScheduleMessage(20e-3);
	}
	break;
    }
}

Module *Thermostat::construct(const char *_new_name = nullptr) {
    return new Thermostat(_new_name);
}

// Class for the boiler-specific behavior
Boiler::Boiler(const char *name) : Opentherm(name, "Boiler") {
    // Constructor
    CreatePinMap();
    // Attributes for user interaction
    m_responder = new RespondAttribute(this);
    addSymbol(m_responder);
    m_tsp = new TSPAttribute(this);
    addSymbol(m_tsp);
    m_str = new StringAttribute(this);
    addSymbol(m_str);
    m_garbage = new Integer("garbage", 0, "Dual protocol");
    addSymbol(m_garbage);

    LogicalLevels(0.1, 4.2);
}

Boiler::~Boiler() {
    // Destructor
    removeSymbol(m_responder);
    delete m_responder;
    removeSymbol(m_tsp);
    delete m_tsp;
    removeSymbol(m_garbage);
    delete m_garbage;
}

const char *Boiler::GetDevice() {
    return "Boiler";
}

void Boiler::SetResponse(unsigned msg) {
    int msgid = msg >> 16 & 0xff;
    switch (msg >> 28 & 7) {
     case kReadAck:
     case kWriteAck:
     case kDataInvalid:
	response[msgid] = msg;
	break;
     default:
	response[msgid] = 0;
    }
}

void Boiler::NewRxMessage(unsigned msg) {
    // printf("Boiler: %08X\n", msg);
    Opentherm::NewRxMessage(msg);
    if (msg & 0x40000000) IncrCounter(kCounterDirection);
    int msgid = msg >> 16 & 0xff;
    message = response[msgid];
    if (!message) {
        unsigned slavestatus;
        int type = msg >> 28 & 7;
	// Unknown data ID
        switch (msgid) {
         case 0:
            slavestatus = Status(msg >> 8 & 0xff);
            message = kReadAck << 28 | msgid << 16 | msg & 0xff00 | slavestatus;
            break;
         case 1:
            ctrlsetpoint = (msg & 0xffff) / 256.;
            message = msg | 0x40000000;
            break;
         default:
            if (tsp.count(msgid)) {
                int index = msg >> 8 & 0xff;
                tspdata &list = tsp[msgid];
                if (index >= list.size()) {
                    message = kDataInvalid << 28 | msg & 0xffff00;
                } else if (type == kReadData) {
                    message = msg & 0x7fffff00 | 0x40000000 | list[index];
                } else if (type == kWriteData) {
                    list[index] = msg;
                    message = msg | 0x40000000;
                } else {
                    return;
                }
            } else if (str.count(msgid)) {
                int index = msg >> 8 & 0xff;
                strdata &data = str[msgid];
                if (index >= data.size()) {
                    message = kDataInvalid << 28 | msg & 0xff0000 | data.size() << 8;
                } else if (type == kReadData) {
                    message = msg & 0x7fff0000 | 0x40000000 | data.size() << 8 | data[index];
                } else {
                    return;
                }
            } else {
                message = kUnknownDataID << 28 | msgid << 16;
            }
            break;
        }
    } else {
	int type = message >> 28 & 7;
	switch (msg >> 28 & 7) {
	 case kReadData:
	    if (type == kDataInvalid || type == kUnknownDataID) {
		// Return the stored message unmodified
	    } else {
		// Return the data from the stored message in a Read-Ack
		message = kReadAck << 28 | msgid << 16 | message & 0xffff;
	    }
	    break;
	 case kWriteData:
	    if (type == kWriteAck) {
		// Return the data from the received message in a Write-Ack
		message = kWriteAck << 28 | msgid << 16 | msg & 0xffff;
                // Update the stored message with the received data
                response[msgid] = message;
            } else if (type == kReadAck) {
                // Return the data from the stored message in a Write-Ack
                message = kWriteAck << 28 | msgid << 16 | message & 0xffff;
	    } else {
		// Return the stored message unmodified
	    }
	    break;
	 case kInvalidData:
	    message = kDataInvalid << 28 | msgid << 16 | msg & 0xffff;
	    break;
	 default:
	    // In all cases of errors being detected in the incoming frame,
	    // the partial frame is rejected and the conversation should be
	    // terminated. No errors are treated as recoverable.
	    return;
	}
    }
    message = SetParity(message);
    gint64 garbage; 
    m_garbage->get(garbage);
    if (garbage > 0) {
        get_cycles().set_break_delta(garbage, this);
    } else {
        // A slave must not respond earlier than after 20ms
        get_cycles().set_break_delta(20000, this);
    }
}

unsigned Boiler::Status(unsigned status) {
    guint64 now = get_cycles().get();
    // Perform actions based on the previous status value
    if (masterstatus & 0x1) {
        // CH enable
        if (slavestatus & 0x2) {
            // CH mode was already active - switch on the flame
            slavestatus |= 0x8;
        } else {
            // Activate CH mode
            slavestatus |= 0x2;
        }
    } else if (slavestatus & 0x2) {
        // CH mode active
        if (slavestatus & 0x8) {
            // Flame on - switch it off
            slavestatus &= ~0x8;
        } else {
            // Switch off CH mode
            slavestatus &= ~0x2;
        }
    }
    if (slavestatus & 0x4) {
        if (now - dhw_time > 5 * get_cycles().instruction_cps()) {
            slavestatus &= ~0x4;
        }
    } else if (masterstatus & 0x2 != status & 0x2) {
        if (status & 0x2) {
            // DHW enable switched on - Heat the DHW tank
            dhw_time = get_cycles().get(5);
        }
    } else if (now - dhw_time < 5 * get_cycles().instruction_cps()) {
        slavestatus |= 0x4;
    }
    masterstatus = status;
    return slavestatus;
}

void Boiler::SetupTSP(guint8 id) {
    SetResponse(SetParity(4 << 28 | id << 16));
    id++;
    tsp.erase(id);
}

void Boiler::SetupTSP(guint8 id, guint8 cnt, guint8 *values) {
    SetResponse(SetParity(4 << 28 | id << 16 | cnt << 8));
    id++;
    tsp[id].assign(values, values + cnt);
}

void Boiler::SetupStr(guint8 id) {
    str.erase(id);
}

void Boiler::SetupStr(guint8 id, guint8 len, char *data) {
    str[id].assign(data, data + len);
}

void Boiler::ReceiveMode(bool b) {
    guint64 now = get_cycles().get();
    if (!m_rx->getState()) {
	// Switch to high power mode
	m_xmit->SetInvert(true);
	mode_time = now;
    } else if (now >= mode_time + (guint64)(0.015 * get_cycles().instruction_cps())) {
	// Switch back to low power mode
	m_xmit->SetInvert(false);
    } else {
	// Stay in medium power mode
    }
}

std::string Boiler::SummaryReport() {
    return "Boiler report\n" + std::string(28, '=') + "\n" + Opentherm::SummaryReport(kCounterMissing);
}

void Boiler::callback() {
    gint64 garbage; 
    m_garbage->get(garbage);
    if (garbage > 0) m_xmit->SendGarbage();
    // printf("Sending: %08X\n", message);
    SendMessage(message);
}

Module *Boiler::construct(const char *_new_name = nullptr) {
    return new Boiler(_new_name);
}

/* IN_MODULE should be defined for modules */
#define IN_MODULE

#include <cassert>
#include <iostream>

#include <gpsim/packages.h>
#include <gpsim/stimuli.h>
#include <gpsim/value.h>

#include "probe.h"

class ProbeStateAttribute : public Boolean {
   Probe *probe;

public:
   explicit ProbeStateAttribute(Probe *_probe, const char *opt_name = nullptr)
     : Boolean(opt_name, false, "logical level"), probe(_probe)
   {
       assert(probe);
   }

   void set(bool b) override {
       std::cout << "Probe is read only\n";
   }

   std::string toString() override {
       return probe->GetState();
   }
};

class ProbeVoltageAttribute : public Float {
   Probe *probe;

public:
   explicit ProbeVoltageAttribute(Probe *_probe, const char *opt_name = nullptr)
     : Float(opt_name, false, "analog level"), probe(_probe)
   {
       assert(probe);
   }

   std::string toString() override {
       return probe->GetVoltage();
   }
};

class ProbeInput : public IO_bi_directional {

public:
   ProbeInput(const char *opt_name = nullptr) : IO_bi_directional(opt_name)
   {
       update_direction(0, true);	// Input
   }
};

Probe::Probe(const char *_name) : Module(_name, "Check signal state") {
    // A probe has only one connection
    if (_name) name = _name;
    create_pkg(1);
    m_state = new ProbeStateAttribute(this, "state");
    m_level = new ProbeVoltageAttribute(this, "voltage");
    m_pin = new ProbeInput("in");
    assign_pin(1, m_pin);
    addSymbol(m_state);
    addSymbol(m_level);
    addSymbol(m_pin);
}

Probe::~Probe() {
    removeSymbol(m_state);
    delete m_state;
    removeSymbol(m_level);
    delete m_level;
    removeSymbol(m_pin);
    delete m_pin;
}

std::string Probe::GetState() {
    return name + " = " + m_pin->getBitChar();
}

std::string Probe::GetVoltage() {
    return name + " = " + std::to_string(m_pin->get_nodeVoltage());
}

Module *Probe::construct(const char *_new_name = nullptr) {
    return new Probe(_new_name);
}

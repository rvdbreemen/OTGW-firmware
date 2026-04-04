#ifndef OTGW_PROBE_H_
#define OTGW_PROBE_H_
   
/* IN_MODULE should be defined for modules */
#define IN_MODULE

#include <gpsim/modules.h>

class ProbeStateAttribute;
class ProbeVoltageAttribute;
class ProbeInput;

class Probe : public Module {
   std::string name;
   ProbeStateAttribute *m_state;
   ProbeVoltageAttribute *m_level;
   ProbeInput *m_pin;

public:
   Probe(const char *);
   virtual ~Probe();
   std::string GetState();
   std::string GetVoltage();
   static Module *construct(const char *new_name);
};

#endif //  OTGW_PROBE_H_

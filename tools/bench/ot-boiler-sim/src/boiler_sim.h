// TASK-1654: scripted OpenTherm boiler (slave) engine for SAT wire-level bench tests.
//
// Pure C++ (no Arduino headers) so the same file runs in the ESP32-S3 sketch (src/main.cpp) and in
// the host test (test/host/test_ot_boiler_sim.py). The engine answers one master request at a time:
// the caller passes the request frame and the time in ms since the first request, and gets the
// answer frame back, or "no answer".
//
// The boiler reacts to the request, as a real boiler does:
//   - MsgID 0: the answer echoes the master flags (high byte) and fills the slave flags (low byte).
//     CH active follows the master's CH enable; the flame comes on ignition_delay seconds after CH
//     enable was first seen (ignition_delay -1 = never lights, lockout-like). A DHW draw sets DHW
//     active and the flame, and clears CH active.
//   - Write IDs are acknowledged with the written value echoed, and remembered for later reads.
// Everything else comes from the scenario: values (flow, return, modulation, ...) and per-ID faults
// (Data-Invalid, Unknown-DataID, no answer), phase by phase. See tools/bench/ot-boiler-sim/README.md.
#pragma once
#include <cstdint>

namespace otsim {

enum : uint8_t {
  READ_DATA = 0, WRITE_DATA = 1, INVALID_DATA = 2,
  READ_ACK = 4, WRITE_ACK = 5, DATA_INVALID = 6, UNKNOWN_DATAID = 7,
};

enum class Reply : uint8_t { Ack = 0, DataInvalid, Unknown, None };

struct Phase {
  uint32_t duration_s = 0;      // 0 on the last phase = hold forever
  float flow = 40.0f, ret = 30.0f, mod = 30.0f, pressure = 1.5f, outside = 8.0f, dhw_temp = 50.0f;
  uint8_t slave_hb = 0x00, member_id = 4;   // MsgID 3
  uint8_t max_kw = 24, min_mod = 20;        // MsgID 15
  bool fault = false;           // MsgID 0 LB bit 0; a faulted boiler does not fire
  bool dhw_draw = false;        // tap open: DHW active + flame, CH active cleared
  int32_t ignition_delay_s = 0; // flame this long after CH enable; -1 = never
  bool silent = false;          // no answer to any request
  uint16_t answer_ms = 100;     // answer delay; OT spec v4.2 slave window is 20..800 ms
  Reply reply[128];             // per-MsgID fault injection, default Ack
  Phase() { for (auto &r : reply) r = Reply::Ack; }
};

static const int MAX_PHASES = 16;

struct Scenario {
  char id[48] = {0};
  int n_phases = 0;
  Phase phases[MAX_PHASES];
};

// Parse the .scn text (see README). Returns true on success; on failure err holds a message.
bool parseScenario(const char *text, Scenario &out, char *err, int err_len);

struct Answer {
  bool send = false;       // false: stay silent
  uint32_t frame = 0;
  uint16_t delay_ms = 0;
};

uint32_t withParity(uint32_t word);
bool parityOk(uint32_t frame);
uint32_t buildFrame(uint8_t type, uint8_t id, uint16_t value);

class Boiler {
 public:
  explicit Boiler(const Scenario &sc) : sc_(sc) { reset(); }
  void reset();
  // t_ms: time of this request since the first request of the run (the caller starts at 0).
  Answer onRequest(uint32_t request, uint32_t t_ms);
  int phaseIndexAt(uint32_t t_ms) const;
  bool flameOn() const { return flame_; }
  bool chActive() const { return ch_active_; }

 private:
  void updateStatus(uint8_t master_hb, uint32_t t_ms, const Phase &ph);
  const Scenario &sc_;
  bool ch_enable_seen_ = false;
  uint32_t ch_enable_since_ms_ = 0;
  bool ch_active_ = false, flame_ = false, dhw_active_ = false;
  bool stored_set_[128];
  uint16_t stored_[128];
};

}  // namespace otsim

// TASK-1654: scripted OpenTherm boiler engine. See boiler_sim.h.
#include "boiler_sim.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace otsim {

uint32_t withParity(uint32_t word) {
  word &= 0x7FFFFFFFu;
  uint32_t v = word, ones = 0;
  while (v) { ones += v & 1u; v >>= 1; }
  return (ones & 1u) ? (word | 0x80000000u) : word;
}

bool parityOk(uint32_t frame) {
  uint32_t ones = 0;
  while (frame) { ones += frame & 1u; frame >>= 1; }
  return (ones & 1u) == 0;
}

uint32_t buildFrame(uint8_t type, uint8_t id, uint16_t value) {
  return withParity(((uint32_t)(type & 7u) << 28) | ((uint32_t)id << 16) | value);
}

static uint16_t f88(float c) {
  float s = c * 256.0f;
  long r = (long)(s < 0 ? s - 0.5f : s + 0.5f);
  return (uint16_t)(r & 0xFFFF);
}

// ------------------------------------------------------------------ scenario parser

static bool setKey(Phase &ph, const char *key, const char *val, char *err, int err_len) {
  if (!strncmp(key, "reply", 5) && key[5] >= '0' && key[5] <= '9') {
    int id = atoi(key + 5);
    if (id < 0 || id > 127) { snprintf(err, err_len, "reply id out of range: %s", key); return false; }
    Reply r;
    if (!strcmp(val, "ack")) r = Reply::Ack;
    else if (!strcmp(val, "data_invalid")) r = Reply::DataInvalid;
    else if (!strcmp(val, "unknown")) r = Reply::Unknown;
    else if (!strcmp(val, "none")) r = Reply::None;
    else { snprintf(err, err_len, "bad reply mode %s=%s", key, val); return false; }
    ph.reply[id] = r;
    return true;
  }
  float f = (float)atof(val);
  long n = atol(val);
  if (!strcmp(key, "flow")) ph.flow = f;
  else if (!strcmp(key, "ret")) ph.ret = f;
  else if (!strcmp(key, "mod")) ph.mod = f;
  else if (!strcmp(key, "pressure")) ph.pressure = f;
  else if (!strcmp(key, "outside")) ph.outside = f;
  else if (!strcmp(key, "dhw_temp")) ph.dhw_temp = f;
  else if (!strcmp(key, "slave_hb")) ph.slave_hb = (uint8_t)strtol(val, nullptr, 0);
  else if (!strcmp(key, "member_id")) ph.member_id = (uint8_t)n;
  else if (!strcmp(key, "max_kw")) ph.max_kw = (uint8_t)n;
  else if (!strcmp(key, "min_mod")) ph.min_mod = (uint8_t)n;
  else if (!strcmp(key, "fault")) ph.fault = n != 0;
  else if (!strcmp(key, "dhw_draw")) ph.dhw_draw = n != 0;
  else if (!strcmp(key, "ignition_delay")) ph.ignition_delay_s = (int32_t)n;
  else if (!strcmp(key, "demand_min")) ph.demand_min = f;
  else if (!strcmp(key, "silent")) ph.silent = n != 0;
  else if (!strcmp(key, "answer_ms")) {
    if (n < 20 || n > 800) { snprintf(err, err_len, "answer_ms outside 20..800: %s", val); return false; }
    ph.answer_ms = (uint16_t)n;
  } else { snprintf(err, err_len, "unknown key %s", key); return false; }
  return true;
}

// Applies "key=value key=value ..." tokens in line (modified in place) to ph.
static bool applyTokens(Phase &ph, char *line, char *err, int err_len) {
  for (char *tok = strtok(line, " \t"); tok; tok = strtok(nullptr, " \t")) {
    char *eq = strchr(tok, '=');
    if (!eq) { snprintf(err, err_len, "expected key=value, got %s", tok); return false; }
    *eq = 0;
    if (!setKey(ph, tok, eq + 1, err, err_len)) return false;
  }
  return true;
}

bool parseScenario(const char *text, Scenario &out, char *err, int err_len) {
  out = Scenario();
  Phase base;
  char line[512];
  const char *p = text;
  int lineno = 0;
  while (*p) {
    const char *e = strchr(p, '\n');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    lineno++;
    if (len >= sizeof(line)) { snprintf(err, err_len, "line %d too long", lineno); return false; }
    memcpy(line, p, len);
    line[len] = 0;
    p = e ? e + 1 : p + len;
    char *cr = strchr(line, '\r'); if (cr) *cr = 0;
    char *hash = strchr(line, '#'); if (hash) *hash = 0;
    char *s = line; while (*s == ' ' || *s == '\t') s++;
    if (!*s) continue;
    char *rest = s; while (*rest && *rest != ' ' && *rest != '\t') rest++;
    if (*rest) *rest++ = 0;
    if (!strcmp(s, "scenario")) {
      while (*rest == ' ') rest++;
      snprintf(out.id, sizeof(out.id), "%s", rest);
    } else if (!strcmp(s, "source")) {
      // provenance only (generator name and version)
    } else if (!strcmp(s, "default")) {
      if (!applyTokens(base, rest, err, err_len)) return false;
    } else if (!strcmp(s, "phase")) {
      if (out.n_phases >= MAX_PHASES) { snprintf(err, err_len, "more than %d phases", MAX_PHASES); return false; }
      while (*rest == ' ') rest++;
      char *dur = rest; while (*rest && *rest != ' ' && *rest != '\t') rest++;
      if (*rest) *rest++ = 0;
      Phase ph = base;
      ph.duration_s = (uint32_t)atol(dur);
      if (!applyTokens(ph, rest, err, err_len)) return false;
      out.phases[out.n_phases++] = ph;
    } else {
      snprintf(err, err_len, "line %d: unknown directive %s", lineno, s);
      return false;
    }
  }
  if (out.n_phases == 0) { snprintf(err, err_len, "no phase lines"); return false; }
  return true;
}

// ------------------------------------------------------------------ boiler

void Boiler::reset() {
  demand_seen_ = false;
  demand_since_ms_ = 0;
  ch_active_ = flame_ = dhw_active_ = false;
  for (int i = 0; i < 128; i++) { stored_set_[i] = false; stored_[i] = 0; }
  // Defaults for write IDs a master may read back before writing them.
  stored_[14] = f88(100.0f);  // max relative modulation 100 %
  stored_[56] = f88(60.0f);   // DHW setpoint
  stored_[57] = f88(80.0f);   // max CH setpoint
}

int Boiler::phaseIndexAt(uint32_t t_ms) const {
  uint32_t start = 0;
  for (int i = 0; i < sc_.n_phases; i++) {
    uint32_t d = sc_.phases[i].duration_s;
    if (i == sc_.n_phases - 1 || d == 0) return i;  // the last phase holds
    if (t_ms < (start + d) * 1000u) return i;
    start += d;
  }
  return sc_.n_phases - 1;
}

void Boiler::updateStatus(uint8_t master_hb, uint32_t t_ms, const Phase &ph) {
  bool ch_enable = master_hb & 0x01;
  bool dhw_enable = master_hb & 0x02;
  dhw_active_ = dhw_enable && ph.dhw_draw && !ph.fault;
  ch_active_ = ch_enable && !ph.fault && !dhw_active_;
  float tset = stored_set_[1] ? (float)(int16_t)stored_[1] / 256.0f : 1000.0f;   // no TSet yet: own setpoint
  bool demand = ch_active_ && tset >= ph.demand_min;
  if (demand && !demand_seen_) { demand_seen_ = true; demand_since_ms_ = t_ms; }
  if (!demand) demand_seen_ = false;
  bool lit_ch = demand && ph.ignition_delay_s >= 0 &&
                (t_ms - demand_since_ms_) >= (uint32_t)ph.ignition_delay_s * 1000u;
  flame_ = dhw_active_ || lit_ch;
}

Answer Boiler::onRequest(uint32_t request, uint32_t t_ms) {
  Answer a;
  if (!parityOk(request)) return a;                       // a frame with bad parity is not answered
  uint8_t type = (request >> 28) & 7u;
  uint8_t id = (request >> 16) & 0xFFu;
  uint16_t data = request & 0xFFFFu;
  if (type != READ_DATA && type != WRITE_DATA) return a;  // only Read-Data / Write-Data are answered
  const Phase &ph = sc_.phases[phaseIndexAt(t_ms)];
  if (id == 0) updateStatus((uint8_t)(data >> 8), t_ms, ph);  // status follows the master, fault or not
  if (ph.silent) return a;
  Reply mode = id < 128 ? ph.reply[id] : Reply::Unknown;
  a.delay_ms = ph.answer_ms;
  if (mode == Reply::None) return a;
  a.send = true;
  if (mode == Reply::DataInvalid) { a.frame = buildFrame(DATA_INVALID, id, data); return a; }
  if (mode == Reply::Unknown) { a.frame = buildFrame(UNKNOWN_DATAID, id, data); return a; }

  if (type == WRITE_DATA) {
    switch (id) {
      case 1: case 2: case 14: case 16: case 24: case 27: case 56: case 57: case 124: case 126:
        stored_[id] = data; stored_set_[id] = true;
        a.frame = buildFrame(WRITE_ACK, id, data);
        return a;
      default:
        a.frame = buildFrame(UNKNOWN_DATAID, id, data);
        return a;
    }
  }
  uint16_t v;
  switch (id) {
    case 0: {
      uint8_t lb = (ph.fault ? 0x01 : 0) | (ch_active_ ? 0x02 : 0) | (dhw_active_ ? 0x04 : 0) | (flame_ ? 0x08 : 0);
      v = (uint16_t)((data & 0xFF00u) | lb);
      break;
    }
    case 3:  v = (uint16_t)((ph.slave_hb << 8) | ph.member_id); break;
    case 15: v = (uint16_t)((ph.max_kw << 8) | ph.min_mod); break;
    case 17: v = f88(flame_ ? ph.mod : 0.0f); break;
    case 18: v = f88(ph.pressure); break;
    case 25: v = f88(ph.flow); break;
    case 26: v = f88(ph.dhw_temp); break;
    case 27: v = stored_set_[27] ? stored_[27] : f88(ph.outside); break;
    case 28: v = f88(ph.ret); break;
    case 1: case 14: case 56: case 57: v = stored_[id]; break;
    case 125: v = f88(4.2f); break;     // OpenTherm version of the slave
    case 127: v = 0x0101; break;        // slave product type / version
    default:
      a.frame = buildFrame(UNKNOWN_DATAID, id, data);
      return a;
  }
  a.frame = buildFrame(READ_ACK, id, v);
  return a;
}

}  // namespace otsim

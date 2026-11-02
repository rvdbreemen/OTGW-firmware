// TASK-1547 AC#3: closed-loop mild-day simulation of the SAT cold cutoff, old rule against the latched
// rule with the 0.5 C band. Both decisions come from the real firmware header SATchEnable.h
// (satColdCutoff = old, satColdCutoffLatched = fix); the house, radiator, boiler and the curve+PI that
// produce the requested setpoint are a simple model written here (not the firmware PID).
// Usage: sim_sat_cold_hysteresis_day <band C> <outside mean C> <outside amplitude C> <seed>
//   band 0 = old rule (bare satColdCutoff), band > 0 = satColdCutoffLatched with that band.
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../src/OTGW-firmware/SATchEnable.h"

int main(int argc, char** argv)
{
  const double band  = argc > 1 ? atof(argv[1]) : 0.0;
  const double outM  = argc > 2 ? atof(argv[2]) : 11.0;
  const double outA  = argc > 3 ? atof(argv[3]) : 3.0;
  unsigned seed      = argc > 4 ? (unsigned)atoi(argv[4]) : 1u;
  const double cold  = 28.2;          // radiator cold setpoint (Sergeant D 2026-10-23)
  const double target = 19.5;
  const double dt = 10.0;             // s
  const int    steps = (int)(24 * 3600 / dt);

  // House: C dT/dt = Qrad - UA (T - Tout). Water: Cw dTf/dt = Qburner - Qrad.
  const double C = 1.5e7, UA = 150.0, Cw = 2.1e5, Krad = 80.0;
  const double Qmin = 3500.0, Qmax = 24000.0;   // gas boiler min / max output (W)
  double room = target, flow = 30.0;
  bool flame = false, cut = false, prevFlame = false;
  double integ = 0.0, requested = 28.5;
  long starts = 0, chEdges = 0; bool prevCh = false;
  double heatJ = 0, flameS = 0, absErr = 0, belowS = 0, sumRoom = 0;
  double cs = 10.0; bool ch = false;

  for (int i = 0; i < steps; ++i) {
    const double t = i * dt;
    const double out = outM + outA * sin(2 * M_PI * (t / 86400.0 - 0.25));   // min at 06:00, max at 18:00
    // Control tick every 60 s: curve + PI on the 0.1 C quantised room sensor, then the cold cutoff.
    if (i % 6 == 0) {
      seed = seed * 1103515245u + 12345u;
      const double noise = ((int)((seed >> 16) % 3u) - 1) * 0.05;   // sensor jitter -0.05/0/+0.05
      const double sensed = floor((room + noise) * 10.0 + 0.5) / 10.0;
      const double e = target - sensed;
      const double d = out - 20.0;
      const double curve = 27.2 + 0.25 * (4.0 * (target - 20.0) + 0.03 * d * d - 0.4 * d);
      integ += e * 60.0 / 3600.0 * 2.0;                             // Ki 2 C per K per hour
      if (integ > 10) integ = 10; if (integ < -10) integ = -10;
      requested = curve + 2.0 * e + integ;                          // Kp 2 C per K
      const bool wasCut = cut;
      cut = band > 0.0 ? satColdCutoffLatched((float)requested, (float)cold, wasCut, (float)band)
                       : satColdCutoff((float)requested, (float)cold);
      ch = !cut; cs = cut ? 10.0 : requested;
      if (ch && !prevCh) chEdges++;
      prevCh = ch;
    }
    // Boiler: modulates between Qmin and Qmax toward CS; own on/off hysteresis when even Qmin is too much.
    double qb = 0.0;
    if (ch) {
      if (flame && flow > cs + 5.0) flame = false;
      else if (!flame && flow < cs) flame = true;
      if (flame) { qb = 4000.0 * (cs - flow) + Krad * pow(fmax(flow - room, 0.0), 1.3); if (qb < Qmin) qb = Qmin; if (qb > Qmax) qb = Qmax; }
    } else flame = false;
    if (flame && !prevFlame) starts++;
    prevFlame = flame;
    const double qr = Krad * pow(fmax(flow - room, 0.0), 1.3);
    flow += (qb - qr) * dt / Cw;
    room += (qr - UA * (room - out)) * dt / C;
    heatJ += qr * dt; if (flame) flameS += dt;
    absErr += fabs(target - room) * dt; sumRoom += room * dt;
    if (room < target - 0.3) belowS += dt;
    if (argc > 5 && i % 360 == 0)
      printf("h=%4.1f out=%5.1f room=%5.2f flow=%5.1f req=%5.2f integ=%5.2f ch=%d flame=%d qb=%5.0f qr=%5.0f\n",
             t / 3600, out, room, flow, requested, integ, (int)ch, (int)flame, qb, qr);
  }
  printf("band=%.1f out=%.1f+-%.1f starts=%ld ch_on_edges=%ld flame_h=%.2f heat_kWh=%.2f room_mean=%.3f mean_abs_err=%.3f below_min=%.0f\n",
         band, outM, outA, starts, chEdges, flameS / 3600.0, heatJ / 3.6e6, sumRoom / (steps * dt),
         absErr / (steps * dt), belowS / 60.0);
  return 0;
}

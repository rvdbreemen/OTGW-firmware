// TASK-1654 host driver: runs the boiler engine (src/boiler_sim.cpp) on a scripted request stream.
//   sim_driver <scenario.scn>    stdin: one "t_ms HEX8" request per line
//   stdout: one line per request, "HEX8 delay_ms" for an answer or "-" for no answer;
//   exit 2 with a message on a scenario parse error.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../src/boiler_sim.h"

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: sim_driver <scenario.scn>\n"); return 2; }
  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
  fclose(f);
  static otsim::Scenario sc;
  char err[160];
  if (!otsim::parseScenario(text.c_str(), sc, err, sizeof(err))) { fprintf(stderr, "parse error: %s\n", err); return 2; }
  otsim::Boiler boiler(sc);
  unsigned long t_ms;
  char hex[32];
  while (scanf("%lu %31s", &t_ms, hex) == 2) {
    uint32_t req = (uint32_t)strtoul(hex, nullptr, 16);
    otsim::Answer a = boiler.onRequest(req, (uint32_t)t_ms);
    if (a.send) printf("%08X %u\n", (unsigned)a.frame, (unsigned)a.delay_ms);
    else printf("-\n");
  }
  return 0;
}

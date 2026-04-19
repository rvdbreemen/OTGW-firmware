#!/usr/bin/env python3
"""Apply all Task #237 changes to SATcycles.ino.

One-shot migration: the generated code (satSaveCycleWindow, satLoadCycleWindow,
satFlushCycleWindow) is now part of the tree. Running this script a second time
used to silently append the cycle functions a second time (Change 4 has no
"already applied" guard), causing redefinition errors. TASK-309 (TEST-M2) adds
the idempotency guard below.
"""

import sys

path = 'src/OTGW-firmware/SATcycles.ino'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Idempotency guard (TASK-309): bail out if the appended functions already exist.
if 'void satSaveCycleWindow()' in content or 'void satFlushCycleWindow()' in content:
    print('[fix_satcycles] already applied: satSaveCycleWindow/satFlushCycleWindow '
          'present in target file. Re-running would duplicate those definitions and '
          'break the build. Exiting cleanly.')
    sys.exit(0)

original_len = len(content)
print(f"Original length: {original_len}")

# Change 1: Update SAT_HCR_FILE declaration
old1 = 'static const char*    SAT_HCR_FILE PROGMEM = "/sat_hcr.json";'
new1 = ('static const char SAT_HCR_FILE_OLD[] PROGMEM = "/sat_hcr.json";\n'
        'static const char SAT_HCR_FILE[]     PROGMEM = "/sat/sat_hcr.json";')
if old1 in content:
    content = content.replace(old1, new1, 1)
    print("Change 1: SAT_HCR_FILE declaration updated")
else:
    print("WARN: Change 1 not found")

# Change 2: Replace satHCRSaveState body
start2 = 'void satHCRSaveState()\n{'
end2_search = ('  strlcat(buf, "]}", sizeof(buf));\n'
               '  f.print(buf);\n'
               '  f.close();\n'
               '}\n')
start2_idx = content.find(start2)
end2_idx = content.find(end2_search, start2_idx)
if start2_idx != -1 and end2_idx != -1:
    end2_pos = end2_idx + len(end2_search)
    new_save = (
        'void satHCRSaveState()\n'
        '{\n'
        '  File f = LittleFS.open(FPSTR(SAT_HCR_FILE), "w");\n'
        '  if (!f) return;\n'
        '  // Format: {"ts":T,"n":N,"h":H,"d":[v0,...,vN-1],"sn":SC,"s":[s0,...,sSC-1]}\n'
        '  // Written chunk-by-chunk to avoid a large stack buffer.\n'
        '  time_t ts = time(nullptr);\n'
        '  char hdr[48];\n'
        '  snprintf_P(hdr, sizeof(hdr), PSTR("{\\\"ts\\\":%lu,\\\"n\\\":%u,\\\"h\\\":%u,\\\"d\\\":["),\n'
        '             (unsigned long)ts, (unsigned)_hcr_count, (unsigned)_hcr_head);\n'
        '  f.print(hdr);\n'
        '  for (uint8_t i = 0; i < HCR_DAYS; i++) {\n'
        '    char fBuf[8];\n'
        '    dtostrf(_hcr_dailyMedian[i], 1, 2, fBuf);\n'
        '    f.print(fBuf);\n'
        '    if (i < HCR_DAYS - 1) f.print(F(","));\n'
        '  }\n'
        '  // Intraday samples: cap at 96 to bound file size on all platforms\n'
        '  uint16_t saveSamples = (_hcr_sCount > 96) ? 96 : _hcr_sCount;\n'
        '  char shdr[24];\n'
        '  snprintf_P(shdr, sizeof(shdr), PSTR("],\\\"sn\\\":%u,\\\"s\\\":["), (unsigned)saveSamples);\n'
        '  f.print(shdr);\n'
        '  for (uint16_t i = 0; i < saveSamples; i++) {\n'
        '    uint16_t src = (uint16_t)((_hcr_sHead + HCR_INTRADAY_SIZE - saveSamples + i) % HCR_INTRADAY_SIZE);\n'
        '    char fBuf[8];\n'
        '    dtostrf(_hcr_samples[src], 1, 2, fBuf);\n'
        '    f.print(fBuf);\n'
        '    if (i < saveSamples - 1) f.print(F(","));\n'
        '  }\n'
        '  f.print(F("]}"));\n'
        '  f.close();\n'
        '}\n'
    )
    content = content[:start2_idx] + new_save + content[end2_pos:]
    print("Change 2: satHCRSaveState updated")
else:
    print(f"WARN: Change 2 not found. start={start2_idx}, end_search_idx={end2_idx}")

# Change 3: Replace satHCRLoadState body
start3 = content.rfind('void satHCRLoadState()\n{')
end3_search = '  DebugTf(PSTR("SAT HCR: loaded %u days'
end3_idx = content.find(end3_search, start3)
if start3 != -1 and end3_idx != -1:
    close = content.find('\n}\n', end3_idx)
    end3_pos = close + 3
    new_load = (
        'void satHCRLoadState()\n'
        '{\n'
        '  satMigrateFile(SAT_HCR_FILE_OLD, SAT_HCR_FILE);\n'
        '  File f = LittleFS.open(FPSTR(SAT_HCR_FILE), "r");\n'
        '  if (!f) return;\n'
        '  // File now includes intraday samples; max size on ESP8266 ~878 bytes.\n'
        '  // Static buffer: persists in BSS, not re-entrant, called once at boot.\n'
        '  static char buf[960];\n'
        '  size_t len = f.readBytes(buf, sizeof(buf) - 1);\n'
        '  buf[len] = 0;\n'
        '  f.close();\n'
        '\n'
        '  char* p;\n'
        '  unsigned n = 0, h = 0;\n'
        '  if ((p = strstr(buf, "\\\"n\\\":")) != nullptr) n = (unsigned)atoi(p + 4);\n'
        '  if ((p = strstr(buf, "\\\"h\\\":")) != nullptr) h = (unsigned)atoi(p + 4);\n'
        '  if (n > HCR_DAYS) n = HCR_DAYS;\n'
        '  if (h >= HCR_DAYS) h = 0;\n'
        '  _hcr_count = (uint8_t)n;\n'
        '  _hcr_head  = (uint8_t)h;\n'
        '\n'
        '  // Parse daily median array\n'
        '  p = strstr(buf, "\\\"d\\\":[");\n'
        '  if (p) {\n'
        '    p += 5;\n'
        '    for (uint8_t i = 0; i < HCR_DAYS; i++) {\n'
        '      _hcr_dailyMedian[i] = strtof(p, &p);\n'
        "      if (*p == ',') p++;\n"
        '    }\n'
        '  }\n'
        '\n'
        '  // Restore intraday samples (Task #237)\n'
        '  unsigned sn = 0;\n'
        '  if ((p = strstr(buf, "\\\"sn\\\":")) != nullptr) sn = (unsigned)atoi(p + 5);\n'
        '  p = strstr(buf, "\\\"s\\\":[");\n'
        '  if (p && sn > 0) {\n'
        '    p += 5;\n'
        '    uint16_t limit = (sn > HCR_INTRADAY_SIZE) ? HCR_INTRADAY_SIZE : (uint16_t)sn;\n'
        '    _hcr_sHead  = 0;\n'
        '    _hcr_sCount = 0;\n'
        '    for (uint16_t i = 0; i < limit; i++) {\n'
        '      float v = strtof(p, &p);\n'
        '      _hcr_samples[_hcr_sHead] = v;\n'
        '      _hcr_sHead = (_hcr_sHead + 1) % HCR_INTRADAY_SIZE;\n'
        '      if (_hcr_sCount < HCR_INTRADAY_SIZE) _hcr_sCount++;\n'
        "      if (*p == ',') p++;\n"
        '    }\n'
        '  }\n'
        '\n'
        '  DebugTf(PSTR("SAT HCR: loaded %u days, %u intraday samples\\r\\n"),\n'
        '          (unsigned)_hcr_count, (unsigned)_hcr_sCount);\n'
        '}\n'
    )
    content = content[:start3] + new_load + content[end3_pos:]
    print("Change 3: satHCRLoadState updated")
else:
    print(f"WARN: Change 3 not found. start={start3}, end_search_idx={end3_idx}")

# Change 4: Add cycle window persistence and flush functions at the end
end_marker = 'return state.sat.sHeatCurveRec;\n}\n'
idx4 = content.rfind(end_marker)
if idx4 != -1:
    end4_pos = idx4 + len(end_marker)
    cycle_funcs = (
        '\n'
        '//=== Cycle Window Persistence (Task #237) ===\n'
        '// Persists the _win4h ring buffer to /sat/sat_cycles.json, capped at 60 entries.\n'
        '// SAT_CYCLES_FILE is defined in SATcontrol.ino (compiled before SATcycles.ino).\n'
        'static const uint32_t SAT_CYCLES_STALE_SEC = 14400UL; // 4h stale threshold\n'
        '\n'
        'void satSaveCycleWindow()\n'
        '{\n'
        '  File f = LittleFS.open(FPSTR(SAT_CYCLES_FILE), "w");\n'
        '  if (!f) return;\n'
        '  uint8_t toWrite = (_win4hCount > 60) ? 60 : (uint8_t)_win4hCount;\n'
        '  time_t ts = time(nullptr);\n'
        '  char hdr[48];\n'
        '  snprintf_P(hdr, sizeof(hdr), PSTR("{\\\"ts\\\":%lu,\\\"n\\\":%u,\\\"r\\\":["),\n'
        '             (unsigned long)ts, (unsigned)toWrite);\n'
        '  f.print(hdr);\n'
        '  for (uint8_t i = 0; i < toWrite; i++) {\n'
        '    uint8_t idx = (uint8_t)((_win4hHead + SAT_WIN4H_SIZE - toWrite + i) % SAT_WIN4H_SIZE);\n'
        '    char fBuf1[8], fBuf2[8];\n'
        '    dtostrf(_win4h[idx].p90FlowTemp, 1, 1, fBuf1);\n'
        '    dtostrf(_win4h[idx].avgFlowRetDelta, 1, 1, fBuf2);\n'
        '    char rec[80];\n'
        '    snprintf_P(rec, sizeof(rec),\n'
        '               PSTR("{\\\"e\\\":%lu,\\\"on\\\":%lu,\\\"off\\\":%lu,\\\"f\\\":%s,\\\"d\\\":%s,\\\"c\\\":%u}%s"),\n'
        '               (unsigned long)_win4h[idx].endMs,\n'
        '               (unsigned long)_win4h[idx].onDurationMs,\n'
        '               (unsigned long)_win4h[idx].offDurationMs,\n'
        '               fBuf1, fBuf2, (unsigned)_win4h[idx].eClass,\n'
        '               (i < toWrite - 1) ? "," : "");\n'
        '    f.print(rec);\n'
        '  }\n'
        '  f.print(F("]}"));\n'
        '  f.close();\n'
        '  DebugTf(PSTR("SAT: cycle window saved (%u records)\\r\\n"), (unsigned)toWrite);\n'
        '}\n'
        '\n'
        'void satLoadCycleWindow()\n'
        '{\n'
        '  File f = LittleFS.open(FPSTR(SAT_CYCLES_FILE), "r");\n'
        '  if (!f) return;\n'
        '  // Read header first to get ts and n\n'
        '  char hdr[48];\n'
        '  size_t hlen = f.readBytes(hdr, sizeof(hdr) - 1);\n'
        '  hdr[hlen] = 0;\n'
        '  f.close();\n'
        '\n'
        '  unsigned long savedTs = 0;\n'
        '  unsigned n = 0;\n'
        '  char* p;\n'
        '  if ((p = strstr(hdr, "\\\"ts\\\":")) != nullptr) savedTs = strtoul(p + 5, nullptr, 10);\n'
        '  if ((p = strstr(hdr, "\\\"n\\\":"))  != nullptr) n = (unsigned)atoi(p + 4);\n'
        '\n'
        '  // Stale check: discard if NTP available and data is too old\n'
        '  time_t nowTs = time(nullptr);\n'
        '  if (nowTs > 1000000L && savedTs > 0) {\n'
        '    uint32_t age = (uint32_t)((unsigned long)nowTs - savedTs);\n'
        '    if (age > SAT_CYCLES_STALE_SEC) {\n'
        '      DebugTf(PSTR("SAT: cycle window discarded (stale, age=%lus)\\r\\n"), (unsigned long)age);\n'
        '      LittleFS.remove(FPSTR(SAT_CYCLES_FILE));\n'
        '      return;\n'
        '    }\n'
        '  }\n'
        '  if (n == 0) return;\n'
        '\n'
        '  // Re-read full file.\n'
        '#if defined(ESP8266)\n'
        '  static char fbuf[2560]; // 30 records * ~80 chars + header ~50 = ~2450 bytes\n'
        '#else\n'
        '  static char fbuf[4896]; // 60 records * ~80 chars + header ~50 = ~4850 bytes\n'
        '#endif\n'
        '  f = LittleFS.open(FPSTR(SAT_CYCLES_FILE), "r");\n'
        '  if (!f) return;\n'
        '  size_t flen = f.readBytes(fbuf, sizeof(fbuf) - 1);\n'
        '  fbuf[flen] = 0;\n'
        '  f.close();\n'
        '\n'
        '  char* arr = strstr(fbuf, "\\\"r\\\":[");\n'
        '  if (!arr) return;\n'
        '  arr += 5;\n'
        '\n'
        '  uint8_t loaded = 0;\n'
        "  while (*arr && *arr != ']' && loaded < n && loaded < SAT_WIN4H_SIZE) {\n"
        "    if (*arr != '{') { arr++; continue; }\n"
        '    SATWindowRecord rec;\n'
        '    memset(&rec, 0, sizeof(rec));\n'
        '    char* ep = arr;\n'
        '    char* vp;\n'
        '    if ((vp = strstr(ep, "\\\"e\\\":"))   != nullptr) rec.endMs           = strtoul(vp + 4,  nullptr, 10);\n'
        '    if ((vp = strstr(ep, "\\\"on\\\":"))  != nullptr) rec.onDurationMs    = strtoul(vp + 5,  nullptr, 10);\n'
        '    if ((vp = strstr(ep, "\\\"off\\\":")) != nullptr) rec.offDurationMs   = strtoul(vp + 6,  nullptr, 10);\n'
        '    if ((vp = strstr(ep, "\\\"f\\\":"))   != nullptr) rec.p90FlowTemp     = strtof(vp + 4,   nullptr);\n'
        '    if ((vp = strstr(ep, "\\\"d\\\":"))   != nullptr) rec.avgFlowRetDelta = strtof(vp + 4,   nullptr);\n'
        '    if ((vp = strstr(ep, "\\\"c\\\":"))   != nullptr) rec.eClass          = (uint8_t)atoi(vp + 4);\n'
        '    _win4h[_win4hHead] = rec;\n'
        '    _win4hHead = (_win4hHead + 1) % SAT_WIN4H_SIZE;\n'
        '    if (_win4hCount < SAT_WIN4H_SIZE) _win4hCount++;\n'
        '    loaded++;\n'
        "    char* end = strchr(arr, '}');\n"
        '    if (!end) break;\n'
        '    arr = end + 1;\n'
        "    if (*arr == ',') arr++;\n"
        '  }\n'
        '  DebugTf(PSTR("SAT: cycle window restored (%u records)\\r\\n"), (unsigned)loaded);\n'
        '}\n'
        '\n'
        'void satFlushCycleWindow()\n'
        '{\n'
        '  LittleFS.remove(FPSTR(SAT_CYCLES_FILE));\n'
        '  _win4hHead  = 0;\n'
        '  _win4hCount = 0;\n'
        '  DebugTln(F("SAT: cycle window flushed"));\n'
        '}\n'
    )
    content = content[:end4_pos] + cycle_funcs + content[end4_pos:]
    print("Change 4: cycle window persistence functions added")
else:
    print(f"WARN: Change 4 end marker not found")

print(f"\nFinal length: {len(content)} (was {original_len})")
with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print('Written successfully')

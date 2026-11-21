"""Builder for tests/test_sat_loop_integration.py: assembles the one host program out of the real production sources."""
import os
import re
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
FW = REPO / "src" / "OTGW-firmware"
PLAT = REPO / "src" / "libraries" / "Platform" / "src"
HERE = Path(__file__).resolve().parent


def rd(p):
    return Path(p).read_text(encoding="utf-8").replace("\r\n", "\n")


def vcvars():
    sys_path = str(HERE)
    import sys
    sys.path.insert(0, sys_path)
    from test_sat_pwm_override import _vcvars
    return _vcvars()


def cl_compile(cpp: Path, exe: Path, extra=""):
    bat = vcvars()
    if not bat:
        return gxx_compile(cpp, exe, extra)
    incs = " ".join(f'/I"{p}"' for p in (HERE / "sat_loop_stubs", FW, PLAT, REPO / "src" / "libraries" / "OpenTherm"))
    cmd = (f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc /utf-8 /Zc:__cplusplus /permissive- /wd4244 /wd4267 /wd4305 /wd4018 /wd4838 /wd4101 /wd4189 /wd4996 /wd4068 '
           f'/DBOARD_NODOSHOP_ESP32_COMBO /DESP32 {incs} {extra} /Fe:"{exe}" /Fo:"{cpp.parent}\\\\" "{cpp}"')
    r = subprocess.run(cmd, shell=True, cwd=cpp.parent, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def gxx_compile(cpp: Path, exe: Path, extra=""):
    """TASK-3020: the same build with g++/clang++ where there is no MSVC (the NAS suite in python:3.12-alpine + g++).
    `extra` holds MSVC-style /D... and /I"..." options; they are translated. (None, why) when there is no compiler."""
    import shutil
    cc = shutil.which("g++") or shutil.which("clang++")
    if not cc:
        return None, "no MSVC and no g++/clang++"
    args = [cc, "-std=c++17", "-w", "-DBOARD_NODOSHOP_ESP32_COMBO", "-DESP32"]
    args += [f"-I{p}" for p in (HERE / "sat_loop_stubs", FW, PLAT, REPO / "src" / "libraries" / "OpenTherm")]
    for opt in re.findall(r'/([DI])("[^"]*"|\S+)', extra or ""):
        args.append(f"-{opt[0]}{opt[1].strip(chr(34))}")
    args += ["-o", str(exe), str(cpp)]
    r = subprocess.run(args, cwd=cpp.parent, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def slice_between(text, start, end_marker, include_end=True):
    a = text.index(start)
    b = text.index(end_marker, a)
    return text[a:b + (len(end_marker) if include_end else 0)]


def real_types() -> str:
    """The real struct layout of OTGWState / OTGWSettings: the component type headers and the two aggregate structs sliced from OTGW-firmware.h."""
    hdr = rd(FW / "OTGW-firmware.h")
    defs = "\n".join(l for l in hdr.split("\n")
                     if re.match(r"#define\s+(_HOSTNAME|MQTT_NAME_DEFAULT|NTP_DEFAULT_TIMEZONE|NTP_HOST_DEFAULT|NTP_RESYNC_TIME|HOME_ASSISTANT_DISCOVERY_PREFIX|CMSG_SIZE|OT_TOPIC_LEN)\b", l))
    enums = slice_between(hdr, "enum class StatusMessage : uint8_t {", "};\n")
    mid = slice_between(hdr, "enum class VerifyOutcome : uint8_t {", "struct OTGWState {", include_end=False)
    state = slice_between(hdr, "struct OTGWState {", "\n};\n")
    sett = slice_between(hdr, "struct OTGWSettings {", "\n};\n")
    incs = "\n".join(f'#include "{h}"' for h in (
        "SATtypes.h", "boards.h", "OTDirecttypes.h", "diagRecorderStuff.h", "Devicetypes.h", "Hardwaretypes.h", "Networktypes.h", "PICtypes.h",
        "OTBustypes.h", "MQTTstuff.h", "Flashtypes.h", "debugStuff.h", "Uptimetypes.h", "NTPtypes.h", "Sensorstypes.h", "S0types.h", "Outputstypes.h",
        "Webhooktypes.h", "UItypes.h"))
    return defs + "\n" + incs + "\n" + enums + "\n" + mid + "\n" + state + "\nstatic OTGWState state;\n" + sett + "\nstatic OTGWSettings settings;\n"


def _mask(src: str) -> str:
    """Same length as src, with comments, string and char literals blanked, so brace/paren scanning is safe."""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if src[k] != "\n":
                    out[k] = " "
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            for k in range(i + 1, min(j, n)):
                out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


_NOT_FUNC = re.compile(r"^\s*(?:typedef|struct|class|enum|union|namespace|extern\s+\"C\"|template|using)\b")


def prototypes(src: str) -> str:
    """Arduino-style auto prototypes for every top-level function DEFINITION in src (the .ino concatenation relies on them)."""
    m = _mask(src)
    protos, depth, stmt_start, i, n = [], 0, 0, 0, len(m)
    while i < n:
        c = m[i]
        if c == "{":
            if depth == 0:
                head = m[stmt_start:i]
                if head.rstrip().endswith(")") or re.search(r"\)\s*(?:const|noexcept)?\s*$", head):
                    if not _NOT_FUNC.match(head) and "=" not in head.split("(")[0] and head.count("(") >= 1:
                        protos.append(re.sub(r"\s+", " ", head).strip())
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                stmt_start = i + 1
        elif c == ";" and depth == 0:
            stmt_start = i + 1
        elif c == "#" and depth == 0 and (i == 0 or m[i - 1] == "\n"):
            j = m.find("\n", i)
            while j > 0 and m[j - 1] == "\\":
                j = m.find("\n", j + 1)
            i = n if j < 0 else j
            stmt_start = i
        i += 1
    return protos


def add_prototypes(src: str):
    """(prototype block, src with default arguments of the definitions removed so the prototype carries them)."""
    m = _mask(src)
    heads = []   # (start, end) of each definition head in src
    depth, stmt_start, i, n = 0, 0, 0, len(m)
    while i < n:
        c = m[i]
        if c == "{":
            if depth == 0:
                head = m[stmt_start:i]
                if re.search(r"\)\s*(?:const|noexcept)?\s*$", head) and not _NOT_FUNC.match(head) and "=" not in head.split("(")[0] and "(" in head:
                    heads.append((stmt_start, i))
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                stmt_start = i + 1
        elif c == ";" and depth == 0:
            stmt_start = i + 1
        elif c == "#" and depth == 0 and (i == 0 or m[i - 1] == "\n"):
            j = m.find("\n", i)
            while j > 0 and m[j - 1] == "\\":
                j = m.find("\n", j + 1)
            i = n if j < 0 else j
            stmt_start = i
        i += 1
    protos, edits = [], []
    for a, b in heads:
        h = m[a:b]
        protos.append(re.sub(r"\s+", " ", h).strip() + ";")
        for mm in re.finditer(r"\s*=\s*[^,()=]+(?=[,)])", h):
            edits.append((a + mm.start(), a + mm.end()))
    out = src
    for a, b in sorted(edits, reverse=True):
        out = out[:a] + out[b:]
    return "\n".join(protos), out


def core_types() -> str:
    """OTdataStruct + OTcurrentSystemState, the OpenTherm message enums and OpenthermData_t, sliced from OTGW-Core.h; f88() from OTGW-Core.ino."""
    h = rd(FW / "OTGW-Core.h")
    a = h.rindex("typedef struct {", 0, h.index("Statusflags"))
    b = h.index("static OTdataStruct OTcurrentSystemState;") + len("static OTdataStruct OTcurrentSystemState;")
    part1 = h[a:b].replace("typedef struct {", "struct OTdataStruct {", 1).replace("} OTdataStruct;", "};")
    e1 = slice_between(h, "enum OTLibMessageType {", "};\n")
    e2 = slice_between(h, "enum OTLibMessageID {", "};\n")
    e3 = slice_between(h, "enum OTGW_response_type {", "};\n")
    ms = "#define OT_MSGTYPE_REQUEST  0\n#define OT_MSGTYPE_RESPONSE 1\n"
    d = slice_between(h, "struct OpenthermData_t {", "\n};\n")
    c = rd(FW / "OTGW-Core.ino")
    f = c.index("float OpenthermData_t::f88() {")
    g = c.index("void OpenthermData_t::f88(float value) {", f)
    f88 = c[f:c.index("\n}\n", g) + 3]   # getter and setter
    return part1 + "\n" + e1 + e2 + e3 + ms + d + "\nstatic OpenthermData_t OTdata;\n" + f88


QUEUE_WRAP = r"""
// ---- logging wrapper for every addCommandToQueue() call made by the SAT sources (and the driver): the refusal is an event of its own
static void ev(const char* kind, const char* fmt, ...);
static const char* rname(CmdQueueResult r) {
  switch (r) { case CmdQueueResult::Queued: return "Queued"; case CmdQueueResult::Replaced: return "Replaced";
               case CmdQueueResult::Sent: return "Sent"; default: return "Dropped"; }
}
static CmdQueueResult addCmdLogged(const char* c, int n, bool force = false, int16_t delay = 1000, bool own = false) {
  const CmdQueueResult r = addCommandToQueue(c, n, force, delay, own);
  char t[40]; int k = n < 39 ? n : 39; memcpy(t, c, k); t[k] = 0;
  if (r == CmdQueueResult::Dropped) ev("ENQ", "%s %s", t, rname(r));   // only refusals are logged here (an accepted command is visible as TX)
  return r;
}
#define addCommandToQueue addCmdLogged
"""


def hooks() -> dict:
    core = rd(FW / "OTGW-Core.ino")
    def cut(a, b, inc_b=False):
        i = core.index(a)
        j = core.index(b, i)
        return core[i:j + (len(b) if inc_b else 0)]
    pm_line = [l for l in core.split(chr(10)) if "pmChainOnBoilerResponse(OTdata.id);" in l and "OTGW_BOILER" in l][0]
    return {
        "STAMP": cut("state.otBus.tBoilerLastSeen = now;", "OTdata.rsptype = OTGW_BOILER;", True),
        "CFG": cut("const bool boilerEvidence", "satNoteSlaveConfig(OTdata.valueHB);", True),
        "PM": pm_line.strip(),
        "FEED": cut("if ((OTdata.id == OT_Tboiler || OTdata.id == OT_Tret)", "// TASK-1412.13 (entry 252)"),
    }


def sat_sources(overrides=None):
    overrides = overrides or {}
    protos, body = "", ""
    for f in ("SATcontrol.ino", "SATcycles.ino", "SATpid.ino"):
        p, s2 = add_prototypes(overrides.get(f) if f in overrides else rd(FW / f))
        protos += p + "\n"
        body += "\n// ===== %s =====\n" % f + s2
    protos = protos.replace("void satDisable(bool persistPid);", "void satDisable(bool persistPid = true);")   # the default lives in OTGW-firmware.h
    return protos, body


def assemble(driver=None, overrides=None) -> str:
    """overrides: {file name: replacement text} for mutants (applied in memory; src/ is never written)."""
    if driver is None:
        driver = rd(HERE / "sat_loop_driver.inc")
    for k, v in hooks().items():
        driver = driver.replace("@HOOK_%s@" % k, v)
    protos, body = sat_sources(overrides or {})
    stubs = rd(HERE / "sat_loop_stubs.h")
    return ("#include <Arduino.h>\n#include \"safeTimers.h\"\n" + real_types() + "\n" + core_types() + "\n" + stubs + "\n" + queue_types() + QUEUE_STUBS +
            queue_pmproto() + "\n" + protos + "\n" + queue_region() + QUEUE_WRAP + "\n" + body + "\n" + driver)


QUEUE_STUBS = r"""
// ---- PIC command queue environment. STUB lines say what is not modelled.
// STUB: the OT debug log macros print nothing (no telnet, no log buffer)
#define OTDebugTf(...) ((void)0)
#define OTDebugT(...) ((void)0)
#define OTDebugf(...) ((void)0)
#define OTDebug(...) ((void)0)
#define OTDebugFlush() ((void)0)
// STUB: the OT-Direct transport is not part of this program (g_pic stays true): its handler records the call and accepts.
static char g_direct[256];
static bool handleOTDirectCommand(const char* b, int n, bool = false) { strncat(g_direct, b, n); strcat(g_direct, ";"); return true; }
static bool platformOnLoopTask() { return true; }          // STUB: the host program is the loop task, no cross-task ring
static void otdCmdTextDrain() {}
static void otdMasterStatus() {}
static bool otdCmdTextPush(const char*, int, bool) { return false; }
static bool otdCmdTextMustWait() { return false; }
struct DiagRecStub { bool active = false; void recOtFrame(char, uint32_t) {} void recPicCommand(const char*, size_t) {}
                     void recOtdCommand(const char*, size_t) {} void recResponse(const char*, size_t) {} };   // STUB: diagnostic recorder never runs
static DiagRecStub diagRec;
static void triggerPICsettingsReadout() {}   // STUB: the PIC settings readout (PR=) cycle is not run
static char cMsg[128];   // scratch buffer the queue code formats a log message into; the message is never shown
#define CCONOFF(b) ((b) ? "ON" : "OFF")   // text helper of the queue's log lines
// the PIC wire: STUB: sendPICSerial records the transmitted line, the serial UART, baud rate and 9600-baud timing are not modelled
static char g_sent[512];
static bool sendPICSerial(const char* b, int n) { strncat(g_sent, b, n); strcat(g_sent, ";"); return true; }
static bool g_simBlock = false;   // STUB: SAT simulation never blocks bus traffic here (bSimulation is off)
#include <mutex>
#include <chrono>
// the queue lock: STUB: a real recursive mutex, never contended (one thread)
static std::recursive_timed_mutex g_qmutex;
struct CmdQueueLock {
  bool locked = false;
  CmdQueueLock() { locked = g_qmutex.try_lock_for(std::chrono::milliseconds(100)); }
  ~CmdQueueLock() { if (locked) g_qmutex.unlock(); }
};
"""


def queue_types() -> str:
    h = rd(FW / "OTGW-firmware.h")
    c = rd(FW / "OTGW-Core.h")
    en = [l for l in h.split("\n") if l.startswith("enum class CmdQueueResult")][0]
    stats = slice_between(h, "struct CmdQueueEchoStats", "void cmdQueueSnapshot(CmdQueueSnap* out);").replace("extern CmdQueueEchoStats g_cmdQueueEcho;", "")
    proto = [l for l in h.split("\n") if l.startswith("CmdQueueResult addCommandToQueue(")][0]
    cmd_t = slice_between(c, "#define PIC_CMD_MAX_LEN", "extern int cmdQueueSize;")
    cmd_t = cmd_t.replace("extern struct OT_cmd_t cmdqueue[CMDQUEUE_MAX];", "static OT_cmd_t cmdqueue[CMDQUEUE_MAX];").replace("extern int cmdQueueSize;", "static int cmdQueueSize = 0;")
    ext = rd(HERE / "test_pic_pm_chain.py")
    return en + "\n" + stats + "\n" + cmd_t + "\n" + proto + "\n"


def queue_pmproto() -> str:
    c = rd(FW / "OTGW-Core.h")
    return slice_between(c, "void pmChainStart();", "bool boilerMsgId28UnknownConfirmed();").split("bool picBoardTretValid")[0]


def queue_region() -> str:
    core = rd(FW / "OTGW-Core.ino")
    a = core.index("#define OTGW_CMD_RETRY")
    b = core.index("//===================[ Send buffer to OTGW ]")
    ch = core.index("bool isCentralHeatingActive() {")
    ch_end = core.index("bool isFlameStatus()", ch)
    hdr = rd(FW / "OTGW-firmware.h")
    m = "inline bool satBlocksExternalCommand("
    pred = hdr[hdr.index(m):hdr.index("\n}\n", hdr.index(m)) + 3]
    pred_decl = "void satNoteExternalRefused(const char* buf, int len);\nbool satOwnsControlSetpoint();\n"
    return pred_decl + pred + "\n" + core[ch:ch_end] + "\n" + core[a:b]

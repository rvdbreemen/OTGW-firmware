#!/usr/bin/env python3
"""TASK-1605: application code never resolves through hostByName() and never connects by hostname.

arduino-esp32 3.3.5 NetworkManager::hostByName() calls dns_clear_cache() without the lwIP core
lock when its static IPv4/IPv6 flags change (always on the first call after boot). A pending SNTP
DNS callback then runs on the caller's task and trips the core-lock assert (bench .63, beta.74,
internet blocked: a panic every 5 min). NetworkClient::connect(host, ...) and HTTPClient's own
connect go through that hostByName(). The firmware resolves with platformResolveHost()
(platform_esp32.h, lwip_getaddrinfo) and connects by IP; HTTPClient then reuses the connection.

Source gate (src/OTGW-firmware only; the vendored libraries are out of scope):
  G1 no hostByName( call (WiFi., Network., Ethernet. or bare)
  G2 no connect / setServer / beginPacket with a string literal or CSTR()/settings host
  G3 every function that sends an HTTPClient request (GET, POST, sendRequest) connects by IP
     with netConnectUrl()/netConnectIp() after its last http.begin() and before the request
  G4 netConnectUrl()/netConnectIp() resolve with platformResolveHost() and connect by IP
Compiled tests (host C++ compiler; the real code is cut out of the sources, not re-implemented):
  C1 platformResolveHost() (cut from platform_esp32.h): a literal IPv4 returns without DNS; a name
     goes to lwip_getaddrinfo with AF_INET and the result is freed; a failed lookup gives false
     and 0.0.0.0; an empty name gives false without DNS
  C2 weatherFetchWork() (SATweather.ino) with netConnectUrl()/netResolveUrlHost()/netConnectIp()
     (networkStuff.ino) over a fake HTTPClient that models arduino-esp32 3.3.5 HTTPClient.cpp:
     begin() stops a connected client whose host differs (:283-287), connect() reuses a connected
     client (:1075-1085) and otherwise connects by hostname (:1109). Expected order: begin, resolve,
     connect(ip, 80, 2000), read timeout 5000, GET on the reused connection, Host header from the
     URL, no hostname connect. A failed lookup sends no request at all.
  C3 netResolveUrlHost() parses host and port (default 80, explicit :8123, malformed port refused)

OTGW_SATWEATHER_INO / OTGW_NETWORK_INO / OTGW_SRC_DIR point at other copies for the old-vs-fix run.

Run: python tests/test_no_hostbyname.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC_DIR = Path(os.environ.get("OTGW_SRC_DIR", REPO / "src" / "OTGW-firmware"))
WEATHER_INO = Path(os.environ.get("OTGW_SATWEATHER_INO", SRC_DIR / "SATweather.ino"))
NETWORK_INO = Path(os.environ.get("OTGW_NETWORK_INO", SRC_DIR / "networkStuff.ino"))
PLATFORM_H = REPO / "src" / "libraries" / "Platform" / "src" / "platform_esp32.h"


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def strip_comments(src: str) -> str:
    """Remove // and /* */ comments; string and char literals are kept as they are."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            out.append("\n" * src.count("\n", i, n if j < 0 else j))
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def app_sources():
    files = sorted(p for p in SRC_DIR.iterdir() if p.suffix in (".ino", ".h", ".cpp"))
    return [(p.name, strip_comments(read(p).replace("\r\n", "\n"))) for p in files]


def line_of(src: str, pos: int) -> int:
    return src.count("\n", 0, pos) + 1


# ---------------------------------------------------------------- source gate
class SourceGate(unittest.TestCase):
    def test_g1_no_hostbyname(self):
        hits = []
        for name, src in app_sources():
            for m in re.finditer(r"\bhostByName\s*\(", src):
                hits.append("%s:%d" % (name, line_of(src, m.start())))
        self.assertEqual(hits, [], "hostByName() clears the lwIP DNS cache without the core lock "
                                   "(TASK-1605); use platformResolveHost(): " + ", ".join(hits))

    def test_g2_no_connect_by_hostname(self):
        pat = re.compile(r"\.(connect|setServer|beginPacket)\s*\(\s*(\"|CSTR\s*\(|settings\.)")
        hits = []
        for name, src in app_sources():
            for m in pat.finditer(src):
                hits.append("%s:%d %s" % (name, line_of(src, m.start()), m.group(0)))
        self.assertEqual(hits, [], "connect by hostname resolves through hostByName(): " + ", ".join(hits))

    def test_g3_requests_preconnect_by_ip(self):
        bad, seen = [], 0
        req = re.compile(r"\b(\w+)\s*\.\s*(GET|POST|sendRequest)\s*\(")
        for name, src in app_sources():
            for m in req.finditer(src):
                obj = m.group(1)
                if not re.search(r"\bHTTPClient\s+%s\b" % re.escape(obj), src):
                    continue          # not an HTTPClient request
                seen += 1
                start = src.rfind("\n}\n", 0, m.start())
                body = src[start + 1:m.start()]
                b = [x.end() for x in re.finditer(r"\b%s\s*\.\s*begin\s*\(" % re.escape(obj), body)]
                if not b:
                    bad.append("%s:%d no %s.begin() before the request" % (name, line_of(src, m.start()), obj))
                    continue
                if not re.search(r"\bnetConnect(Url|Ip)\s*\(", body[b[-1]:]):
                    bad.append("%s:%d %s.%s() without netConnectUrl()/netConnectIp() after begin()"
                               % (name, line_of(src, m.start()), obj, m.group(2)))
        self.assertGreater(seen, 0, "no HTTPClient request found: the gate scans the wrong tree")
        self.assertEqual(bad, [], "HTTPClient would connect by hostname (hostByName): " + "; ".join(bad))

    def test_g4_helpers_resolve_with_the_shim(self):
        src = strip_comments(read(NETWORK_INO).replace("\r\n", "\n"))
        for fn in ("netResolveUrlHost", "netConnectIp", "netConnectUrl"):
            self.assertRegex(src, r"\bbool %s\(" % fn, fn + "() not found in networkStuff.ino")
        body = src[src.index("bool netResolveUrlHost("):]
        body = body[:body.index("\n}\n")]
        self.assertIn("platformResolveHost(", body)
        ip = src[src.index("bool netConnectIp("):]
        ip = ip[:ip.index("\n}\n")]
        self.assertRegex(ip, r"client\.connect\(ip,\s*port,")
        self.assertIn("client.setTimeout(readTimeoutMs)", ip)

    def test_g5_shim_uses_getaddrinfo_ipv4(self):
        h = read(PLATFORM_H)
        self.assertIn("inline bool platformResolveHost(const char *name, IPAddress &out)", h)
        body = h[h.index("inline bool platformResolveHost("):]
        body = body[:body.index("\n}")]
        self.assertIn("lwip_getaddrinfo(", body)
        self.assertIn("hints.ai_family = AF_INET;", body)
        self.assertIn("lwip_freeaddrinfo(res);", body)
        self.assertNotIn("hostByName", body)
        self.assertNotIn("dns_clear_cache", body)


# ---------------------------------------------------------------- compiled tests
COMMON = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define PSTR(x) x
#define F(x) x
#define snprintf_P snprintf
#ifdef _MSC_VER
#define strncasecmp_P _strnicmp
#else
#include <strings.h>
#define strncasecmp_P strncasecmp
#endif
#define DebugTf(...) ((void)0)
#define DebugTln(x) ((void)0)
#define OTDebugln(x) ((void)0)
static void yield() {}

// Minimal IPAddress (IPv4) with the Arduino API the cut code uses.
struct IPAddress {
  uint8_t b[4] = {0, 0, 0, 0};
  IPAddress() {}
  IPAddress(uint32_t v) { memcpy(b, &v, 4); }          // network byte order, as Arduino
  IPAddress(uint8_t a, uint8_t c, uint8_t d, uint8_t e) { b[0] = a; b[1] = c; b[2] = d; b[3] = e; }
  operator uint32_t() const { uint32_t v; memcpy(&v, b, 4); return v; }
  uint8_t operator[](int i) const { return b[i]; }
  bool fromString(const char* s) {
    unsigned v[4]; char tail;
    if (sscanf(s, "%u.%u.%u.%u%c", &v[0], &v[1], &v[2], &v[3], &tail) != 4) return false;
    for (int i = 0; i < 4; i++) { if (v[i] > 255) return false; b[i] = (uint8_t)v[i]; }
    return true;
  }
};
static std::vector<std::string> g_log;
static std::string ipstr(const IPAddress& ip) {
  char buf[20]; snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); return buf;
}
'''

RESOLVE_HARNESS = COMMON + r'''
#define AF_INET 2
#define SOCK_STREAM 1
struct in_addr { uint32_t s_addr; };
struct sockaddr { uint16_t sa_family; char sa_data[14]; };
struct sockaddr_in { uint16_t sin_family; uint16_t sin_port; struct in_addr sin_addr; char zero[8]; };
struct addrinfo { int ai_flags, ai_family, ai_socktype, ai_protocol; size_t ai_addrlen;
                  struct sockaddr* ai_addr; char* ai_canonname; struct addrinfo* ai_next; };
static int g_gai_calls = 0, g_gai_family = -1, g_gai_ret = 0, g_free_calls = 0;
static std::string g_gai_name;
static struct sockaddr_in g_sin; static struct addrinfo g_ai;
static int lwip_getaddrinfo(const char* name, const char*, const struct addrinfo* hints, struct addrinfo** res) {
  g_gai_calls++; g_gai_name = name; g_gai_family = hints->ai_family;
  if (g_gai_ret != 0) return g_gai_ret;
  memset(&g_sin, 0, sizeof(g_sin)); g_sin.sin_family = AF_INET;
  g_sin.sin_addr.s_addr = (uint32_t)IPAddress(1, 2, 3, 4);
  memset(&g_ai, 0, sizeof(g_ai)); g_ai.ai_family = AF_INET; g_ai.ai_addr = (struct sockaddr*)&g_sin;
  *res = &g_ai; return 0;
}
static void lwip_freeaddrinfo(struct addrinfo*) { g_free_calls++; }

#include "resolve_cut.inc"

#define P(k, v) printf("%s=%s\n", k, std::string(v).c_str())
#define N(k, v) printf("%s=%d\n", k, (int)(v))
int main() {
  IPAddress ip;
  N("lit_ok", platformResolveHost("192.168.88.32", ip)); P("lit_ip", ipstr(ip)); N("lit_gai", g_gai_calls);
  N("empty_ok", platformResolveHost("", ip)); N("null_ok", platformResolveHost(nullptr, ip)); N("empty_gai", g_gai_calls);
  N("name_ok", platformResolveHost("api.open-meteo.com", ip)); P("name_ip", ipstr(ip));
  N("name_gai", g_gai_calls); P("name_gai_name", g_gai_name); N("name_family", g_gai_family); N("name_free", g_free_calls);
  g_gai_ret = -1; ip = IPAddress(9, 9, 9, 9);
  N("fail_ok", platformResolveHost("blocked.example", ip)); P("fail_ip", ipstr(ip)); N("fail_free", g_free_calls);
  return 0;
}
'''

WEATHER_HARNESS = COMMON + r'''
static uint32_t g_resolveFail = 0;
static bool platformResolveHost(const char* name, IPAddress& out) {
  g_log.push_back(std::string("resolve:") + name);
  out = IPAddress((uint32_t)0);
  if (g_resolveFail) return false;
  out = IPAddress(93, 184, 0, 7);
  return true;
}
// Fake NetworkClient: records how it was connected.
struct WiFiClient {
  bool conn = false;
  int connect(const IPAddress& ip, uint16_t port, int32_t t) {
    char b[64]; snprintf(b, sizeof(b), "connect_ip:%s:%u:%d", ipstr(ip).c_str(), port, (int)t);
    g_log.push_back(b); conn = true; return 1;
  }
  int connect(const char* host, uint16_t port, int32_t t) {
    char b[96]; snprintf(b, sizeof(b), "connect_host:%s:%u:%d", host, port, (int)t);
    g_log.push_back(b); conn = true; return 1;
  }
  void setTimeout(unsigned long t) { g_log.push_back("read_timeout:" + std::to_string(t)); }
  bool connected() { return conn; }
  void stop() { g_log.push_back("stop"); conn = false; }
  int available() { return 0; }
  int read() { return -1; }
  int peek() { return -1; }
};
// Fake HTTPClient modelled on arduino-esp32 3.3.5 HTTPClient.cpp.
struct HTTPClient {
  WiFiClient* c = nullptr; std::string host; uint16_t port = 80; int32_t ct = 5000;
  void setConnectTimeout(int32_t t) { ct = t; }
  void setTimeout(uint16_t) {}
  bool begin(WiFiClient& cl, const char* url) {
    c = &cl;                                          // :122
    std::string u(url); u = u.substr(u.find("://") + 3);
    std::string h = u.substr(0, u.find_first_of("/:"));
    g_log.push_back("begin:" + h);
    if (host != h && c->connected()) { g_log.push_back("begin_teardown"); c->stop(); }   // :283-287
    host = h; return true;
  }
  void useHTTP10(bool) {}
  bool connect() {
    if (c->connected()) { g_log.push_back("reuse"); return true; }                       // :1075-1085
    return c->connect(host.c_str(), port, ct) != 0;                                      // :1109
  }
  int GET() {
    if (!connect()) return -1;
    g_log.push_back("GET Host: " + host);                                                // :1145
    return 503;
  }
  WiFiClient* getStreamPtr() { return c; }
  bool connected() { return c && c->connected(); }
  void end() { g_log.push_back("end"); }
};
struct { bool bOk; float fReqLat, fReqLon; } _wxWork;
static void weatherParseStream(WiFiClient*, HTTPClient*) {}
static const char kWeatherUrlFmt[] = "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f";

#include "net_cut.inc"
#include "weather_cut.inc"

static void dump(const char* tag) {
  printf("%s=", tag);
  for (size_t i = 0; i < g_log.size(); i++) printf("%s%s", i ? "|" : "", g_log[i].c_str());
  printf("\n");
  g_log.clear();
}
int main() {
  _wxWork.fReqLat = 52.1f; _wxWork.fReqLon = 6.2f;
  weatherFetchWork(); dump("ok");
  g_resolveFail = 1;
  weatherFetchWork(); dump("dnsfail");
  g_resolveFail = 0;
  IPAddress ip; uint16_t port = 0;
  int r = netResolveUrlHost("http://homeassistant.local:8123/api/webhook/x", ip, port);   // call first: argument order is unspecified
  printf("url1=%d:%s:%u\n", r, ipstr(ip).c_str(), port);
  r = netResolveUrlHost("http://10.0.0.5/x", ip, port);
  printf("url2=%d:%u\n", r, port);
  printf("url3=%d\n", (int)netResolveUrlHost("http://host:99999/x", ip, port));
  printf("url4=%d\n", (int)netResolveUrlHost("https://host/x", ip, port));
  g_log.clear();
  return 0;
}
'''


def cut_function(src: str, signature: str) -> str:
    a = src.index(signature)
    return src[a:src.index("\n}\n", a) + 3]


def run(workdir: Path, harness: str, incs: dict, name: str):
    for fn, text in incs.items():
        (workdir / fn).write_text(text, encoding="utf-8")
    cpp = workdir / (name + ".cpp")
    cpp.write_text(harness, encoding="utf-8")
    exe = compile_cpp(cpp, workdir / (name + ".exe"))
    if exe is None:
        return None
    r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    print("\n" + r.stdout.strip())
    return dict(line.split("=", 1) for line in r.stdout.splitlines())


class CompiledResolve(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        h = read(PLATFORM_H).replace("\r\n", "\n")
        try:
            inc = cut_function(h, "inline bool platformResolveHost(")
            cls.out, cls.err = run(Path(cls.tmp.name), RESOLVE_HARNESS, {"resolve_cut.inc": inc}, "resolve"), None
        except ValueError as e:
            cls.out, cls.err = None, e

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if self.err is not None:
            self.fail("platformResolveHost() not found: %s" % self.err)
        if self.out is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def test_c1_literal_without_dns(self):
        o = self.out
        self.assertEqual(o["lit_ok"], "1")
        self.assertEqual(o["lit_ip"], "192.168.88.32")
        self.assertEqual(o["lit_gai"], "0")

    def test_c1_empty_name(self):
        self.assertEqual(self.out["empty_ok"], "0")
        self.assertEqual(self.out["null_ok"], "0")
        self.assertEqual(self.out["empty_gai"], "0")

    def test_c1_name_via_getaddrinfo_ipv4(self):
        o = self.out
        self.assertEqual(o["name_ok"], "1")
        self.assertEqual(o["name_ip"], "1.2.3.4")
        self.assertEqual(o["name_gai"], "1")
        self.assertEqual(o["name_gai_name"], "api.open-meteo.com")
        self.assertEqual(o["name_family"], "2")       # AF_INET
        self.assertEqual(o["name_free"], "1")

    def test_c1_failed_lookup(self):
        self.assertEqual(self.out["fail_ok"], "0")
        self.assertEqual(self.out["fail_ip"], "0.0.0.0")
        self.assertEqual(self.out["fail_free"], "1")   # nothing extra freed on failure


class CompiledWeatherConnect(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        w = read(WEATHER_INO).replace("\r\n", "\n")
        n = read(NETWORK_INO).replace("\r\n", "\n")
        try:
            consts = "\n".join(re.findall(r"^static const uint32_t WEATHER_\w+_MS\s*=.*$", w, re.M))
            if "WEATHER_READ_TIMEOUT_MS" not in consts:   # old code: keep the harness compiling
                consts += "\nstatic const uint32_t WEATHER_READ_TIMEOUT_MS = 5000UL;"
            weather = consts + "\n" + cut_function(w, "static void weatherFetchWork()")
            if "bool netConnectUrl(" in n:
                net = n[n.index("static const size_t NET_HOST_MAX"):]
                net = net[:net.index("\n}\n", net.index("bool netConnectUrl(")) + 3]
            else:                                          # old networkStuff: no helpers
                net = ""
            cls.out, cls.err = run(Path(cls.tmp.name), WEATHER_HARNESS,
                                   {"weather_cut.inc": weather, "net_cut.inc": net}, "weather"), None
        except (ValueError, AssertionError) as e:
            cls.out, cls.err = None, e

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if self.err is not None:
            self.fail("weather connect harness: %s" % self.err)
        if self.out is None:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def test_c2_connect_by_ip_then_reuse(self):
        steps = self.out["ok"].split("|")
        self.assertEqual(steps, [
            "begin:api.open-meteo.com",
            "resolve:api.open-meteo.com",
            "connect_ip:93.184.0.7:80:2000",
            "read_timeout:5000",
            "reuse",
            "GET Host: api.open-meteo.com",
            "end",
        ])
        self.assertFalse(any(s.startswith("connect_host") for s in steps), "HTTPClient connected by hostname")
        self.assertNotIn("begin_teardown", steps)

    def test_c2_failed_lookup_sends_nothing(self):
        steps = self.out["dnsfail"].split("|")
        self.assertFalse(any(s.startswith("connect") for s in steps), steps)
        self.assertFalse(any(s.startswith("GET") for s in steps), steps)

    def test_c3_url_parse(self):
        self.assertEqual(self.out["url1"], "1:93.184.0.7:8123")
        self.assertEqual(self.out["url2"], "1:80")
        self.assertEqual(self.out["url3"], "0")
        self.assertEqual(self.out["url4"], "0")


if __name__ == "__main__":
    unittest.main(verbosity=2)

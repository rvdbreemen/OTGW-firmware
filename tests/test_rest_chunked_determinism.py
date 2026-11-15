#!/usr/bin/env python3
"""TASK-1910: a chunked REST body gains an extra '}' when a field is read live instead of from the snapshot.

    python tests/test_rest_chunked_determinism.py
    OTGW_CHUNK_REV=<sha> python tests/test_rest_chunked_determinism.py     # another revision (old vs fix)

restSendChunked() (jsonChunked.h) re-runs the emit closure for every TCP window and copies bytes
[index, index+maxLen) through RestChunkWindow; a pass that lands 0 bytes ends the response. That needs
byte-identical output on every pass (the file's DETERMINISM CONTRACT). device/info read three heap
values live in its closure (internal_free, internal_maxblk, psram_free; restAPI.ino). Under a
connection storm the heap moves between passes: when a value gains a digit between the pass that
sent the last bytes and the pass that should find the end, that pass returns the last byte again,
the closing '}'. That is 09's body: a complete object plus one '}' (2 open, 3 close).

Part 1 (mechanism): the real RestChunkWindow class, sliced from jsonChunked.h, driven by a filler loop
like AsyncChunkedResponse (index = bytes produced so far), over an emit whose one number grows by a
digit between passes. Part 2 (gate): no restSendChunked() closure in restAPI.ino reads the heap live.
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
REV = os.environ.get("OTGW_CHUNK_REV")


def read(rel):
    if REV is None:
        return (REPO / rel).read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:{rel}"], capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
struct Print {
  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) = 0;
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  virtual ~Print() {}
};
@@WINDOW@@
static int g_pass = 0;
static int g_growAt = -1;          // pass from which the live value has one digit more
static void emit(Print& p) {       // {"a":{"internal_free":NNNN,"x":1}}
  char num[16];
  snprintf(num, sizeof(num), "%d", g_pass >= g_growAt && g_growAt >= 0 ? 10000 : 9999);
  p.print("{\"a\":{\"internal_free\":"); p.print(num); p.print(",\"x\":1}}");
  g_pass++;
}
int main(int argc, char** argv) {
  const size_t win = (size_t)atoi(argv[1]);
  g_growAt = atoi(argv[2]);
  std::string body;
  uint8_t buf[2048];
  for (int guard = 0; guard < 100; ++guard) {          // AsyncChunkedResponse filler loop
    RestChunkWindow w(buf, win, body.size());
    emit(w);
    if (w.written() == 0) break;
    body.append((const char*)buf, w.written());
  }
  printf("%s\n", body.c_str());
  return 0;
}
"""

LIVE = re.compile(r"\b(heap_caps_get_free_size|heap_caps_get_largest_free_block|ESP\.getFree\w*|ESP\.getMaxAlloc\w*|"
                  r"platformFreeHeap|platformMaxFreeBlock|millis)\s*\(")


def chunked_closures(src):
    out, i = [], 0
    while True:
        a = src.find('restSendChunked("application/json", [', i)
        if a < 0:
            return out
        b = src.find("\n  });", a)
        out.append((src.count("\n", 0, a) + 1, src[a:b]))
        i = b


class RestChunkedDeterminism(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        hdr = read("src/OTGW-firmware/jsonChunked.h")
        a = hdr.find("class RestChunkWindow : public Print {")
        b = hdr.find("};", a)
        if a < 0 or b < 0:
            raise AssertionError("RestChunkWindow not found in jsonChunked.h")
        window = hdr[a:b + 2].replace("  using Print::write;\n", "")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(HARNESS.replace("@@WINDOW@@", window).replace("#include <string>", "#include <string>\n#include <cstdlib>"),
                       encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        cls.rest = read("src/OTGW-firmware/restAPI.ino")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_body(self, win, grow_at):
        return subprocess.run([str(self.exe), str(win), str(grow_at)], capture_output=True, text=True,
                              check=True).stdout.strip()

    def test_stable_emit_gives_the_exact_body(self):
        self.assertEqual(self.run_body(8, -1), '{"a":{"internal_free":9999,"x":1}}')

    def test_a_digit_gained_before_the_end_pass_repeats_the_closing_brace(self):
        good = '{"a":{"internal_free":9999,"x":1}}'
        passes = -(-len(good) // 8)                     # windows that carry bytes
        body = self.run_body(8, passes)                 # the value grows on the end-detect pass
        self.assertEqual(body, good + "}", "09's symptom: a complete object plus one '}'")

    def test_no_chunked_closure_reads_the_heap_live(self):
        found = []
        for line, body in chunked_closures(self.rest):
            for m in LIVE.finditer(body):
                found.append(f"restAPI.ino closure at line {line}: {m.group(1)}()")
        self.assertEqual(found, [], "volatile values must come from the closure's snapshot")


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""TASK-1969: GET /api/v2/sat/weather/needs-setup reaches its own answer.

processAPI() (restAPI.ino) splits the URI on '/' with a leading empty word, so
/api/v2/sat/weather/needs-setup is six words ("", api, v2, sat, weather, needs-setup) and words[5] is
"needs-setup" only when wc is 6. The sub-route checked wc == 5, never true for that path, so the request
fell through to the full weather status.

This test compiles the REAL word split of processAPI() and the REAL needs-setup condition of handleSAT(),
both cut from restAPI.ino, and feeds them the real paths. Red on the old code, green on the fix.
Run: python tests/test_sat_weather_needs_setup_route.py   (OTGW_SRC_DIR=<dir> for other sources)
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = Path(os.environ.get("OTGW_SRC_DIR", HERE.parent / "src" / "OTGW-firmware"))
REST = SRC / "restAPI.ino"


class NeedsSetupRoute(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = REST.read_text(encoding="utf-8")
        lines = text.splitlines()
        i = next(k for k, l in enumerate(lines) if "if (URI[0] == '/' && wc < API_MAX_WORDS)" in l)
        split_code = "\n".join(lines[i:i + 5])          # the leading-slash word and the strtok_r loop
        assert lines[i + 4].strip() == "}" and "strtok_r" in lines[i + 1], "processAPI word split changed shape"
        cond = re.search(r'if \((wc [^&\n]+&& strcasecmp_P\(words\[5\], PSTR\("needs-setup"\)\) == 0)\)', text)
        assert cond, "needs-setup condition not found"
        mw = re.search(r"#define API_MAX_WORDS\s+(\d+)", text)
        src = ("#include <cstdio>\n#include <cstring>\n#include <strings.h>\n" if os.name != "nt" else
               "#include <cstdio>\n#include <cstring>\n#define strcasecmp _stricmp\n#define strtok_r strtok_s\n")
        src += ("#define PSTR(s) s\n#define strcasecmp_P strcasecmp\n#define API_WORD_LEN 32\n"
                f"#define API_MAX_WORDS {mw.group(1) if mw else 8}\n"
                "#define strlcpy otgw_test_strlcpy\n"
                "static size_t strlcpy(char* d, const char* s, size_t n){ size_t l=strlen(s); if(n){ size_t c=l<n-1?l:n-1; memcpy(d,s,c); d[c]=0;} return l; }\n"
                "static bool hit(const char* path, int* wcOut) {\n"
                "  char URI[64]; strlcpy(URI, path, sizeof(URI)); char words[API_MAX_WORDS][API_WORD_LEN] = {{0}};\n"
                "  int wc = 0; char* savePtr = nullptr;\n"
                + split_code + "\n"
                f"  *wcOut = wc; return {cond.group(1)};\n}}\n"
                "int main(){ const char* p[] = {\"/api/v2/sat/weather/needs-setup\", \"/api/v2/sat/weather\", \"/api/v2/sat/weather/other\"};\n"
                "  for (const char* s : p) { int wc = 0; bool h = hit(s, &wc); std::printf(\"%s wc=%d hit=%d\\n\", s, wc, h ? 1 : 0); } return 0; }\n")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(src, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t" + (".exe" if os.name == "nt" else "")))
        if exe is None:
            raise unittest.SkipTest("no C++ compiler on this host")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.r = {l.split()[0]: l for l in out.strip().splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_needs_setup_path_matches(self):
        self.assertEqual(self.r["/api/v2/sat/weather/needs-setup"], "/api/v2/sat/weather/needs-setup wc=6 hit=1")

    def test_weather_status_and_other_subpaths_do_not(self):
        self.assertEqual(self.r["/api/v2/sat/weather"], "/api/v2/sat/weather wc=5 hit=0")
        self.assertEqual(self.r["/api/v2/sat/weather/other"], "/api/v2/sat/weather/other wc=6 hit=0")


if __name__ == "__main__":
    unittest.main(verbosity=2)

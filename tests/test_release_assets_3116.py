#!/usr/bin/env python3
"""TASK-3116: no 2.x release asset may be one that an old 1.x flash tool downloads and writes to an ESP8266.

Facts (architect checklist, otgw-field-data/_comms/task3116_acceptance_checklist_2026-11-27.md):
- 1.x flash_otgw.sh/.bat pick OTGW-firmware-*.ino.bin and OTGW-firmware*.littlefs.bin from the newest release and write them
  with --chip esp8266 --erase-all; 1.x flash_esp.py takes the first asset whose lowercased name contains .ino.bin, fw.bin,
  firmware.bin (and not littlefs) and littlefs.bin, fs.bin or filesystem.bin; esptool does not refuse an ESP32-S3 image there.
- On an API failure the 1.x script downloads every *.ino.bin / *.littlefs.bin named in SHA256SUMS, so that list must not
  name them either.

Part 1 (this class): the asset list of scripts/beta_publish.py. The asset names come from the real asset_names(); the
matcher is loose_asset_violations(). Red on the list before the change (the app and filesystem images were uploaded
one by one), mutants for names a future change could add (-fs.bin, -fw.bin) must fail.
Part 2 (class OldTools): the real 1.x scripts from the v1.7.10 tag run against a stubbed release; see below.

Run: python tests/test_release_assets_3116.py
"""
import hashlib
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("beta_publish", REPO / "scripts" / "beta_publish.py")
bp = importlib.util.module_from_spec(spec)
sys.modules["beta_publish"] = bp
spec.loader.exec_module(bp)

SEMVER_FULL = "2.0.0-beta.452+42b40d6"


class AssetList(unittest.TestCase):
    def setUp(self):
        self.names = bp.asset_names(SEMVER_FULL)
        self.uploaded = [self.names[k] for k in bp.UPLOADED_KEYS] + ["SHA256SUMS"]

    def test_uploaded_names_hit_no_1x_matcher(self):
        self.assertEqual(bp.loose_asset_violations(self.uploaded), [])

    def test_uploaded_set_is_exactly_the_four_files_and_sums(self):
        self.assertEqual(sorted(self.uploaded), sorted([
            self.names["zip"], self.names["merged"], self.names["elf"], self.names["safeboot"], "SHA256SUMS"]))

    def test_the_old_list_is_red(self):
        """The list before the change uploaded every name of asset_names(): the app and filesystem image are hit."""
        old = list(self.names.values()) + ["SHA256SUMS"]
        hit = {n for n, _ in bp.loose_asset_violations(old)}
        self.assertEqual(hit, {self.names["app"], self.names["fs"]})

    def test_mutants_a_future_name_must_fail(self):
        p = f"OTGW-firmware-esp32-combo-{SEMVER_FULL}"
        for bad in (f"{p}-fs.bin", f"{p}-fw.bin", f"{p}-firmware.bin", f"{p}-filesystem.bin", f"{p}.ino.bin",
                    f"{p}.littlefs.bin", f"{p}-APP.INO.BIN", "x.littlefs.bin", "OTGW-firmware-2.0.0.ino.bin"):
            self.assertTrue(bp.loose_asset_violations([bad]), bad)

    def test_the_inner_zip_names_stay_unchanged(self):
        """The update page of the 2.x betas reads exactly one .ino.bin with its .littlefs.bin from the zip."""
        self.assertTrue(self.names["app"].endswith(".ino.bin"))
        self.assertTrue(self.names["fs"].endswith(".littlefs.bin"))

    def test_sha256sums_is_not_a_matcher_hit(self):
        self.assertEqual(bp.loose_asset_violations(["SHA256SUMS"]), [])


def find_bash():
    """Git Bash on Windows (a bash on PATH there can be the WSL launcher, which cannot see temp dirs); bash elsewhere."""
    if os.name == "nt":
        for base in (os.environ.get("ProgramFiles", r"C:\Program Files"), os.environ.get("ProgramW6432", "")):
            cand = Path(base) / "Git" / "usr" / "bin" / "bash.exe"
            if base and cand.is_file():
                return str(cand)
        return None
    return shutil.which("bash")


CURL_STUB = r"""#!/bin/bash
# curl stand-in: serves the fixture release. -o OUT URL, prints the http code like -w "%{http_code}".
out=""; url=""
while [ $# -gt 0 ]; do
  case "$1" in
    -o) out="$2"; shift 2 ;;
    -A|-w|--retry) shift 2 ;;
    -*) shift ;;
    *) url="$1"; shift ;;
  esac
done
fx="$STUB_FIXTURE"
case "$url" in
  *api.github.com*releases/latest)
    if [ "$STUB_API" = "403" ]; then echo 403; exit 22; fi
    : > "$out"
    echo '{"assets":[' >> "$out"
    first=1
    for f in "$fx"/*; do
      n="$(basename "$f")"
      [ $first = 1 ] || echo ',' >> "$out"; first=0
      printf '{"name": "%s", "browser_download_url": "https://stub/dl/%s"}\n' "$n" "$n" >> "$out"
    done
    echo ']}' >> "$out"
    echo 200; exit 0 ;;
  *releases/latest/download/*|https://stub/dl/*)
    n="${url##*/}"
    if [ -f "$fx/$n" ]; then cp "$fx/$n" "$out"; echo 200; exit 0; fi
    echo 404; exit 22 ;;
esac
echo 000; exit 7
"""

ESPTOOL_STUB = """#!/bin/bash
echo "$@" >> "$STUB_LOG"
exit 0
"""


class OldTools(unittest.TestCase):
    """AC1/AC1b of TASK-3116: the REAL flash_otgw.sh of the 1.x line (tag v1.7.10, via git show), a stubbed curl and a
    recording esptool, in an empty temp dir (a stale local .ino.bin would be found by find_highest_version)."""

    TAG = "v1.7.10"

    @classmethod
    def script(cls):
        r = subprocess.run(["git", "show", f"{cls.TAG}:flash_otgw.sh"], cwd=REPO, capture_output=True)
        if r.returncode != 0:
            raise unittest.SkipTest(f"tag {cls.TAG} not available in this clone")
        return r.stdout

    def run_old_tool(self, release_names, api):
        """release_names: the asset names the (fake) release holds, SHA256SUMS lists them (the one that
        beta_publish writes: exactly the uploaded files)."""
        bash = find_bash()
        if not bash:
            self.skipTest("no bash")
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            fx, work, binp = d / "fixture", d / "work", d / "bin"
            for x in (fx, work, binp):
                x.mkdir()
            sums = []
            for n in release_names:
                data = (n + " payload of a 2.x S3 image").encode()
                (fx / n).write_bytes(data)
                sums.append(f"{hashlib.sha256(data).hexdigest()}  {n}\n")
            (fx / "SHA256SUMS").write_text("".join(sums), newline="\n")
            (work / "flash_otgw.sh").write_bytes(self.script())
            (work / "tools" / "esptool").mkdir(parents=True)
            et = work / "tools" / "esptool" / "esptool"
            et.write_text(ESPTOOL_STUB, newline="\n")
            (binp / "curl").write_text(CURL_STUB, newline="\n")
            for f in (et, binp / "curl"):
                f.chmod(0o755)
            log = d / "esptool.log"
            env = dict(os.environ, PATH=f"{binp}{os.pathsep}{os.environ['PATH']}", STUB_FIXTURE=str(fx),
                       STUB_API=api, STUB_LOG=str(log))
            r = subprocess.run([bash, (work / "flash_otgw.sh").as_posix(), "--port", "/dev/stub", "--yes", "--no-sudo"],
                               cwd=work, env=env, capture_output=True, text=True, timeout=120)
            calls = log.read_text().splitlines() if log.exists() else []
            return r.returncode, r.stdout + r.stderr, calls

    def old_list(self):
        n = bp.asset_names(SEMVER_FULL)
        return [n["zip"], n["app"], n["fs"], n["merged"], n["elf"], n["safeboot"]]   # what 2.x uploaded before

    def new_list(self):
        n = bp.asset_names(SEMVER_FULL)
        return [n[k] for k in bp.UPLOADED_KEYS]

    def test_empty_dir_1x_sh_never_reaches_the_download_branch(self):
        """FACT about v1.7.10 (found while writing this test): in an empty directory flash_otgw.sh exits 2 right after
        'esptool: ...' because find_highest_version() (ls | sort | head) fails under set -e + pipefail when nothing matches.
        The auto-download branch is unreachable, so the .sh cannot download a 2.x image today, whatever the release holds.
        If a later 1.x tag fixes that, the new-list tests below keep guarding the asset names."""
        for names in (self.old_list(), self.new_list()):
            for api in ("200", "403"):
                rc, out, calls = self.run_old_tool(names, api)
                self.assertEqual(rc, 2, out[-400:])
                self.assertEqual(calls, [])
                self.assertNotIn("Downloading OTGW-firmware", out)

    def test_new_asset_list_gives_the_1x_tool_nothing_to_pick(self):
        """The names a fixed 1.x .sh would look for: nothing in the new release matches either glob."""
        import fnmatch
        for g in ("OTGW-firmware-*.ino.bin", "OTGW-firmware*.littlefs.bin", "*.ino.bin", "*.littlefs.bin"):
            self.assertEqual([n for n in self.new_list() + ["SHA256SUMS"] if fnmatch.fnmatchcase(n, g)], [], g)
            self.assertTrue([n for n in self.old_list() if fnmatch.fnmatchcase(n, g)], g + " (red on the old list)")


FLASH_ESP_TAG = "v1.7.10"


class OldFlashEsp(unittest.TestCase):
    """AC2: the REAL 1.x flash_esp.py (tag v1.7.10). Only urllib's urlretrieve is replaced; nothing is flashed. The decision
    that matters is download_release_assets(): which release assets it picks to write to an ESP8266."""

    @classmethod
    def setUpClass(cls):
        r = subprocess.run(["git", "show", f"{FLASH_ESP_TAG}:flash_esp.py"], cwd=REPO, capture_output=True)
        if r.returncode != 0:
            raise unittest.SkipTest(f"tag {FLASH_ESP_TAG} not available in this clone")
        cfg = subprocess.run(["git", "show", f"{FLASH_ESP_TAG}:config.py"], cwd=REPO, capture_output=True)
        cls.tmp = tempfile.TemporaryDirectory()
        path = Path(cls.tmp.name) / "flash_esp_1x.py"
        path.write_bytes(r.stdout)
        (Path(cls.tmp.name) / "config.py").write_bytes(cfg.stdout)   # the 1.x script imports its sibling config.py
        sys.path.insert(0, cls.tmp.name)
        try:
            sys.modules.pop("config", None)
            s = importlib.util.spec_from_file_location("flash_esp_1x", path)
            cls.mod = importlib.util.module_from_spec(s)
            s.loader.exec_module(cls.mod)
        finally:
            sys.path.remove(cls.tmp.name)
            sys.modules.pop("config", None)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def picked(self, names):
        got = []

        def fake_retrieve(url, dest):
            got.append(Path(dest).name)
            Path(dest).write_bytes(b"x")

        real = self.mod.urllib.request.urlretrieve
        self.mod.urllib.request.urlretrieve = fake_retrieve
        try:
            with tempfile.TemporaryDirectory() as td:
                info = {"assets": [{"name": n, "size": 1, "browser_download_url": "https://stub/" + n} for n in names]}
                res = self.mod.download_release_assets(info, Path(td))
        finally:
            self.mod.urllib.request.urlretrieve = real
        return res, got

    def test_old_asset_list_is_picked_up_for_an_esp8266(self):
        """RED: the list before the change: the 1.x tool downloads the S3 app and filesystem image."""
        n = bp.asset_names(SEMVER_FULL)
        res, got = self.picked([n["zip"], n["app"], n["fs"], n["merged"], n["elf"], n["safeboot"], "SHA256SUMS"])
        self.assertEqual(sorted(res), ["filesystem", "firmware"])
        self.assertEqual(sorted(got), sorted([n["app"], n["fs"]]))

    def test_new_asset_list_is_not_picked_up(self):
        n = bp.asset_names(SEMVER_FULL)
        res, got = self.picked([n[k] for k in bp.UPLOADED_KEYS] + ["SHA256SUMS"])
        self.assertEqual(res, {})
        self.assertEqual(got, [])


class Wiring(unittest.TestCase):
    """Text check (not a run) of the publish script: the refusal sits before the tag and the upload, and SHA256SUMS and
    the upload both come from the same `uploads` dict. The end-to-end run is tests/test_beta_publish.py."""

    def test_refusal_and_single_source(self):
        src = (REPO / "scripts" / "beta_publish.py").read_text(encoding="utf-8")
        main = src[src.index("    # 3. build files"):]
        self.assertLess(main.index("loose_asset_violations([*uploads"), main.index("# 5. tag"))
        self.assertIn("for n, p in uploads.items()", main)
        self.assertIn("assets_all = {**uploads", main)
        self.assertNotIn("**local", main)


class BuildScripts(unittest.TestCase):
    """Follow-up (beta.458 publish): check_githash() with --build-commit must only fail on files the BUILD uses. A change to a
    host-only tool under scripts/ (SAT replay, load tests) between the build commit and the release commit is not a
    different binary. Real git repo in a temp dir, the real check_githash()."""

    FILES = {
        "build.py": "import webui_pages\nprint('scripts/check_app_size.py')\n",
        "platformio.ini": "[env]\nextra_scripts =\n  pre:scripts/patch_pio_libs.py\n",
        "scripts/webui_pages.py": "import helperlib\n",
        "scripts/helperlib.py": "X = 1\n",
        "scripts/patch_pio_libs.py": "Y = 1\n",
        "scripts/check_app_size.py": "Z = 1\n",
        "scripts/otgw_capture.py": "import sat_log_to_csv\n",
        "scripts/sat_log_to_csv.py": "W = 1\n",
        "scripts/sat_selftune_replay.py": "# host only\nimport sat_selftune_fixture\n",
        "scripts/sat_selftune_fixture.py": "# host only\n",
        "scripts/loadtest_sweep.py": "# host only\n",
        "src/a.ino": "int a;\n",
        "docs/x.md": "x\n",
    }

    def setUp(self):
        self.td = tempfile.TemporaryDirectory()
        self.repo = Path(self.td.name)

        def git(*a):
            return subprocess.run(["git", "-C", str(self.repo), *a], capture_output=True, text=True, check=True).stdout.strip()
        self.git = git
        git("init", "-q")
        git("config", "user.email", "t@example.invalid")
        git("config", "user.name", "t")
        for n, c in self.FILES.items():
            f = self.repo / n
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text(c, newline="\n")
        git("add", "-A")
        git("commit", "-q", "-m", "build")
        self.build = git("rev-parse", "HEAD")
        self.tools = bp.Tools(self.repo, ["gh"], "x/y")

    def tearDown(self):
        self.td.cleanup()

    def release_with(self, changes):
        for n, c in changes.items():
            (self.repo / n).write_text(c, newline="\n")
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "release")
        return self.git("rev-parse", "HEAD")

    def check(self, rel):
        return bp.check_githash(self.tools, rel, self.build[:9], self.build)

    def test_build_scripts_derivation(self):
        self.assertEqual(bp.build_scripts(self.tools, self.build), [
            "scripts/check_app_size.py", "scripts/helperlib.py", "scripts/otgw_capture.py", "scripts/patch_pio_libs.py",
            "scripts/sat_log_to_csv.py", "scripts/webui_pages.py"])

    def test_host_tools_may_change(self):
        rel = self.release_with({"scripts/sat_selftune_replay.py": "# changed\n", "scripts/loadtest_sweep.py": "# changed\n",
                                 "docs/x.md": "y\n"})
        self.assertIn("nothing changed", self.check(rel))

    def test_a_file_the_build_uses_may_not_change(self):
        for name in ("scripts/patch_pio_libs.py", "scripts/helperlib.py", "scripts/sat_log_to_csv.py",
                     "scripts/check_app_size.py", "src/a.ino", "build.py", "platformio.ini"):
            with self.subTest(name):
                self.setUp()
                rel = self.release_with({name: "# changed\n"})
                with self.assertRaises(bp.Refuse):
                    self.check(rel)

    def test_the_old_check_refused_a_host_tool(self):
        """RED on the code before: the whole scripts/ directory was checked (the file at origin/dev)."""
        old = subprocess.run(["git", "show", "origin/dev:scripts/beta_publish.py"], cwd=REPO, capture_output=True, text=True,
                             encoding="utf-8")
        if old.returncode != 0 or "def build_scripts" in old.stdout:
            self.skipTest("origin/dev already holds the new check")
        m = type(sys)("bp_old")
        exec(compile(old.stdout, "beta_publish_old.py", "exec"), m.__dict__)
        rel = self.release_with({"scripts/sat_selftune_replay.py": "# changed\n"})
        tools = m.Tools(self.repo, ["gh"], "x/y")
        with self.assertRaises(m.Refuse):
            m.check_githash(tools, rel, self.build[:9], self.build)


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""build.py must load the config.py next to itself, not one that comes first on sys.path.

The embedded build Python (.build-python, python312._pth) names one fixed tree and
does not add the script's folder to sys.path. A build from another worktree then
imported that tree's config.py, renamed and archived the fresh artifacts in the
other tree's build/, and merged a stale .ino.bin into this tree's images while
still printing "Build completed successfully".

The test imports build.py from a temporary copy with a decoy config.py placed
first on sys.path, the way the ._pth file does.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_PY = os.environ.get("OTGW_BUILD_PY", os.path.join(ROOT, "build.py"))

PROBE = r"""
import importlib.util, sys
sys.path.insert(0, sys.argv[2])          # decoy tree first, as the ._pth file does
spec = importlib.util.spec_from_file_location("build_under_test", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
print("CONFIG_FILE=" + m.config.__file__)
"""


class BuildLoadsItsOwnConfig(unittest.TestCase):
    def test_config_comes_from_the_build_py_folder(self):
        with tempfile.TemporaryDirectory() as tmp:
            tree = os.path.join(tmp, "tree")
            decoy = os.path.join(tmp, "decoy")
            os.makedirs(tree)
            os.makedirs(decoy)
            shutil.copy2(BUILD_PY, os.path.join(tree, "build.py"))
            shutil.copy2(os.path.join(ROOT, "config.py"), os.path.join(tree, "config.py"))
            os.makedirs(os.path.join(tree, "scripts"))  # build.py imports scripts/webui_pages.py (TASK-1640)
            shutil.copy2(os.path.join(ROOT, "scripts", "webui_pages.py"), os.path.join(tree, "scripts", "webui_pages.py"))
            with open(os.path.join(decoy, "config.py"), "w", encoding="utf-8") as f:
                f.write(open(os.path.join(ROOT, "config.py"), encoding="utf-8").read())
            r = subprocess.run([sys.executable, "-c", PROBE, os.path.join(tree, "build.py"), decoy],
                               capture_output=True, text=True, cwd=tmp, timeout=120)
            out = r.stdout + r.stderr
            self.assertEqual(r.returncode, 0, out)
            line = [l for l in r.stdout.splitlines() if l.startswith("CONFIG_FILE=")]
            self.assertTrue(line, out)
            loaded = os.path.normcase(os.path.realpath(line[0].split("=", 1)[1]))
            self.assertEqual(loaded, os.path.normcase(os.path.realpath(os.path.join(tree, "config.py"))),
                             "build.py loaded " + loaded)


if __name__ == "__main__":
    unittest.main()

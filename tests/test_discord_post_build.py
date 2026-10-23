"""TASK-1313: the refusal checks of scripts/discord-post-build.sh.

Runs the real script against a throwaway git repo (OTGW_REPO) with --dry-run, so
nothing is posted. The one non-dry-run case points the webhook file at a missing
path, which must refuse before any network call.

Run: python tests/test_discord_post_build.py
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SCRIPT = Path(os.environ.get("OTGW_POST_BUILD_SH", REPO / "scripts" / "discord-post-build.sh"))


def find_bash():
    # On Windows a bare "bash" can resolve to WSL; prefer Git for Windows' bash.
    git = shutil.which("git")
    if git:
        for cand in (Path(git).parents[1] / "bin" / "bash.exe", Path(git).parent / "bash.exe"):
            if cand.exists():
                return str(cand)
    return shutil.which("bash")


BASH = find_bash()


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], check=True,
                          capture_output=True, text=True).stdout.strip()


@unittest.skipUnless(BASH, "bash not found")
class DiscordPostBuildRefusals(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp, True)
        git(self.tmp, "init", "-q")
        git(self.tmp, "config", "user.email", "t@example.invalid")
        git(self.tmp, "config", "user.name", "t")
        git(self.tmp, "config", "core.autocrlf", "false")
        src = self.tmp / "src" / "OTGW-firmware" / "data"
        src.mkdir(parents=True)
        (self.tmp / "src" / "OTGW-firmware" / "version.h").write_text("v1\n")
        (src / "version.hash").write_text("h1\n")
        (src / "v2.js").write_text("js1\n")
        git(self.tmp, "add", "-A")
        git(self.tmp, "commit", "-q", "-m", "init")
        self.head = git(self.tmp, "rev-parse", "--short=7", "HEAD")
        (self.tmp / "build").mkdir()

    def make_zip(self, hash_, size=1000):
        z = self.tmp / "build" / f"OTGW-firmware-esp32-combo-2.0.0-alpha.1+{hash_}-flash.zip"
        with open(z, "wb") as f:
            f.truncate(size)
        return z

    def run_script(self, *args, webhook="/nonexistent/hook.url"):
        env = dict(os.environ, OTGW_REPO=str(self.tmp), OTGW_DISCORD_WEBHOOK_FILE=webhook)
        return subprocess.run([BASH, str(SCRIPT), *args], env=env,
                              capture_output=True, text=True, timeout=60)

    def test_clean_tree_matching_hash_passes(self):
        self.make_zip(self.head)
        r = self.run_script("--dry-run", "-m", 'TASK-1 "quoted"')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("DRY RUN OK", r.stdout)
        self.assertIn("2.0.0-alpha.1+" + self.head, r.stdout)

    def test_hash_not_head_refused(self):
        self.make_zip("deadbee")
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 2)
        self.assertIn("is not HEAD", r.stderr)

    def test_dirty_src_refused(self):
        self.make_zip(self.head)
        (self.tmp / "src" / "OTGW-firmware" / "data" / "v2.js").write_text("js2\n")
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 2)
        self.assertIn("uncommitted changes under src/", r.stderr)

    def test_untracked_src_file_refused(self):
        self.make_zip(self.head)
        (self.tmp / "src" / "new.ino").write_text("x\n")
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 2)

    def test_build_stamp_churn_allowed(self):
        self.make_zip(self.head)
        (self.tmp / "src" / "OTGW-firmware" / "version.h").write_text("v2\n")
        (self.tmp / "src" / "OTGW-firmware" / "data" / "version.hash").write_text("h2\n")
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_oversize_refused(self):
        self.make_zip(self.head, size=10_000_001)
        r = self.run_script("--dry-run")
        self.assertEqual(r.returncode, 2)
        self.assertIn("upload limit", r.stderr)

    def test_missing_webhook_file_refused_before_network(self):
        self.make_zip(self.head)
        r = self.run_script()
        self.assertEqual(r.returncode, 2)
        self.assertIn("webhook file not readable", r.stderr)

    def test_webhook_url_never_printed(self):
        self.make_zip(self.head)
        hook = self.tmp / "hook.url"
        hook.write_text("https://example.invalid/not-a-discord-hook/SECRET123")
        r = self.run_script(webhook=str(hook))
        self.assertEqual(r.returncode, 2)
        self.assertNotIn("SECRET123", r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

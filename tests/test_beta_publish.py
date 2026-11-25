#!/usr/bin/env python3
"""TASK-3090: scripts/beta_publish.py against a fake gh (tests/fixtures/fake_gh.py) and a real local git repo.

Every refusal is tested for what matters most: it stops BEFORE anything is created (no draft, no tag on origin).
The happy paths run the real script end to end: dry run (throw-away tag, draft deleted again) and a real run
(tag pushed, draft, publish, re-download).

    python tests/test_beta_publish.py
    BETA_PUBLISH_SCRIPT=<path> python tests/test_beta_publish.py     # run the same tests against a mutant copy
"""
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = Path(os.environ.get("BETA_PUBLISH_SCRIPT", HERE.parent / "scripts" / "beta_publish.py"))
FAKE_GH = HERE / "fixtures" / "fake_gh.py"
sys.path.insert(0, str(SCRIPT.parent))
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location("beta_publish_under_test", SCRIPT)
bp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bp)

TAG, SEMVER = "v2.0.0-beta.440", "2.0.0-beta.440"
EM = "—"


def sh(cwd, *a):
    p = subprocess.run(a, cwd=cwd, capture_output=True, text=True, check=True,
                       env={**os.environ, "GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@t", "GIT_COMMITTER_NAME": "t",
                            "GIT_COMMITTER_EMAIL": "t@t"})
    return p.stdout.strip()


def notes_text(extra=0):
    parts = ["# OTGW-firmware v2.0.0-beta.440 Release Notes\n\nHeader.\n",
             "## What this beta is\n\nA beta.\n",
             "## What changed since beta.439\n\n- TASK-1 a change\n",
             "## Old stuff nobody needs twice\n\n" + ("old line\n" * extra),
             "## SAT is experimental and opt-in\n\nSAT text.\n",
             "## How to report\n\nHere.\n"]
    return "\n".join(parts)


class World:
    """repo + bare origin + build dir + fake gh state; built fresh for every test."""

    def __init__(self, tc, hash_of="candidate"):
        self.tmp = Path(tempfile.mkdtemp(prefix="bp-test-"))
        tc.addCleanup(shutil.rmtree, self.tmp, True)
        self.origin, self.repo, self.build = self.tmp / "origin.git", self.tmp / "repo", self.tmp / "build"
        self.ghdir, self.scratch, self.ledger = self.tmp / "gh", self.tmp / "scratch", self.tmp / "ledger.tsv"
        sh(self.tmp, "git", "init", "-q", "--bare", str(self.origin))
        sh(self.tmp, "git", "init", "-q", "-b", "dev", str(self.repo))
        sh(self.repo, "git", "remote", "add", "origin", str(self.origin))
        (self.repo / "src").mkdir()
        (self.repo / "src" / "a.cpp").write_text("v0\n")
        sh(self.repo, "git", "add", "."), sh(self.repo, "git", "commit", "-qm", "base")
        self.base = sh(self.repo, "git", "rev-parse", "HEAD")
        (self.repo / "src" / "a.cpp").write_text("v1\n")
        sh(self.repo, "git", "commit", "-qam", "candidate (src change)")
        self.cand = sh(self.repo, "git", "rev-parse", "HEAD")
        (self.repo / "notes.md").write_text("docs\n")
        sh(self.repo, "git", "add", "."), sh(self.repo, "git", "commit", "-qm", "release commit (docs only)")
        self.rel = sh(self.repo, "git", "rev-parse", "HEAD")
        sh(self.repo, "git", "push", "-q", "origin", "dev")
        self.ghdir.mkdir()
        (self.ghdir / "state.json").write_text("{}")
        self.make_build(self.cand[:7])
        self.verdict = self.tmp / "verdict.txt"
        self.write_verdict()
        self.notes = self.tmp / "notes.md"
        self.notes.write_text(notes_text(), encoding="utf-8")
        self.ledger.write_text("")

    # build dir ------------------------------------------------------------------------------------------------
    def make_build(self, githash):
        shutil.rmtree(self.build, ignore_errors=True)
        self.build.mkdir()
        self.full = f"{SEMVER}+{githash}"
        n = bp.asset_names(self.full)
        self.names = n
        for key in ("app", "fs", "merged", "elf", "safeboot"):
            (self.build / n[key]).write_bytes((key * 100).encode() + githash.encode())
        with zipfile.ZipFile(self.build / n["zip"], "w") as z:
            for name in (n["app"], n["fs"], n["merged"], "flash_otgw.bat", "flash_otgw.sh", "otgw_capture.py"):
                z.writestr(name, "x" * 50)
        self.soaked = hashlib.sha256((self.build / n["app"]).read_bytes()).hexdigest()

    def write_verdict(self, verdict="PASS", minutes="60", sha=None):
        lines = [f"verdict: {verdict}", f"soak_minutes: {minutes}"] + ([f"sha256: {sha}"] if sha else [])
        self.verdict.write_text("\n".join(lines) + "\n")

    # gh state ---------------------------------------------------------------------------------------------------
    def state(self, **set_):
        p = self.ghdir / "state.json"
        st = json.loads(p.read_text())
        if set_:
            st.update(set_)
            p.write_text(json.dumps(st))
        return st

    def two_dry_runs_on_record(self):
        self.ledger.write_text("t\t2.0.0-beta.430\tok\nt\t2.0.0-beta.435\tok\n")

    # run ----------------------------------------------------------------------------------------------------------
    def run(self, *extra, tag=TAG, theme="web UI polish", notes=None, soaked=None, commit=None):
        argv = ["--tag", tag, "--release-commit", commit or self.rel, "--build-dir", str(self.build),
                "--soaked-sha256", soaked or self.soaked, "--verdict", str(self.verdict),
                "--notes", str(notes or self.notes), "--theme", theme, "--repo", str(self.repo),
                "--scratch", str(self.scratch), "--ledger", str(self.ledger), *extra]
        out, err = io.StringIO(), io.StringIO()
        os.environ["FAKE_GH_DIR"] = str(self.ghdir)
        with redirect_stdout(out), redirect_stderr(err):
            rc = bp.main(argv, gh_cmd=[sys.executable, str(FAKE_GH)])
        return rc, out.getvalue(), err.getvalue()

    def remote_tags(self):
        return sh(self.repo, "git", "ls-remote", "--tags", "origin").split()


class Base(unittest.TestCase):
    def refused(self, w, needle, *extra, **kw):
        rc, out, err = w.run(*extra, **kw)
        self.assertEqual(rc, 2, f"expected a refusal, got rc={rc}\n{out}\n{err}")
        self.assertIn(needle, err)
        self.assertEqual(w.state().get("releases", []), [], "a refusal must not leave a release behind")
        self.assertEqual(w.remote_tags(), [], "a refusal must not push a tag")
        return out, err


class Refusals(Base):
    def test_soaked_sha_mismatch(self):
        w = World(self)
        self.refused(w, "not the binary that was soaked", "--dry-run", soaked="0" * 64)

    def test_build_file_changed_after_soak(self):
        w = World(self)
        (w.build / w.names["app"]).write_bytes(b"patched after the soak")
        self.refused(w, "not the binary that was soaked", "--dry-run")

    def test_verdict_not_pass(self):
        w = World(self)
        w.write_verdict(verdict="FAIL")
        self.refused(w, "not PASS", "--dry-run")

    def test_soak_too_short(self):
        w = World(self)
        w.write_verdict(minutes="59")
        self.refused(w, "59 min", "--dry-run")

    def test_verdict_for_another_binary(self):
        w = World(self)
        w.write_verdict(sha="f" * 64)
        self.refused(w, "another binary", "--dry-run")

    def test_verdict_missing(self):
        w = World(self)
        w.verdict.unlink()
        self.refused(w, "not found", "--dry-run")

    def test_zip_githash_not_the_release_commit_and_src_changed(self):
        w = World(self)
        w.make_build(w.base[:7])           # the soaked binary is from BEFORE the src change the tag contains
        self.refused(w, "src/ changed", "--dry-run")

    def test_zip_githash_unknown_commit(self):
        w = World(self)
        w.make_build("deadbee")
        self.refused(w, "not a commit", "--dry-run")

    def test_em_dash_in_notes(self):
        w = World(self)
        w.notes.write_text(notes_text() + f"\nA line {EM} with a dash.\n", encoding="utf-8")
        self.refused(w, "em dash", "--dry-run")

    def test_latest_flag(self):
        w = World(self)
        self.refused(w, "never the latest", "--dry-run", "--latest")

    def test_stable_tag(self):
        w = World(self)
        self.refused(w, "not a 2.x beta tag", "--dry-run", tag="v2.0.0")

    def test_missing_asset(self):
        w = World(self)
        (w.build / w.names["elf"]).unlink()
        self.refused(w, "missing or empty build file", "--dry-run")

    def test_two_flash_zips_for_one_tag(self):
        w = World(self)
        shutil.copy(w.build / w.names["zip"], w.build / w.names["zip"].replace(w.full.split("+")[1], "abcdef0"))
        self.refused(w, "several flash zips", "--dry-run")

    def test_zip_without_flash_scripts(self):
        w = World(self)
        with zipfile.ZipFile(w.build / w.names["zip"], "w") as z:
            for name in (w.names["app"], w.names["fs"], w.names["merged"]):
                z.writestr(name, "x")
        self.refused(w, "flash_otgw.bat", "--dry-run")

    def test_real_run_needs_yes_publish(self):
        w = World(self)
        w.two_dry_runs_on_record()
        self.refused(w, "--yes-publish")

    def test_real_run_refused_without_two_dry_runs(self):
        w = World(self)
        w.ledger.write_text("t\t2.0.0-beta.435\tok\n")
        self.refused(w, "the gate is 2", "--yes-publish")

    def test_same_beta_twice_counts_once(self):
        w = World(self)
        w.ledger.write_text("t\t2.0.0-beta.435\tok\nu\t2.0.0-beta.435\tok\n")
        self.refused(w, "the gate is 2", "--yes-publish")

    def test_real_run_release_commit_not_on_origin(self):
        w = World(self)
        w.two_dry_runs_on_record()
        (w.repo / "later.md").write_text("x")
        sh(w.repo, "git", "add", "."), sh(w.repo, "git", "commit", "-qm", "unpushed")
        self.refused(w, "not on origin/dev", "--yes-publish", commit=sh(w.repo, "git", "rev-parse", "HEAD"))

    def test_real_run_tag_already_on_origin(self):
        w = World(self)
        w.two_dry_runs_on_record()
        sh(w.repo, "git", "push", "-q", "origin", f"{w.rel}:refs/tags/{TAG}")
        rc, out, err = w.run("--yes-publish")
        self.assertEqual(rc, 2)
        self.assertIn("exists on origin", err)
        self.assertEqual(w.state().get("releases", []), [])

    def test_auth_failure(self):
        w = World(self)
        w.state(auth_fail=True)
        self.refused(w, "auth status", "--dry-run")


class Body(Base):
    def test_long_notes_become_the_shortened_body(self):
        w = World(self)
        w.notes.write_text(notes_text(extra=20000), encoding="utf-8")                  # about 160 KB
        self.assertGreater(len(w.notes.read_text(encoding="utf-8")), bp.MAX_BODY)
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("shortened", out)
        sent = (w.scratch / "body.md").read_text(encoding="utf-8")
        self.assertLessEqual(len(sent), bp.MAX_BODY)
        self.assertIn("## What changed since beta.439", sent)
        self.assertIn("## Earlier changes", sent)
        self.assertIn(f"https://github.com/{bp.REPO_SLUG}/blob/{TAG}/docs/releases/RELEASE_NOTES_{SEMVER}.md", sent)
        self.assertIn("## SAT is experimental and opt-in", sent)
        self.assertIn("## How to report", sent)
        self.assertNotIn("old line", sent)

    def test_long_notes_without_cut_headings_are_refused(self):
        w = World(self)
        w.notes.write_text("# t\n\n" + "line\n" * 40000)
        self.refused(w, "lack the", "--dry-run")

    def test_short_notes_go_out_whole(self):
        w = World(self)
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 0, out + err)
        self.assertEqual((w.scratch / "body.md").read_text(encoding="utf-8"), notes_text())


class DryRun(Base):
    def test_dry_run_end_to_end(self):
        w = World(self)
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 0, out + err)
        for n in range(1, 9):
            self.assertIn(f"[ {n}]", out)
        st = w.state()
        self.assertEqual(st["releases"], [], "the throw-away draft is deleted")
        self.assertEqual(w.remote_tags(), [], "nothing was pushed")
        self.assertEqual(sh(w.repo, "git", "tag", "--list"), "", "the local throw-away tag is deleted")
        creates = [a for a in st["log"] if a[:2] == ["release", "create"]]
        self.assertEqual(len(creates), 1)
        c = creates[0]
        self.assertIn("-dryrun.", c[2])
        self.assertTrue(c[2].startswith(TAG))
        for flag in ("--draft", "--prerelease", "--latest=false", "--target"):
            self.assertIn(flag, c)
        self.assertNotIn("--verify-tag", c)
        self.assertEqual(c[c.index("--target") + 1], w.rel)
        given = sorted(Path(x).name for x in c if x.endswith((".zip", ".bin", ".elf")) or x.endswith("SHA256SUMS"))
        self.assertEqual(given, sorted([*w.names.values(), "SHA256SUMS"]))
        self.assertEqual(w.ledger.read_text().split("\t")[1:], [SEMVER, "ok\n"])

    def test_sha256sums_cover_the_six_assets(self):
        w = World(self)
        self.assertEqual(w.run("--dry-run")[0], 0)
        lines = (w.scratch / "SHA256SUMS").read_text().splitlines()
        self.assertEqual(len(lines), 6)
        for line in lines:
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((w.build / name).read_bytes()).hexdigest())
        self.assertFalse((w.build / "SHA256SUMS").exists(), "SHA256SUMS belongs in the scratch dir, not build/")

    def test_docs_only_release_commit_accepts_candidate_zip(self):
        w = World(self)                         # zip githash = candidate, release commit = docs-only child
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("ancestor of the release commit", out)

    def test_zip_githash_equal_to_release_commit(self):
        w = World(self)
        w.make_build(w.rel[:7])
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 0, out + err)
        self.assertIn("zip githash is the release commit", out)

    def test_draft_with_a_wrong_size_is_refused_and_removed(self):
        w = World(self)
        w.state(shrink_asset=True)
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 2)
        self.assertIn("sizes differ", err)
        self.assertEqual(w.state()["releases"], [], "the dry run removes its draft even when it refuses")
        self.assertEqual(sh(w.repo, "git", "tag", "--list"), "")
        self.assertEqual(w.ledger.read_text(), "", "a failed dry run is not recorded")

    def test_draft_missing_an_asset_is_refused_and_removed(self):
        w = World(self)
        w.state(drop_asset=True)
        rc, out, err = w.run("--dry-run")
        self.assertEqual(rc, 2)
        self.assertIn("asset names differ", err)
        self.assertEqual(w.state()["releases"], [])

    def test_draft_that_cannot_be_deleted_is_reported(self):
        w = World(self)
        w.state(delete_fails=True)
        rc, out, err = w.run("--dry-run")
        self.assertNotEqual(rc, 0)
        self.assertEqual(sh(w.repo, "git", "tag", "--list"), "")


class RealRun(Base):
    def real(self, w, **state):
        w.two_dry_runs_on_record()
        if state:
            w.state(**state)
        return w.run("--yes-publish")

    def test_real_run_end_to_end(self):
        w = World(self)
        rc, out, err = self.real(w)
        self.assertEqual(rc, 0, out + err)
        for n in range(1, 10):
            self.assertIn(f"[ {n}]", out)
        self.assertIn("PUBLISHED", out)
        # tag: annotated, on the release commit, on origin
        self.assertEqual(sh(w.repo, "git", "cat-file", "-t", TAG), "tag")
        self.assertEqual(sh(w.repo, "git", "rev-parse", f"{TAG}^{{}}"), w.rel)
        self.assertIn(f"refs/tags/{TAG}", w.remote_tags())
        self.assertIn(w.rel, sh(w.repo, "git", "ls-remote", "origin", f"refs/tags/{TAG}^{{}}"))
        # release: published prerelease, still not latest, 7 assets
        st = w.state()
        (r,) = st["releases"]
        self.assertEqual((r["tag"], r["draft"], r["prerelease"]), (TAG, False, True))
        self.assertEqual(st["latest"], "v1.7.10")
        self.assertEqual(len(r["assets"]), 7)
        create = next(a for a in st["log"] if a[:2] == ["release", "create"])
        for flag in ("--draft", "--prerelease", "--latest=false", "--verify-tag"):
            self.assertIn(flag, create)
        self.assertIn(f"-F draft=false", " ".join(next(a for a in st["log"] if a[:2] == ["api", "-X"] and "PATCH" in a)))
        self.assertIn(f"zip sha256: {hashlib.sha256((w.build / w.names['zip']).read_bytes()).hexdigest()}", out)

    def test_published_asset_differs_after_redownload(self):
        w = World(self)
        rc, out, err = self.real(w, corrupt_download=True)
        self.assertEqual(rc, 2)
        self.assertIn("differs from the local file", err)

    def test_beta_that_became_latest_is_reported(self):
        w = World(self)
        rc, out, err = self.real(w, patch_makes_latest=True)
        self.assertEqual(rc, 2)
        self.assertIn("became the latest", err)

    def test_body_too_long_for_gh_is_a_refusal_not_a_crash(self):
        w = World(self)
        w.notes.write_text(notes_text(extra=20000), encoding="utf-8")
        rc, out, err = self.real(w)
        self.assertEqual(rc, 0, out + err)                 # shortened body fits: the fake enforces 125000
        self.assertLessEqual(len(w.state()["releases"][0]["body"]), 125000)


if __name__ == "__main__":
    unittest.main(verbosity=2)

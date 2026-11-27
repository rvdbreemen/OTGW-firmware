#!/usr/bin/env python3
"""Publish a 2.x beta prerelease from a soaked build: one command for Phases 4 to 5 of the beta-release-2x skill.

Input: the release commit, the build directory, the SHA-256 of the soaked binary, the soak verdict file, the
release notes file, the tag and a short theme. Steps (each prints one line, any failure stops before anything
irreversible):

  1. preflight    tag is a beta (not the stable), gh is logged in, the release commit is on origin/dev
  2. verdict      the soak verdict file says PASS for at least 60 minutes (and its sha256, when given, matches)
  3. build files  the six assets exist, the zip githash is the release commit (or an ancestor with no change under
                  src/ or libraries), the .ino.bin sha256 equals the soaked sha256, the flash zip holds the images
  4. body         the notes file has no em dash; a body over 125000 characters becomes the shortened body of the skill
  5. tag          annotated tag on the release commit, pushed (real run only)
  6. SHA256SUMS   written in a scratch directory, never in build/
  7. draft        gh release create --draft --prerelease --latest=false, then names and sizes equal the local files
  8. publish      PATCH the draft to published (prerelease, never latest)          [real run only]
  9. re-download  every asset downloaded again and compared by sha256              [real run only]
 10. report       what the coordinator posts (BlueFlame does the Discord posting; this script posts nothing)

--dry-run runs 1 to 7 against a throw-away tag name (nothing is pushed, the draft is created with --target and
deleted again, the local throw-away tag is removed) and records the success in the dry-run ledger. A real run needs
--yes-publish and refuses until two dry runs on two different betas are in that ledger.

    python scripts/beta_publish.py --tag v2.0.0-beta.440 --release-commit <sha> --build-dir build \\
        --soaked-sha256 <sha256 of the .ino.bin that was soaked> --verdict soak-verdict.txt \\
        --notes docs/releases/RELEASE_NOTES_2.0.0-beta.440.md --theme "web UI polish" --dry-run

Run it from any worktree that has the release commit in its history (git fetch first): the tool only reads history
(rev-parse, merge-base, diff between the build commit and the release commit), so extra commits on the lane it runs
from do not matter; only the diff between the BUILD commit and the RELEASE commit is checked.
--build-commit X names the commit the soaked build was made from (a fix build under a docs-only release commit);
--require-text TEXT (repeatable) makes the run refuse when the final body, shortened or not, lacks TEXT.

Soak verdict file (my format, one `key: value` per line, `#` lines ignored):
    verdict: PASS
    soak_minutes: 60
    sha256: <sha256 of the soaked .ino.bin>        (optional; must equal --soaked-sha256 when present)
"""
import argparse
import datetime
import fnmatch
import hashlib
import os
import re
import shlex
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

REPO_SLUG = "rvdbreemen/OTGW-firmware"
MAX_BODY = 125000                      # GitHub refuses a release body over this many characters (HTTP 422)
MIN_SOAK_MINUTES = 60
EM_DASH = "—"
TARGET = "esp32-combo"
TAG_RE = re.compile(r"^v(\d+\.\d+\.\d+-beta\.\d+)$")
SEMVER_FULL_RE = re.compile(r'#define\s+_SEMVER_FULL\s+"([^"]+)"')


class Refuse(Exception):
    """A gate said no. Nothing irreversible happened before this was raised."""


def step(n, text):
    print(f"[{n:>2}] {text}", flush=True)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def default_ledger_dir():
    base = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return Path(base) / "OTGW-capture"


class Tools:
    """git and gh through subprocess, so tests can put a fake gh in place."""

    def __init__(self, repo, gh_cmd, slug):
        self.repo, self.gh_cmd, self.slug = Path(repo), list(gh_cmd), slug

    def git(self, *args, check=True):
        p = subprocess.run(["git", "-C", str(self.repo), *args], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        if check and p.returncode != 0:
            raise Refuse(f"git {' '.join(args)} failed: {(p.stderr or p.stdout).strip()}")
        return p

    def gh(self, *args, check=True):
        p = subprocess.run([*self.gh_cmd, *args], capture_output=True, text=True, encoding="utf-8",
                           errors="replace", cwd=str(self.repo))
        if check and p.returncode != 0:
            raise Refuse(f"gh {' '.join(args[:3])} failed (exit {p.returncode}): {(p.stderr or p.stdout).strip()}")
        return p

    def api_json(self, path, *extra):
        import json
        return json.loads(self.gh("api", path, *extra).stdout or "null")


# ---------------------------------------------------------------------------------------------------------------
def read_verdict(path, soaked_sha):
    if not Path(path).is_file():
        raise Refuse(f"soak verdict file not found: {path}")
    kv = {}
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        k, v = line.split(":", 1)
        kv[k.strip().lower()] = v.strip()
    if kv.get("verdict", "").upper() != "PASS":
        raise Refuse(f"soak verdict is not PASS (verdict: {kv.get('verdict', '<missing>')!r})")
    try:
        minutes = float(kv.get("soak_minutes", ""))
    except ValueError:
        raise Refuse("soak verdict has no numeric soak_minutes") from None
    if minutes < MIN_SOAK_MINUTES:
        raise Refuse(f"soak was {minutes:g} min, the gate is {MIN_SOAK_MINUTES} min on the published binary")
    if "sha256" in kv and kv["sha256"].lower() != soaked_sha.lower():
        raise Refuse("soak verdict sha256 differs from --soaked-sha256: the verdict is for another binary")
    return minutes


def asset_names(semver_full):
    p = f"OTGW-firmware-{TARGET}-{semver_full}"
    return {"zip": f"{p}-flash.zip", "app": f"{p}.ino.bin", "fs": f"{p}.littlefs.bin",
            "merged": f"{p}-merged-full.bin", "elf": f"{p}.elf", "safeboot": f"OTGW-safeboot-{semver_full}.bin"}


# TASK-3116: a 1.x flash tool (flash_otgw.sh/.bat, flash_esp.py) picks the newest release asset by name and writes it to
# an ESP8266 with --erase-all; esptool does not stop an ESP32-S3 image there. So no loose release asset may carry one
# of the names those tools look for. The app and filesystem images stay inside the flash zip, under their own names
# (the update page of the 2.x betas reads them from there), and are not uploaded one by one.
UPLOADED_KEYS = ("zip", "merged", "elf", "safeboot")
LOOSE_GLOBS = ("*.ino.bin", "*.littlefs.bin", "OTGW-firmware-*.ino.bin", "OTGW-firmware*.littlefs.bin")   # flash_otgw.sh/.bat
LOOSE_SUBSTRINGS = (".ino.bin", "fw.bin", "firmware.bin", "littlefs.bin", "fs.bin", "filesystem.bin")      # 1.x flash_esp.py, lowercased


def loose_asset_violations(names):
    """[(asset name, matcher)] for every release asset name a 1.x flash tool would pick up."""
    hits = []
    for n in names:
        for g in LOOSE_GLOBS:
            if fnmatch.fnmatchcase(n, g):
                hits.append((n, f"glob {g}"))
        for s in LOOSE_SUBSTRINGS:
            if s in n.lower():
                hits.append((n, f"substring {s}"))
    return hits


def find_semver_full(build_dir, semver):
    """The zip in build/ carries <semver>+<githash>; there must be exactly one for this tag."""
    pat = re.compile(rf"^OTGW-firmware-{TARGET}-({re.escape(semver)}\+[0-9a-f]{{7,40}})-flash\.zip$")
    hits = sorted(m.group(1) for f in Path(build_dir).glob(f"OTGW-firmware-{TARGET}-*-flash.zip")
                  if (m := pat.match(f.name)))
    if not hits:
        raise Refuse(f"no {TARGET} flash zip for {semver} in {build_dir}")
    if len(hits) > 1:
        raise Refuse(f"several flash zips for {semver} in {build_dir}: {hits}; remove the stale ones")
    return hits[0]


BUILD_COMMIT_PATHS = ("src/", "libraries/", "scripts/", "build.py", "platformio.ini")


def check_githash(tools, commit_full, githash, build_commit=None):
    """The zip githash is the release commit, or an ancestor of it with nothing changed under src/ or libraries
    (the normal slot release: a docs-only release commit on top of the soaked candidate).

    With an explicit build_commit (--build-commit) the zip githash must belong to that commit and the release
    commit must be it or a descendant with an empty diff under src/, libraries/, scripts/, build.py and
    platformio.ini (a docs-only release commit on top of a fix build)."""
    if build_commit:
        built = tools.git("rev-parse", "--verify", "-q", f"{build_commit}^{{commit}}", check=False)
        if built.returncode != 0:
            raise Refuse(f"--build-commit {build_commit} is not a commit in this repository")
        built_full = built.stdout.strip()
        if not built_full.startswith(githash):
            raise Refuse(f"zip githash {githash} is not the --build-commit {built_full[:9]}: this is not the build "
                         "that was named")
        if built_full == commit_full:
            return "zip githash is the build commit and the release commit"
        anc = tools.git("merge-base", "--is-ancestor", built_full, commit_full, check=False)
        if anc.returncode != 0:
            raise Refuse(f"--build-commit {built_full[:9]} is not an ancestor of the release commit {commit_full[:9]}")
        changed = tools.git("diff", "--name-only", built_full, commit_full, "--", *BUILD_COMMIT_PATHS).stdout.split()
        if changed:
            raise Refuse(f"{changed[0]} changed between the build commit {built_full[:9]} and the release commit "
                         f"{commit_full[:9]} (checked: {' '.join(BUILD_COMMIT_PATHS)}): the binary is not what the "
                         "tag contains")
        return (f"build commit {built_full[:9]} is an ancestor of the release commit, nothing changed under "
                f"{', '.join(BUILD_COMMIT_PATHS)} since")
    if commit_full.startswith(githash):
        return "zip githash is the release commit"
    full = tools.git("rev-parse", "--verify", "-q", f"{githash}^{{commit}}", check=False)
    if full.returncode != 0:
        raise Refuse(f"zip githash {githash} is not a commit in this repository")
    anc = tools.git("merge-base", "--is-ancestor", full.stdout.strip(), commit_full, check=False)
    if anc.returncode != 0:
        raise Refuse(f"zip githash {githash} is not the release commit {commit_full[:9]} nor one of its ancestors")
    changed = tools.git("diff", "--name-only", full.stdout.strip(), commit_full, "--",
                        "src/", "libraries/").stdout.split()
    if changed:
        raise Refuse(f"zip githash {githash} is not the release commit {commit_full[:9]}, and src/ changed in "
                     f"between ({changed[0]} ...): the binary is not what the tag contains")
    return f"zip githash {githash} is an ancestor of the release commit, no change under src/ since"


def check_zip(zip_path, names):
    want = {names["app"], names["fs"], names["merged"]}
    with zipfile.ZipFile(zip_path) as z:
        bad = z.testzip()
        if bad:
            raise Refuse(f"flash zip is corrupt at {bad}")
        have = set(z.namelist())
    missing = sorted(want - have)
    if missing:
        raise Refuse(f"flash zip lacks {missing}")
    for must in ("flash_otgw.bat", "flash_otgw.sh", "otgw_capture.py"):
        if must not in have:
            raise Refuse(f"flash zip lacks {must} (flash scripts and capture tools ship in it)")
    return len(have)


def missing_text(text, body, require_text):
    """The body that goes out keeps the honest part of the notes: the disclaimer block (when the notes have one) and
    every --require-text string (for example the numbers of a check the coordinator named)."""
    if "**Disclaimer" in text and "**Disclaimer" not in body:
        raise Refuse("the shortened body lost the disclaimer block of the notes")
    for needle in require_text:
        if needle not in body:
            raise Refuse(f"the release body lacks the required text: {needle!r}")


def add_appended(body, append_text):
    """Put the appended paragraph in front of the footer (the '## How to report' section), or at the end."""
    add = append_text.strip("\n") + "\n\n"
    i = body.rfind("\n## How to report")
    if i < 0:
        return body.rstrip("\n") + "\n\n" + add.rstrip("\n") + "\n"
    return body[:i + 1] + add + body[i + 1:]


def make_body(notes_path, tag, require_text=(), append_text=None):
    try:
        text = Path(notes_path).read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as e:
        raise Refuse(f"cannot read the release notes {notes_path}: {e}") from None
    if EM_DASH in text:
        i = text.index(EM_DASH)
        raise Refuse(f"release notes contain an em dash (line {text.count(chr(10), 0, i) + 1}); "
                     "the writing rules forbid it")
    if append_text is not None:
        if EM_DASH in append_text:
            raise Refuse("the --append-body text contains an em dash")
        require_text = (*require_text, append_text.strip("\n"))
    full = add_appended(text, append_text) if append_text is not None else text
    if len(full) <= MAX_BODY:
        missing_text(text, full, require_text)
        return full, "full notes file" + (" + appended text" if append_text is not None else "")
    # Shortened body (SKILL.md Phase 4.4): the header through the "What changed since" section, a short
    # "Earlier changes" section linking the full file at the tag, then every section from the 'SAT is ...' heading on.
    heads = [(m.start(), m.group(0)) for m in re.finditer(r"(?m)^## .*$", text)]
    what = next((i for i, (_, h) in enumerate(heads) if h.startswith("## What changed since")), None)
    sat = next((p for p, h in heads if h.startswith("## SAT is ")), None)
    if what is None or sat is None:
        raise Refuse(f"notes are {len(text)} characters (limit {MAX_BODY}) and lack the 'What changed since' or "
                     "'SAT is ...' heading the shortened body is cut at")
    end_what = heads[what + 1][0] if what + 1 < len(heads) else len(text)
    if sat < end_what:
        raise Refuse("the SAT section comes before the end of 'What changed since': cannot cut the body")
    semver = TAG_RE.match(tag).group(1)
    link = (f"https://github.com/{REPO_SLUG}/blob/{tag}/docs/releases/RELEASE_NOTES_{semver}.md")
    earlier = (f"## Earlier changes\n\nThe changes of earlier betas are in the full release notes at the tag: "
               f"{link}\n\n")
    body = text[:end_what].rstrip("\n") + "\n\n" + earlier + text[sat:]
    if append_text is not None:
        body = add_appended(body, append_text)
    if len(body) > MAX_BODY:
        raise Refuse(f"even the shortened body is {len(body)} characters (limit {MAX_BODY})")
    if EM_DASH in body:
        raise Refuse("the shortened body contains an em dash")
    missing_text(text, body, require_text)
    return body, f"shortened ({len(text)} -> {len(body)} characters)" + (" + appended text" if append_text is not None else "")


def read_ledger(path):
    if not Path(path).is_file():
        return set()
    out = set()
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) >= 3 and parts[2] == "ok":
            out.add(parts[1])
    return out


def find_draft(tools, tag):
    """The id of the draft for this tag. A draft that has no pushed tag is only visible through the list."""
    for r in tools.api_json(f"repos/{tools.slug}/releases?per_page=100") or []:
        if r.get("tag_name") == tag and r.get("draft"):
            return r["id"]
    raise Refuse(f"no draft release found for {tag} after gh release create")


def verify_assets(release, local):
    got = {a["name"]: a["size"] for a in release.get("assets", [])}
    want = {n: Path(p).stat().st_size for n, p in local.items()}
    if set(got) != set(want):
        raise Refuse(f"asset names differ: missing {sorted(set(want) - set(got))}, extra {sorted(set(got) - set(want))}")
    wrong = {n: (got[n], want[n]) for n in want if got[n] != want[n]}
    if wrong:
        raise Refuse(f"asset sizes differ (release, local): {wrong}")


def run(args, gh_cmd):
    repo = Path(args.repo).resolve()
    tools = Tools(repo, gh_cmd, args.slug)
    m = TAG_RE.match(args.tag)
    if not m:
        raise Refuse(f"{args.tag!r} is not a 2.x beta tag (v<major>.<minor>.<patch>-beta.<n>); the stable release "
                     "is not this script's job")
    semver = m.group(1)
    if args.latest:
        raise Refuse("--latest: a 2.x beta is never the latest release (dev is the default branch, 1.x stays latest)")
    if not args.dry_run and not args.yes_publish:
        raise Refuse("a real run needs --yes-publish; use --dry-run first")
    ledger = Path(args.ledger) if args.ledger else default_ledger_dir() / "beta-publish-dryruns.tsv"
    if not args.dry_run:
        done = read_ledger(ledger)
        if len(done) < 2:
            raise Refuse(f"{len(done)} successful dry run(s) on different betas in {ledger}, the gate is 2")
        step(0, f"dry-run gate: {len(done)} betas dry-run clean ({', '.join(sorted(done))})")

    # 1. preflight
    commit = tools.git("rev-parse", "--verify", f"{args.release_commit}^{{commit}}").stdout.strip()
    tools.gh("auth", "status")
    tools.git("fetch", "--quiet", "origin", "dev", check=False)
    onrem = tools.git("merge-base", "--is-ancestor", commit, "origin/dev", check=False)
    if onrem.returncode != 0 and not args.dry_run:
        raise Refuse(f"release commit {commit[:9]} is not on origin/dev: push it first")
    step(1, f"preflight ok: {args.tag}, release commit {commit[:9]}"
            f"{'' if onrem.returncode == 0 else ' (not on origin/dev yet, dry run)'}")

    # 2. verdict
    minutes = read_verdict(args.verdict, args.soaked_sha256)
    step(2, f"soak verdict PASS, {minutes:g} min")

    # 3. build files
    semver_full = find_semver_full(args.build_dir, semver)
    githash = semver_full.split("+")[1]
    names = asset_names(semver_full)
    local = {}
    for key, name in names.items():
        p = Path(args.build_dir) / name
        if not p.is_file() or p.stat().st_size == 0:
            raise Refuse(f"missing or empty build file: {p}")
        local[name] = p
    uploads = {names[k]: local[names[k]] for k in UPLOADED_KEYS}
    bad = loose_asset_violations([*uploads, "SHA256SUMS"])
    if bad:
        raise Refuse(f"release asset {bad[0][0]} matches {bad[0][1]}, which an old 1.x flash tool would download and write "
                     "to an ESP8266 (TASK-3116); app and filesystem images stay inside the flash zip")
    got = sha256_file(local[names["app"]])
    if got.lower() != args.soaked_sha256.lower():
        raise Refuse(f"{names['app']} sha256 {got} differs from the soaked sha256 {args.soaked_sha256}: "
                     "this is not the binary that was soaked")
    how = check_githash(tools, commit, githash, args.build_commit)
    n_in_zip = check_zip(local[names["zip"]], names)
    step(3, f"build files ok: {len(uploads)} assets to upload, .ino.bin (stays in the zip) equals the soaked sha256, {how}, flash zip holds {n_in_zip} files")

    # 4. body
    append_text = None
    if args.append_body:
        try:
            append_text = Path(args.append_body).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as e:
            raise Refuse(f"cannot read --append-body {args.append_body}: {e}") from None
        if not append_text.strip():
            raise Refuse(f"--append-body {args.append_body} is empty")
    body, how_body = make_body(args.notes, args.tag, args.require_text, append_text)
    step(4, f"release body ok: {how_body}, no em dash, {len(body)} characters")

    scratch = Path(args.scratch) if args.scratch else Path(tempfile.mkdtemp(prefix="beta-publish-"))
    scratch.mkdir(parents=True, exist_ok=True)
    if Path(args.build_dir).resolve() in scratch.resolve().parents or scratch.resolve() == Path(args.build_dir).resolve():
        raise Refuse("the scratch directory must not be inside the build directory")
    body_file = scratch / "body.md"
    body_file.write_text(body, encoding="utf-8", newline="\n")
    sums = scratch / "SHA256SUMS"
    sums.write_text("".join(f"{sha256_file(p)}  {n}\n" for n, p in uploads.items()), encoding="utf-8", newline="\n")
    assets_all = {**uploads, "SHA256SUMS": sums}

    use_tag = args.tag if not args.dry_run else f"{args.tag}-dryrun.{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}"
    tag_msg = f"{semver}: {args.theme}"

    # 5. tag
    if not args.dry_run:
        have = tools.git("rev-parse", "-q", "--verify", f"refs/tags/{use_tag}", check=False)
        if have.returncode == 0:
            if tools.git("rev-parse", f"{use_tag}^{{}}").stdout.strip() != commit:
                raise Refuse(f"tag {use_tag} exists and points elsewhere")
        else:
            remote_tag = tools.git("ls-remote", "--tags", "origin", f"refs/tags/{use_tag}", check=False).stdout.strip()
            if remote_tag:
                raise Refuse(f"tag {use_tag} exists on origin: a release with it exists or was burned; use the next beta")
            tools.git("tag", "-a", use_tag, commit, "-m", tag_msg)
    else:
        tools.git("tag", "-a", use_tag, commit, "-m", tag_msg)
    try:
        pointed = tools.git("rev-parse", f"{use_tag}^{{}}").stdout.strip()
        if pointed != commit:
            raise Refuse(f"tag {use_tag} points at {pointed[:9]}, not at the release commit {commit[:9]}")
        if not args.dry_run:
            tools.git("push", "origin", f"refs/tags/{use_tag}")
        step(5, f"tag {use_tag} -> {commit[:9]}{' (local only, throw-away)' if args.dry_run else ' pushed'}")
        step(6, f"SHA256SUMS written in {scratch} ({len(uploads)} lines)")

        # 7. draft
        create = ["release", "create", use_tag, "--repo", tools.slug, "--draft", "--prerelease", "--latest=false",
                  "--title", f"{use_tag} ({args.theme})", "--notes-file", str(body_file)]
        create += ["--target", commit] if args.dry_run else ["--verify-tag"]
        draft_id = None
        try:
            tools.gh(*create, *[str(p) for p in assets_all.values()])
            draft_id = find_draft(tools, use_tag)
            rel = tools.api_json(f"repos/{tools.slug}/releases/{draft_id}")
            if not (rel.get("draft") and rel.get("prerelease")):
                raise Refuse(f"draft flags wrong: draft={rel.get('draft')} prerelease={rel.get('prerelease')}")
            verify_assets(rel, assets_all)
            step(7, f"draft ok (id {draft_id}): prerelease, not latest, {len(rel['assets'])} assets, sizes equal the local files")
            if args.dry_run:
                tools.gh("api", "-X", "DELETE", f"repos/{tools.slug}/releases/{draft_id}")
                gone = [r for r in tools.api_json(f"repos/{tools.slug}/releases?per_page=100")
                        if r.get("id") == draft_id]
                if gone:
                    raise Refuse(f"throw-away draft {draft_id} is still there after DELETE")
                step(8, "throw-away draft deleted; nothing pushed, nothing published")
                draft_id = None
        except Refuse:
            if args.dry_run and draft_id is not None:
                tools.gh("api", "-X", "DELETE", f"repos/{tools.slug}/releases/{draft_id}", check=False)
            raise
        if args.dry_run:
            ledger.parent.mkdir(parents=True, exist_ok=True)
            with open(ledger, "a", encoding="utf-8", newline="\n") as f:
                f.write(f"{datetime.datetime.now(datetime.timezone.utc):%Y-%m-%dT%H:%M:%SZ}\t{semver}\tok\n")
            print(f"DRY RUN OK for {args.tag}. Ledger: {ledger} ({len(read_ledger(ledger))} beta(s) clean)")
            return 0

        # 8. publish: prerelease, never latest
        tools.gh("api", "-X", "PATCH", f"repos/{tools.slug}/releases/{draft_id}", "-F", "draft=false",
                 "-F", "prerelease=true", "-f", "make_latest=false")
        rel = tools.api_json(f"repos/{tools.slug}/releases/{draft_id}")
        if rel.get("draft") or not rel.get("prerelease") or rel.get("tag_name") != use_tag:
            raise Refuse(f"published release flags wrong: {rel.get('draft')=} {rel.get('prerelease')=} {rel.get('tag_name')=}")
        latest = tools.api_json(f"repos/{tools.slug}/releases/latest")
        if latest and latest.get("tag_name") == use_tag:
            raise Refuse(f"{use_tag} became the latest release: fix this on GitHub now (edit: Set as latest = off)")
        step(8, f"published as prerelease; latest is still {latest.get('tag_name') if latest else '<none>'}")

        # 9. re-download and compare
        dl = scratch / "redownload"
        dl.mkdir(exist_ok=True)
        tools.gh("release", "download", use_tag, "--repo", tools.slug, "--dir", str(dl), "--clobber")
        for n, p in assets_all.items():
            f = dl / n
            if not f.is_file():
                raise Refuse(f"re-download lacks {n}")
            if sha256_file(f) != sha256_file(p):
                raise Refuse(f"re-downloaded {n} differs from the local file: the published asset is wrong, ship the next beta")
        step(9, f"re-downloaded {len(assets_all)} assets, every sha256 equals the local file")
    finally:
        if args.dry_run:
            tools.git("tag", "-d", use_tag, check=False)

    # 10. what to post (this script posts nothing)
    post_dir = default_ledger_dir()
    key = semver
    keys = post_dir / "published-tags.txt"
    posted = keys.is_file() and key in keys.read_text(encoding="utf-8").split()
    print("\n=== PUBLISHED. For the coordinator / BlueFlame to post ===")
    print(f"release   : https://github.com/{tools.slug}/releases/tag/{use_tag}")
    print(f"tag       : {use_tag} -> {commit[:9]}   zip githash {githash}")
    print(f"zip sha256: {sha256_file(local[names['zip']])}  ({names['zip']})")
    print(f"ledger    : zip key {key} {'FOUND in published-tags.txt: post a short note with the release link '
          'and the earlier zip message id from published-posts.tsv, do not run discord-post-build.sh' if posted else 'not in published-tags.txt: the zip is not posted yet (discord-post-build.sh from a worktree at the tag commit)'}")
    print("then    : #beta-testing announcement per SKILL.md Phase 6.2; read every post back")
    return 0


def main(argv=None, gh_cmd=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--tag", required=True, help="v2.0.0-beta.N")
    ap.add_argument("--release-commit", required=True)
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--soaked-sha256", required=True, help="sha256 of the .ino.bin that passed the soak")
    ap.add_argument("--verdict", required=True, help="soak verdict file (verdict: PASS, soak_minutes: >=60)")
    ap.add_argument("--notes", required=True, help="docs/releases/RELEASE_NOTES_<semver>.md")
    ap.add_argument("--theme", required=True, help="short theme for the release title and the tag message")
    ap.add_argument("--build-commit", default=None,
                    help="the commit the soaked build was made from; the zip githash must be it and the release "
                         "commit must add nothing under src/, libraries/, scripts/, build.py, platformio.ini")
    ap.add_argument("--require-text", action="append", default=[],
                    help="text that must be in the release body (repeatable), for example the numbers of a check")
    ap.add_argument("--append-body", default=None,
                    help="file whose text is appended to the body AFTER the shortening, in front of the footer "
                         "('## How to report'); it is also a required text, so it cannot be dropped")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--yes-publish", action="store_true", help="required for a real (non dry) run")
    ap.add_argument("--latest", action="store_true", help="refused: a beta is never the latest release")
    ap.add_argument("--repo", default=".", help="the git worktree at (or containing) the release commit")
    ap.add_argument("--slug", default=REPO_SLUG)
    ap.add_argument("--scratch", default=None, help="scratch directory (default: a new temp dir)")
    ap.add_argument("--ledger", default=None, help="dry-run ledger file (default: <OTGW-capture>/beta-publish-dryruns.tsv)")
    ap.add_argument("--gh", default="gh")
    a = ap.parse_args(argv)
    try:
        return run(a, gh_cmd or shlex.split(a.gh, posix=(os.name != "nt")))
    except Refuse as e:
        print(f"REFUSED: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())

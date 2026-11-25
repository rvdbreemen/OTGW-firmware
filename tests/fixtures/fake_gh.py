#!/usr/bin/env python3
"""A fake `gh` for tests/test_beta_publish.py (TASK-3090): the few subcommands scripts/beta_publish.py calls.

State lives in the directory named by FAKE_GH_DIR: state.json (releases, latest, switches) and store/<id>/<asset>.
Switches a test can set in state.json to make the fake misbehave like GitHub can:
  auth_fail, drop_asset (the last asset is not stored), shrink_asset (the first asset is cut short),
  corrupt_download (the first asset comes back with a changed byte), patch_makes_latest, delete_fails.
It enforces what GitHub enforces: a body over 125000 characters is HTTP 422, --verify-tag needs the tag on origin.
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

D = Path(os.environ["FAKE_GH_DIR"])
S = D / "state.json"


def load():
    return json.loads(S.read_text()) if S.exists() else {}


def save(st):
    S.write_text(json.dumps(st))


def out(obj):
    print(json.dumps(obj))


def rel_json(st, r):
    return {"id": r["id"], "tag_name": r["tag"], "draft": r["draft"], "prerelease": r["prerelease"],
            "assets": [{"name": n, "size": s} for n, s in r["assets"].items()]}


def main(a):
    st = load()
    st.setdefault("releases", [])
    st.setdefault("next_id", 100)
    st.setdefault("latest", "v1.7.10")
    st.setdefault("log", [])
    st["log"].append(a)
    if a[:2] == ["auth", "status"]:
        save(st)
        if st.get("auth_fail"):
            print("not logged in", file=sys.stderr)
            return 1
        return 0
    if a[0] == "api":
        method, path, fields = "GET", None, {}
        i = 1
        while i < len(a):
            if a[i] == "-X":
                method = a[i + 1]
                i += 2
            elif a[i] in ("-F", "-f"):
                k, v = a[i + 1].split("=", 1)
                fields[k] = v
                i += 2
            else:
                path = a[i]
                i += 1
        rest = path.split("/", 3)[3] if path.count("/") >= 3 else ""   # after repos/<owner>/<repo>/
        if rest.startswith("releases?"):
            save(st)
            out([rel_json(st, r) for r in reversed(st["releases"])])
            return 0
        if rest == "releases/latest":
            save(st)
            out({"tag_name": st["latest"]})
            return 0
        rid = int(rest.split("/")[1])
        r = next((x for x in st["releases"] if x["id"] == rid), None)
        if r is None:
            save(st)
            print("HTTP 404", file=sys.stderr)
            return 1
        if method == "DELETE":
            if st.get("delete_fails"):
                save(st)
                return 1
            st["releases"] = [x for x in st["releases"] if x["id"] != rid]
            shutil.rmtree(D / "store" / str(rid), ignore_errors=True)
            save(st)
            return 0
        if method == "PATCH":
            r["draft"] = fields.get("draft") == "true"
            r["prerelease"] = fields.get("prerelease") == "true"
            if not r["draft"] and (fields.get("make_latest") != "false" or st.get("patch_makes_latest")):
                st["latest"] = r["tag"]
            save(st)
            out(rel_json(st, r))
            return 0
        save(st)
        out(rel_json(st, r))
        return 0
    if a[:2] == ["release", "create"]:
        tag, i, assets, flags, notes = a[2], 3, [], set(), None
        while i < len(a):
            x = a[i]
            if x in ("--repo", "--title", "--target"):
                flags.add(f"{x}={a[i + 1]}")
                i += 2
            elif x == "--notes-file":
                notes = a[i + 1]
                i += 2
            elif x.startswith("--"):
                flags.add(x)
                i += 1
            else:
                assets.append(x)
                i += 1
        if any(r["tag"] == tag for r in st["releases"]):
            print("HTTP 422: tag_name already exists", file=sys.stderr)
            save(st)
            return 1
        if "--verify-tag" in flags:
            p = subprocess.run(["git", "ls-remote", "--tags", "origin", f"refs/tags/{tag}"], capture_output=True, text=True)
            if not p.stdout.strip():
                print(f"tag {tag} not found on origin", file=sys.stderr)
                save(st)
                return 1
        body = Path(notes).read_text(encoding="utf-8")
        if len(body) > 125000:
            print("HTTP 422: body is too long (maximum is 125000 characters)", file=sys.stderr)
            save(st)
            return 1
        rid = st["next_id"]
        st["next_id"] += 1
        (D / "store" / str(rid)).mkdir(parents=True, exist_ok=True)
        stored = {}
        keep = assets[:-1] if st.get("drop_asset") else assets
        for n, p in enumerate(keep):
            data = Path(p).read_bytes()
            if st.get("shrink_asset") and n == 0:
                data = data[:-1]
            (D / "store" / str(rid) / Path(p).name).write_bytes(data)
            stored[Path(p).name] = len(data)
        st["releases"].append({"id": rid, "tag": tag, "draft": "--draft" in flags, "prerelease": "--prerelease" in flags,
                               "assets": stored, "body": body, "flags": sorted(flags), "assets_given": [Path(p).name for p in assets]})
        save(st)
        print(f"https://github.com/x/y/releases/tag/untagged-{rid}")
        return 0
    if a[:2] == ["release", "download"]:
        tag, dest = a[2], None
        for i, x in enumerate(a):
            if x == "--dir":
                dest = Path(a[i + 1])
        r = next((x for x in st["releases"] if x["tag"] == tag and not x["draft"]), None)
        if r is None:
            save(st)
            print("release not found", file=sys.stderr)
            return 1
        names = sorted(r["assets"])
        for n, name in enumerate(names):
            data = (D / "store" / str(r["id"]) / name).read_bytes()
            if st.get("corrupt_download") and n == 0:
                data = bytes([data[0] ^ 1]) + data[1:]
            (dest / name).write_bytes(data)
        save(st)
        return 0
    save(st)
    print(f"fake gh: unsupported {a}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

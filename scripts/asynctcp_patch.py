"""ADR-232 / TASK-1746.1: apply upstream AsyncTCP commit 7befe25 to the pinned 3.4.10 copy.

The registry copy of AsyncTCP 3.4.10 in .pio/libdeps/<env>/AsyncTCP ends tcp_accept() in
`tcp_abort(pcb); return ERR_OK;`, which sends lwIP into a freed pcb. Upstream fixed it in
7befe25 (https://github.com/ESP32Async/AsyncTCP/commit/7befe25a549c71b0fc7794596ba9d21fa6fd9d1f),
not yet in a release. scripts/patch_pio_libs.py calls ensure_patched() before every esp32 build.

The file is identified by its sha256 with line endings normalized:
  BASE_SHA256     AsyncTCP v3.4.10 src/AsyncTCP.cpp       -> apply the stored upstream patch
  PATCHED_SHA256  v3.4.10 + 7befe25                        -> already patched, nothing to do
  anything else   -> PatchError: a changed pin or a moved upstream file must not build unpatched.

Pure Python (no git, no SCons), so tests/test_asynctcp_accept_patch.py can call it directly.
"""
import hashlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
PATCH_FILE = HERE / "patches" / "asynctcp-3.4.10-7befe25.patch"
UPSTREAM_COMMIT = "7befe25a549c71b0fc7794596ba9d21fa6fd9d1f"
BASE_SHA256 = "10b7124eea6b9b0b8734d058d480219f81e2663ddfd958eb6be27241bac048da"
PATCHED_SHA256 = "c786989e25a4372d7a0c855910d84a823c1e3a6e011c5b7a052a31214044e4e5"


class PatchError(RuntimeError):
    pass


def _sha(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _hunks(patch_text):
    """Yield (old_block, new_block) for each hunk of the single-file unified diff."""
    lines = patch_text.replace("\r\n", "\n").split("\n")
    i = 0
    while i < len(lines):
        if lines[i].startswith("@@"):
            old, new = [], []
            i += 1
            while i < len(lines) and not lines[i].startswith(("@@", "-- ", "diff ")):
                ln = lines[i]
                if ln.startswith(" ") or ln == "":
                    old.append(ln[1:]); new.append(ln[1:])
                elif ln.startswith("-"):
                    old.append(ln[1:])
                elif ln.startswith("+"):
                    new.append(ln[1:])
                i += 1
            while old and new and old[-1] == "" and new[-1] == "":   # trailing blank padding
                old.pop(); new.pop()
            yield "\n".join(old) + "\n", "\n".join(new) + "\n"
        else:
            i += 1


def apply_patch(text, patch_text):
    """Apply every hunk to text (LF). Each hunk's old block must occur exactly once."""
    for old, new in _hunks(patch_text):
        n = text.count(old)
        if n != 1:
            raise PatchError(f"hunk context found {n} times, expected once")
        text = text.replace(old, new)
    return text


def ensure_patched(cpp_path, patch_file=PATCH_FILE):
    """Bring cpp_path to the patched state. Returns 'applied' or 'already'; raises PatchError otherwise."""
    p = Path(cpp_path)
    if not p.is_file():
        raise PatchError(f"{p} not found (is AsyncTCP 3.4.10 still in lib_deps?)")
    raw = p.read_bytes().decode("utf-8")
    crlf = "\r\n" in raw
    text = raw.replace("\r\n", "\n")
    h = _sha(text)
    if h == PATCHED_SHA256:
        return "already"
    if h != BASE_SHA256:
        raise PatchError(
            f"{p} is neither AsyncTCP 3.4.10 nor 3.4.10+7befe25 (sha256 {h[:12]}): "
            "the pin or upstream moved; re-check ADR-232 before building")
    patched = apply_patch(text, Path(patch_file).read_text(encoding="utf-8"))
    if _sha(patched) != PATCHED_SHA256:
        raise PatchError(f"patched {p} has sha256 {_sha(patched)[:12]}, expected {PATCHED_SHA256[:12]}")
    p.write_bytes((patched.replace("\n", "\r\n") if crlf else patched).encode("utf-8"))
    return "applied"


def libdeps_cpp(project_dir, pioenv):
    return Path(project_dir) / ".pio" / "libdeps" / pioenv / "AsyncTCP" / "src" / "AsyncTCP.cpp"


if __name__ == "__main__":
    import sys
    print(ensure_patched(sys.argv[1]))

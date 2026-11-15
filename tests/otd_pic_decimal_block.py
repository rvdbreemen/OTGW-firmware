"""Shared helper for host tests that cut OT-Direct command branches (TASK-1804 B8/B9).

MH=/MW=/M2= and VS= call otdPicDecimalArg() (the PIC GetDecimalArg, block "// --- otdPicDecimal begin/end" in
src/OTGW-firmware/OTDirect.ino). A harness that cuts those branches needs the block too; with_block() puts it, and
<cctype> for isdigit(), in front of the generated source. On an older OTDirect.ino without the block nothing is added,
so old-vs-fix runs keep compiling the old branches as they were.
"""
import os
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def _source():
    p = Path(os.environ.get("OTGW_OTD_INO", REPO / "src" / "OTGW-firmware" / "OTDirect.ino"))
    return p.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def with_block(generated):
    m = re.search(r"// --- otdPicDecimal begin\n(.*?)// --- otdPicDecimal end", _source(), re.S)
    if not m:
        return generated
    prefix = "#include <cctype>\n"
    # after the harness's own includes, so uint8_t is known
    last_include = generated.rfind("#include <")
    if last_include >= 0:
        cut = generated.index("\n", last_include) + 1
        return generated[:cut] + prefix + m.group(1) + "\n" + generated[cut:]
    return "#include <cstdint>\n" + prefix + m.group(1) + "\n" + generated

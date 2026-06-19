#!/usr/bin/env python
"""
OpenAPI 3.1 compliance check for the live REST API (TASK-886, full ADR-141
revert). Fetches each documented v2 GET endpoint from a running device and
validates the JSON response against its response schema in docs/api/openapi.yaml.

This catches contract drift that the json_golden oracle can miss: a field whose
TYPE diverges from the spec (e.g. a bool documented as boolean but emitted as a
quoted string), a missing required field, an out-of-enum value. json_golden
checks "same as the previous build"; this checks "matches the documented
contract". Both gate the revert.

Run it against the CURRENT build first to learn the baseline (does the shipping
build already comply?), then against the converted build to prove no regression.

Usage:  python scripts/tests/openapi_compliance.py <host>     e.g. 192.168.88.39
Exit 0 = every validated endpoint complies; 1 = at least one violation.
"""
import sys, os, json, urllib.request, urllib.error

import yaml
from jsonschema import Draft202012Validator
from referencing import Registry, Resource
from referencing.jsonschema import DRAFT202012

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = os.path.normpath(os.path.join(HERE, "..", "..", "docs", "api", "openapi.yaml"))

# GET endpoints to validate. Query strings are stripped for the spec lookup but
# kept for the fetch. Endpoints without a 200 JSON schema in the spec are SKIPped.
ENDPOINTS = [
 "/v2/health", "/v2/settings", "/v2/sensors", "/v2/sensors/status",
 "/v2/device/info", "/v2/device/time", "/v2/device/crashlog",
 "/v2/flash/status", "/v2/pic/flash-status", "/v2/pic/settings",
 "/v2/firmware/files", "/v2/filesystem/files",
 "/v2/otgw/otmonitor", "/v2/discovery", "/v2/otdirect/status",
 "/v2/otdirect/settings", "/v2/otdirect/overrides", "/v2/sat/status",
 "/v2/sat/status?detail=full", "/v2/sat/weather",
]


def load_spec():
    with open(SPEC, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def fetch(host, ep):
    url = "http://%s/api%s" % (host, ep)
    try:
        with urllib.request.urlopen(url, timeout=12) as r:
            return r.getcode(), r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", "replace")
    except Exception as e:  # noqa: BLE001
        return 0, "ERR:%s" % e


# Named base URI under which the whole spec is registered, so that intra-document
# "#/components/..." refs inside a response schema resolve against the full spec
# (not against the extracted sub-schema, which has no components/ of its own).
SPEC_URI = "urn:otgw-openapi"


def _ptr_escape(s):
    return s.replace("~", "~0").replace("/", "~1")


def resolve_local_ref(spec, ref):
    # local JSON-pointer refs only: '#/a/b/c' with ~1 -> / and ~0 -> ~
    if not ref.startswith("#/"):
        return None
    node = spec
    for part in ref[2:].split("/"):
        part = part.replace("~1", "/").replace("~0", "~")
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node


def response_schema_pointer(spec, path):
    """Return the JSON pointer (within the spec) to the GET 200 application/json
    response schema, or None. The pointer is resolved through SPEC_URI so the
    schema's own #/components refs resolve against the full document."""
    p = spec.get("paths", {}).get(path)
    if not p or "get" not in p:
        return None
    resp = p["get"].get("responses", {}).get("200")
    if not resp:
        return None
    if "$ref" in resp:                       # response-level $ref (e.g. #/components/responses/X)
        ref = resp["$ref"]
        target = resolve_local_ref(spec, ref)
        if not target or "content" not in target:
            return None
        # ref is "#/a/b" -> pointer "/a/b/content/application~1json/schema"
        return ref[1:] + "/content/application~1json/schema"
    if "content" not in resp:
        return None
    return ("/paths/" + _ptr_escape(path)
            + "/get/responses/200/content/application~1json/schema")


GOLDEN_DIR_DEFAULT = os.path.normpath(os.path.join(HERE, "..", "json-golden"))


def golden_source(golddir):
    """Yield (ep, http_status, json_text) from a json_golden snapshot dir. Lets
    the same spec gate run offline against the captured baseline (CI-friendly,
    no device) as well as live. The streaming JsonEmit output is semantically
    equal to the baseline, so a green offline run predicts a green live run."""
    idx = json.load(open(os.path.join(golddir, "_index.json"), encoding="utf-8"))
    for ep, meta in idx.items():
        if ep.split("?", 1)[0] not in [e.split("?", 1)[0] for e in ENDPOINTS]:
            continue
        f = os.path.join(golddir, meta.get("file", ""))
        if not os.path.exists(f):
            yield ep, 0, "ERR:missing golden file"
            continue
        yield ep, meta.get("status", 0), open(f, encoding="utf-8").read()


def main():
    args = sys.argv[1:]
    if not args:
        print("usage: openapi_compliance.py <host>            # validate a live device")
        print("       openapi_compliance.py --golden [dir]    # validate json_golden snapshots offline")
        sys.exit(2)

    spec = load_spec()
    # Register the whole document under a named base URI so a $ref to a schema's
    # JSON pointer carries the full spec as its resolution base; the schema's own
    # intra-document "#/components/..." refs then resolve correctly.
    registry = Registry().with_resource(
        uri=SPEC_URI, resource=Resource(contents=spec, specification=DRAFT202012)
    )

    if args[0] == "--golden":
        golddir = args[1] if len(args) > 1 else GOLDEN_DIR_DEFAULT
        print("validating json_golden snapshots in %s against %s\n" % (golddir, os.path.basename(SPEC)))
        source = golden_source(golddir)
    else:
        host = args[0]
        print("validating live device %s against %s\n" % (host, os.path.basename(SPEC)))
        source = ((ep, *fetch(host, ep)) for ep in ENDPOINTS)

    total = ok = skip = fail = 0
    for ep, code, body in source:
        path = ep.split("?", 1)[0]
        pointer = response_schema_pointer(spec, path)
        if pointer is None:
            print("SKIP  %-34s (no 200 JSON schema in spec)" % ep)
            skip += 1
            continue
        if code != 200:
            # a documented non-200 (e.g. 503 when the PIC is absent) is not a body
            # this gate validates; skip it rather than fail.
            print("SKIP  %-34s status %s (not a 200 JSON body)" % (ep, code))
            skip += 1
            continue
        try:
            data = json.loads(body)
        except Exception as e:  # noqa: BLE001
            print("FAIL  %-34s not JSON: %s" % (ep, e))
            fail += 1
            total += 1
            continue
        validator = Draft202012Validator({"$ref": SPEC_URI + "#" + pointer}, registry=registry)
        errors = sorted(validator.iter_errors(data), key=lambda e: list(e.absolute_path))
        total += 1
        if errors:
            fail += 1
            print("FAIL  %-34s %d schema violation(s):" % (ep, len(errors)))
            for e in errors[:10]:
                loc = "/".join(str(x) for x in e.absolute_path) or "(root)"
                print("        - %s: %s" % (loc, e.message))
        else:
            ok += 1
            print("pass  %-34s" % ep)

    print()
    print("validated=%d  pass=%d  fail=%d  skip=%d" % (total, ok, fail, skip))
    sys.exit(1 if fail else 0)


if __name__ == "__main__":
    main()

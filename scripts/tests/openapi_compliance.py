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


def response_schema_for(spec, path):
    p = spec.get("paths", {}).get(path)
    if not p or "get" not in p:
        return None
    resp = p["get"].get("responses", {}).get("200")
    if not resp:
        return None
    if "$ref" in resp:                     # response-level $ref
        resp = resolve_local_ref(spec, resp["$ref"])
    content = (resp or {}).get("content", {}).get("application/json", {})
    return content.get("schema")


def main():
    if len(sys.argv) < 2:
        print("usage: openapi_compliance.py <host>")
        sys.exit(2)
    host = sys.argv[1]
    spec = load_spec()

    # Register the whole document under an empty base URI so intra-document
    # "#/components/..." refs inside the response schemas resolve.
    registry = Registry().with_resource(
        uri="", resource=Resource(contents=spec, specification=DRAFT202012)
    )

    total = ok = skip = fail = 0
    for ep in ENDPOINTS:
        path = ep.split("?", 1)[0]
        schema = response_schema_for(spec, path)
        if schema is None:
            print("SKIP  %-34s (no 200 JSON schema in spec)" % ep)
            skip += 1
            continue
        code, body = fetch(host, ep)
        if code != 200:
            print("FAIL  %-34s HTTP %s" % (ep, code))
            fail += 1
            total += 1
            continue
        try:
            data = json.loads(body)
        except Exception as e:  # noqa: BLE001
            print("FAIL  %-34s not JSON: %s" % (ep, e))
            fail += 1
            total += 1
            continue
        validator = Draft202012Validator(schema, registry=registry)
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

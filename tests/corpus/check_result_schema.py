#!/usr/bin/env python3
"""Validate run_matrix.py result records against tests/corpus/result.schema.json (T014).

This is a *shape* check, not a correctness check: it proves the runner emits the
fixed, versioned schema T016 relies on. It has no third-party dependency and
implements only the subset of JSON Schema the record actually uses (required,
type, enum, additionalProperties=false, minimum, minLength).

Exit codes:
    0  every supplied result file validates
    1  at least one file failed validation
    2  usage error
"""

from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = os.path.join(HERE, "result.schema.json")


def _type_ok(value, spec) -> bool:
    specs = spec if isinstance(spec, list) else [spec]
    for one in specs:
        if one == "object" and isinstance(value, dict):
            return True
        if one == "array" and isinstance(value, list):
            return True
        if one == "string" and isinstance(value, str):
            return True
        if one == "integer" and isinstance(value, int) and not isinstance(value, bool):
            return True
        if one == "number" and isinstance(value, (int, float)) and not isinstance(value, bool):
            return True
        if one == "boolean" and isinstance(value, bool):
            return True
        if one == "null" and value is None:
            return True
    return False


def _validate(value, schema, path, errors):
    if "type" in schema and not _type_ok(value, schema["type"]):
        errors.append("%s: type %s is not %s" % (path, type(value).__name__, schema["type"]))
        return
    if "enum" in schema and value not in schema["enum"]:
        errors.append("%s: %r not in enum %s" % (path, value, schema["enum"]))
    if isinstance(value, str):
        if "minLength" in schema and len(value) < schema["minLength"]:
            errors.append("%s: string shorter than %d" % (path, schema["minLength"]))
    if isinstance(value, int) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            errors.append("%s: %d < minimum %d" % (path, value, schema["minimum"]))
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                errors.append("%s: missing required key %r" % (path, key))
        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for key in value:
                if key not in props:
                    errors.append("%s: unexpected key %r" % (path, key))
        for key, sub in props.items():
            if key in value:
                _validate(value[key], sub, "%s.%s" % (path, key), errors)
    if isinstance(value, list) and "items" in schema:
        for i, item in enumerate(value):
            _validate(item, schema["items"], "%s[%d]" % (path, i), errors)


def validate_file(schema: dict, path: str) -> list[str]:
    with open(path, "r", encoding="utf-8") as fh:
        record = json.load(fh)
    errors: list[str] = []
    _validate(record, schema, os.path.basename(path), errors)
    return errors


def main(argv: list[str]) -> int:
    if not argv:
        print("usage: check_result_schema.py <result.json> [...]", file=sys.stderr)
        return 2
    with open(SCHEMA, "r", encoding="utf-8") as fh:
        schema = json.load(fh)
    failures = 0
    for path in argv:
        if not os.path.isfile(path):
            print("FAIL: result file not found: %s" % path)
            failures += 1
            continue
        errors = validate_file(schema, path)
        if errors:
            for error in errors:
                print("FAIL: %s" % error)
            failures += 1
        else:
            print("ok: %s" % path)
    if failures:
        print("RESULT SCHEMA FAILED: %d file(s)" % failures)
        return 1
    print("RESULT SCHEMA OK: %d file(s)" % len(argv))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

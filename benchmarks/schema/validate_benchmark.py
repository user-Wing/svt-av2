#!/usr/bin/env python3
"""Minimal, dependency-free JSON Schema validator for SVT-AV2 benchmark records.

Owner: T002. The project forbids third-party runtime dependencies, so this
implements only the subset of JSON Schema draft-07 used by
benchmarks/schema/benchmark.schema.json: type, required, properties,
additionalProperties, enum, minimum, minLength and items.

Usage: python validate_benchmark.py <record.json> [<schema.json>]
Exit: 0 valid, 1 invalid, 2 usage error.
"""

from __future__ import annotations

import json
import os
import sys

TYPES = {
    "object": dict,
    "array": list,
    "string": str,
    "integer": int,
    "number": (int, float),
    "boolean": bool,
    "null": type(None),
}


def _type_ok(value, expected) -> bool:
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "boolean":
        return isinstance(value, bool)
    py = TYPES.get(expected)
    return py is not None and isinstance(value, py)


def validate(value, schema, path: str, errors: list) -> None:
    if "type" in schema:
        expected = schema["type"]
        options = expected if isinstance(expected, list) else [expected]
        if not any(_type_ok(value, opt) for opt in options):
            errors.append("%s: expected type %s" % (path, expected))
            return

    if "enum" in schema and value not in schema["enum"]:
        errors.append("%s: %r not in enum %r" % (path, value, schema["enum"]))

    if isinstance(value, str) and "minLength" in schema and len(value) < schema["minLength"]:
        errors.append("%s: string shorter than minLength %d" % (path, schema["minLength"]))

    if isinstance(value, (int, float)) and not isinstance(value, bool) and "minimum" in schema:
        if value < schema["minimum"]:
            errors.append("%s: %r < minimum %r" % (path, value, schema["minimum"]))

    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                errors.append("%s: missing required property '%s'" % (path, key))
        props = schema.get("properties", {})
        if schema.get("additionalProperties", True) is False:
            for key in value:
                if key not in props:
                    errors.append("%s: additional property '%s' not allowed" % (path, key))
        for key, subschema in props.items():
            if key in value:
                validate(value[key], subschema, "%s.%s" % (path, key), errors)

    if isinstance(value, list) and "items" in schema:
        for index, item in enumerate(value):
            validate(item, schema["items"], "%s[%d]" % (path, index), errors)


def _semantic_errors(record: dict) -> list:
    """Rules the structural schema cannot express.

    The central M0 anti-fabrication rule: a parse_init record performs no
    encoding, so encoded_frames and fps must be null.
    """
    errors = []
    if record.get("mode") == "parse_init":
        encoded = record.get("encoded", {})
        if encoded.get("encoded_frames") is not None:
            errors.append("$.encoded.encoded_frames: must be null when mode=parse_init")
        if encoded.get("fps") is not None:
            errors.append("$.encoded.fps: must be null when mode=parse_init (no encoding happened)")
    return errors


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    record_path = argv[0]
    schema_path = argv[1] if len(argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "benchmark.schema.json")
    with open(schema_path, "r", encoding="utf-8") as fh:
        schema = json.load(fh)
    with open(record_path, "r", encoding="utf-8") as fh:
        record = json.load(fh)

    errors: list[str] = []
    validate(record, schema, "$", errors)
    errors.extend(_semantic_errors(record))
    if errors:
        for error in errors:
            print("INVALID: %s" % error)
        return 1
    print("VALID: %s" % record_path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

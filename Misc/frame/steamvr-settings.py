#!/usr/bin/env python3
"""Snapshot and restore only SteamVR's performance-check setting overrides."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


KEYS = ("recordStats", "drawPerfCriteriaGameTextures")
SECTION = "perfcheck"


def snapshot(config: Path) -> dict:
    data = json.loads(config.read_text(encoding="utf-8"))
    section = data.get(SECTION, {})
    return {
        "section_present": SECTION in data,
        "values": {key: section[key] for key in KEYS if key in section},
    }


def _object_end(text: str, opening: int) -> int:
    depth = 0
    in_string = False
    escaped = False
    for index in range(opening, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index + 1
    raise ValueError("unterminated SteamVR settings object")


def _section_span(text: str) -> tuple[int, int, int, int] | None:
    match = re.search(r'(?m)^[ \t]*"perfcheck"[ \t]*:[ \t]*\{', text)
    if not match:
        return None
    opening = text.rfind("{", match.start(), match.end())
    end = _object_end(text, opening)
    line_start = text.rfind("\n", 0, match.start()) + 1
    return line_start, opening, end, end


def _remove_section(text: str, span: tuple[int, int, int, int]) -> str:
    line_start, _, end, _ = span
    cursor = end
    while cursor < len(text) and text[cursor] in " \t\r":
        cursor += 1
    if cursor < len(text) and text[cursor] == ",":
        cursor += 1
        while cursor < len(text) and text[cursor] in " \t\r":
            cursor += 1
        if cursor < len(text) and text[cursor] == "\n":
            cursor += 1
        return text[:line_start] + text[cursor:]

    # If perfcheck is the final root property, remove its preceding root-level
    # comma as well. Ignore commas nested in objects/arrays and quoted strings.
    depth = 0
    in_string = False
    escaped = False
    separator = None
    for index, char in enumerate(text[:line_start]):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char in "{[":
            depth += 1
        elif char in "}]":
            depth -= 1
        elif char == "," and depth == 1:
            separator = index
    if separator is None:
        raise ValueError("could not find the preceding JSON property separator")
    return text[:separator] + text[end:]


def _render_object(value: dict) -> str:
    rendered = json.dumps(value, indent=3, ensure_ascii=False)
    rendered = rendered.replace('\":', '\" :')
    return "\n".join(line if index == 0 else "   " + line
                       for index, line in enumerate(rendered.splitlines()))


def restore(config: Path, state: dict) -> None:
    text = config.read_text(encoding="utf-8")
    current = json.loads(text)
    section = current.get(SECTION)
    if not isinstance(section, dict):
        section = {}
    for key in KEYS:
        if key in state["values"]:
            section[key] = state["values"][key]
        else:
            section.pop(key, None)

    should_exist = state["section_present"] or bool(section)
    span = _section_span(text)
    if not should_exist:
        if span is not None:
            text = _remove_section(text, span)
    elif span is not None:
        _, opening, end, _ = span
        text = text[:opening] + _render_object(section) + text[end:]
    else:
        raise ValueError("SteamVR perfcheck section disappeared during test")

    parsed = json.loads(text)
    expected = state["values"]
    actual = parsed.get(SECTION, {})
    for key in KEYS:
        if (key in expected) != (key in actual) or (key in expected and actual[key] != expected[key]):
            raise ValueError(f"failed to restore {SECTION}.{key}")
    config.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("snapshot", "restore"))
    parser.add_argument("config", type=Path)
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    if args.mode == "snapshot":
        args.snapshot.write_text(json.dumps(snapshot(args.config), indent=2) + "\n", encoding="utf-8")
    else:
        restore(args.config, json.loads(args.snapshot.read_text(encoding="utf-8")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Fail when a user-visible tr() string has no Chinese catalogue entry.

The .ts files are updated by lupdate during the build, but lupdate only adds
new sources as unfinished. Without this check a new dialog silently ships in
English on a Chinese system. Scans every C++/Objective-C++ source under src/
for tr(), QObject::tr(), QCoreApplication::translate(), QT_TR_NOOP() and
QT_TRANSLATE_NOOP(), then requires each source text to exist in
translations/synergy_zh_CN.ts with a non-empty translation.
"""

import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

# One pass over the file: comments, string literals and call starts. Matching
# string literals here (instead of a second regex) keeps '//' inside a URL from
# being mistaken for a comment.
TOKEN = re.compile(
    r"""
      (?P<comment>//[^\n]*|/\*.*?\*/)
    | (?P<raw>R"(?P<delim>[^()\\ ]*)\((?P<rawbody>.*?)\)(?P=delim)")
    | (?P<string>"(?:[^"\\]|\\.)*")
    | (?P<call>
        (?:\b(?:QObject|QCoreApplication|QApplication|QGuiApplication)::)?\btr\s*\(
      | \btranslate\s*\(
      | \bQT_TR_NOOP\s*\(
      | \bQT_TRANSLATE_NOOP\s*\(
      )
    """,
    re.VERBOSE | re.DOTALL,
)

# Same set lupdate applies when it reads a C++ literal.
ESCAPES = {
    "n": "\n",
    "t": "\t",
    "r": "\r",
    "\\": "\\",
    '"': '"',
    "'": "'",
    "0": "\0",
}


def decode_literal(raw: str) -> str:
    body = raw[1:-1]
    out = []
    i = 0
    while i < len(body):
        char = body[i]
        if char != "\\":
            out.append(char)
            i += 1
            continue
        i += 1
        if i >= len(body):
            break
        escape = body[i]
        out.append(ESCAPES.get(escape, escape))
        i += 1
    return "".join(out)


def calls(text: str):
    """Yield (line, [decoded argument literals]) for each translation call."""
    position = 0
    while True:
        match = TOKEN.search(text, position)
        if not match:
            return
        if match.lastgroup != "call":
            position = match.end()
            continue
        line = text.count("\n", 0, match.start()) + 1
        kind = match.group("call")
        takes_context = "translate" in kind
        depth = 0
        literals = []
        index = match.end() - 1  # points at '('
        while index < len(text):
            char = text[index]
            if char == "(":
                depth += 1
                index += 1
                continue
            if char == ")":
                depth -= 1
                index += 1
                if depth == 0:
                    break
                continue
            if char == '"' or (char == "R" and text.startswith('R"', index)):
                literal = TOKEN.match(text, index)
                if literal and literal.lastgroup == "string":
                    literals.append(decode_literal(literal.group("string")))
                    index = literal.end()
                    continue
                if literal and literal.lastgroup == "raw":
                    literals.append(literal.group("rawbody"))
                    index = literal.end()
                    continue
            if text.startswith("//", index):
                newline = text.find("\n", index)
                index = len(text) if newline < 0 else newline + 1
                continue
            if text.startswith("/*", index):
                end = text.find("*/", index + 2)
                index = len(text) if end < 0 else end + 2
                continue
            index += 1
        yield line, takes_context, literals
        position = index


def translated_sources(ts_path: pathlib.Path):
    root = ET.parse(ts_path).getroot()
    translated = set()
    unfinished = []
    for context in root.findall("context"):
        name = context.findtext("name") or ""
        for message in context.findall("message"):
            source = message.findtext("source") or ""
            translation = message.find("translation")
            if translation is None:
                continue
            if translation.get("type") == "vanished":
                continue
            if (translation.text or "").strip() or translation.findall("numerusform"):
                translated.add(source)
            else:
                unfinished.append((name, source))
    return translated, unfinished


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=".", help="repository root")
    parser.add_argument("--catalogue", default="translations/synergy_zh_CN.ts")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    catalogue = root / args.catalogue
    translated, unfinished = translated_sources(catalogue)

    missing = []
    for path in sorted(root.joinpath("src").rglob("*")):
        if path.suffix not in {".cpp", ".h", ".mm", ".cc", ".cxx"}:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for line, takes_context, literals in calls(text):
            if not literals:
                continue
            # translate()/QT_TRANSLATE_NOOP() take the context as the first
            # argument; tr() and QT_TR_NOOP() do not. Adjacent literals are one
            # string after C++ concatenation.
            source = "".join(literals[1:] if takes_context else literals)
            if not source or source in translated:
                continue
            missing.append(f"{path.relative_to(root)}:{line}: {source!r}")

    for name, source in unfinished:
        print(f"EMPTY TRANSLATION [{name}] {source!r}", file=sys.stderr)
    for item in missing:
        print(f"MISSING TRANSLATION {item}", file=sys.stderr)

    if missing or unfinished:
        print(
            f"FAIL: {len(missing)} missing, {len(unfinished)} empty Chinese translations",
            file=sys.stderr,
        )
        return 1

    print(f"translations OK: every tr() source has a Chinese catalogue entry ({len(translated)} sources)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

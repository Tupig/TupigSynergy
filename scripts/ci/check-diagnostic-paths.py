#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Validate diagnostic path metadata beneath cwd without reading file contents.

Missing paths are allowed. This check does not provide atomic protection against
concurrent path replacement; callers validate immediately before each use.
"""
import argparse
import os
from pathlib import Path, PurePosixPath
import stat
import sys


class InvalidPath(ValueError):
    pass


def relative_parts(value):
    if (not value or value.startswith("/") or "\\" in value or ":" in value
            or any(ord(character) < 32 or ord(character) == 127 for character in value)):
        raise InvalidPath("invalid relative path: " + repr(value))
    if ".." in value.split("/"):
        raise InvalidPath("parent traversal is forbidden: " + repr(value))
    parts = PurePosixPath(value).parts
    if not parts:
        raise InvalidPath("empty relative path")
    return parts


def metadata(path, root):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return None
    if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
        raise InvalidPath("linked or reparse path: " + repr(str(path.relative_to(root))))
    if not (stat.S_ISDIR(info.st_mode) or stat.S_ISREG(info.st_mode)):
        raise InvalidPath("nonregular path: " + repr(str(path.relative_to(root))))
    try:
        path.resolve(strict=True).relative_to(root)
    except ValueError as error:
        raise InvalidPath("path resolves outside the workspace") from error
    return info


def validate(value, root, recursive):
    parts = relative_parts(value)
    current = root
    for index, part in enumerate(parts):
        current = current / part
        info = metadata(current, root)
        if info is None:
            return
        if index < len(parts) - 1 and not stat.S_ISDIR(info.st_mode):
            raise InvalidPath("nondirectory ancestor: " + repr(str(current.relative_to(root))))
    if recursive and stat.S_ISDIR(info.st_mode):
        with os.scandir(current) as entries:
            names = sorted(entry.name for entry in entries)
        for name in names:
            validate((current / name).relative_to(root).as_posix(), root, True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recursive", action="store_true")
    parser.add_argument("paths", nargs="+")
    arguments = parser.parse_args()
    root = Path.cwd()
    try:
        info = root.lstat()
        if (not stat.S_ISDIR(info.st_mode) or stat.S_ISLNK(info.st_mode)
                or getattr(info, "st_file_attributes", 0) & 0x400):
            raise InvalidPath("workspace is not a regular directory")
        for value in arguments.paths:
            validate(value, root, arguments.recursive)
    except (OSError, ValueError, RuntimeError) as error:
        print("Diagnostic path validation failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

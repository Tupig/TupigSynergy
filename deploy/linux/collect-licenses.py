#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Collect license texts for bundled libraries without publishing build paths."""

import glob
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def output(command):
    result = subprocess.run(command, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return result.stdout.strip() if result.returncode == 0 else ""


def copy_files(files, destination):
    destination.mkdir(parents=True, exist_ok=True)
    copied = False
    expanded = []
    for file in files:
        source = Path(file)
        if source.is_dir():
            # Some packages list a license directory, including a symlink to
            # their shared notices. Resolve that root once; rglob does not
            # descend into nested directory symlinks and cannot cycle there.
            expanded.extend(str(child) for child in source.resolve().rglob("*") if child.is_file())
        else:
            expanded.append(file)
    for file in sorted(set(expanded)):
        source = Path(file)
        if source.is_file():
            content = source.read_bytes()
            if not content:
                continue
            target = destination / source.name
            counter = 0
            while target.exists() and target.read_bytes() != content:
                suffix = hashlib.sha256(content).hexdigest()[:12]
                tail = "" if counter == 0 else "-" + str(counter)
                target = destination / (source.name + "-" + suffix + tail)
                counter += 1
            shutil.copy2(source, target, follow_symlinks=True)
            copied = True
    return copied


def package_notices(listing):
    files = []
    for file in listing:
        if "/licenses/" in file or re.search(r"/(COPYING|LICENSE|copyright)", file, re.I):
            files.append(file)
        elif re.match(r"README(?:[._-]|$)", Path(file).name, re.I) and Path(file).is_file():
            # libglvnd embeds its complete notices in its package-owned README.
            # Discover notices, without treating an arbitrary README as one.
            text = Path(file).read_text(errors="replace")
            if (re.search(r"\blicens(?:e|ed|ing)\b", text, re.I)
                    and re.search(r"\bcopyright\b", text, re.I)
                    and re.search(r"permission\s+is\s+hereby\s+granted|redistribution\s+and\s+use", text, re.I)):
                files.append(file)
    return files


def main():
    source_list, destination, vcpkg, extra = sys.argv[1:5]
    source_map = {}
    for item in filter(None, (sys.argv[5] if len(sys.argv) > 5 else "").split(";")):
        soname, directory = item.split("=", 1)
        if "/" in soname or not soname or not Path(directory).is_dir():
            raise RuntimeError("Invalid source-built SONAME/license mapping")
        source_map[soname] = Path(directory)
    destination = Path(destination)
    vcpkg = Path(vcpkg)
    copyrights = list(vcpkg.glob("share/*/copyright")) if str(vcpkg) != "/" else []
    for copyright_file in copyrights:
        copy_files([copyright_file], destination / "vcpkg" / copyright_file.parent.name)
    for directory in filter(None, extra.split(";")):
        source = Path(directory)
        if not source.is_dir():
            raise RuntimeError("Configured source-built license directory is missing")
        target = destination / "source-built" / source.name
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source, target)

    manifest = []
    rpm_sources = None
    rpm_notices = {}
    for source in Path(source_list).read_text().splitlines():
        path = Path(source)
        if copyrights and os.path.commonpath([str(path.resolve()), str(vcpkg.resolve())]) == str(vcpkg.resolve()):
            manifest.append(f"{path.name}\tvcpkg")
            continue
        # Exact SONAME mappings are explicit provenance, not a blanket exemption
        # for every unowned library whenever any extra license directory exists.
        soname = path.name
        if source_map and soname not in source_map and shutil.which("readelf"):
            match = re.search(r"\(SONAME\).*?\[([^\]]+)\]", output(["readelf", "-d", str(path)]))
            if match:
                soname = match.group(1)
        if soname in source_map:
            files = [str(file) for file in source_map[soname].rglob("*") if file.is_file()]
            if not copy_files(files, destination / "source-built" / soname):
                raise RuntimeError("Empty source-built license mapping: " + soname)
            manifest.append(f"{path.name}\tsource-built/{soname}")
            continue
        # Package ownership can use /lib while ELF resolution uses merged /usr/lib.
        candidates = list(dict.fromkeys([str(path), str(path.resolve())]))
        candidates += [candidate[4:] for candidate in candidates if candidate.startswith("/usr/lib/")]
        package = ""
        files = []
        manager = ""
        if shutil.which("dpkg-query"):
            manager = "dpkg"
            for candidate in candidates:
                owner = output(["dpkg-query", "-S", candidate]).splitlines()
                if owner:
                    package = owner[0].split(": ", 1)[0]
                    break
            if package:
                name = package.split(":", 1)[0]
                files = glob.glob(f"/usr/share/doc/{name}/copyright")
        elif shutil.which("rpm"):
            manager = "rpm"
            for candidate in candidates:
                package = output(["rpm", "-qf", "--qf", "%{NAME}", candidate])
                if package:
                    break
            if package:
                source_rpm = output(["rpm", "-q", "--qf", "%{SOURCERPM}", package])
                owners = [package]
                if source_rpm and source_rpm != "(none)":
                    if rpm_sources is None:
                        rpm_sources = []
                        for line in output(["rpm", "-qa", "--qf", "%{NAME}\t%{SOURCERPM}\n"]).splitlines():
                            fields = line.split("\t")
                            if len(fields) == 2:
                                rpm_sources.append(fields)
                    owners += [name for name, source in rpm_sources if source == source_rpm and name != package]
                for owner in owners:
                    if owner not in rpm_notices:
                        rpm_notices[owner] = package_notices(output(["rpm", "-ql", owner]).splitlines())
                    files.extend(rpm_notices[owner])
        elif shutil.which("pacman"):
            manager = "pacman"
            for candidate in candidates:
                package = output(["pacman", "-Qoq", candidate])
                if package:
                    break
            if package:
                listing = output(["pacman", "-Qlq", package]).splitlines()
                files = [file for file in listing if "/licenses/" in file]
                # Arch puts standard licenses in a common package.
                if not files:
                    files = glob.glob("/usr/share/licenses/common/*/*")
        if not package or not copy_files(files, destination / manager / package):
            raise RuntimeError(f"Missing license provenance for bundled runtime {path.name}")
        manifest.append(f"{path.name}\t{manager}/{package}")
    (destination / "runtime-license-index.tsv").write_text("\n".join(sorted(set(manifest))) + "\n")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)

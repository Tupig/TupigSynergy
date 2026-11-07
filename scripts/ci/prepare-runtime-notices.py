#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Prepare exact-source notices for two CI runtime providers, never at app runtime."""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
import sys
import tarfile
import urllib.request


SOURCES = {
    ("libdrm", "2.4.115"): {
        "sha256": "554cfbfe0542bddb391b4e3e05bfbbfc3e282b955bd56218d21c0616481f65eb",
        "url": "https://dri.freedesktop.org/libdrm/libdrm-2.4.115.tar.xz",
        "authority": "https://raw.githubusercontent.com/buildroot/buildroot/2023.02/package/libdrm/libdrm.hash",
    },
    ("libdrm", "2.4.128"): {
        "sha256": "3bb35db8700c2a0b569f2c6729a53f5495786856b310854c8de57782a22bddac",
        "url": "https://dri.freedesktop.org/libdrm/libdrm-2.4.128.tar.xz",
        "authority": "https://raw.githubusercontent.com/buildroot/buildroot/e68414dc61b84d0f69c1ed1bb30bcd3d67473eb4/package/libdrm/libdrm.hash",
    },
    ("libdrm", "2.4.133"): {
        "sha256": "fc68f9d0ba2ea63c9432a299e14fea09fad7a8a66e8039fcd7802ca59f77b4f5",
        "url": "https://dri.freedesktop.org/libdrm/libdrm-2.4.133.tar.xz",
        "authority": "https://www.mail-archive.com/dri-devel%40lists.freedesktop.org/msg603051.html",
    },
    ("libdrm", "2.4.134"): {
        "sha256": "ac5e74d157830eb8bee44c6a6bf3ad49774ef0dd2a72bdad74a8f20308b52a95",
        "url": "https://dri.freedesktop.org/libdrm/libdrm-2.4.134.tar.xz",
        "authority": "https://raw.githubusercontent.com/buildroot/buildroot/a2602d10a16e1d32d619fa8dfb696df81265a2c9/package/libdrm/libdrm.hash",
    },
    ("e2fsprogs", "1.47.4"): {
        "sha256": "fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b",
        "url": "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/e2fsprogs-1.47.4.tar.xz",
        "authority": "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/sha256sums.asc",
    },
}
# Names/ABI majors are declared by the authenticated libdrm meson sources.
SONAMES = {
    "libdrm": {"libdrm.so.2", "libdrm_amdgpu.so.1", "libdrm_intel.so.1", "libdrm_nouveau.so.2",
               "libdrm_radeon.so.1", "libdrm_freedreno.so.1", "libdrm_tegra.so.0", "libdrm_omap.so.1",
               "libdrm_exynos.so.1", "libdrm_etnaviv.so.1"},
    "e2fsprogs": {"libcom_err.so.2"},
}


def command(args):
    result = subprocess.run(args, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise RuntimeError("Runtime provider metadata command failed: " + args[0])
    return result.stdout.strip()


def validate_sonames(provider, names):
    if provider not in SONAMES or not names or not set(names).issubset(SONAMES[provider]):
        raise RuntimeError("Unknown or missing exact runtime SONAME")


def discover(provider):
    pattern = r"libdrm(?:_[a-z]+)?\.so\.[0-9.]+" if provider == "libdrm" else r"libcom_err\.so\.[0-9.]+"
    if provider == "libdrm":
        query = ["--qf", "%{NAME}\t%{VERSION}\t%{SOURCERPM}"]
        record = command(["rpm", "-q", "--whatprovides"] + query + ["libdrm.so.2()(64bit)"])
        fields = record.split("\t")
        if (len(fields) != 3 or fields[0] not in {"libdrm", "libdrm2"}
                or not fields[2].startswith("libdrm-" + fields[1] + "-") or not fields[2].endswith(".src.rpm")):
            raise RuntimeError("Unknown installed libdrm source identity")
        version = fields[1]
        runtime_names = {"libdrm"} | {name.replace(".so.", "") for name in SONAMES[provider]}
        owners = {fields[0]: record}
        for entry in command(["rpm", "-qa", "--qf", "%{NAME}\t%{VERSION}\t%{SOURCERPM}\n"]).splitlines():
            sibling = entry.split("\t")
            if len(sibling) != 3 or sibling[0] not in runtime_names:
                continue
            if sibling[1:] != fields[1:]:
                raise RuntimeError("Conflicting installed libdrm runtime source family")
            owners[sibling[0]] = entry
        identity = " | ".join(value.replace("\t", " ") for value in sorted(owners.values()))
        listing = []
        expected_owners = {}
        for owner, owner_record in sorted(owners.items()):
            for path in command(["rpm", "-ql", owner]).splitlines():
                if not re.fullmatch(pattern, Path(path).name):
                    continue
                if path in expected_owners and expected_owners[path] != owner_record:
                    raise RuntimeError("Ambiguous installed runtime file owner")
                expected_owners[path] = owner_record
                listing.append(path)
    elif provider == "e2fsprogs":
        identity = command(["pacman", "-Q", "e2fsprogs"])
        fields = identity.split()
        if len(fields) != 2 or fields[0] != "e2fsprogs" or fields[1] != "1.47.4-1":
            raise RuntimeError("Unknown installed e2fsprogs package identity")
        version = fields[1].rsplit("-", 1)[0]
        listing = command(["pacman", "-Qlq", "e2fsprogs"]).splitlines()
    else:
        raise RuntimeError("Unsupported runtime notice provider")
    if (provider, version) not in SOURCES:
        raise RuntimeError("Unknown installed runtime provider version")
    names = set()
    visited = set()
    for listed in listing:
        path = Path(listed)
        if not re.fullmatch(pattern, path.name):
            continue
        if not path.is_absolute() or not path.is_file():
            raise RuntimeError("Missing installed runtime provider file")
        if provider == "libdrm":
            owner = command(["rpm", "-qf"] + query + [str(path)])
            if owner != expected_owners[listed]:
                raise RuntimeError("Installed runtime file has a different package owner")
        elif command(["pacman", "-Qoq", str(path)]) != "e2fsprogs":
            raise RuntimeError("Installed runtime file has a different package owner")
        resolved = str(path.resolve())
        if resolved in visited:
            continue
        visited.add(resolved)
        with path.open("rb") as file:
            if file.read(4) != b"\x7fELF":
                raise RuntimeError("Installed runtime provider is not ELF")
        matches = re.findall(r"\(SONAME\)[^\n]*\[([^\]]+)\]", command(["readelf", "-d", str(path)]))
        if len(matches) != 1:
            raise RuntimeError("Missing or ambiguous installed runtime SONAME")
        names.add(matches[0])
    validate_sonames(provider, names)
    return version, identity, sorted(names)


def source_files(data, expected_root):
    files = {}
    seen = set()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:*") as archive:
        members = archive.getmembers()
        if len(members) > 20000 or sum(member.size for member in members) > 256 * 1024 * 1024:
            raise RuntimeError("Source archive exceeds bounded limits")
        for member in members:
            path = PurePosixPath(member.name)
            if (path.is_absolute() or ".." in path.parts or not path.parts
                    or path.parts[0] != expected_root or str(path) in seen):
                raise RuntimeError("Unsafe, duplicate or wrong-version source member")
            seen.add(str(path))
            if member.issym() or member.islnk():
                target = member.linkname if member.islnk() else posixpath.join(posixpath.dirname(member.name), member.linkname)
                normalized = PurePosixPath(posixpath.normpath(target))
                if normalized.is_absolute() or not normalized.parts or normalized.parts[0] != expected_root:
                    raise RuntimeError("Escaping source archive link")
                continue
            if member.isfile():
                files[str(path)] = member
            elif not member.isdir():
                raise RuntimeError("Unsupported source archive member")
        result = {}
        for name, member in files.items():
            if name.endswith((".c", ".h")) or name == expected_root + "/NOTICE":
                result[name] = archive.extractfile(member).read()
        return result


def prepare(provider, version, archive_bytes, sonames, destination, package_identity):
    pin = SOURCES.get((provider, version))
    if pin is None:
        raise RuntimeError("Unknown exact runtime source version")
    validate_sonames(provider, sonames)
    if hashlib.sha256(archive_bytes).hexdigest() != pin["sha256"]:
        raise RuntimeError("Runtime source archive checksum mismatch")
    root = provider + "-" + version
    files = source_files(archive_bytes, root)
    foundation = root + ("/xf86drm.c" if provider == "libdrm" else "/lib/et/com_err.c")
    if foundation not in files:
        raise RuntimeError("Missing foundational runtime source notice")
    package_notice = files.get(root + "/NOTICE", b"") if provider == "e2fsprogs" else b""
    if provider == "e2fsprogs" and not package_notice.strip():
        raise RuntimeError("Missing exact e2fsprogs package NOTICE")
    notices = []
    for name in sorted(files):
        if not name.endswith((".c", ".h")) or (provider == "e2fsprogs" and not name.startswith(root + "/lib/et/")):
            continue
        comments = re.findall(rb"/\*.*?\*/(?:\r?\n)?|(?m:^[ \t]*//[^\n]*(?:\n|$))", files[name], re.S)
        if comments:
            notices.append(b"\nSOURCE " + name.encode("utf-8") + b"\n" + b"\n".join(comments))
    content = b"\n".join(notices)
    if not content or b"copyright" not in content.lower() or b"permission" not in content.lower():
        raise RuntimeError("Missing complete source copyright and permission notice")
    directory = Path(destination).resolve() / root
    if any(char in str(directory) for char in ";=\r\n"):
        raise RuntimeError("Unsafe notice output mapping path")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "COPYING.source-notices").write_bytes(content)
    if package_notice:
        (directory / "COPYING.package-NOTICE").write_bytes(package_notice)
    provenance = dict(pin, provider=provider, version=version, package_identity=package_identity,
                      sonames=sorted(set(sonames)))
    (directory / "provenance.json").write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n")
    return {name: str(directory) for name in sorted(set(sonames))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--provider", required=True, choices=sorted(SONAMES))
    parser.add_argument("--destination", required=True)
    args = parser.parse_args()
    version, identity, names = discover(args.provider)
    pin = SOURCES[(args.provider, version)]
    with urllib.request.urlopen(pin["url"], timeout=60) as response:
        data = response.read(32 * 1024 * 1024 + 1)
    if len(data) > 32 * 1024 * 1024:
        raise RuntimeError("Runtime source archive exceeds download limit")
    mapping = prepare(args.provider, version, data, names, args.destination, identity)
    print(";".join(name + "=" + directory for name, directory in sorted(mapping.items())))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        sys.exit(1)

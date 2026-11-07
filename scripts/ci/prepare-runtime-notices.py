#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Prepare exact-source notices for four CI runtime providers, never at app runtime."""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import socket
import subprocess
import sys
import tarfile
import time
import urllib.error
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
        "mirror": "https://sources.buildroot.net/libdrm/libdrm-2.4.134.tar.xz",
    },
    ("e2fsprogs", "1.47.4"): {
        "sha256": "fd5bf388cbdbe006a3d3b318d983b2948382440acc85a87f1e7d108653e8db0b",
        "url": "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/e2fsprogs-1.47.4.tar.xz",
        "authority": "https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.4/sha256sums.asc",
    },
    ("xcb-util-keysyms", "0.4.1"): {
        "sha256": "7c260a5294412aed429df1da2f8afd3bd07b7cba3fec772fba15a613a6d5c638",
        "url": "https://www.x.org/releases/individual/xcb/xcb-util-keysyms-0.4.1.tar.xz",
        "authority": "https://lists.x.org/archives/xorg-announce/2022-October/003233.html",
        "mirror": "https://sources.buildroot.net/xcb-util-keysyms/xcb-util-keysyms-0.4.1.tar.xz",
    },
    ("glib", "2.90.1"): {
        "sha256": "93c941aa17d5eb1d53fe838365f29a8b4e539c222a256d974ec8f30fc413e396",
        "url": "https://download.gnome.org/sources/glib/2.90/glib-2.90.1.tar.xz",
        "authority": "https://download.gnome.org/sources/glib/2.90/glib-2.90.1.sha256sum",
    },
}
# Names/ABI majors are declared by the authenticated libdrm meson sources.
SONAMES = {
    "libdrm": {"libdrm.so.2", "libdrm_amdgpu.so.1", "libdrm_intel.so.1", "libdrm_nouveau.so.2",
               "libdrm_radeon.so.1", "libdrm_freedreno.so.1", "libdrm_tegra.so.0", "libdrm_omap.so.1",
               "libdrm_exynos.so.1", "libdrm_etnaviv.so.1"},
    "e2fsprogs": {"libcom_err.so.2"},
    "xcb-util-keysyms": {"libxcb-keysyms.so.1"},
    "glib": {"libglib-2.0.so.0", "libgobject-2.0.so.0", "libgmodule-2.0.so.0",
             "libgthread-2.0.so.0", "libgio-2.0.so.0", "libgirepository-2.0.so.0"},
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
    patterns = {
        "libdrm": r"libdrm(?:_[a-z]+)?\.so\.[0-9.]+",
        "e2fsprogs": r"libcom_err\.so\.[0-9.]+",
        "xcb-util-keysyms": r"libxcb-keysyms\.so\.[0-9.]+",
        "glib": r"lib(?:glib|gobject|gmodule|gthread|gio|girepository)-2\.0\.so\.[0-9.]+",
    }
    if provider not in patterns:
        raise RuntimeError("Unsupported runtime notice provider")
    pattern = patterns[provider]
    if provider in {"libdrm", "xcb-util-keysyms"}:
        query = ["--qf", "%{NAME}\t%{VERSION}\t%{SOURCERPM}"]
        capability = "libdrm.so.2()(64bit)" if provider == "libdrm" else "libxcb-keysyms.so.1()(64bit)"
        primary_names = {"libdrm", "libdrm2"} if provider == "libdrm" else {"xcb-util-keysyms"}
        record = command(["rpm", "-q", "--whatprovides"] + query + [capability])
        fields = record.split("\t")
        if (len(fields) != 3 or fields[0] not in primary_names
                or not fields[2].startswith(provider + "-" + fields[1] + "-") or not fields[2].endswith(".src.rpm")):
            raise RuntimeError("Unknown installed runtime source identity")
        version = fields[1]
        runtime_names = ({"libdrm"} | {name.replace(".so.", "") for name in SONAMES[provider]}
                         if provider == "libdrm" else {"xcb-util-keysyms"})
        owners = {fields[0]: record}
        for entry in command(["rpm", "-qa", "--qf", "%{NAME}\t%{VERSION}\t%{SOURCERPM}\n"]).splitlines():
            sibling = entry.split("\t")
            if len(sibling) != 3 or sibling[0] not in runtime_names:
                continue
            if sibling[1:] != fields[1:]:
                raise RuntimeError("Conflicting installed runtime source family")
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
    elif provider in {"e2fsprogs", "glib"}:
        package, expected_version = ("e2fsprogs", "1.47.4-1") if provider == "e2fsprogs" else ("glib2", "2.90.1-1")
        identity = command(["pacman", "-Q", package])
        fields = identity.split()
        if len(fields) != 2 or fields[0] != package or fields[1] != expected_version:
            raise RuntimeError("Unknown installed runtime package identity")
        version = fields[1].rsplit("-", 1)[0]
        listing = command(["pacman", "-Qlq", package]).splitlines()
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
        if provider in {"libdrm", "xcb-util-keysyms"}:
            owner = command(["rpm", "-qf"] + query + [str(path)])
            if owner != expected_owners[listed]:
                raise RuntimeError("Installed runtime file has a different package owner")
        elif command(["pacman", "-Qoq", str(path)]) != package:
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
    copying_alias = None
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
                if expected_root == "glib-2.90.1" and str(path) == expected_root + "/COPYING":
                    if not member.issym() or member.linkname != "LICENSES/LGPL-2.1-or-later.txt":
                        raise RuntimeError("Unsupported GLib COPYING alias")
                    copying_alias = expected_root + "/" + member.linkname
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
            if (name.endswith((".c", ".h")) or name in {expected_root + "/NOTICE", expected_root + "/COPYING"}
                    or name.startswith(expected_root + "/LICENSES/")):
                result[name] = archive.extractfile(member).read()
        if copying_alias is not None:
            if copying_alias not in files:
                raise RuntimeError("Missing ordinary GLib COPYING alias target")
            result[expected_root + "/COPYING"] = result[copying_alias]
        return result


def source_comments(data):
    # C literals may contain comment-like text; consume those tokens without shipping them.
    tokens = re.finditer(rb'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|(?P<comment>/\*.*?\*/(?:\r?\n)?|//[^\n]*(?:\n|$))',
                         data, re.S)
    return [token.group("comment") for token in tokens if token.group("comment") is not None]


def prepare(provider, version, archive_bytes, sonames, destination, package_identity):
    pin = SOURCES.get((provider, version))
    if pin is None:
        raise RuntimeError("Unknown exact runtime source version")
    validate_sonames(provider, sonames)
    if hashlib.sha256(archive_bytes).hexdigest() != pin["sha256"]:
        raise RuntimeError("Runtime source archive checksum mismatch")
    root = provider + "-" + version
    files = source_files(archive_bytes, root)
    foundations = {
        "libdrm": ["xf86drm.c"], "e2fsprogs": ["lib/et/com_err.c"],
        "xcb-util-keysyms": ["keysyms/keysyms.c"],
        "glib": ["glib/glib.h", "gobject/gobject.h", "gmodule/gmodule.h", "gthread/gthread-impl.c",
                 "gio/gio.h", "girepository/girepository.h"],
    }
    if any(root + "/" + name not in files for name in foundations[provider]):
        raise RuntimeError("Missing foundational runtime source notice")
    package_notice = files.get(root + "/NOTICE", b"") if provider == "e2fsprogs" else files.get(root + "/COPYING", b"")
    if provider != "libdrm" and not package_notice.strip():
        raise RuntimeError("Missing exact runtime package notice")
    license_files = {name[len(root) + 1:]: data for name, data in files.items()
                     if name.startswith(root + "/LICENSES/")} if provider == "glib" else {}
    if provider == "glib" and (not license_files.get("LICENSES/LGPL-2.1-or-later.txt")
                               or any(not data.strip() for data in license_files.values())):
        raise RuntimeError("Missing or empty complete GLib license text")
    notices = []
    comment_roots = {"e2fsprogs": ["lib/et/"], "xcb-util-keysyms": ["keysyms/"],
                     "glib": ["glib/", "gobject/", "gmodule/", "gthread/", "gio/", "girepository/"]}
    for name in sorted(files):
        if (not name.endswith((".c", ".h")) or provider in comment_roots and
                not any(name.startswith(root + "/" + prefix) for prefix in comment_roots[provider])):
            continue
        comments = (source_comments(files[name]) if provider in {"xcb-util-keysyms", "glib"} else
                    re.findall(rb"/\*.*?\*/(?:\r?\n)?|(?m:^[ \t]*//[^\n]*(?:\n|$))", files[name], re.S))
        if comments:
            notices.append(b"\nSOURCE " + name.encode("utf-8") + b"\n" + b"\n".join(comments))
    content = b"\n".join(notices)
    license_marker = b"spdx-license-identifier: lgpl-2.1" if provider == "glib" else b"permission"
    if not content or b"copyright" not in content.lower() or license_marker not in content.lower():
        raise RuntimeError("Missing complete source copyright and permission notice")
    directory = Path(destination).resolve() / root
    if any(char in str(directory) for char in ";=\r\n"):
        raise RuntimeError("Unsafe notice output mapping path")
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "COPYING.source-notices").write_bytes(content)
    if package_notice:
        (directory / "COPYING.package-NOTICE").write_bytes(package_notice)
    for relative, data in sorted(license_files.items()):
        target = directory / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    provenance = dict(pin, provider=provider, version=version, package_identity=package_identity,
                      sonames=sorted(set(sonames)))
    (directory / "provenance.json").write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n")
    return {name: str(directory) for name in sorted(set(sonames))}


def download_source(url):
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read(32 * 1024 * 1024 + 1)
            if len(data) > 32 * 1024 * 1024:
                raise RuntimeError("Runtime source archive exceeds download limit")
            return data
        except (urllib.error.URLError, ConnectionResetError, TimeoutError, socket.timeout) as error:
            reason = error.reason if isinstance(error, urllib.error.URLError) else error
            if not isinstance(reason, (ConnectionResetError, TimeoutError, socket.timeout)) or attempt == 2:
                raise
            print("Transient runtime source transport failure; retrying ({}/3)".format(attempt + 2),
                  file=sys.stderr)
            time.sleep(attempt + 1)


def download_runtime_source(provider, version):
    pin = SOURCES[(provider, version)]
    print("Fetching runtime source provider={} version={} url={}".format(provider, version, pin["url"]),
          file=sys.stderr)
    try:
        return download_source(pin["url"])
    except (urllib.error.URLError, ConnectionResetError, TimeoutError, socket.timeout) as error:
        reason = error.reason if isinstance(error, urllib.error.URLError) else error
        if "mirror" not in pin or not isinstance(reason, (ConnectionResetError, TimeoutError, socket.timeout)):
            raise
    print("Fetching runtime source mirror provider={} version={} url={}".format(provider, version, pin["mirror"]),
          file=sys.stderr)
    return download_source(pin["mirror"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--provider", required=True, choices=sorted(SONAMES))
    parser.add_argument("--destination", required=True)
    args = parser.parse_args()
    version, identity, names = discover(args.provider)
    data = download_runtime_source(args.provider, version)
    mapping = prepare(args.provider, version, data, names, args.destination, identity)
    print(";".join(name + "=" + directory for name, directory in sorted(mapping.items())))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail

if [[ $(uname -s) != Linux ]]; then
  echo 'ERROR: Linux package verification must run on Linux.' >&2
  exit 1
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/root" "$work/home" "$work/runtime"
chmod 700 "$work/runtime"
if [[ ${1:-} == --tree ]]; then
  root=$(realpath "${2:?install tree required}")
else
  build=${1:?build directory required}
  case ${2:-} in
    debian) pattern='*.deb' ;;
    fedora|rhel|suse) pattern='*.rpm' ;;
    arch) pattern='*.pkg.tar.zst' ;;
    '') pattern='*' ;;
    *) echo 'ERROR: unknown package family.' >&2; exit 1 ;;
  esac
  mapfile -d '' packages < <(find "$build" -maxdepth 1 -type f -name "$pattern" \
    \( -name '*.deb' -o -name '*.rpm' -o -name '*.pkg.tar.zst' \) -print0)
  if [[ ${#packages[@]} != 1 ]]; then
    echo 'ERROR: expected exactly one native Linux package.' >&2
    exit 1
  fi
  package=${packages[0]}
  case "$package" in
    *.deb) dpkg-deb --extract "$package" "$work/root" ;;
    *.rpm) rpm2cpio "$package" | (cd "$work/root" && cpio -idm --quiet) ;;
    *.pkg.tar.zst) tar --no-same-owner -xf "$package" -C "$work/root" ;;
  esac
  root="$work/root"
fi

python3 - "$root" "$work" <<'PY'
import os
from pathlib import Path
import re
import subprocess
import sys

root, work = map(Path, sys.argv[1:])
cores = list(root.glob("**/libexec/synergy/synergy-core"))
if len(cores) != 1:
    sys.exit("ERROR: expected one private synergy-core executable")
private = cores[0].parent.resolve()
prefix = private.parent.parent
lib = private / "lib"
os_library = re.compile(r"^(ld-linux[^/]*|ld64[^/]*|lib(c|m|dl|pthread|rt|resolv|util|anl|nss_[^.]+)\.so\.[0-9]+)$")
environment = {
    "PATH": "/usr/bin:/bin", "HOME": str(work / "home"),
    "XDG_CONFIG_HOME": str(work / "home/config"),
    "XDG_CONFIG_DIRS": str(work / "empty-system-config"),
    "XDG_STATE_HOME": str(work / "home/state"),
    "XDG_CACHE_HOME": str(work / "home/cache"),
    "XDG_RUNTIME_DIR": str(work / "runtime"), "LC_ALL": "C",
    "QT_QPA_PLATFORM": "offscreen", "QT_PLUGIN_PATH": str(private / "plugins"),
    "QT_QPA_PLATFORM_PLUGIN_PATH": str(private / "plugins/platforms"),
    "XKB_CONFIG_ROOT": str(private / "share/X11/xkb"),
    "OPENSSL_MODULES": str(private / "ossl-modules"),
}

Path(environment["XDG_CONFIG_DIRS"]).mkdir(parents=True, exist_ok=True)
for config_root in (work / "home/.config", Path(environment["XDG_CONFIG_HOME"])):
    config = config_root / "TuPig Synergy"
    config.mkdir(parents=True, exist_ok=True)
    (config / "TuPig Synergy.extra.conf").write_text(
        "[migration]\nschemaVersion=2147483647\nnotifiedFor=2147483647\n[scope]\npreferSystem=false\n")
    (config / "TuPig Synergy.conf").write_text("[core]\ncomputerName=synthetic-package-check\n")

def command(args, trace=False, require_offscreen=False, launcher=False):
    env = dict(environment)
    if trace:
        if launcher:
            # Trace only the final app, not the launcher's host shell utilities.
            env["SYNERGY_TRACE_RUNTIME"] = "1"
        else:
            env.update({"LD_DEBUG": "libs", "QT_DEBUG_PLUGINS": "1"})
    result = subprocess.run(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            universal_newlines=True, timeout=30)
    if result.returncode:
        raise RuntimeError("Command failed: {}\n{}".format(Path(args[0]).name, result.stderr))
    if trace:
        for loaded in re.findall(r"calling init:\s*(/[^\n]+)", result.stderr):
            path = Path(loaded.strip())
            if not os_library.fullmatch(path.name) and not contained(path):
                raise RuntimeError("Runtime loaded a non-OS host library: " + path.name)
        if require_offscreen:
            plugin = (private / "plugins/platforms/libqoffscreen.so").resolve()
            qt_loaded = re.findall(r'''["'](/[^"'\r\n]+)["']\s+loaded library(?:\s|$)''', result.stderr)
            initialized = re.findall(r"calling init:\s*(/[^\n]+)", result.stderr)
            if not any(Path(path.strip()).is_file() and Path(path.strip()).resolve() == plugin
                       and contained(Path(path.strip())) for path in qt_loaded + initialized):
                detail = [line[:500] for line in result.stderr.splitlines()
                          if "qt." in line.lower() or "calling init:" in line or "libqoffscreen" in line]
                raise RuntimeError("Qt did not load the packaged offscreen platform plugin: {} ({})\n{}".format(
                    plugin, Path(args[0]).name, "\n".join(detail[-16:])))
    return result.stdout

def contained(path):
    return os.path.commonpath([str(path.resolve()), str(private)]) == str(private)

def is_elf(path):
    with path.open("rb") as stream:
        return stream.read(4) == b"\x7fELF"

for path in private.rglob("*"):
    if path.is_symlink() and (not path.exists() or not contained(path)):
        raise RuntimeError("Broken or escaped package symlink: " + path.name)
elf_files = [path for path in private.rglob("*") if path.is_file() and not path.is_symlink() and is_elf(path)]
if not elf_files:
    sys.exit("ERROR: no packaged ELF files")
interpreter_info = command(["readelf", "-l", str(cores[0])])
match = re.search(r"Requesting program interpreter: ([^\]]+)", interpreter_info)
if not match or not os_library.fullmatch(Path(match.group(1)).name):
    sys.exit("ERROR: unsupported ELF interpreter")
loader = match.group(1)
uses_qt_shared = False
for path in elf_files:
    dynamic = command(["readelf", "-d", str(path)])
    needed = re.findall(r"\(NEEDED\).*?\[([^\]]+)\]", dynamic)
    runpaths = re.findall(r"\((?:RUNPATH|RPATH)\).*?\[([^\]]*)\]", dynamic)
    if len(runpaths) != 1:
        raise RuntimeError("Missing or ambiguous private RUNPATH: " + path.name)
    for entry in runpaths[0].split(":"):
        if not entry.startswith("$ORIGIN") or not contained(Path(entry.replace("$ORIGIN", str(path.parent)))):
            raise RuntimeError("Non-private RUNPATH: " + path.name)
    for name in needed:
        if "/" in name:
            raise RuntimeError("Absolute DT_NEEDED: " + path.name)
        if name.startswith(("libQt5", "libQt6")):
            uses_qt_shared = True
        if os_library.fullmatch(name):
            continue
        dependency = lib / name
        if not dependency.is_file() or not contained(dependency):
            raise RuntimeError("Missing packaged dependency: " + name)
    # Audit the real loader's resolution too; host-installed dependencies cannot
    # turn a missing payload entry into success merely because ldd finds them.
    resolved = command([loader, "--inhibit-cache", "--library-path", str(lib), "--list", str(path)])
    if "not found" in resolved:
        raise RuntimeError("Unresolved runtime dependency: " + path.name)
    for name, target in re.findall(r"^\s*(\S+) => (\S+)", resolved, re.M):
        if not os_library.fullmatch(name) and not contained(Path(target)):
            raise RuntimeError("Loader escaped package: " + name)

if uses_qt_shared:
    for plugin in ("libqxcb.so", "libqoffscreen.so"):
        path = private / "plugins/platforms" / plugin
        if not path.is_file() or not is_elf(path):
            raise RuntimeError("Missing or non-ELF Qt platform plugin: " + plugin)
if not (private / "share/X11/xkb/rules/evdev").is_file():
    raise RuntimeError("Missing bundled XKB data")
license_indices = list(prefix.glob("share/licenses/*/third-party/runtime-license-index.tsv"))
if len(license_indices) != 1 or not license_indices[0].read_text().strip():
    raise RuntimeError("Missing third-party license index")

# Force both ELF dependency resolution and the installed public entry points.
for name, argument in (("synergy-core", "--version"), ("synergy", "--help")):
    executable = private / name
    if not executable.exists():
        raise RuntimeError("Missing packaged executable: " + name)
    command([loader, "--inhibit-cache", "--library-path", str(lib), str(executable), argument],
            trace=True, require_offscreen=uses_qt_shared and name == "synergy")
    command([str(prefix / "bin" / name), argument],
            trace=True, require_offscreen=uses_qt_shared and name == "synergy", launcher=True)
print("PASS: {} ELF files; private dependency closure, licenses and clean-environment launch".format(len(elf_files)))
PY

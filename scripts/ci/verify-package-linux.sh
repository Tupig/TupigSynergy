#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail

if [[ $(uname -s) != Linux ]]; then
  echo 'ERROR: Linux package verification must run on Linux.' >&2
  exit 1
fi
if [[ ${1:-} != --expected-arch || -z ${2:-} ]]; then
  echo 'ERROR: expected target architecture required (--expected-arch x64|arm64).' >&2
  exit 1
fi
expected_arch=$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')
shift 2
case $expected_arch in
  x64|x86_64|amd64) expected_arch=x64 ;;
  arm64|aarch64) expected_arch=arm64 ;;
  *) echo 'ERROR: unsupported expected target architecture.' >&2; exit 1 ;;
esac
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

python3 - "$root" "$work" "$expected_arch" <<'PY'
import os
import hashlib
import json
import struct
from pathlib import Path
import re
import subprocess
import sys

# Audit every physical payload file before invoking a loader or application.
# Archive members stay in memory: thin archives never resolve external files.
def expected_target(value, multiple=False):
    aliases = {'x64': 'x64', 'x86_64': 'x64', 'amd64': 'x64',
               'arm64': 'arm64', 'aarch64': 'arm64'}
    parts = str(value).lower().split(',') if value else []
    if not parts or (not multiple and len(parts) != 1) or any(p not in aliases for p in parts):
        raise ValueError('Missing or unsupported expected target architecture')
    return set(aliases[p] for p in parts)


def audit_native_tree(root, targets, family):
    root = root.resolve()
    counts = dict.fromkeys(('native_files', 'native_units', 'native_slices', 'malformed_native_units',
                            'archive_files', 'archive_members', 'native_archive_members',
                            'archive_metadata_members', 'neutral_archive_members',
                            'unresolved_thin_external_members', 'symlinks_not_followed'), 0)
    errors = []
    expected = ','.join(sorted(targets))

    def fail(relative, observed, member=None):
        observed = observed.replace('_', '/')
        detail = '\nfile={};observed={};expected={}'.format(relative, observed, expected)
        if member is not None:
            detail += ';member=' + member
        errors.append(detail)

    def native(data, relative, member=None, index=0, offset=0):
        record = {'path': relative, 'member': member, 'index': index, 'offset': offset,
                  'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'slices': []}
        malformed = False
        observed = None
        if data.startswith(b'\x7fELF'):
            record['format'] = 'ELF'
            if len(data) < 20 or data[5] not in (1, 2):
                malformed = True; observed = 'malformed_truncated_ELF'
            else:
                machine = struct.unpack_from('<H' if data[5] == 1 else '>H', data, 18)[0]
                arch = {62: 'x64', 183: 'arm64', 3: 'i386', 40: 'arm'}.get(machine, 'unknown_'+str(machine))
                bits = {1: 32, 2: 64}.get(data[4], 0)
                record['slices'].append({'machine': arch, 'bits': bits, 'byte_order': data[5],
                                         'sha256': record['sha256'], 'bytes': len(data)})
                observed = 'ELF{}/{}{}'.format(bits, arch, '/bigendian' if data[5] != 1 else '')
                if len(data) < (64 if bits == 64 else 52) or not bits:
                    malformed = True; observed = 'malformed_truncated_ELF_header'
                elif family == 'ELF' and bits == 64 and data[5] == 1 and arch in targets:
                    observed = None
        elif data[:4] in (b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xce', b'\xfe\xed\xfa\xcf',
                           b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca', b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'):
            record['format'] = 'Mach-O'
            cpu_names = {0x1000007: 'x64', 0x100000c: 'arm64', 7: 'i386', 12: 'arm'}

            def thin(body, advertised=None, position=0):
                magic = body[:4]
                if magic not in (b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xce', b'\xfe\xed\xfa\xcf'):
                    raise ValueError('malformed_Mach_thin_magic')
                order = '<' if magic in (b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe') else '>'
                bits = 64 if magic in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf') else 32
                if len(body) < (32 if bits == 64 else 28):
                    raise ValueError('malformed_truncated_Mach')
                cpu = struct.unpack_from(order+'I', body, 4)[0]
                arch = cpu_names.get(cpu, 'unknown_'+hex(cpu))
                part = {'machine': arch, 'bits': bits, 'index': position, 'bytes': len(body),
                        'sha256': hashlib.sha256(body).hexdigest()}
                if advertised is not None:
                    part['advertised_cpu'] = hex(advertised)
                record['slices'].append(part)
                if advertised is not None and advertised != cpu:
                    raise ValueError('malformed_Mach_fat_CPU_mismatch')

            try:
                if data[:4] in (b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca', b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'):
                    order = '>' if data[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf') else '<'
                    fat64 = data[:4] in (b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca')
                    width = 32 if fat64 else 20
                    if len(data) < 8:
                        raise ValueError('malformed_truncated_Mach_fat')
                    total = struct.unpack_from(order+'I', data, 4)[0]
                    if not total or total > (len(data)-8)//width:
                        raise ValueError('malformed_empty_or_truncated_Mach_fat')
                    spans = []
                    for position in range(total):
                        values = struct.unpack_from(order+('IIQQII' if fat64 else '5I'), data, 8+width*position)
                        cpu, subtype, begin, size, align = values[:5]
                        if not size or begin < 8+total*width or begin > len(data) or size > len(data)-begin:
                            raise ValueError('malformed_out_of_bounds_Mach_fat')
                        if any(begin < end and start < begin+size for start, end in spans):
                            raise ValueError('malformed_overlapping_Mach_fat')
                        spans.append((begin, begin+size))
                        thin(data[begin:begin+size], cpu, position)
                else:
                    thin(data)
                arches = {p['machine'] for p in record['slices']}
                unsupported = [p for p in record['slices'] if p['bits'] != 64 or p['machine'] not in ('x64', 'arm64')]
                if family == 'Mach-O' and not unsupported and targets.issubset(arches):
                    observed = None
                else:
                    observed = 'Mach{}/{}'.format(','.join(str(p['bits']) for p in record['slices']), ','.join(sorted(arches)))
            except (ValueError, struct.error) as error:
                malformed = str(error) != 'malformed_Mach_fat_CPU_mismatch'
                observed = str(error)
        elif data.startswith(b'MZ') or (len(data) >= 2 and struct.unpack_from('<H', data)[0] in (0x8664, 0xaa64, 0x14c, 0x1c4)):
            record['format'] = 'foreign_PE_COFF'; observed = 'unsupported_foreign_PE_COFF'
            malformed = True
        else:
            return False
        counts['native_units'] += 1
        counts['native_slices'] += len(record['slices'])
        counts['malformed_native_units'] += int(malformed)
        print('V64_NATIVE_UNIT ' + json.dumps(record, separators=(',', ':')))
        if observed:
            fail(relative, observed, member)
        return True

    def archive(data, relative):
        counts['archive_files'] += 1
        cursor = 8; table = b''; index = 0; found = False
        thin_archive = data[:8] == b'!<thin>\n'
        while cursor < len(data):
            raw = data[cursor:cursor+60]
            if len(raw) != 60 or raw[58:] != b'`\n':
                fail(relative, 'malformed_truncated_archive'); break
            try:
                stored = raw[:16].decode('ascii').rstrip(); size = int(raw[48:58])
                if size < 0: raise ValueError('negative size')
            except (ValueError, UnicodeError):
                fail(relative, 'malformed_archive_header'); break
            cursor += 60
            metadata = stored in ('/', '//', '__.SYMDEF', '__.SYMDEF SORTED')
            external = thin_archive and not metadata
            name = stored; body = b''
            if not external:
                if size > len(data)-cursor:
                    fail(relative, 'malformed_truncated_archive'); break
                body = data[cursor:cursor+size]
            body_offset = cursor
            try:
                if stored == '//':
                    table = body
                elif stored.startswith('#1/'):
                    length = int(stored[3:])
                    if length > len(body): raise ValueError('name outside member')
                    name = body[:length].decode('utf-8'); body = body[length:]; body_offset += length
                elif stored.startswith('/') and stored[1:].isdigit():
                    start = int(stored[1:]); end = table.index(b'/\n', start); name = table[start:end].decode('utf-8')
                elif stored.endswith('/') and not metadata:
                    name = stored[:-1]
            except (ValueError, UnicodeError):
                fail(relative, 'malformed_archive_name'); break
            item = {'path': relative, 'name': name, 'index': index, 'offset': body_offset,
                    'classification': 'metadata' if metadata else ('external_unresolved' if external else 'neutral')}
            if external:
                counts['archive_members'] += 1; counts['unresolved_thin_external_members'] += 1
                fail(relative, 'thin_archive', name)
            else:
                item.update(bytes=len(body), sha256=hashlib.sha256(body).hexdigest())
                if metadata:
                    counts['archive_metadata_members'] += 1
                else:
                    counts['archive_members'] += 1
                    if native(body, relative, name, index, body_offset):
                        found = True; counts['native_archive_members'] += 1; item['classification'] = 'native'
                    else:
                        counts['neutral_archive_members'] += 1
                cursor += size + size % 2
            print('V64_NATIVE_MEMBER ' + json.dumps(item, separators=(',', ':')))
            index += 1
        return found

    def contained_link(path):
        pending = list(path.relative_to(root).parts)
        base = root
        followed = 0
        while pending:
            part = pending.pop(0)
            if part == '..':
                base = base.parent
                if base != root and root not in base.parents:
                    return False
                continue
            if part == '.':
                continue
            candidate = base / part
            if candidate.is_symlink():
                followed += 1
                if followed > 40:
                    return False
                target = Path(os.readlink(candidate))
                if target.is_absolute():
                    if target != root and root not in target.parents:
                        return False
                    base = root
                    pending = list(target.relative_to(root).parts) + pending
                else:
                    pending = list(target.parts) + pending
            else:
                base = candidate
        return base.exists()

    for directory, dirs, files in os.walk(root, followlinks=False):
        for name in sorted(dirs+files):
            path = Path(directory)/name
            relative = str(path.relative_to(root))
            if path.is_symlink():
                target = os.readlink(path); counts['symlinks_not_followed'] += 1
                print('V64_NATIVE_FILE ' + json.dumps({'path': relative, 'symlink': target}, separators=(',', ':')))
                if relative == 'Applications' and target == '/Applications' and family == 'Mach-O':
                    continue
                if not contained_link(path):
                    fail(relative, 'escaped_or_broken_symlink')
                continue
            if not path.is_file():
                continue
            data = path.read_bytes()
            print('V64_NATIVE_FILE ' + json.dumps({'path': relative, 'sha256': hashlib.sha256(data).hexdigest()}, separators=(',', ':')))
            if data[:8] in (b'!<arch>\n', b'!<thin>\n'):
                found = archive(data, relative)
            else:
                found = native(data, relative)
            counts['native_files'] += int(found)
    print('V64_NATIVE_COUNTS ' + json.dumps(dict(counts, complete=True), separators=(',', ':')))
    if errors:
        raise RuntimeError(errors[0])


if len(sys.argv) != 4:
    raise ValueError("Missing expected target architecture")
root, work = map(Path, sys.argv[1:3])
expected = expected_target(sys.argv[3])
audit_native_tree(root, expected, 'ELF')
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

# License/layout checks precede every loader invocation.
for path in elf_files:
    # Audit the real loader's resolution too; host-installed dependencies cannot
    # turn a missing payload entry into success merely because ldd finds them.
    resolved = command([loader, "--inhibit-cache", "--library-path", str(lib), "--list", str(path)])
    if "not found" in resolved:
        raise RuntimeError("Unresolved runtime dependency: " + path.name)
    for name, target in re.findall(r"^\s*(\S+) => (\S+)", resolved, re.M):
        if not os_library.fullmatch(name) and not contained(Path(target)):
            raise RuntimeError("Loader escaped package: " + name)
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

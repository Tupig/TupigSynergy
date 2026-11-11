#!/bin/bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail

if [ "$(uname -s)" != Darwin ]; then
  echo "This verifier requires macOS." >&2
  exit 1
fi
python3 - "$@" <<'PY'
import hashlib
import os
import json
import struct
import plistlib
import re
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout


def inside(path, root):
    return path == root or root in path.parents


def system(path):
    return path.startswith('/usr/lib/') or path.startswith('/System/Library/')


def macho(path):
    with path.open('rb') as stream:
        return stream.read(4) in (
            b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe',
            b'\xfe\xed\xfa\xcf', b'\xcf\xfa\xed\xfe',
            b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
            b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca',
        )


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
        raise ValueError(errors[0])


def version(value):
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}', str(value)):
        raise ValueError('Invalid macOS deployment version: ' + str(value))
    parts = tuple(int(part) for part in str(value).split('.'))
    return parts + (0,) * (3 - len(parts))


def audit(app, expected):
    app = app.resolve()
    with (app / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
    declared_floor = version(info['LSMinimumSystemVersion'])
    name = info['CFBundleExecutable']
    if Path(name).name != name:
        raise ValueError('CFBundleExecutable must be a filename')
    gui = app / 'Contents/MacOS' / name
    core = app / 'Contents/MacOS/synergy-core'
    images = set()
    for directory, dirs, files in os.walk(app, followlinks=False):
        for entry in dirs + files:
            path = Path(directory) / entry
            if path.is_symlink() and (not path.exists() or not inside(path.resolve(), app)):
                raise ValueError('Broken or escaping bundle symlink: ' + str(path.relative_to(app)))
        for entry in files:
            path = Path(directory) / entry
            if path.is_file() and macho(path):
                images.add(path.resolve())
    if gui.resolve() not in images or core.resolve() not in images:
        raise ValueError('Both GUI and synergy-core Mach-O executables are required')
    cache = {}
    architectures = {}

    def commands(image, arch):
        key = (image, arch)
        if key not in cache:
            output = run(['/usr/bin/otool', '-arch', arch, '-l', str(image)])
            dependencies, rpaths, minima = [], [], []
            command = ''
            for line in output.splitlines():
                line = line.strip()
                if line.startswith('cmd '):
                    command = line[4:]
                elif command == 'LC_BUILD_VERSION' and line.startswith('minos '):
                    minima.append(version(line.split()[1]))
                elif command == 'LC_VERSION_MIN_MACOSX' and line.startswith('version '):
                    minima.append(version(line.split()[1]))
                elif command == 'LC_RPATH' and line.startswith('path '):
                    rpaths.append(line[5:].rsplit(' (offset ', 1)[0])
                elif command in ('LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB',
                                 'LC_LOAD_UPWARD_DYLIB', 'LC_LAZY_LOAD_DYLIB') and line.startswith('name '):
                    dependencies.append(line[5:].rsplit(' (offset ', 1)[0])
            if len(minima) != 1:
                raise ValueError('Missing or ambiguous macOS deployment metadata: ' + str(image))
            if minima[0] > declared_floor:
                raise ValueError('Bundled image requires a newer macOS than declared: ' + str(image))
            if image in (gui.resolve(), core.resolve()) and minima[0] != declared_floor:
                raise ValueError('GUI/core deployment metadata differs from bundle declaration: ' + str(image))
            cache[key] = dependencies, rpaths
        return cache[key]

    def arches(image):
        if image not in architectures:
            architectures[image] = run(['/usr/bin/lipo', '-archs', str(image)]).split()
        return architectures[image]

    def expand(value, image, executable):
        for token, base in (('@loader_path', image.parent), ('@executable_path', executable.parent)):
            if value == token or value.startswith(token + '/'):
                return (base / value[len(token):].lstrip('/')).resolve()
        if value.startswith('/'):
            return Path(value).resolve()
        raise ValueError('Unsupported loader path: ' + value)

    visited = set()

    def visit(image, arch, executable, inherited):
        dependencies, raw_rpaths = commands(image, arch)
        local = []
        for value in raw_rpaths:
            path = expand(value, image, executable)
            if not inside(path, app) and not system(str(path) + '/'):
                raise ValueError('External LC_RPATH in ' + str(image.relative_to(app)) + ': ' + value)
            local.append(path)
        rpaths = tuple(dict.fromkeys(local + list(inherited)))
        key = (image, arch, executable, rpaths)
        if key in visited:
            return
        visited.add(key)
        for dependency in dependencies:
            if system(dependency):
                continue  # System libraries may exist only in the dyld shared cache.
            if dependency.startswith('@rpath/'):
                suffix = dependency[len('@rpath/'):]
                candidates = [(path / suffix).resolve() for path in rpaths]
                target = next((path for path in candidates if path.is_file()), None)
                if target is None:
                    raise ValueError('Unresolved dependency in ' + str(image.relative_to(app)) + ': ' + dependency)
            elif dependency.startswith(('@loader_path/', '@executable_path/')):
                target = expand(dependency, image, executable)
            else:
                raise ValueError('Non-relocatable dependency in ' + str(image.relative_to(app)) + ': ' + dependency)
            if not inside(target, app) or target not in images:
                raise ValueError('Dependency escapes or is absent from bundle: ' + dependency)
            if arch not in arches(target):
                raise ValueError('Dependency lacks architecture ' + arch + ': ' + dependency)
            visit(target, arch, executable, rpaths)

    required_arches = {"x86_64" if arch == "x64" else arch for arch in expected}
    if set(arches(gui.resolve())) != required_arches:
        raise ValueError("file=Contents/MacOS/" + name + ";observed=" + ",".join(arches(gui.resolve())) + ";expected=" + ",".join(sorted(expected)))
    if set(arches(core.resolve())) != required_arches:
        raise ValueError('file=Contents/MacOS/synergy-core;observed=' + ','.join(arches(core.resolve())) + ';expected=' + ','.join(sorted(expected)))
    for image in sorted(images):
        if not required_arches.issubset(arches(image)):
            raise ValueError('Bundled Mach-O lacks a target architecture: ' + str(image.relative_to(app)))
    for executable in (gui.resolve(), core.resolve()):
        for arch in arches(executable):
            visit(executable, arch, executable, ())
            root_rpaths = tuple(expand(value, executable, executable) for value in commands(executable, arch)[1])
            # Dynamically loaded plugins are not necessarily reachable through LC_LOAD_DYLIB.
            for image in sorted(images):
                visit(image, arch, executable, root_rpaths)
    # Audit additional universal-library slices too; they must not hide external paths.
    for image in sorted(images):
        for arch in set(arches(image)) - required_arches:
            visit(image, arch, gui.resolve(), ())
    print('PASS: dependency closure for', len(images), 'Mach-O images; GUI and core plus plugins audited')
    return gui, core


def launch(gui, core, root):
    home = root / 'home'
    config = root / 'config'
    state = root / 'state'
    runtime = root / 'runtime'
    for path in (home, config, state, runtime):
        path.mkdir(mode=0o700)
    # Native-format legacy preferences are deliberately bypassed before GUI onPreInit.
    # Settings::UserDir uses kAppName, which is the branded bundle executable name.
    app_name = gui.name
    extra = home / 'Library' / app_name
    extra.mkdir(parents=True)
    (extra / (app_name + '.extra.conf')).write_text(
        '[migration]\nschemaVersion=2147483647\nnotifiedFor=2147483647\n[scope]\npreferSystem=false\n'
    )
    private = config / app_name
    private.mkdir()
    settings = private / (app_name + '.conf')
    settings.write_text('[core]\ncomputerName=package-smoke\npreventSleep=false\nport=0\n'
                        'enableEnterCommand=false\nenableExitCommand=false\n'
                        '[gui]\nstartCoreWithGui=false\n[log]\nlevel=DEBUG\ntoFile=false\n')
    env = {'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'HOME': str(home),
           'TMPDIR': str(root) + '/', 'XDG_CONFIG_HOME': str(config),
           'XDG_STATE_HOME': str(state), 'XDG_RUNTIME_DIR': str(runtime),
           'QT_QPA_PLATFORM': 'cocoa', 'QT_DEBUG_PLUGINS': '1',
           'DYLD_PRINT_LIBRARIES': '1', 'LC_ALL': 'C'}

    def check_loaded(output):
        loaded = set()
        for line in output.splitlines():
            match = re.match(r'dyld(?:\[\d+\])?:\s+(?:<[^>]+>\s+|loaded:\s+)?(/.*)', line)
            if not match:
                continue
            path = match.group(1).strip()
            if not system(path) and not inside(Path(path).resolve(), gui.parents[2].resolve()):
                raise ValueError('Startup loaded an external non-system image: ' + path)
            loaded.add(str(Path(path).resolve()))
        return loaded

    gui_result = subprocess.run([str(gui), '--help'], env=env, capture_output=True, text=True, timeout=30)
    (root / 'gui.log').write_text(gui_result.stdout + gui_result.stderr)
    if gui_result.returncode or 'Display Help' not in gui_result.stdout:
        raise ValueError('GUI QApplication/Cocoa startup failed (exit ' + str(gui_result.returncode) + ')')
    check_loaded(gui_result.stderr)
    print('PASS: clean-environment GUI QApplication/Cocoa help startup (no main-window/TCC acceptance)')
    # The server and identity belong only to this test. No host TLS settings,
    # keychain, certificate files or existing services are used.
    tls = private / 'tls'
    tls.mkdir(mode=0o700)
    certificate = tls / 'server.pem'
    key = tls / 'key.pem'
    openssl_config = tls / 'openssl.cnf'
    openssl_config.write_text('[req]\ndistinguished_name=dn\nx509_extensions=extensions\n'
                              '[dn]\n[extensions]\nbasicConstraints=critical,CA:TRUE\n')
    run(['/usr/bin/openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
         '-days', '1', '-subj', '/CN=package-smoke', '-config', str(openssl_config),
         '-keyout', str(key), '-out', str(certificate)], timeout=30)
    identity = tls / 'identity.pem'
    identity.write_bytes(certificate.read_bytes() + key.read_bytes())
    identity.chmod(0o600)
    der = ssl.PEM_cert_to_DER_cert(certificate.read_text())
    fingerprint = hashlib.sha256(der).hexdigest()
    (tls / 'trusted-servers').write_text('v2:sha256:' + fingerprint + '\n')
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(str(certificate), str(key))
    context.load_verify_locations(cafile=str(certificate))
    context.verify_mode = ssl.CERT_REQUIRED
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(20)
        settings.write_text(settings.read_text().replace('port=0\n', 'port=' + str(listener.getsockname()[1]) + '\n') +
                            '[client]\nremoteHost=127.0.0.1\n[security]\ntlsEnabled=true\ncertificate=' +
                            str(identity) + '\n')
        with (root / 'core.log').open('w') as log:
            process = subprocess.Popen([str(core), 'client', '--new-instance', '--settings', str(settings)],
                                       env=env, stdout=log, stderr=log)
            try:
                connection, _ = listener.accept()
                connection.settimeout(20)
                with connection, context.wrap_socket(connection, server_side=True) as secure:
                    if hashlib.sha256(secure.getpeercert(binary_form=True)).hexdigest() != fingerprint:
                        raise ValueError('Core did not present the private test identity')
                    deadline = time.monotonic() + 10
                    # Handshake alone does not prove the product accepted its peer.
                    # Its existing success log occurs only after fingerprint checks.
                    while 'peer tls certificate info: package-smoke' not in (root / 'core.log').read_text():
                        if process.poll() is not None or time.monotonic() >= deadline:
                            raise ValueError('Core did not accept the trusted TLS peer')
                        time.sleep(0.05)
                    print('PASS: clean-environment core completed', secure.version(),
                          'loopback handshake and accepted private trusted identity')
                    # No Synergy greeting is sent; input sharing never activates.
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
        loaded = check_loaded((root / 'core.log').read_text())
        if not loaded:
            raise ValueError('TLS smoke has no loaded-image evidence')
        bundle = gui.parents[2]
        if (bundle / 'Contents/PlugIns/tls/libqopensslbackend.dylib').exists():
            for stem in ('libssl.', 'libcrypto.'):
                if not any(Path(path).name.startswith(stem) for path in loaded):
                    raise ValueError('Retained Qt OpenSSL backend did not load packaged ' + stem + ' runtime')
        print('PASS: TLS loaded-image closure stays inside package/system paths')


def main():
    args = sys.argv[1:]
    audit_only = bool(args and args[0] == '--audit-only')
    if audit_only:
        args.pop(0)
    if len(args) != 3 or args[0] != '--expected-arch':
        raise ValueError('Usage: verify-package-macos.sh [--audit-only] --expected-arch <x64|arm64|x64,arm64> <build-directory-or-app-or-dmg>; expected target required')
    expected = expected_target(args[1], multiple=True)
    args = args[2:]
    source = Path(args[0]).resolve(strict=True)
    if not audit_only and source.is_dir() and source.suffix != '.app':
        packages = list(source.glob('*.dmg'))
        if len(packages) != 1:
            raise ValueError('Expected exactly one DMG in the build directory')
        source = packages[0]
    if audit_only:
        if source.suffix != '.app':
            raise ValueError('--audit-only requires an app bundle')
        audit_native_tree(source, expected, 'Mach-O')
        audit(source, expected)
        return
    if source.is_file():
        print('V64_PACKAGE_SHA256 ' + hashlib.sha256(source.read_bytes()).hexdigest())
    with tempfile.TemporaryDirectory(prefix='synergy-package-') as temporary:
        root = Path(temporary)
        mounted = False
        mount = root / 'mounted'
        try:
            if source.suffix == '.dmg':
                mount.mkdir()
                run(['/usr/bin/hdiutil', 'attach', str(source), '-readonly', '-nobrowse',
                     '-mountpoint', str(mount)], input='Y\n')
                mounted = True
                audit_native_tree(mount, expected, 'Mach-O')
                apps = list(mount.glob('*.app'))
                if len(apps) != 1:
                    raise ValueError('Expected exactly one app in the DMG')
                source = apps[0]
            if source.suffix != '.app' or not source.is_dir():
                raise ValueError('Expected an app bundle or DMG')
            if not mounted:
                audit_native_tree(source, expected, 'Mach-O')
            app = root / source.name
            run(['/usr/bin/ditto', str(source), str(app)])
        finally:
            if mounted:
                run(['/usr/bin/hdiutil', 'detach', str(mount)])
        gui, core = audit(app, expected)
        launch(gui, core, root)


try:
    main()
except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
    print('FAIL:', error, file=sys.stderr)
    sys.exit(1)
PY

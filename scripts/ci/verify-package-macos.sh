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


def audit(app):
    app = app.resolve()
    with (app / 'Contents/Info.plist').open('rb') as stream:
        info = plistlib.load(stream)
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
            dependencies, rpaths = [], []
            command = ''
            for line in output.splitlines():
                line = line.strip()
                if line.startswith('cmd '):
                    command = line[4:]
                elif command == 'LC_RPATH' and line.startswith('path '):
                    rpaths.append(line[5:].rsplit(' (offset ', 1)[0])
                elif command in ('LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB',
                                 'LC_LOAD_UPWARD_DYLIB', 'LC_LAZY_LOAD_DYLIB') and line.startswith('name '):
                    dependencies.append(line[5:].rsplit(' (offset ', 1)[0])
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

    required_arches = set(arches(gui.resolve()))
    if set(arches(core.resolve())) != required_arches:
        raise ValueError('GUI and core must support the same architectures')
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
    if len(args) != 1:
        raise ValueError('Usage: verify-package-macos.sh [--audit-only] <build-directory-or-app-or-dmg>')
    source = Path(args[0]).resolve(strict=True)
    if not audit_only and source.is_dir() and source.suffix != '.app':
        packages = list(source.glob('*.dmg'))
        if len(packages) != 1:
            raise ValueError('Expected exactly one DMG in the build directory')
        source = packages[0]
    if audit_only:
        if source.suffix != '.app':
            raise ValueError('--audit-only requires an app bundle')
        audit(source)
        return
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
                apps = list(mount.glob('*.app'))
                if len(apps) != 1:
                    raise ValueError('Expected exactly one app in the DMG')
                source = apps[0]
            if source.suffix != '.app' or not source.is_dir():
                raise ValueError('Expected an app bundle or DMG')
            app = root / source.name
            run(['/usr/bin/ditto', str(source), str(app)])
        finally:
            if mounted:
                run(['/usr/bin/hdiutil', 'detach', str(mount)])
        gui, core = audit(app)
        launch(gui, core, root)


try:
    main()
except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
    print('FAIL:', error, file=sys.stderr)
    sys.exit(1)
PY

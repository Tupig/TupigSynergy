#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Supply exact-version source notices for the prebuilt Qt CI exception."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import re
import tarfile
import urllib.request


SOURCE_SHA256 = {
    ('6.9.3', 'qtbase'): 'c5a1a2f660356ec081febfa782998ae5ddbc5925117e64f50e4be9cd45b8dc6e',
    ('6.9.3', 'qtsvg'): 'db76aa3358cbbe6fce7da576ff4669cb9801920188c750d3b12783bbe97026e2',
    ('6.10.3', 'qtbase'): '383dc907816338f0cba72088a524c07458dfc69ce684ca9132fcc4fe91c24b0b',
    ('6.10.3', 'qtsvg'): 'b3223fe005f6a4c7f21f34e4ee6ce0094737cff9503ba15bbb171ac18794f76d',
    ('6.11.1', 'qtbase'): 'd9594a31228aa23ad6b531719a29b45f0f3989fe6c136d45767ea179f233c1ac',
    ('6.11.1', 'qtsvg'): '7f3cf02f4824bf03c2c5859ea6db173bf1482a1daf24e6cdf7bc78cfa26a8a94',
}
MODULES = {'qtbase': 'Core', 'qtsvg': 'Svg'}


def digest_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def source_url(version, module):
    if (version, module) not in SOURCE_SHA256:
        raise ValueError('Unsupported selected Qt source version/module')
    series = '.'.join(version.split('.')[:2])
    return ('https://download.qt.io/archive/qt/' + series + '/' + version +
            '/submodules/' + module + '-everywhere-src-' + version + '.tar.xz')


def contained_file(root, relative):
    path = PurePosixPath(relative)
    if path.is_absolute() or '..' in path.parts or '\\' in relative or not path.parts:
        raise ValueError('Unsafe notice path')
    root = Path(root).resolve()
    file = root.joinpath(*path.parts)
    real = file.resolve(strict=True)
    if root not in real.parents or not real.is_file() or file.is_symlink():
        raise ValueError('Notice/provider path escapes selected root or is not a regular file')
    if real.stat().st_size == 0:
        raise ValueError('Empty notice/provider file')
    return real


def inspect_sdk(sdk, version):
    for module in MODULES:
        source_url(version, module)
    sdk = Path(sdk).resolve(strict=True)
    identity = {}
    for module, target in MODULES.items():
        config = 'lib/cmake/Qt6' + target + '/Qt6' + target + 'ConfigVersionImpl.cmake'
        config_file = contained_file(sdk, config)
        match = re.search(r'set\(PACKAGE_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)"\)',
                          config_file.read_text())
        if not match or match.group(1) != version:
            raise ValueError('Selected Qt module version mismatch: ' + module)
        candidates = ['bin/Qt6' + target + '.dll',
                      'lib/Qt' + target + '.framework/Versions/A/Qt' + target]
        providers = [name for name in candidates if (sdk / name).is_file()]
        if len(providers) != 1:
            raise ValueError('Missing or ambiguous selected Qt module provider: ' + module)
        files = {config: digest_file(config_file)}
        files[providers[0]] = digest_file(contained_file(sdk, providers[0]))
        for file in sorted((sdk / 'sbom').glob(module + '-' + version + '.*')):
            relative = file.relative_to(sdk).as_posix()
            files[relative] = digest_file(contained_file(sdk, relative))
        identity[module] = files
    return identity


def collect_source_notices(archive, expected_hash, module, version, output):
    if digest_file(archive) != expected_hash:
        raise ValueError('Qt source archive SHA256 mismatch: ' + module)
    prefix = module + '-everywhere-src-' + version
    output = Path(output)
    copied = {}
    with tarfile.open(archive, 'r:xz') as source:
        members = {}
        for member in source.getmembers():
            path = PurePosixPath(member.name)
            canonical = path.as_posix()
            if (path.is_absolute() or '..' in path.parts or '\\' in member.name or
                    canonical != member.name.rstrip('/') or
                    member.name.endswith('/') and not member.isdir() or
                    not path.parts or path.parts[0] != prefix or canonical in members):
                raise ValueError('Unsafe, duplicate or wrong-version Qt source archive path')
            members[canonical] = member

        def read(relative):
            member = members.get(prefix + '/' + relative)
            if member is None or not member.isfile() or not member.size:
                raise ValueError('Missing/empty/non-regular Qt source notice: ' + relative)
            if member.size > 16 * 1024 * 1024:
                raise ValueError('Unexpectedly large Qt notice file')
            return source.extractfile(member).read()

        def copy(relative):
            data = read(relative)
            destination = output / module / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.is_symlink() or destination.exists() and destination.read_bytes() != data:
                raise ValueError('Conflicting Qt notice output')
            destination.write_bytes(data)
            copied[module + '/' + relative] = hashlib.sha256(data).hexdigest()

        config = read('.cmake.conf').decode('utf-8')
        if not re.search(r'set\(QT_REPO_MODULE_VERSION\s+"' + re.escape(version) + r'"\)', config):
            raise ValueError('Qt source module version mismatch')
        license_paths = [name[len(prefix) + 1:] for name, member in members.items()
                         if member.isfile() and name.startswith(prefix + '/LICENSES/')]
        if not license_paths:
            raise ValueError('Qt source has no full license texts')
        for path in license_paths:
            copy(path)
        attributions = [name[len(prefix) + 1:] for name, member in members.items()
                        if member.isfile() and name.startswith(prefix + '/src/') and
                        name.endswith('/qt_attribution.json')]
        for path in attributions:
            copy(path)
            # Published Qt metadata contains multiline strings. It remains data.
            entries = json.loads(read(path), strict=False)
            if isinstance(entries, dict):
                entries = [entries]
            if not isinstance(entries, list):
                raise ValueError('Unsupported Qt attribution metadata')
            for entry in entries:
                if not isinstance(entry, dict):
                    raise ValueError('Unsupported Qt attribution entry')
                references = entry.get('LicenseFiles', [])
                if not isinstance(references, list):
                    raise ValueError('Invalid Qt license file list')
                references = list(references)
                for key in ('LicenseFile', 'CopyrightFile'):
                    if entry.get(key):
                        references.append(entry[key])
                for reference in references:
                    if not isinstance(reference, str) or '\\' in reference or reference.startswith('/'):
                        raise ValueError('Invalid Qt attribution notice reference')
                    relative = posixpath.normpath(posixpath.join(posixpath.dirname(path), reference))
                    if relative.startswith('../') or relative == '..':
                        raise ValueError('Qt attribution notice escapes module')
                    copy(relative)
                if not entry.get('LicenseFile') and not entry.get('LicenseFiles'):
                    identifiers = set(re.findall(r'[A-Za-z0-9.+-]+', entry.get('LicenseId', '')))
                    identifiers -= {'AND', 'OR', 'WITH'}
                    if not identifiers:
                        raise ValueError('Qt attribution lacks a full license reference')
                    for identifier in identifiers:
                        copy('LICENSES/' + identifier + '.txt')
    return copied


def collect(sdk, version, cache, output):
    identity = inspect_sdk(sdk, version)
    cache, output = Path(cache), Path(output)
    cache.mkdir(parents=True, exist_ok=True)
    if output.exists() and any(output.iterdir()):
        raise ValueError('Qt notice collection requires a fresh output; verify existing output against its checkpoint')
    output.mkdir(parents=True, exist_ok=True)
    files, sources = {}, []
    for module in MODULES:
        url = source_url(version, module)
        archive = cache / url.rsplit('/', 1)[1]
        expected_hash = SOURCE_SHA256[(version, module)]
        if not archive.exists():
            partial = archive.with_suffix(archive.suffix + '.partial')
            with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as stream:
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    stream.write(block)
            if digest_file(partial) != expected_hash:
                raise ValueError('Downloaded Qt source SHA256 mismatch: ' + module)
            partial.replace(archive)
        source_files = collect_source_notices(archive, expected_hash, module, version, output)
        files.update(source_files)
        sources.append({'module': module, 'source_url': url, 'authority_url': url + '.mirrorlist',
                        'sha256': expected_hash, 'files': sorted(source_files)})
        for relative in identity[module]:
            if relative.startswith('sbom/'):
                data = contained_file(sdk, relative).read_bytes()
                destination = output / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
                files[relative] = hashlib.sha256(data).hexdigest()
    manifest = {'version': version, 'sdk': identity, 'sources': sources, 'files': files}
    (output / 'provenance.json').write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n')
    verify_notices(sdk, version, output)


def verify_notices(sdk, version, output, manifest_sha256=None):
    output = Path(output)
    provenance = contained_file(output, 'provenance.json')
    if manifest_sha256 is not None and digest_file(provenance) != manifest_sha256:
        raise ValueError('Qt notice manifest differs from the collection checkpoint')
    manifest = json.loads(provenance.read_text())
    if manifest.get('version') != version or manifest.get('sdk') != inspect_sdk(sdk, version):
        raise ValueError('Qt notice provenance does not match the selected SDK')
    expected_sources = [{'module': module, 'source_url': source_url(version, module),
                         'authority_url': source_url(version, module) + '.mirrorlist',
                         'sha256': SOURCE_SHA256[(version, module)]} for module in MODULES]
    sources = manifest.get('sources', [])
    if len(sources) != len(expected_sources) or not manifest.get('files'):
        raise ValueError('Qt notice source provenance mismatch')
    required = set()
    for source, expected in zip(sources, expected_sources):
        inventory = source.get('files', [])
        if ({key: value for key, value in source.items() if key != 'files'} != expected or
                not isinstance(inventory, list) or len(inventory) != len(set(inventory))):
            raise ValueError('Qt source inventory provenance mismatch')
        module = expected['module']
        if any(not path.startswith(module + '/') for path in inventory):
            raise ValueError('Qt source inventory crosses modules')
        licenses = [path for path in inventory if path.startswith(module + '/LICENSES/')]
        attributions = [path for path in inventory if path.startswith(module + '/src/') and
                        path.endswith('/qt_attribution.json')]
        if not licenses or not attributions:
            raise ValueError('Qt source module lacks full license texts or production attributions')
        required.update(inventory)
        # Check every attribution reference against the source-backed inventory.
        for path in attributions:
            entries = json.loads(contained_file(output, path).read_bytes(), strict=False)
            for entry in entries if isinstance(entries, list) else [entries]:
                references = list(entry.get('LicenseFiles', []))
                for key in ('LicenseFile', 'CopyrightFile'):
                    if entry.get(key):
                        references.append(entry[key])
                for reference in references:
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(path), reference))
                    if target not in inventory:
                        raise ValueError('Qt source inventory omits an attribution notice')
                if not entry.get('LicenseFile') and not entry.get('LicenseFiles'):
                    identifiers = set(re.findall(r'[A-Za-z0-9.+-]+', entry.get('LicenseId', '')))
                    identifiers -= {'AND', 'OR', 'WITH'}
                    if not identifiers or any(module + '/LICENSES/' + identifier + '.txt' not in inventory
                                              for identifier in identifiers):
                        raise ValueError('Qt source inventory omits full standard license text')
    required.update(path for files in manifest['sdk'].values() for path in files if path.startswith('sbom/'))
    if set(manifest['files']) != required:
        raise ValueError('Qt notice manifest omits source inventory files')
    for relative, expected_hash in manifest['files'].items():
        if digest_file(contained_file(output, relative)) != expected_hash:
            raise ValueError('Qt notice content hash mismatch: ' + relative)
    actual = {file.relative_to(output).as_posix() for file in output.rglob('*') if file.is_file()}
    if actual != set(manifest['files']) | {'provenance.json'}:
        raise ValueError('Unexpected Qt notice payload files')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--manifest-sha256')
    parser.add_argument('--github-output', type=Path)
    args = parser.parse_args()
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise ValueError('Prebuilt Qt source notices are a CI-only dependency exception')
    if args.verify:
        if not args.manifest_sha256 or not re.fullmatch(r'[0-9a-f]{64}', args.manifest_sha256):
            parser.error('--verify requires the original collection --manifest-sha256')
        verify_notices(args.sdk_root, args.version, args.output, args.manifest_sha256)
    else:
        if args.cache is None:
            parser.error('--cache is required when collecting')
        collect(args.sdk_root, args.version, args.cache, args.output)
    if args.github_output:
        if args.verify:
            parser.error('--github-output is only available after collection')
        with args.github_output.open('a') as stream:
            stream.write('root=' + str(args.output.resolve()) + '\n')
            stream.write('sha256=' + digest_file(args.output / 'provenance.json') + '\n')
    print('PASS: selected Qt ' + args.version + ' source notices verified')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, tarfile.TarError) as error:
        raise SystemExit('ERROR: ' + str(error))

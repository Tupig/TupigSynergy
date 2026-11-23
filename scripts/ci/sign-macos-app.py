#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Sign every nested Mach-O before the application and verify its identity."""
import argparse
import os
from pathlib import Path
import plistlib
import re
import subprocess
import sys

MACHO_MAGICS = {
    bytes.fromhex(value) for value in
    ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")
}
APP_ID = "com.tupig.synergy"
CORE_ID = "com.tupig.synergy.core"


def checked(arguments, capture=False):
    return subprocess.run(arguments, check=True, stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None)


def signing_identity(selected, keychain, mode):
    if not selected or selected == "-":
        return "-", None
    command = ["/usr/bin/security", "find-identity", "-v", "-p", "codesigning"]
    if keychain:
        command.append(keychain)
    output = checked(command, capture=True).stdout.decode()
    identities = re.findall(r'^\s*\d+\)\s+([0-9A-Fa-f]{40})\s+"([^"]+)"', output, re.MULTILINE)
    matches = [(fingerprint.upper(), name) for fingerprint, name in identities
               if fingerprint.lower() == selected.lower() or name == selected]
    if len(matches) != 1:
        raise ValueError("The exact selected signing identity is unavailable or ambiguous.")
    fingerprint, name = matches[0]
    prefix = {"development": "Apple Development:", "developer-id": "Developer ID Application:"}[mode]
    if not name.startswith(prefix):
        raise ValueError("The selected certificate does not match the signing mode.")
    return fingerprint, fingerprint


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--identity", default="-")
    parser.add_argument("--keychain")
    parser.add_argument("--mode", choices=("development", "developer-id"), default="developer-id")
    parser.add_argument("--installer-template", type=Path, required=True)
    args = parser.parse_args()
    app = args.app.resolve(strict=True)
    info_path = app / "Contents/Info.plist"
    with info_path.open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleIdentifier") != APP_ID:
        raise ValueError("Unexpected application bundle identifier.")
    executable = info.get("CFBundleExecutable", "")
    if not executable or Path(executable).name != executable:
        raise ValueError("Invalid application executable name.")
    gui = app / "Contents/MacOS" / executable
    core = app / "Contents/MacOS/synergy-core"
    if not gui.is_file() or not core.is_file() or gui.is_symlink() or core.is_symlink():
        raise ValueError("Expected application and core executables are required.")
    identity, fingerprint = signing_identity(args.identity, args.keychain, args.mode)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        checked(["/usr/bin/xattr", "-dr", "com.apple.quarantine", str(app)])
    options = ["/usr/bin/codesign", "--force", "--sign", identity]
    if fingerprint:
        options += ["--options", "runtime", "--timestamp" if args.mode == "developer-id" else "--timestamp=none"]
    if args.keychain:
        options += ["--keychain", args.keychain]

    def sign(path, identifier=None):
        command = options.copy()
        if identifier:
            command += ["--identifier", identifier]
        checked(command + [str(path)])

    machos = []
    for path in (app / "Contents").rglob("*"):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            if stream.read(4) in MACHO_MAGICS:
                machos.append(path)
    # Code resources and plugins must have the same signer as the hardened app.
    for path in sorted(machos):
        if path not in (gui, core):
            sign(path)
    bundles = [path for path in (app / "Contents").rglob("*")
               if path.is_dir() and not path.is_symlink() and path.suffix in (".framework", ".bundle", ".app", ".xpc")]
    for path in sorted(bundles, key=lambda item: len(item.parts), reverse=True):
        sign(path)
    sign(core, CORE_ID)
    sign(app, APP_ID)
    for path in machos + [app]:
        requirement = []
        if fingerprint:
            requirement = ["-R", '=certificate leaf = H"' + fingerprint + '"']
        checked(["/usr/bin/codesign", "--verify", "--strict", "--all-architectures"] + requirement + [str(path)])
    checked(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(app)])
    for path, identifier in ((app, APP_ID), (core, CORE_ID)):
        checked(["/usr/bin/codesign", "--verify", "--strict", "-R", '=identifier "' + identifier + '"', str(path)])
    installer = app.parent / "Install.command"
    if fingerprint:
        template = args.installer_template.read_text()
        template = template.replace("@CMAKE_PROJECT_PROPER_NAME@", app.stem).replace("@APPLE_CODESIGN_ID@", fingerprint)
        installer.write_text(template)
        installer.chmod(0o755)
    elif installer.exists():
        installer.unlink()
    print("Application and core identities verified. Signing mode: " + (args.mode if fingerprint else "ad-hoc"))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("Mac signing failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)

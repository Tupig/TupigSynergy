#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Validate one encrypted Apple Development P12 and configure this repository."""
import argparse
import base64
import getpass
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

PROMPT = "\u8bf7\u8f93\u5165\u5bfc\u51fa\u201c\u8bc1\u4e66.p12\u201d\u65f6\u8bbe\u7f6e\u7684\u5bc6\u7801\u3002\u5bc6\u7801\u53ea\u7528\u4e8e\u672c\u673a\u6821\u9a8c\u8bc1\u4e66\uff0c\u5e76\u4fdd\u5b58\u5230 GitHub \u52a0\u5bc6\u673a\u5bc6\u4e2d\u3002"
CANCEL_BUTTON = "\u53d6\u6d88"
UPLOAD_BUTTON = "\u6821\u9a8c\u5e76\u4e0a\u4f20"
REPOSITORY = "Tupig/TupigSynergy"
ROOT = Path(__file__).resolve().parents[1]


def run(arguments, data=None):
    return subprocess.run(arguments, input=data, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=False)


def fail(message):
    raise RuntimeError(message)


def password_input(gui):
    if gui:
        cancel = json.dumps(CANCEL_BUTTON, ensure_ascii=False)
        upload = json.dumps(UPLOAD_BUTTON, ensure_ascii=False)
        script = (
            'text returned of (display dialog ' + json.dumps(PROMPT, ensure_ascii=False)
            + ' default answer "" with hidden answer buttons {' + cancel + ', ' + upload
            + '} default button ' + upload + ' cancel button ' + cancel
            + ' with title "TuPig Synergy")'
        )
        result = run(["/usr/bin/osascript", "-e", script])
        if result.returncode:
            fail("Password entry cancelled. No Secrets changed.")
        password = result.stdout.rstrip(b"\r\n")
    else:
        password = getpass.getpass("P12 export password: ").encode("utf-8")
    if not password or b"\n" in password or b"\r" in password:
        fail("A nonempty single-line export password is required.")
    return password


def validate(p12, password, openssl):
    if p12.is_symlink() or not p12.is_file():
        fail("The selected P12 must be a regular file.")
    if p12.stat().st_size > 36 * 1024:
        fail("The encrypted P12 exceeds GitHub's Secret size limit.")
    encrypted_bytes = p12.read_bytes()
    encoded = base64.b64encode(encrypted_bytes)
    if len(encoded) > 48 * 1024:
        fail("The encrypted P12 exceeds GitHub's Secret size limit.")
    with tempfile.TemporaryDirectory(prefix="tupig-signing-verify-") as directory:
        snapshot = Path(directory) / "selected.p12"
        snapshot.write_bytes(encrypted_bytes)
        snapshot.chmod(0o600)
        common = [openssl, "pkcs12", "-in", str(snapshot)]
        options = None
        for candidate in ([], ["-legacy"]):
            certificate = run(common + candidate + ["-clcerts", "-nokeys", "-passin", "stdin"], password + b"\n")
            if certificate.returncode == 0:
                options = candidate
                break
        if options is None:
            fail("The P12 export password is incorrect or its encryption is unsupported. No Secrets changed.")
        empty = run(common + options + ["-clcerts", "-nokeys", "-passin", "stdin"], b"\n")
        if empty.returncode == 0:
            fail("Re-export the P12 with a nonempty encryption password.")
        private_key = run(common + options + ["-nocerts", "-nodes", "-passin", "stdin"], password + b"\n")
        key_count = len(re.findall(rb"-----BEGIN (?:RSA |EC |ENCRYPTED )?PRIVATE KEY-----", private_key.stdout))
        if private_key.returncode or certificate.stdout.count(b"-----BEGIN CERTIFICATE-----") != 1 or key_count != 1:
            fail("Export exactly one development certificate and its private key.")
        subject = run([openssl, "x509", "-subject", "-noout"], certificate.stdout)
        valid = run([openssl, "x509", "-checkend", "0", "-noout"], certificate.stdout)
        if subject.returncode or b"Apple Development:" not in subject.stdout or valid.returncode:
            fail("A current Apple Development certificate is required.")
        certificate_public = run([openssl, "x509", "-pubkey", "-noout"], certificate.stdout)
        private_public = run([openssl, "pkey", "-pubout"], private_key.stdout)
        if certificate_public.returncode or private_public.returncode or certificate_public.stdout != private_public.stdout:
            fail("The certificate and private key do not match.")
        der = run([openssl, "x509", "-outform", "DER"], certificate.stdout)
        if der.returncode:
            fail("Certificate fingerprint validation failed.")
        fingerprint = hashlib.sha1(der.stdout).hexdigest().upper().encode("ascii")
    return encoded, fingerprint


def configure(p12, gui):
    for tool in ("git", "gh", "openssl"):
        if not shutil.which(tool):
            fail("Required tool is unavailable: " + tool)
    origin = run(["git", "-C", str(ROOT), "remote", "get-url", "origin"])
    accepted = {"git@github.com:" + REPOSITORY + ".git", "https://github.com/" + REPOSITORY + ".git", "https://github.com/" + REPOSITORY}
    if origin.returncode or origin.stdout.decode().strip().lower() not in {value.lower() for value in accepted}:
        fail("This script must run from the Tupig/TupigSynergy checkout.")
    if run(["gh", "repo", "view", REPOSITORY, "--json", "nameWithOwner"]).returncode:
        fail("Sign in to GitHub CLI with repository administration access first.")
    print("Configuring encrypted signing credentials for " + REPOSITORY, flush=True)
    password = password_input(gui)
    encoded, fingerprint = validate(p12, password, shutil.which("openssl"))

    def github(arguments, data=None):
        if run(["gh"] + arguments + ["--repo", REPOSITORY], data).returncode:
            fail("GitHub configuration failed. Signing stays disabled until setup succeeds.")

    github(["variable", "set", "MACOS_SIGNING_MODE", "--body", "disabled"])
    for name, value in (("APPLE_P12_CERTIFICATE", encoded), ("APPLE_P12_PASSWORD", password), ("APPLE_CODESIGN_ID", fingerprint)):
        github(["secret", "set", name], value)
        print("Configured GitHub Secret " + name, flush=True)
    github(["variable", "set", "MACOS_SIGNING_MODE", "--body", "development"])
    print("Configured development signing for " + REPOSITORY + ". Temporary encrypted verification file removed.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui", action="store_true", help="Read the export password through a hidden macOS dialog")
    parser.add_argument("p12", nargs="?", type=Path)
    arguments = parser.parse_args()
    p12 = arguments.p12 or Path(input("Encrypted P12 path: ").strip())
    try:
        configure(p12.expanduser(), arguments.gui)
    except (OSError, RuntimeError) as error:
        print("Certificate setup failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

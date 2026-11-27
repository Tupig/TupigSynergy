#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Contract checks for macOS development signing and first installation.

Tests inspect repository artifacts or work entirely with disposable files and
disk images. They do not load a Keychain, decode a P12, invoke GitHub, change
TCC/quarantine, or install an app. Runtime signing, notarization, Gatekeeper and
user TCC acceptance need separate trusted-run evidence.
"""

from pathlib import Path
import os
import plistlib
import re
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = ROOT / ".github/workflows/ci.yml"
KEYCHAIN = ROOT / "scripts/ci/macos-signing-keychain.sh"
SETUP = ROOT / "scripts/setup-macos-signing.sh"
SETUP_PYTHON = ROOT / "scripts/setup-macos-signing.py"
INSTALLER = ROOT / "deploy/mac/install.command.in"
DEPLOY = ROOT / "deploy/mac/deploy.cmake"
CODESIGN = ROOT / "deploy/mac/codesign.cmake.in"
SIGNER = ROOT / "scripts/ci/sign-macos-app.py"
DOCS = ROOT / "docs/macos-signing.md"


def source(path):
    return path.read_text(encoding="utf-8")


class MacOSSigningContract(unittest.TestCase):
    """Protect the approved boundary without touching credential-bearing state."""

    def test_manual_workflow_only_imports_secret_into_ephemeral_keychain(self):
        """A PR or automatic route must not acquire signing-key access."""
        workflow = source(WORKFLOW)
        keychain = source(KEYCHAIN)
        self.assertRegex(
            workflow,
            r"MAC_SIGNING:\s*\$\{\{\s*github\.event_name\s*==\s*'workflow_dispatch'",
        )
        self.assertIn("if: env.MAC_SIGNING == 'true'", workflow)
        self.assertIn("if: ${{ always() && env.MAC_SIGNING == 'true' }}", workflow)
        self.assertIn("RUNNER_TEMP", keychain)
        self.assertIn("umask 077", keychain)
        self.assertIn("security create-keychain", keychain)
        self.assertIn("security import", keychain)
        self.assertIn("security delete-keychain", keychain)
        self.assertNotIn("security list-keychains -d user -s", keychain)

    def test_selected_mode_fails_closed_and_keeps_development_notarization_separate(self):
        """An invalid mode or a development build must not reach notarization."""
        workflow = source(WORKFLOW)
        development = workflow.index('if [ "$MACOS_SIGNING_MODE" = development ]; then')
        reject = workflow.index('if [ "$MACOS_SIGNING_MODE" != developer-id ]; then')
        notarize = workflow.index("xcrun notarytool submit")
        self.assertLess(development, reject)
        self.assertLess(reject, notarize)
        self.assertIn("unsupported MACOS_SIGNING_MODE", workflow)
        self.assertIn("Development-signed DMG verified; notarization is intentionally not performed.", workflow)
        self.assertRegex(workflow, r'developer-id\)\s*test "\$MAC_SIGNING" = true')

    def test_configure_uses_the_guarded_signing_identity(self):
        """An explicit ad-hoc build cannot reuse a retained signing secret."""
        workflow = source(WORKFLOW)
        mac_job = workflow.split("  build-macos:\n", 1)[1].split("  github-release:\n", 1)[0]
        configure = mac_job.split("      - name: Configure\n", 1)[1].split("      - name:", 1)[0]
        self.assertIn('-DAPPLE_CODESIGN_ID="$MAC_CODESIGN_ID"', configure)
        self.assertNotIn("secrets.APPLE_CODESIGN_ID", configure)

    def test_keychain_decode_and_cleanup_are_macos_safe(self):
        """A macOS runner must decode locally and delete all temporary key material."""
        keychain = source(KEYCHAIN)
        self.assertIn("base64 -D", keychain)
        self.assertNotIn("base64 --decode", keychain)
        self.assertIn("certificate_file", keychain)
        self.assertIn("Path(name).unlink(missing_ok=True)", keychain)
        self.assertIn('security delete-keychain "$keychain"', keychain)
        self.assertNotIn("echo \"$APPLE_P12_CERTIFICATE\"", keychain)
        self.assertNotIn("echo \"$APPLE_P12_PASSWORD\"", keychain)

    def test_runner_search_list_and_prebuild_probe_stay_ephemeral_and_nonlogging(self):
        """The necessary search-list exception is temporary and the probe reveals no P12 data."""
        keychain = source(KEYCHAIN)
        workflow = source(WORKFLOW)
        self.assertIn("search_file=\"$runner_directory/tupig-signing.search-list\"", keychain)
        self.assertIn("['/usr/bin/security', 'list-keychains', '-d', 'user']", keychain)
        self.assertIn("sys.argv[2], *previous", keychain)
        self.assertIn("['/usr/bin/security', 'list-keychains', '-d', 'user', '-s', *previous]", keychain)
        capability = workflow.split("      - name: Check signing capability before building\n", 1)[1].split(
            "      - name: Install dependencies\n", 1)[0]
        self.assertIn("trap 'scripts/ci/macos-signing-keychain.sh cleanup' EXIT", capability)
        self.assertIn("/usr/bin/clang -x c - -o \"$probe\"", capability)
        self.assertIn("--keychain \"$SIGN_KEYCHAIN\" --sign \"$MAC_CODESIGN_ID\" \"$probe\"", capability)
        self.assertIn("Path(sys.argv[1]).unlink()", capability)
        self.assertNotIn("echo \"$APPLE_P12_CERTIFICATE\"", capability)
        self.assertNotIn("echo \"$APPLE_P12_PASSWORD\"", capability)

    def test_bundle_signing_has_stable_nested_and_outer_identities_with_verification(self):
        """A wrong order, helper ID, outer ID, or omitted strict verification breaks this."""
        template = source(CODESIGN)
        signer = source(SIGNER)
        core = signer.index("sign(core, CORE_ID)")
        outer = signer.index("sign(app, APP_ID)")
        self.assertLess(core, outer)
        self.assertIn('CORE_ID = "com.tupig.synergy.core"', signer)
        self.assertIn('APP_ID = "com.tupig.synergy"', signer)
        self.assertIn('for path, identifier in ((app, APP_ID), (core, CORE_ID)):', signer)
        self.assertIn('"--verify", "--strict", "-R",', signer)
        self.assertNotIn('sign(app, APP_ID, deep=True)', signer)
        self.assertIn("sign-macos-app.py", template)

    def test_setup_rejects_ambiguous_p12_restores_echo_and_uses_gh_variable_body(self):
        """Credential setup must not choose an arbitrary identity or leave a terminal muted."""
        setup = source(SETUP)
        implementation = source(SETUP_PYTHON)
        self.assertIn("set +x", setup)
        self.assertIn("setup-macos-signing.py", setup)
        self.assertIn("with hidden answer", implementation)
        self.assertIn("TemporaryDirectory", implementation)
        self.assertIn("certificate and private key do not match", implementation)
        self.assertIn("certificate.stdout.count", implementation)
        self.assertIn("key_count", implementation)
        self.assertIn("-checkend", implementation)
        self.assertIn("MACOS_SIGNING_MODE", implementation)
        self.assertIn("REPOSITORY = \"Tupig/TupigSynergy\"", implementation)
        self.assertNotIn("shell=True", implementation)
        self.assertIn("if p12.stat().st_size > 36 * 1024:", implementation)
        self.assertLess(implementation.index("if p12.stat().st_size > 36 * 1024:"),
                        implementation.index("encrypted_bytes = p12.read_bytes()"))

    def test_first_install_helper_is_packaged_verified_and_bounded(self):
        """A first-install helper must be usable from the DMG yet never become an updater."""
        installer = source(INSTALLER)
        deploy = source(DEPLOY)
        signer = source(SIGNER)
        self.assertIn("installer-template", source(CODESIGN))
        self.assertIn('installer = app.parent / "Install.command"', signer)
        self.assertIn("fingerprint=", installer)
        self.assertIn("core_id=", installer)
        self.assertIn("/usr/bin/codesign --verify --deep --strict", installer)
        self.assertIn("Existing installation: use the in-app updater.", installer)
        self.assertIn("/usr/bin/sudo /bin/sh \"$0\" --as-root", installer)
        self.assertNotIn("exec /usr/bin/sudo", installer)
        self.assertIn('if [ "${1:-}" != --as-root ]; then\n  finish_first_install\nfi', installer)
        self.assertIn('nohup /usr/bin/osascript', installer)
        self.assertNotIn('tell application "Terminal"', installer)
        self.assertNotIn('"detach", "-force"', installer)
        self.assertIn("com.apple.quarantine", installer)
        self.assertIn("umask 077", installer)
        self.assertIn("verify_bundle \"$stage/$app\"", installer)
        self.assertIn("verify_bundle \"$destination\"", installer)
        self.assertIn("renameatx_np", installer)
        self.assertIn("stage_parent='/private/tmp'", installer)
        self.assertIn('mktemp -d "$stage_parent/.tupig-synergy-install.XXXXXX"', installer)
        self.assertNotIn("/bin/sh -c", installer)
        self.assertNotIn('mktemp -d "$source_dir/', installer)
        self.assertNotRegex(installer, r"/usr/bin/spctl\b|\bspctl\s+--")
        self.assertNotRegex(installer, r"\btccutil\b")
        self.assertIn('/usr/bin/xattr -dr com.apple.quarantine "$stage/$app"', installer)
        self.assertNotIn('com.apple.quarantine "$source_app"', installer)
        self.assertNotIn('com.apple.quarantine "$destination"', installer)

    def test_first_install_executes_normal_and_privileged_routes_without_replacing_an_app(self):
        """Both real script branches must share the transaction and preserve an existing destination."""
        raw = source(INSTALLER)
        if "verify_bundle()" not in raw or "--as-root" not in raw:
            self.skipTest("reviewed installer transaction has not been applied yet")
        with tempfile.TemporaryDirectory(prefix="mac-signing-installer-") as temporary:
            temp = Path(temporary)
            for privileged in (False, True):
                root = temp / ("sudo" if privileged else "normal")
                self._run_install_fixture(raw, root, privileged, expect=0)
            existing = temp / "existing"
            result, destination, _, _ = self._run_install_fixture(raw, existing, False, existing=True)
            self.assertEqual(2, result.returncode)
            self.assertEqual("original", (destination / "marker").read_text())
            rejected = temp / "rejected"
            result, destination, log, _ = self._run_install_fixture(raw, rejected, False, reject_signature=True)
            self.assertNotEqual(0, result.returncode)
            self.assertFalse(destination.exists())
            self.assertFalse(log.exists())

    @unittest.skipUnless(sys.platform == "darwin", "requires a disposable Darwin disk image")
    def test_ejection_waits_for_the_invoking_process_and_matches_the_exact_image(self):
        installer = source(INSTALLER)
        match = re.search(r"^eject_worker='\n(.*?)\n'\n", installer, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(match, "The installer has no owned-image ejection worker")
        worker = ["/usr/bin/osascript", "-l", "JavaScript", "-e", match.group(1)]
        hdiutil = "/usr/bin/hdiutil"

        def attached_images():
            inventory = subprocess.check_output([hdiutil, "info", "-plist"], timeout=15)
            return {Path(record["image-path"]).resolve() for record in plistlib.loads(inventory)["images"]
                    if "image-path" in record}

        with tempfile.TemporaryDirectory(prefix="tupig-first-install-ejection-") as temporary:
            root = Path(temporary).resolve()
            payload = root / "payload"
            payload.mkdir()
            (payload / "fixture").write_text("ejection fixture", encoding="ascii")
            fixtures = []
            try:
                for name in ("target owner's disk", "untouched disk"):
                    mount = root / name
                    mount.mkdir()
                    image = root / (name + ".dmg")
                    subprocess.run([hdiutil, "create", "-quiet", "-format", "UDRO", "-srcfolder",
                                    str(payload), str(image)], check=True, timeout=35)
                    fixtures.append((image, mount))
                    subprocess.run([hdiutil, "attach", "-nobrowse", "-noautoopen", "-readonly",
                                    "-mountpoint", str(mount), str(image)], check=True, capture_output=True,
                                   timeout=40)

                (target, target_mount), (untouched, _) = fixtures
                expected = subprocess.check_output(worker + ["identify", str(target_mount)],
                                                   text=True, timeout=15).strip()
                self.assertTrue(expected)
                copied_script = subprocess.check_output(worker + ["identify", str(payload)],
                                                       text=True, timeout=15).strip()
                self.assertFalse(copied_script)

                with subprocess.Popen(["/bin/sh", "-c", "read finish"], stdin=subprocess.PIPE,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as owner:
                    owner.stdin.close()
                    owner.wait(timeout=5)
                    subprocess.run(worker + ["detach", str(target_mount), expected + "-stale", str(owner.pid)],
                                   check=True, capture_output=True, timeout=15)
                self.assertTrue({target, untouched}.issubset(attached_images()))

                with subprocess.Popen(["/bin/sh", "-c", "read finish"], stdin=subprocess.PIPE,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as owner:
                    with subprocess.Popen(worker + ["detach", str(target_mount), expected, str(owner.pid)],
                                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) as eject:
                        try:
                            time.sleep(0.3)
                            self.assertIsNone(owner.poll())
                            self.assertIn(target, attached_images())
                        finally:
                            owner.stdin.close()
                            owner.wait(timeout=5)
                        self.assertEqual(0, eject.wait(timeout=25))
                self.assertNotIn(target, attached_images())
                self.assertIn(untouched, attached_images())
            finally:
                for image, mount in fixtures:
                    if image in attached_images():
                        subprocess.run([hdiutil, "detach", str(mount)], check=True, capture_output=True, timeout=15)

    def _run_install_fixture(self, raw, root, privileged, expect=None, existing=False, reject_signature=False):
        root.mkdir()
        applications = root / "Applications"
        applications.mkdir()
        image = root / "image"
        app = image / "TuPig Synergy.app"
        (app / "Contents/MacOS").mkdir(parents=True)
        (app / "Contents/Info.plist").write_text("fixture")
        (app / "Contents/MacOS/TuPig Synergy").write_text("gui")
        (app / "Contents/MacOS/synergy-core").write_text("core")
        for item in sorted(app.rglob("*"), reverse=True):
            item.chmod(0o555 if item.is_dir() else 0o444)
        app.chmod(0o555)
        source_snapshot = {item.relative_to(app): item.read_bytes()
                           for item in app.rglob("*") if item.is_file()}
        destination = applications / "TuPig Synergy.app"
        if existing:
            destination.mkdir()
            (destination / "marker").write_text("original")
        tools = root / "tools"
        tools.mkdir()
        private_stage_parent = root / "private-stage"
        private_stage_parent.mkdir(mode=0o700)
        log = root / "xattr.log"
        open_log = root / "open.log"
        eject_log = root / "eject.log"
        scripts = {
            "codesign": "#!/bin/sh\n[ \"${TEST_CODESIGN_FAIL:-0}\" = 1 ] && exit 1\nexit 0\n",
            # A source bundle may be read-only. This stub models ditto copying
            # it into a private, owner-writable staging directory; it must not
            # relax the source itself.
            "ditto": "#!/bin/sh\ncp -R \"$2\" \"$3\"\nchmod -R u+w \"$3\"\n",
            "xattr": "#!/bin/sh\nprintf '%s\\n' \"$@\" >> \"$TEST_XATTR_LOG\"\nexit 0\n",
            "id": "#!/bin/sh\n[ \"${TEST_FAKE_ROOT:-0}\" = 1 ] && echo 0 || echo 501\n",
            # Return to the original unprivileged process, just as real sudo
            # does. The outer script must open the GUI/settings only there.
            "sudo": "#!/bin/sh\nTEST_FAKE_ROOT=1 \"$@\"\n",
            "open": "#!/bin/sh\nprintf '%s:%s\\n' \"${TEST_FAKE_ROOT:-0}\" \"$*\" >> \"$TEST_OPEN_LOG\"\nexit 0\n",
            "osascript": (
                "#!/usr/bin/env python3\nimport os, shutil, sys\n"
                "if '-e' not in sys.argv:\n    shutil.move(sys.argv[-2], sys.argv[-1])\n"
                "elif sys.argv[5] == 'identify':\n    print('fixture image identity')\n"
                "elif sys.argv[5] == 'detach':\n"
                "    with open(os.environ['TEST_EJECT_LOG'], 'a') as log:\n"
                "        log.write(os.environ.get('TEST_FAKE_ROOT', '0') + ':' + sys.argv[6] + '\\n')\n"
            ),
        }
        for name, body in scripts.items():
            path = tools / name
            path.write_text(body)
            path.chmod(0o700)
        rendered = raw.replace("@CMAKE_PROJECT_PROPER_NAME@", "TuPig Synergy").replace(
            "@APPLE_CODESIGN_ID@", "A" * 40).replace("'/Applications'", repr(str(applications))).replace(
            "'/private/tmp'", repr(str(private_stage_parent)))
        for name in scripts:
            rendered = rendered.replace("/usr/bin/" + name, str(tools / name))
        rendered = rendered.replace('/usr/bin/mktemp', '/usr/bin/mktemp')
        if privileged:
            rendered = rendered.replace('[ ! -w \"$applications\" ]', '[ \"${TEST_FORCE_SUDO:-0}\" = 1 ]')
        program = image / "Install.command"
        program.write_text(rendered)
        program.chmod(0o700)
        environment = {**os.environ, "TEST_XATTR_LOG": str(log), "TEST_OPEN_LOG": str(open_log),
                       "TEST_EJECT_LOG": str(eject_log),
                       "TEST_CODESIGN_FAIL": "1" if reject_signature else "0"}
        if privileged:
            environment["TEST_FORCE_SUDO"] = "1"
        result = subprocess.run(["/bin/sh", str(program)], cwd=image, env=environment,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(source_snapshot, {item.relative_to(app): item.read_bytes()
                                           for item in app.rglob("*") if item.is_file()})
        if expect is not None:
            self.assertEqual(expect, result.returncode, result.stderr)
            self.assertTrue(destination.is_dir())
            self.assertIn(".tupig-synergy-install.", log.read_text())
            self.assertNotIn(str(destination), log.read_text())
            # The real root retry returns to this original user process. Exactly
            # two user-level opens are permitted: the app and its Accessibility
            # settings page; no root helper may launch either surface.
            opened = open_log.read_text().splitlines()
            self.assertEqual(2, len(opened), opened)
            self.assertTrue(all(line.startswith("0:") for line in opened), opened)
            self.assertIn(str(destination), opened[0])
            self.assertIn("Privacy_Accessibility", opened[1])
            for _ in range(40):
                if eject_log.exists() and eject_log.stat().st_size:
                    break
                time.sleep(0.05)
            self.assertEqual(["0:" + str(image.resolve())], eject_log.read_text().splitlines())
        else:
            self.assertFalse(eject_log.exists())
        return result, destination, log, rendered

    def test_docs_describe_actual_secret_variable_and_permission_limits(self):
        """Operators must not be told a different credential or TCC model than CI uses."""
        docs = source(DOCS)
        self.assertIn("APPLE_CODESIGN_ID", docs)
        self.assertNotIn("non-secret variable `APPLE_CODESIGN_ID`", docs)
        self.assertIn("global Gatekeeper/spctl policy", docs)
        self.assertIn("Accessibility or Input", docs)
        self.assertIn("private key", docs.lower())

    def test_authorized_scope_contains_no_literal_credential_or_pii_values(self):
        """A literal private key/token must report only its location, never its contents."""
        suspect = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----|\bghp_[A-Za-z0-9]+|\bgithub_pat_[A-Za-z0-9_]+")
        locations = []
        for path in (WORKFLOW, KEYCHAIN, SETUP, SETUP_PYTHON, INSTALLER, DEPLOY, CODESIGN, SIGNER, DOCS):
            for line_number, line in enumerate(source(path).splitlines(), start=1):
                if suspect.search(line):
                    locations.append(f"{path.relative_to(ROOT)}:{line_number}")
        self.assertEqual([], locations, "P0 suspected credential/PII locations: " + ", ".join(locations))


if __name__ == "__main__":
    unittest.main()

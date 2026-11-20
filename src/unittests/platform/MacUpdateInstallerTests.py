# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

import fcntl
import hashlib
import os
import plistlib
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


INSTALLER_SOURCE = Path(__file__).resolve().parents[2] / "lib/platform/macos/UpdateInstallerMac.cpp"
BUNDLE_ID = "com.tupig.synergy.updatertest"
CODESIGN = "/usr/bin/codesign"


def run(arguments, *, timeout=20, **kwargs):
    return subprocess.run(arguments, capture_output=True, text=True, timeout=timeout, **kwargs)


def tree_contents(root):
    files = {}
    for child in sorted(root.rglob("*")):
        relative = child.relative_to(root)
        if child.is_symlink():
            files[relative] = ("link", child.readlink())
        elif child.is_file():
            files[relative] = ("file", hashlib.sha256(child.read_bytes()).hexdigest())
        elif child.is_dir():
            files[relative] = ("directory",)
    return files


def directory_identity(path):
    info = path.lstat()
    return info.st_dev, info.st_ino


def shell_literal(source, name):
    pattern = rf'\b{re.escape(name)}\s*(?:\[\])?\s*=\s*R"SH\((.*?)\)SH"'
    literals = re.findall(pattern, source, re.DOTALL)
    if len(literals) != 1:
        raise AssertionError(f"Expected one production shell literal for {name}")
    return literals[0]


class MacUpdateInstallerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = INSTALLER_SOURCE.read_text(encoding="utf-8")
        cls.install_script = shell_literal(source, "kInstallBundle")
        cls.disk_image_script = shell_literal(source, "kInstallFromDiskImage")
        cls.runner_script = shell_literal(source, "kRunUpdate")

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="tupig-update-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.environment = {
            "PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
            "TMPDIR": str(self.root),
            "LC_ALL": "C",
        }
        self.source = self.root / "download's 新 content" / "Fixture.app"
        self.destination = self.root / "owner's 已装 apps" / "Fixture.app"
        self.write_app(self.source, "2")
        self.write_app(self.destination, "1")
        self.neighbor = self.destination.parent / "unrelated.txt"
        self.neighbor.write_text("untouched", encoding="ascii")
        self.old_identity = directory_identity(self.destination)
        self.old_contents = tree_contents(self.destination)

    def assert_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def verify_signature(self, path):
        return run([CODESIGN, "--verify", "--deep", "--strict", str(path)])

    def write_app(self, root, version, bundle_id=BUNDLE_ID):
        contents = root / "Contents"
        macos = contents / "MacOS"
        resources = contents / "Resources"
        macos.mkdir(parents=True)
        resources.mkdir()
        plist = {
            "CFBundleExecutable": "Fixture",
            "CFBundleIdentifier": bundle_id,
            "CFBundleInfoDictionaryVersion": "6.0",
            "CFBundleName": "Fixture",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": version,
            "CFBundleVersion": version,
        }
        (contents / "Info.plist").write_bytes(plistlib.dumps(plist))
        executable = macos / "Fixture"
        shutil.copyfile("/usr/bin/true", executable)
        executable.chmod(0o755)
        self.assert_success(run([CODESIGN, "--force", "--sign", "-", str(executable)]))
        (resources / f"version-{version}.txt").write_text(version, encoding="ascii")
        self.assert_success(run([CODESIGN, "--force", "--sign", "-", str(root)]))
        self.assert_success(self.verify_signature(root))

    def install(self, *, script=None, source=None, destination=None, expected_identity=None):
        expected_root = "{}:{}".format(*(expected_identity or self.old_identity))
        return run([
            "/bin/sh", "-c", script if script is not None else self.install_script,
            "mac-update-install", str(source or self.source), str(destination or self.destination), BUNDLE_ID,
            str(self.root), expected_root,
        ], env=self.environment)

    def assert_original_installation(self):
        self.assertEqual(directory_identity(self.destination), self.old_identity)
        self.assertEqual(tree_contents(self.destination), self.old_contents)
        self.assert_success(self.verify_signature(self.destination))
        self.assertEqual(self.neighbor.read_text(encoding="ascii"), "untouched")

    def test_update_keeps_app_directory_identity_and_exact_signed_contents(self):
        expected = tree_contents(self.source)
        self.assert_success(self.install())
        self.assertEqual(directory_identity(self.destination), self.old_identity)
        self.assertEqual(tree_contents(self.destination), expected)
        self.assertEqual(tree_contents(self.source), expected)
        self.assert_success(self.verify_signature(self.destination))
        self.assertEqual(self.neighbor.read_text(encoding="ascii"), "untouched")

    def test_rejects_invalid_payload_without_changes(self):
        (self.source / "Contents/Resources/version-2.txt").write_text("changed", encoding="ascii")
        self.assertNotEqual(self.verify_signature(self.source).returncode, 0)
        result = self.install()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_original_installation()

    def test_rejects_wrong_bundle_identifier_without_changes(self):
        shutil.rmtree(self.source)
        self.write_app(self.source, "2", "com.tupig.synergy.unrelatedtest")
        result = self.install()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_original_installation()

    def test_rejects_changed_destination_identity_without_changes(self):
        device, inode = self.old_identity
        result = self.install(expected_identity=(device, inode + 1))
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_original_installation()

    def test_rejects_symlinked_app_paths_without_changes(self):
        for argument, path in [
            ("source", self.source),
            ("destination", self.destination),
        ]:
            with self.subTest(argument=argument):
                alias = self.root / f"{argument}-Alias.app"
                alias.symlink_to(path, target_is_directory=True)
                result = self.install(**{argument: alias})
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue(alias.is_symlink())
                self.assertEqual(alias.readlink(), path)
                self.assert_original_installation()

    def test_in_use_destination_rejects_concurrent_installation(self):
        descriptor = os.open(self.destination, os.O_RDONLY | os.O_DIRECTORY)
        try:
            fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
            result = self.install()
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assert_original_installation()
        finally:
            os.close(descriptor)
        self.assert_success(self.install())
        self.assertEqual(directory_identity(self.destination), self.old_identity)

    def test_post_exchange_verification_failure_restores_original_installation(self):
        check = 'if ! verify_bundle "$dest"; then'
        self.assertEqual(self.install_script.count(check), 1)
        # Keep the real filesystem exchange and signature verification. Force
        # just the installed-bundle verification result to enter the rollback
        # branch, after observing the staged payload at the live path.
        marker = "saw signed new payload before simulated verification failure"
        forced_failure = (
            'if ! { test -f "$dest/Contents/Resources/version-2.txt" && '
            'verify_bundle "$dest" && '
            f'printf "%s\\n" "{marker}" >&2; /usr/bin/false; }}; then'
        )
        result = self.install(script=self.install_script.replace(check, forced_failure))
        self.assertIn(marker, result.stderr)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_original_installation()

    def test_runner_verifies_downloaded_image_and_installs_without_replacing_outer_directory(self):
        image_path = self.root / "test download's 更新.dmg"
        self.assert_success(run([
            "/usr/bin/hdiutil", "create", "-quiet", "-format", "UDRO",
            "-volname", "Update fixture", "-srcfolder", str(self.source.parent), str(image_path),
        ], timeout=40))
        digest = hashlib.sha256(image_path.read_bytes()).hexdigest()
        expected = tree_contents(self.source)
        launch = '/usr/bin/open "$dest"'
        self.assertEqual(self.runner_script.count(launch), 2)
        marker = "would relaunch the verified fixture"
        # Run the exact installer chain except for the final GUI launch.
        # Neither fixture is running, so there is no process to stop.
        runner = self.runner_script.replace(launch, f'printf "%s\\n" "{marker}"')
        executable_dir = self.destination / "Contents/MacOS"

        def update(expected_digest):
            return run([
                "/bin/sh", "-c", runner, "mac-update-run",
                str(os.getpid()), str(image_path), expected_digest, str(self.destination), self.source.name, BUNDLE_ID,
                str(executable_dir / "Fixture"), str(executable_dir / self.root.name),
                self.disk_image_script, self.install_script,
            ], env=self.environment, timeout=40)

        wrong_digest = ("1" if digest[0] == "0" else "0") + digest[1:]
        rejected = update(wrong_digest)
        self.assertNotEqual(rejected.returncode, 0, rejected.stdout + rejected.stderr)
        self.assertIn(marker, rejected.stdout)
        self.assert_original_installation()
        result = update(digest)
        self.assert_success(result)
        self.assertIn(marker, result.stdout)
        self.assertEqual(directory_identity(self.destination), self.old_identity)
        self.assertEqual(tree_contents(self.destination), expected)
        self.assert_success(self.verify_signature(self.destination))
        self.assertEqual(self.neighbor.read_text(encoding="ascii"), "untouched")


if __name__ == "__main__":
    unittest.main(verbosity=2)

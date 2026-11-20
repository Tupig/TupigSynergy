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
        temporary = tempfile.TemporaryDirectory(prefix="tupig-update-fixture-")
        cls.addClassCleanup(temporary.cleanup)
        cls.fixture_executable = Path(temporary.name) / "fixture"
        # Use our own executable so the fixtures do not inherit platform-binary signatures or flags.
        fixture_source = """\
#include <unistd.h>

int main(int argc, char **argv)
{
    (void)argv;
    if (argc > 1)
        sleep(45);
    return 0;
}
"""
        compilation = run([
            "/usr/bin/xcrun", "--sdk", "macosx", "clang", "-x", "c", "-", "-o", str(cls.fixture_executable),
        ], input=fixture_source)
        if compilation.returncode != 0:
            raise AssertionError(f"Could not compile updater fixture:\n{compilation.stdout}{compilation.stderr}")

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

    def write_fixture_executable(self, executable):
        shutil.copyfile(self.fixture_executable, executable)
        executable.chmod(0o755)
        self.assert_success(run([CODESIGN, "--force", "--sign", "-", str(executable)]))

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
        self.write_fixture_executable(macos / "Fixture")
        (resources / f"version-{version}.txt").write_text(version, encoding="ascii")
        self.assert_success(run([CODESIGN, "--force", "--sign", "-", str(root)]))
        self.assert_success(self.verify_signature(root))

    def install(self, *, script=None, source=None, destination=None, expected_identity=None):
        expected_root = "{}:{}".format(*(expected_identity or self.old_identity))
        dest = destination or self.destination
        executable_dir = dest / "Contents/MacOS"
        return run([
            "/bin/sh", "-c", script if script is not None else self.install_script,
            "mac-update-install", str(source or self.source), str(dest), BUNDLE_ID,
            str(self.root), expected_root,
        ], env={
            **self.environment,
            "SYNERGY_MAC_UPDATE_GUI": str(executable_dir / "Fixture"),
            "SYNERGY_MAC_UPDATE_CORE": str(executable_dir / "FixtureCore"),
        })

    def start_bundle_process(self, bundle, name="Fixture"):
        executable = bundle / "Contents/MacOS" / name
        self.write_fixture_executable(executable)
        self.assert_success(run([CODESIGN, "--force", "--sign", "-", str(bundle)]))
        self.assert_success(self.verify_signature(bundle))
        process = subprocess.Popen([str(executable), "--wait"], env=self.environment,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)

        def stop_fixture():
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=5)

        self.addCleanup(process.stderr.close)
        self.addCleanup(stop_fixture)
        identity = run(["/bin/ps", "-ww", "-p", str(process.pid), "-o", "comm="])
        exit_code = process.poll()
        if exit_code is not None:
            self.fail(
                f"Fixture {executable} exited before the path check (status {exit_code}; "
                f"ps: {identity.stdout + identity.stderr!r}; stderr: {process.stderr.read()!r})"
            )
        self.assert_success(identity)
        self.assertEqual(identity.stdout.strip(), str(executable))
        return process

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

    def test_installer_rejects_live_core_before_exchanging_contents(self):
        core = self.start_bundle_process(self.destination, "FixtureCore")
        self.old_contents = tree_contents(self.destination)
        old_contents_identity = directory_identity(self.destination / "Contents")

        # Exercise the final guard directly, independently of the runner wait.
        result = self.install()
        self.assertEqual(result.returncode, 75, result.stdout + result.stderr)
        self.assertIsNone(core.poll())
        self.assertEqual(directory_identity(self.destination / "Contents"), old_contents_identity)
        self.assert_original_installation()

    def test_runner_waits_for_all_gui_instances_at_the_target_path(self):
        unrelated_bundle = self.root / "unrelated apps" / "Fixture.app"
        self.write_app(unrelated_bundle, "2")
        unrelated = self.start_bundle_process(unrelated_bundle)
        executable_dir = self.destination / "Contents/MacOS"
        runner = self.runner_script.replace('/usr/bin/open "$dest"', ":")
        install_marker = "reached image staging"
        install_image = f'printf "%s\\n" "{install_marker}"; exit 65'

        def update():
            return run([
                "/bin/sh", "-c", runner, "mac-update-run", str(os.getpid()),
                str(self.root / "fixture.dmg"), "0" * 64, str(self.destination), self.source.name, BUNDLE_ID,
                str(executable_dir / "Fixture"), str(executable_dir / "FixtureCore"),
                install_image, self.install_script,
            ], env=self.environment)

        # An executable with the same basename in another app must be ignored.
        unrelated_result = update()
        self.assertEqual(unrelated_result.returncode, 65, unrelated_result.stdout + unrelated_result.stderr)
        self.assertIn(install_marker, unrelated_result.stdout)
        old_gui = self.start_bundle_process(self.destination)
        self.old_contents = tree_contents(self.destination)
        result = update()
        self.assertEqual(result.returncode, 75, result.stdout + result.stderr)
        self.assertNotIn(install_marker, result.stdout)
        self.assertIsNone(old_gui.poll())
        self.assertIsNone(unrelated.poll())
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

#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Exercise the macOS verifier's owned-process TLS image gate in isolation."""

import ast
from contextlib import nullcontext, redirect_stdout
import io
from pathlib import Path
import ssl
import struct
import subprocess
import tempfile
from types import ModuleType, SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
VERIFIER = ROOT / "scripts/ci/verify-package-macos.sh"
PID = 4711
LSOF = ["/usr/sbin/lsof", "-nP", "-a", "-p", str(PID), "-d", "txt", "-F", "pfn"]
VMMAP = ["/usr/bin/vmmap", "-w", str(PID)]
PEER = b"private test peer der"


def load_verifier():
    # Load the real embedded Python definitions without running the package CLI.
    header, _, heredoc = VERIFIER.read_text(encoding="utf-8").partition("<<'PY'\n")
    embedded = heredoc.rsplit("\nPY", 1)[0]
    syntax = ast.parse(embedded, filename=str(VERIFIER))
    syntax.body = [
        node for node in syntax.body
        if isinstance(node, (ast.Import, ast.ImportFrom, ast.Assign, ast.FunctionDef))
    ]
    ast.increment_lineno(syntax, header.count("\n") + 1)
    module = ModuleType("macos_package_verifier")
    exec(compile(syntax, str(VERIFIER), "exec"), module.__dict__)
    return module


def write_macho(path, filetype=6):
    path.parent.mkdir(parents=True, exist_ok=True)
    # A Mach-O 64-bit arm64 header distinguishes a mapped native file from resources.
    path.write_bytes(struct.pack("<IIIIIIII", 0xFEEDFACF, 0x0100000C, 0, filetype, 0, 0, 0, 0))
    return path


def text_records(*paths, pid=PID):
    return f"p{pid}\n" + "".join(f"ftxt\nn{path}\n" for path in paths)


def vmmap_region(kind, path, current="r--", maximum="r--"):
    return f"{kind:27} 105210000-105224000    [   80K    80K     0K     0K] {current}/{maximum} SM=COW          {path}"


def vmmap_report(core, *regions, pid=PID, core_permissions="r-x"):
    return "\n".join((
        f"Process:             synergy-core [{pid}]",
        "Path:                /Volumes/VOLUME/Test Payload.app/Contents/MacOS/synergy-core",
        f"==== Non-writable regions for process {pid}",
        vmmap_region("__TEXT", core, core_permissions, core_permissions),
        *regions,
        f"==== Writable regions for process {pid}",
        "==== Legend",
        f"==== Summary for process {pid}",
        "TOTAL                                    1.4G",
        "",
    ))


class MacOSPackageLoadedImages(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory(prefix="package-images-")
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name).resolve()
        self.bundle = self.root / "Test Payload.app"
        self.core = write_macho(self.bundle / "Contents/MacOS/synergy-core", filetype=2)
        self.gui = self.core.with_name("TuPig Synergy")
        frameworks = self.bundle / "Contents/Frameworks"
        self.libssl = write_macho(frameworks / "libssl.3.dylib")
        self.libcrypto = write_macho(frameworks / "libcrypto.3.dylib")
        self.backend = write_macho(self.bundle / "Contents/PlugIns/tls/libqopensslbackend.dylib")
        self.paths = [self.core, self.libssl, self.libcrypto, self.backend]
        self.process = mock.Mock(spec=subprocess.Popen)
        self.process.pid = PID
        self.process.poll.return_value = None
        self.verifier = load_verifier()
        self.runner = mock.Mock(return_value=subprocess.CompletedProcess(LSOF, 0, text_records(*self.paths), ""))
        self.vmmap_runner = mock.Mock(return_value=subprocess.CompletedProcess(VMMAP, 0, vmmap_report(self.core), ""))
        self.verifier.subprocess = SimpleNamespace(
            run=self.run_observer,
            Popen=mock.Mock(),
            TimeoutExpired=subprocess.TimeoutExpired,
            CalledProcessError=subprocess.CalledProcessError,
            SubprocessError=subprocess.SubprocessError,
        )

    def observe(self):
        self.verifier.observe_loaded_images(self.process, self.core, self.bundle)

    def run_observer(self, args, **kwargs):
        command = list(args)
        if command == LSOF:
            return self.runner(args, **kwargs)
        self.assertEqual(VMMAP, command)
        return self.vmmap_runner(args, **kwargs)

    def observer_output(self, output, status=0, stderr=""):
        self.runner.return_value = subprocess.CompletedProcess(LSOF, status, output, stderr)

    def vmmap_output(self, output, status=0, stderr=""):
        self.vmmap_runner.return_value = subprocess.CompletedProcess(VMMAP, status, output, stderr)

    def test_live_owned_pid_with_bundle_libraries_and_non_image_mapping(self):
        resource = self.root / "mapped resource.dat"
        resource.write_bytes(b"this is a mapped data file, not a Mach-O image")
        self.observer_output(text_records(*self.paths, resource, "/usr/lib/dyld"))
        self.observe()
        self.runner.assert_called_once()
        self.assertEqual(LSOF, list(self.runner.call_args.args[0]))
        self.assertTrue(self.runner.call_args.kwargs["capture_output"])
        self.assertTrue(self.runner.call_args.kwargs["text"])
        self.assertGreater(self.runner.call_args.kwargs["timeout"], 0)
        self.assertLessEqual(self.runner.call_args.kwargs["timeout"], 20)
        self.process.terminate.assert_not_called()
        self.vmmap_runner.assert_not_called()

    def test_access_denial_fails_even_if_output_contains_the_expected_paths(self):
        self.observer_output(text_records(*self.paths), 1, "permission denied")
        with self.assertRaisesRegex(ValueError, r"(?i)lsof|observer"):
            self.observe()

    def test_lsof_timeout_fails(self):
        self.runner.side_effect = subprocess.TimeoutExpired(LSOF, 10)
        with self.assertRaisesRegex(ValueError, r"(?i)timed? ?out|timeout"):
            self.observe()

    def test_child_death_before_observation_blocks_the_command(self):
        self.process.poll.return_value = 3
        with self.assertRaisesRegex(ValueError, r"(?i)exit|not alive"):
            self.observe()
        self.runner.assert_not_called()

    def test_child_death_during_observation_invalidates_successful_command(self):
        def result_after_exit(*args, **kwargs):
            self.process.poll.return_value = 3
            return self.runner.return_value

        self.runner.side_effect = result_after_exit
        with self.assertRaisesRegex(ValueError, r"(?i)exit|not alive"):
            self.observe()
        self.runner.assert_called_once()

    def test_refuses_wrong_missing_or_multiple_reported_pids(self):
        for output in (text_records(*self.paths, pid=PID + 1),
                       text_records(*self.paths) + f"p{PID + 1}\n",
                       text_records(*self.paths) + f"p{PID}\n",
                       text_records(*self.paths).split("\n", 1)[1]):
            with self.subTest(output=output), self.assertRaisesRegex(ValueError, r"(?i)pid"):
                self.observer_output(output)
                self.observe()

    def test_requires_well_formed_nonempty_text_records(self):
        valid = text_records(*self.paths)
        for output in (f"p{PID}\n", valid + "ftxt\nnrelative\n", valid + f"fcwd\nn{self.core}\n", valid + "ftxt\n"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.observer_output(output)
                self.observe()

    def test_requires_the_exact_audited_core_path(self):
        wrong_core = write_macho(self.core.with_name("different-core"), filetype=2)
        self.observer_output(text_records(wrong_core, *self.paths[1:]))
        with self.assertRaisesRegex(ValueError, r"(?i)core"):
            self.observe()

    def test_requires_each_packaged_openssl_library(self):
        for missing in (self.libssl, self.libcrypto):
            with self.subTest(missing=missing.name), self.assertRaisesRegex(ValueError, missing.name.split(".")[0]):
                # A system path of the same name cannot satisfy a packaged-library requirement.
                self.observer_output(text_records(
                    *(path for path in self.paths if path != missing), Path("/usr/lib") / missing.name
                ))
                self.observe()

    def test_non_macho_file_cannot_fulfill_the_packaged_openssl_requirement(self):
        self.libssl.write_bytes(b"not a dynamic library")
        with self.assertRaisesRegex(ValueError, "libssl"):
            self.observe()

    def test_different_bundle_directory_cannot_fulfill_the_framework_requirement(self):
        misplaced = write_macho(self.bundle / "Contents/Resources" / self.libssl.name)
        self.observer_output(text_records(self.core, self.libcrypto, self.backend, misplaced))
        with self.assertRaisesRegex(ValueError, "libssl"):
            self.observe()

    def test_package_without_openssl_backend_can_use_the_system_tls_runtime(self):
        self.backend.unlink()
        self.observer_output(text_records(self.core, "/usr/lib/libssl.3.dylib"))
        self.observe()

    def test_rejects_external_framework_without_a_dylib_suffix(self):
        external = write_macho(self.bundle.with_suffix(".app.shadow") / "Other.framework/Versions/A/Other")
        self.observer_output(text_records(*self.paths, external))
        with self.assertRaisesRegex(ValueError, r"(?i)external|outside"):
            self.observe()

    def test_symlink_cannot_make_an_external_image_appear_packaged(self):
        external = write_macho(self.root / "outside" / self.libssl.name)
        self.libssl.unlink()
        self.libssl.symlink_to(external)
        with self.assertRaisesRegex(ValueError, r"(?i)external|outside"):
            self.observe()

    def test_does_not_silently_drop_unclassifiable_outside_mapping(self):
        self.observer_output(text_records(*self.paths, self.root / "no-longer-readable"))
        with self.assertRaisesRegex(ValueError, r"(?i)inspect|classif|read|resolv|exist"):
            self.observe()

    def test_vmmap_confirms_exact_data_regions_for_multiple_unreadable_paths(self):
        vanished = self.root / "former mapped file"
        protected = self.root / "protected mapped file"
        protected.write_text("protected data fixture")
        self.observer_output(text_records(*self.paths, vanished, protected))
        self.vmmap_output(vmmap_report(
            self.core,
            vmmap_region("mapped file", vanished),
            vmmap_region("mapped file", vanished, "rw-", "rwx"),
            vmmap_region("mapped file", protected, "rw-", "rwx"),
            vmmap_region("__TEXT", "/System/Library/Permission Framework", "r-x", "r-x"),
        ))
        actual_macho = self.verifier.macho

        def read_header(path):
            if path == protected:
                raise PermissionError("test resource cannot be opened")
            return actual_macho(path)

        with mock.patch.object(self.verifier, "macho", side_effect=read_header):
            self.observe()
        self.vmmap_runner.assert_called_once()
        self.assertEqual(VMMAP, list(self.vmmap_runner.call_args.args[0]))
        self.assertTrue(self.vmmap_runner.call_args.kwargs["capture_output"])
        self.assertTrue(self.vmmap_runner.call_args.kwargs["text"])
        self.assertGreater(self.vmmap_runner.call_args.kwargs["timeout"], 0)
        self.assertLessEqual(self.vmmap_runner.call_args.kwargs["timeout"], 20)

    def test_vmmap_rejects_incomplete_or_wrong_owned_process_report(self):
        missing = self.root / "unreadable"
        self.observer_output(text_records(*self.paths, missing))
        data = vmmap_region("mapped file", missing)
        valid = vmmap_report(self.core, data)
        legend_start = valid.index("==== Legend")
        summary_start = valid.index("==== Summary for process")
        reordered = valid[:legend_start] + valid[summary_start:] + valid[legend_start:summary_start]
        for output in (
            "",
            valid.replace("==== Non-writable regions", "==== incomplete regions"),
            valid.replace("==== Writable regions", "==== incomplete regions"),
            valid.replace("==== Summary for process", "==== incomplete summary"),
            valid.replace("==== Legend", "==== incomplete legend"),
            reordered,
            vmmap_report(self.core, data, pid=PID + 1),
            valid + f"Process: synergy-core [{PID}]\n",
            vmmap_report(self.core.with_name("other-core"), data),
            vmmap_report(self.core, data, core_permissions="r--"),
        ):
            with self.subTest(output=output):
                self.vmmap_runner.reset_mock()
                self.vmmap_output(output)
                with self.assertRaises(ValueError):
                    self.observe()
                self.vmmap_runner.assert_called_once()

    def test_vmmap_rejects_denied_and_nonzero_report(self):
        missing = self.root / "unreadable"
        self.observer_output(text_records(*self.paths, missing))
        output = vmmap_report(self.core, vmmap_region("mapped file", missing))
        for status, diagnostic in (
            (0, "vmmap: cannot examine process: Permission denied"),
            (0, "vmmap: incomplete region enumeration"),
            (1, ""),
        ):
            with self.subTest(status=status, diagnostic=diagnostic):
                self.vmmap_runner.reset_mock()
                self.vmmap_output(output, status, diagnostic)
                with self.assertRaises(ValueError):
                    self.observe()
                self.vmmap_runner.assert_called_once()

    def test_vmmap_timeout_fails(self):
        self.observer_output(text_records(*self.paths, self.root / "unreadable"))
        self.vmmap_runner.side_effect = subprocess.TimeoutExpired(VMMAP, 10)
        with self.assertRaisesRegex(ValueError, r"(?i)timed? ?out|timeout"):
            self.observe()
        self.vmmap_runner.assert_called_once()

    def test_vmmap_tool_unavailable_fails(self):
        self.observer_output(text_records(*self.paths, self.root / "unreadable"))
        self.vmmap_runner.side_effect = OSError("test vmmap is unavailable")
        with self.assertRaisesRegex(ValueError, r"(?i)vmmap|observer"):
            self.observe()
        self.vmmap_runner.assert_called_once()

    def test_vmmap_requires_exact_data_only_regions_for_each_unreadable_path(self):
        missing = self.root / "unreadable"
        self.observer_output(text_records(*self.paths, missing))
        data = vmmap_region("mapped file", missing)
        for regions in (
            (vmmap_region("mapped file", self.root / "lookalike" / missing.name),),
            (data, vmmap_region("__DATA", missing)),
            (data, vmmap_region("mapped file", missing, "r-x", "r-x")),
            (data, f"mapped file confusing-region-attributes {missing}"),
        ):
            with self.subTest(regions=regions):
                self.vmmap_runner.reset_mock()
                self.vmmap_output(vmmap_report(self.core, *regions))
                with self.assertRaises(ValueError):
                    self.observe()
                self.vmmap_runner.assert_called_once()

    def test_vmmap_never_overrides_lsof_image_requirements(self):
        missing = self.root / "unreadable"
        self.vmmap_output(vmmap_report(
            self.core, vmmap_region("mapped file", missing),
            vmmap_region("__TEXT", self.libssl, "r-x", "r-x"),
            vmmap_region("__TEXT", self.libcrypto, "r-x", "r-x"),
        ))
        for absent in (self.core, self.libssl, self.libcrypto):
            expected = "core" if absent == self.core else absent.name.split(".")[0]
            with self.subTest(absent=absent.name), self.assertRaisesRegex(ValueError, expected):
                self.observer_output(text_records(*(path for path in self.paths if path != absent), missing))
                self.observe()

    def test_vmmap_never_reclassifies_a_readable_external_macho(self):
        external = write_macho(self.root / "external-binary")
        self.observer_output(text_records(*self.paths, external))
        self.vmmap_output(vmmap_report(self.core, vmmap_region("mapped file", external)))
        with self.assertRaisesRegex(ValueError, r"(?i)external|outside"):
            self.observe()
        self.vmmap_runner.assert_not_called()

    def test_vmmap_not_called_if_lsof_child_has_already_exited(self):
        self.observer_output(text_records(*self.paths, self.root / "unreadable"))

        def lsof_then_exit(*args, **kwargs):
            self.process.poll.return_value = 3
            return self.runner.return_value

        self.runner.side_effect = lsof_then_exit
        with self.assertRaisesRegex(ValueError, r"(?i)exit|not alive"):
            self.observe()
        self.vmmap_runner.assert_not_called()

    def test_vmmap_rejects_child_exit_during_data_observation(self):
        missing = self.root / "unreadable"
        self.observer_output(text_records(*self.paths, missing))
        self.vmmap_output(vmmap_report(self.core, vmmap_region("mapped file", missing)))

        def vmmap_then_exit(*args, **kwargs):
            self.process.poll.return_value = 3
            return self.vmmap_runner.return_value

        self.vmmap_runner.side_effect = vmmap_then_exit
        with self.assertRaisesRegex(ValueError, r"(?i)exit|not alive"):
            self.observe()
        self.vmmap_runner.assert_called_once()

    def test_launch_uses_live_observation_when_dyld_reports_nothing(self):
        self.run_launch()
        self.runner.assert_called_once()

    def test_launch_does_not_observe_after_failed_handshake(self):
        self.run_launch(handshake_error=ssl.SSLError("test handshake failed"),
                        outcome=self.assertRaisesRegex(ssl.SSLError, "test handshake failed"))
        self.runner.assert_not_called()
        self.vmmap_runner.assert_not_called()

    def test_launch_does_not_observe_after_peer_identity_mismatch(self):
        self.run_launch(peer=b"other peer", outcome=self.assertRaisesRegex(ValueError, "private test identity"))
        self.runner.assert_not_called()
        self.vmmap_runner.assert_not_called()

    def test_launch_does_not_observe_without_product_trust_confirmation(self):
        self.run_launch(trusted=False, outcome=self.assertRaisesRegex(ValueError, "trusted TLS peer"))
        self.runner.assert_not_called()
        self.vmmap_runner.assert_not_called()

    def test_launch_reaps_child_if_observer_fails(self):
        self.observer_output("", 1, "permission denied")
        self.run_launch(outcome=self.assertRaisesRegex(ValueError, r"(?i)lsof|observer"))
        self.runner.assert_called_once()

    def test_launch_retains_early_dyld_path_validation(self):
        external = write_macho(self.root / "early-library")
        self.run_launch(early_images=(external,), outcome=self.assertRaisesRegex(ValueError, r"(?i)external|outside"))

    def test_launch_requires_live_evidence_even_with_successful_dyld_trace(self):
        self.observer_output("", 1, "permission denied")
        self.run_launch(early_images=self.paths, outcome=self.assertRaisesRegex(ValueError, r"(?i)lsof|observer"))
        self.runner.assert_called_once()

    def test_launch_uses_vmmap_while_trusted_connection_is_still_open(self):
        missing = self.root / "unreadable"
        self.observer_output(text_records(*self.paths, missing))
        self.vmmap_output(vmmap_report(self.core, vmmap_region("mapped file", missing)))
        self.run_launch()
        self.runner.assert_called_once()
        self.vmmap_runner.assert_called_once()

    def run_launch(self, peer=PEER, trusted=True, handshake_error=None, outcome=None, early_images=()):
        scratch = self.root / "launch"
        scratch.mkdir()
        listener = mock.MagicMock()
        listener.__enter__.return_value = listener
        listener.getsockname.return_value = ("127.0.0.1", 37119)
        connection = mock.MagicMock()
        listener.accept.return_value = (connection, ("127.0.0.1", 37120))
        secure = mock.MagicMock()
        secure.__enter__.return_value = secure
        secure.getpeercert.return_value = peer
        secure.version.return_value = "TLSv1.3"
        context = mock.Mock()
        context.wrap_socket.return_value = secure
        context.wrap_socket.side_effect = handshake_error
        runner = self.verifier.subprocess.run

        def run(args, **kwargs):
            if args == [str(self.gui), "--help"]:
                return subprocess.CompletedProcess(args, 0, "Display Help", "")
            if args[0] == "/usr/bin/openssl":
                for option in ("-keyout", "-out"):
                    Path(args[args.index(option) + 1]).write_text("synthetic fixture")
                return subprocess.CompletedProcess(args, 0, "", "")
            if args[0] == VMMAP[0]:
                self.assertEqual(VMMAP, list(args))
            else:
                self.assertEqual(LSOF, list(args))
            context.wrap_socket.assert_called_once_with(connection, server_side=True)
            secure.getpeercert.assert_called_once_with(binary_form=True)
            secure.__enter__.assert_called_once()
            secure.__exit__.assert_not_called()
            connection.__exit__.assert_not_called()
            self.process.terminate.assert_not_called()
            self.assertIn("peer tls certificate info: package-smoke", (scratch / "core.log").read_text())
            return runner(args, **kwargs)

        def start(argv, *, env, stdout, stderr):
            self.assertEqual([str(self.core), "client", "--new-instance", "--settings"], argv[:4])
            self.assertTrue(Path(argv[4]).is_relative_to(scratch))
            self.assertEqual("1", env["DYLD_PRINT_LIBRARIES"])
            for early_image in early_images:
                stdout.write(f"dyld[{PID}]: <ABC> {early_image}\n")
            if trusted:
                stdout.write("peer tls certificate info: package-smoke\n")
                stdout.flush()
            return self.process

        self.verifier.subprocess.run = run
        self.verifier.subprocess.Popen.side_effect = start
        self.verifier.socket = SimpleNamespace(socket=mock.Mock(return_value=listener))
        self.verifier.ssl = SimpleNamespace(
            TLSVersion=ssl.TLSVersion,
            PROTOCOL_TLS_SERVER=ssl.PROTOCOL_TLS_SERVER,
            CERT_REQUIRED=ssl.CERT_REQUIRED,
            PEM_cert_to_DER_cert=mock.Mock(return_value=PEER),
            SSLContext=mock.Mock(return_value=context),
        )
        self.verifier.time = SimpleNamespace(monotonic=mock.Mock(side_effect=(0, 11)), sleep=mock.Mock())
        with redirect_stdout(io.StringIO()), outcome or nullcontext():
            self.verifier.launch(self.gui, self.core, scratch)
        self.process.terminate.assert_called_once()
        self.process.wait.assert_called_once_with(timeout=5)
        secure.send.assert_not_called()
        secure.sendall.assert_not_called()


if __name__ == "__main__":
    unittest.main()

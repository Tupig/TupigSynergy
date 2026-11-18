#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Read-only, standard-library checks for the approved GitHub delivery contract.

This bounded textual contract validates this repository's fixed workflow shape;
it is not a general YAML parser or a claim about a completed GitHub run.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = ROOT / ".github/workflows/ci.yml"
R3_SUITES = (
    "R3RuntimeContextTests", "R3OptionContractTests", "R3SerializationTests",
    "R3MotionContractTests", "R3StartupContractTests", "R3VisualContractTests", "R3TrayQuitTests",
)
R2_SUITES = ("PageHostTests", "EditorTransactionTests", "NativeDesignTests", "LayoutAcceptanceTests")
CRITICAL_SUITES = R2_SUITES + R3_SUITES + (
    "R4VisualContractTests", "MainWindowFlowTests", "DialogRenderTests",
)
SOURCE_SHA = "1" * 40
OTHER_SHA = "2" * 40
DIAGNOSTIC_HELPER = ROOT / "scripts/ci/check-diagnostic-paths.py"
SAFE_UPLOAD_IF = (r"if:\s*\$\{\{\s*always\(\)\s*&&\s*"
                  r"steps\.diagnostics-safe\.outputs\.safe\s*==\s*'true'\s*\}\}")
PROBE_PATHS = (
    "build/bin/R3StartupProbe.exe", "build/bin/R4VisualProbe.exe",
    "build/bin/R3StartupProbe.app/Contents/MacOS/R3StartupProbe",
    "build/bin/R4VisualProbe.app/Contents/MacOS/R4VisualProbe",
)
UPLOAD_PATHS = (
    "build/r3-test-evidence/", "build/r3-test-evidence/R3StartupProbe-identity.json",
    "build/r3-test-evidence/R4VisualProbe-identity.json", "build/src/unittests/test-results/",
    "build/src/unittests/test-results/ctest.xml", "build/src/unittests/Testing/Temporary/LastTest.log",
)


def copy_actual_diagnostic_helper(workspace):
    # Mechanical fixture dependency only. The security implementation is real.
    if DIAGNOSTIC_HELPER.is_file():
        target = workspace / "scripts/ci/check-diagnostic-paths.py"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(DIAGNOSTIC_HELPER, target)


def actual_run(block, indentation=8):
    prefix = " " * indentation
    match = re.search(r"^" + prefix + r"run: \|\n((?:" + prefix + r"  .*\n|\n)+)", block, re.M)
    if not match:
        raise RuntimeError("EXTRACTION: missing actual inline run block")
    script = textwrap.dedent(match.group(1))
    if "${{" in script:
        raise RuntimeError("EXTRACTION: unresolved GitHub expression in actual run block")
    return script


def steps(text):
    return re.split(r"^      - ", text, flags=re.M)[1:]


def publish_script():
    blocks = [block for block in steps(job("github-release"))
              if block.startswith("name: Publish GitHub Release\n")]
    if len(blocks) != 1:
        raise RuntimeError("EXTRACTION: expected one actual Publish GitHub Release step")
    match = re.search(r"^        run: \|\n((?:          .*\n|\n)+)", blocks[0], re.M)
    if not match:
        raise RuntimeError("EXTRACTION: missing actual inline publication run block")
    script = textwrap.dedent(match.group(1))
    if "${{" in script:
        raise RuntimeError("EXTRACTION: unresolved GitHub expression in publication script")
    return script


# This double implements only the external CLI responses. Publication policy,
# HTTP interpretation, SHA comparison and mutation ordering remain real shell.
GH_DOUBLE = r'''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
args = sys.argv[1:]
state_path = Path(os.environ["GH_STUB_STATE"])
state = json.loads(state_path.read_text())
with Path(os.environ["GH_STUB_LOG"]).open("a") as output:
    output.write(json.dumps(args) + "\n")
def reply(status, body, include=False):
    if include:
        print("HTTP/2 " + str(status))
        print("content-type: application/json\n")
        print(json.dumps(body))
    elif status == 200:
        print(body.get("sha", json.dumps(body)))
    if status != 200:
        print("gh: HTTP " + str(status), file=sys.stderr)
    sys.exit(0 if status == 200 else 1)
if args and args[0] == "api":
    endpoint = next((x for x in args[1:] if x.startswith("repos/")), "")
    if endpoint.endswith("/commits/v2.1.1"):
        reply(state["tag_status"], {"sha": state["tag_sha"]}, "--include" in args)
    if endpoint.endswith("/commits/main"):
        reply(state["main_status"], {"sha": state["main_sha"]}, "--include" in args)
    if endpoint.endswith("/releases/tags/v2.1.1"):
        if state.get("missing_http_status"):
            print("unstructured upstream error", file=sys.stderr)
            sys.exit(1)
        reply(state["release_status"], state["release"], "--include" in args)
if len(args) > 1 and args[0] == "release":
    if args[1] in ("create", "upload", "edit"):
        if args[1] == "create":
            state["created"] = True
            state_path.write_text(json.dumps(state))
        sys.exit(0)
    if args[1] == "view":
        if state.get("created"):
            print("Generated release notes")
            sys.exit(0)
        if state["release_status"] == 200:
            print(json.dumps(state["release"]))
            sys.exit(0)
        print("gh: HTTP " + str(state["release_status"]), file=sys.stderr)
        sys.exit(1)
print("STUB_UNSUPPORTED: " + repr(args), file=sys.stderr)
sys.exit(91)
'''


def release_case(changes=None, environment=None):
    script = publish_script()
    bash = shutil.which("bash")
    if not bash:
        raise RuntimeError("ENVIRONMENT: bash is unavailable")
    version = subprocess.run([bash, "-c", "printf '%s' \"$BASH_VERSINFO\""],
                             capture_output=True, text=True, check=True).stdout
    if re.search(r"\bmapfile\b", script) and int(version) < 4:
        raise RuntimeError("ENVIRONMENT: actual Publish script requires bash >= 4 (mapfile)")
    with tempfile.TemporaryDirectory(prefix="gh-delivery-contract-") as temporary:
        temp = Path(temporary)
        (temp / "bin").mkdir()
        (temp / "dist").mkdir()
        for name in ("synergy-2.1.1-Windows-x64.msi", "synergy package-2.1.1-macOS-arm64.dmg"):
            (temp / "dist" / name).write_bytes(b"synthetic package boundary fixture\n")
        stub = temp / "bin/gh"
        stub.write_text(GH_DOUBLE)
        stub.chmod(0o700)
        state = {"tag_status": 200, "tag_sha": SOURCE_SHA, "main_status": 200,
                 "main_sha": SOURCE_SHA, "release_status": 404,
                 "release": {"tag_name": "v2.1.1", "draft": False,
                             "target_commitish": SOURCE_SHA,
                             "body": "Built-From: " + SOURCE_SHA,
                             "assets": [{"name": "existing.msi", "id": 1}]}}
        state.update(changes or {})
        (temp / "state.json").write_text(json.dumps(state))
        env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
               if key in os.environ}
        env.update({"PATH": str(temp / "bin") + os.pathsep + env.get("PATH", ""),
                    "VERSION": "2.1.1", "GH_REPO": "fixture/fixture",
                    "GITHUB_SHA": SOURCE_SHA, "GITHUB_REF": "refs/tags/v2.1.1",
                    "GITHUB_EVENT_NAME": "workflow_dispatch", "GH_TOKEN": "synthetic",
                    "GH_STUB_STATE": str(temp / "state.json"),
                    "GH_STUB_LOG": str(temp / "calls.jsonl"), "RUNNER_TEMP": str(temp),
                    "GITHUB_OUTPUT": str(temp / "output")})
        env.update(environment or {})
        result = subprocess.run([bash, "-euo", "pipefail", "-c", script], cwd=temp,
                                env=env, capture_output=True, text=True, timeout=30)
        calls = [json.loads(line) for line in (temp / "calls.jsonl").read_text().splitlines()] \
            if (temp / "calls.jsonl").exists() else []
        if "STUB_UNSUPPORTED:" in result.stderr:
            raise RuntimeError("INTERFACE: unsupported actual gh call: " + result.stderr)
        # The outer runner preserves this actual stdout/stderr and command log.
        print(json.dumps({"release_fixture": changes or {}, "environment": environment or {},
                          "exit": result.returncode, "gh_argv": calls,
                          "stdout": result.stdout, "stderr": result.stderr}))
        return result, calls


def mutations(calls):
    return [args for args in calls if len(args) > 1 and args[0] == "release"
            and args[1] in ("create", "upload", "edit")]


def native_critical_case(native, mode, unsafe=None):
    blocks = [block for block in steps(job(native)) if "ctest " in block]
    if not blocks:
        raise RuntimeError("EXTRACTION: native job has no actual critical CTest run block")
    scripts = []
    for block in blocks:
        match = re.search(r"^        run: \|\n((?:          .*\n|\n)+)", block, re.M)
        if not match:
            raise RuntimeError("EXTRACTION: missing native critical run block")
        script = textwrap.dedent(match.group(1))
        if "${{" in script:
            raise RuntimeError("EXTRACTION: unresolved expression in critical run block")
        scripts.append(script)
    bash, cmake, ctest = (shutil.which(name) for name in ("bash", "cmake", "ctest"))
    if not all((bash, cmake, ctest)):
        raise RuntimeError("ENVIRONMENT: bash/cmake/ctest required for critical step fixture")
    with tempfile.TemporaryDirectory(prefix="gh-native-critical-") as temporary:
        temp = Path(temporary) / "workspace"
        temp.mkdir()
        copy_actual_diagnostic_helper(temp)
        tests = temp / "build/src/unittests"
        tests.mkdir(parents=True)
        (temp / "bin").mkdir()
        qt = temp / "fixture Qt6"
        qt.mkdir()
        (qt / "Qt6Config.cmake").write_text("# selected Qt6 fixture\n")
        (temp / "build/CMakeCache.txt").write_text(
            "BUILD_TESTS:BOOL=ON\nSKIP_BUILD_TESTS:BOOL=ON\nQT_DIR:PATH=" + str(qt) + "\n")
        names = CRITICAL_SUITES + ("ScreenTests", "ScreenSetupModelTests", "ValidatorTests")
        definitions = []
        for name in names:
            if mode == "missing" and name == "DialogRenderTests":
                continue
            outcome = "false" if mode == "failed" and name == "R3StartupContractTests" else "true"
            definitions.append('add_test(' + name + ' "' + cmake.replace('\\', '/')
                               + '" "-E" "' + outcome + '")\n')
            if mode.startswith("backend"):
                definitions[-1] = ('add_test(' + name + ' "' + sys.executable.replace('\\', '/')
                                   + '" "' + str(temp / "bin/cmake").replace('\\', '/')
                                   + '" "--nb-env" "' + name + '")\n')
        if mode in ("backend-environment", "backend-modification"):
            prop, value = (("ENVIRONMENT", "QT_QPA_PLATFORM=offscreen") if mode == "backend-environment"
                           else ("ENVIRONMENT_MODIFICATION", "QT_QPA_PLATFORM=set:offscreen"))
            definitions.append('set_tests_properties(PageHostTests PROPERTIES ' + prop + ' "' + value + '")\n')
        (tests / "CTestTestfile.cmake").write_text("".join(definitions))
        sentinel = Path(temporary) / "outside.txt"
        sentinel_bytes = b"OUTSIDE_DIAGNOSTIC_SENTINEL_DO_NOT_READ_OR_WRITE\n"
        sentinel.write_bytes(sentinel_bytes)
        if unsafe:
            links = {
                "cache": "build/CMakeCache.txt",
                "tee": "build/r3-test-evidence/PageHostTests-build.log",
                "qt-output": "build/src/unittests/test-results/PageHostTests.txt",
                "last-test": "build/src/unittests/Testing/Temporary/LastTest.log",
                "copy-target": "build/r3-test-evidence/PageHostTests-PageHostTests.txt",
                "last-copy-target": "build/r3-test-evidence/PageHostTests-LastTest.log",
            }
            if unsafe == "copy-target":
                (tests / "test-results").mkdir(exist_ok=True)
                (tests / "test-results/PageHostTests.txt").write_text("owned failure result\n")
            path = temp / links[unsafe]
            path.parent.mkdir(parents=True, exist_ok=True)
            if path.exists():
                path.unlink()
            path.symlink_to(sentinel)
        backend_witness = r'''if sys.argv[1:2] == ["--nb-env"]:
    expected = json.loads(os.environ["NB_EXPECTED_ENV"])
    observed = {key: os.environ.get(key) for key in expected}
    child = subprocess.run([sys.executable, "-c",
        'import json,os; print(json.dumps({k:os.environ.get(k) for k in ("QT_QPA_PLATFORM","QT_STYLE_OVERRIDE")}))'],
        capture_output=True, text=True)
    inherited = json.loads(child.stdout) if child.returncode == 0 else {}
    proof = {"suite": sys.argv[2], "self": observed, "child": inherited,
             "ok": observed == expected and inherited == expected and child.returncode == 0}
    with Path(os.environ["NB_ENV_PROOF"]).open("a") as output:
        output.write(json.dumps(proof) + "\n")
    sys.exit(0 if proof["ok"] else 1)
'''
        for tool in ("cmake", "ctest", "sed", "cp", "tee", "python3"):
            wrapper = temp / "bin" / tool
            real = shutil.which(tool)
            if not real:
                raise RuntimeError("ENVIRONMENT: actual command unavailable: " + tool)
            wrapper.write_text(
                '#!' + sys.executable + '\nimport json,os,subprocess,sys\n'
                'from pathlib import Path\n'
                'with Path(os.environ["COMMAND_LOG"]).open("a") as output:\n'
                '    output.write(json.dumps([Path(sys.argv[0]).name]+sys.argv[1:])+"\\n")\n'
                + backend_witness +
                # Only the compile boundary is replaced. CTest and its synthetic
                # cmake -E true/false tests execute real installed tools.
                'if Path(sys.argv[0]).name == "cmake":\n'
                '    sys.exit(0)\n'
                'sys.exit(subprocess.call(' + repr([real]) + '+sys.argv[1:]))\n')
            wrapper.chmod(0o700)
        env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
               if key in os.environ}
        env.update({"PATH": str(temp / "bin") + os.pathsep + env.get("PATH", ""),
                    "COMMAND_LOG": str(temp / "calls.jsonl"), "REAL_CTEST": ctest,
                    "RUNNER_OS": "Windows" if native == "build-windows" else "macOS",
                    "GITHUB_OUTPUT": str(temp / "output")})
        if mode.startswith("backend"):
            if len(blocks) != 1:
                raise RuntimeError("EXTRACTION: backend witness requires one critical step")
            actual_env = dict(re.findall(r"(?m)^          (QT_QPA_PLATFORM|QT_STYLE_OVERRIDE):\s*(\S+)\s*$", blocks[0]))
            expected_env = {"QT_QPA_PLATFORM": "windows" if native == "build-windows" else "cocoa",
                            "QT_STYLE_OVERRIDE": "fusion" if native == "build-windows" else None}
            env.update(actual_env)
            env.update({"NB_EXPECTED_ENV": json.dumps(expected_env), "NB_ENV_PROOF": str(temp / "backend-proof.jsonl")})
        result = subprocess.run([bash, "-euo", "pipefail", "-c", "\n".join(scripts)],
                                cwd=temp, env=env, capture_output=True, text=True, timeout=60)
        calls = [json.loads(line) for line in (temp / "calls.jsonl").read_text().splitlines()] \
            if (temp / "calls.jsonl").exists() else []
        print(json.dumps({"actual_native_critical_step": native, "fixture_mode": mode,
                          "exit": result.returncode, "argv": calls,
                          "stdout": result.stdout, "stderr": result.stderr,
                          "compile_boundary_only_replaced": True, "ctest_is_real": True,
                          "unsafe_path_fixture": unsafe}))
        if unsafe:
            leaked = any(sentinel_bytes in path.read_bytes() for path in
                         (temp / "build/r3-test-evidence").glob("*")
                         if path.is_file() and not path.is_symlink())
            return result, calls, {"outside_unchanged": sentinel.read_bytes() == sentinel_bytes,
                                   "outside_content_copied": leaked}
        if mode.startswith("backend"):
            proof = temp / "backend-proof.jsonl"
            records = [json.loads(line) for line in proof.read_text().splitlines()] if proof.exists() else []
            print(json.dumps({"actual_native_backend_inheritance": native, "fixture": mode, "records": records}))
            return result, calls, records
        return result, calls


def job(name):
    match = re.search(r"^  " + re.escape(name) + r":\n(.*?)(?=^  [A-Za-z0-9_-]+:\n|\Z)",
                      WORKFLOW.read_text(), re.M | re.S)
    if not match:
        raise AssertionError("Missing required job: " + name)
    return match.group(1)


def gate_case(event, package, platform, flags, results):
    blocks = [block for block in steps(job("ci-passed")) if "run: |" in block]
    if len(blocks) != 1:
        raise RuntimeError("EXTRACTION: expected one actual ci-passed shell")
    match = re.search(r"^        run: \|\n((?:          .*\n|\n)+)", blocks[0], re.M)
    if not match:
        raise RuntimeError("EXTRACTION: missing actual ci-passed shell")
    values = {"needs." + name + ".result": value for name, value in results.items()}
    values.update({"needs.get-version.outputs." + name: value for name, value in flags.items()})
    values.update({"github.event_name": event, "inputs.package-type": package,
                   "github.event.inputs.package-type": package,
                   "inputs.platform": platform, "github.event.inputs.platform": platform})
    def substitute(found):
        key = found.group(1).strip()
        if key not in values:
            raise RuntimeError("EXTRACTION: unapproved gate expression: " + key)
        return values[key]
    script = re.sub(r"\$\{\{(.*?)\}\}", substitute, textwrap.dedent(match.group(1)))
    bash = shutil.which("bash")
    if not bash:
        raise RuntimeError("ENVIRONMENT: bash required for actual final gate")
    with tempfile.TemporaryDirectory(prefix="gh-final-gate-") as temporary:
        env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
               if key in os.environ}
        env.update({"GITHUB_EVENT_NAME": event, "PACKAGE_TYPE": package, "PLATFORM": platform,
                    "GITHUB_STEP_SUMMARY": str(Path(temporary) / "summary")})
        result = subprocess.run([bash, "-euo", "pipefail", "-c", script], cwd=temporary,
                                env=env, capture_output=True, text=True, timeout=15)
        print(json.dumps({"actual_final_gate": {"event": event, "package": package,
                          "platform": platform, "plan": flags, "results": results},
                          "exit": result.returncode, "stdout": result.stdout, "stderr": result.stderr}))
        return result


def diagnostic_stage_case(native, stage, unsafe=None):
    if stage == "failure-read":
        action = (ROOT / ".github/actions/run-tests/action.yml").read_text()
        blocks = re.split(r"^    - ", action, flags=re.M)[1:]
        selected = [block for block in blocks if block.startswith("name: Show current test failure diagnostics\n")]
        indentation = 6
    else:
        selected = [block for block in steps(job(native)) if
                    (stage == "probe" and block.startswith("name: Preserve test probe identities\n"))
                    or (stage == "final" and re.search(r"(?m)^        id:\s*diagnostics-safe\s*$", block))]
        indentation = 8
    if len(selected) != 1:
        raise AssertionError("Required actual diagnostic stage missing: " + stage)
    script = actual_run(selected[0], indentation)
    bash = shutil.which("bash")
    if not bash:
        raise RuntimeError("ENVIRONMENT: bash required for diagnostic stage")
    with tempfile.TemporaryDirectory(prefix="gh-diagnostic-stage-") as temporary:
        outer = Path(temporary)
        workspace = outer / "workspace"
        workspace.mkdir()
        copy_actual_diagnostic_helper(workspace)
        outside = outer / "outside"
        outside.mkdir()
        sentinel = outside / "sentinel.txt"
        content = b"OUTSIDE_DIAGNOSTIC_SENTINEL_NOT_A_REAL_SECRET\n"
        sentinel.write_bytes(content)
        if unsafe != "missing":
            for name in UPLOAD_PATHS:
                path = workspace / name
                if name.endswith("/"):
                    path.mkdir(parents=True, exist_ok=True)
                else:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text('{"owned":"failed-test diagnostic fixture"}\n')
            for name in PROBE_PATHS:
                path = workspace / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"synthetic probe identity, never executed\n")
        def link(path, target):
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            elif path.exists() or path.is_symlink():
                path.unlink()
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(target, target_is_directory=target.is_dir())
        if isinstance(unsafe, tuple):
            kind, name = unsafe
            path = workspace / name
            if kind == "root-link":
                link(path, outside if name.endswith("/") else sentinel)
            elif kind == "nested-link":
                link(path / "nested/linked.txt", sentinel)
            elif kind == "dangling-link":
                link(path, outside / "absent")
            elif kind == "fifo":
                if not hasattr(os, "mkfifo"):
                    raise RuntimeError("ENVIRONMENT: native FIFO fixture unavailable on this host")
                path.parent.mkdir(parents=True, exist_ok=True)
                os.mkfifo(path)
            else:
                raise RuntimeError("INTERFACE: unknown diagnostic fixture")
        elif unsafe == "ancestor":
            link(workspace / "build", outside)
        elif unsafe == "probe-leaf":
            link(workspace / PROBE_PATHS[0], sentinel)
        elif unsafe == "probe-parent":
            target = outside / "probe-bundle"
            (target / "Contents/MacOS").mkdir(parents=True)
            real_probe = target / "Contents/MacOS/R3StartupProbe"
            real_probe.write_bytes(content)
            sentinel = real_probe
            link(workspace / "build/bin/R3StartupProbe.app", target)
        elif unsafe == "identity":
            link(workspace / "build/r3-test-evidence/R3StartupProbe-identity.json", sentinel)
        elif unsafe == "raw-read":
            link(workspace / "build/src/unittests/test-results/current.txt", sentinel)
        audit = outer / "audit"
        audit.mkdir()
        (audit / "sitecustomize.py").write_text(
            'import json,os,sys\n'
            'def watch(event,args):\n'
            '    if event != "open" or not isinstance(args[0],(str,bytes)): return\n'
            '    if os.path.realpath(args[0]) == os.environ["DIAG_SENTINEL"]:\n'
            '        with open(os.environ["DIAG_AUDIT"],"a") as log:\n'
            '            log.write(json.dumps({"event":event,"path":str(args[0])})+"\\n")\n'
            'sys.addaudithook(watch)\n')
        output = workspace / "step-output"
        env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
               if key in os.environ}
        env.update({"GITHUB_OUTPUT": str(output), "PYTHONPATH": str(audit),
                    "DIAG_SENTINEL": str(sentinel.resolve()), "DIAG_AUDIT": str(outer / "opened.jsonl")})
        result = subprocess.run([bash, "-euo", "pipefail", "-c", script], cwd=workspace,
                                env=env, capture_output=True, text=True, timeout=30)
        safe = output.is_file() and "safe=true" in output.read_text().splitlines()
        opened = (outer / "opened.jsonl").read_text() if (outer / "opened.jsonl").exists() else ""
        digest = hashlib.sha256(content).hexdigest().encode()
        leaked = False
        evidence = workspace / "build/r3-test-evidence"
        if evidence.is_dir() and not evidence.is_symlink():
            for path in evidence.rglob("*"):
                if path.is_file() and not path.is_symlink():
                    data = path.read_bytes()
                    leaked = leaked or content in data or digest in data
        proof = {"safe": safe, "outside_unchanged": sentinel.read_bytes() == content,
                 "outside_open_audit": opened, "sentinel_in_evidence": leaked,
                 "sentinel_in_console": content.decode().strip() in result.stdout + result.stderr}
        print(json.dumps({"actual_diagnostic_stage": stage, "native": native, "unsafe": unsafe,
                          "exit": result.returncode, "stdout": result.stdout,
                          "stderr": result.stderr, "proof": proof,
                          "limitation": "No atomic guarantee against active concurrent path replacement"}))
        return result, proof


class GitHubDeliveryContract(unittest.TestCase):
    def test_release_requires_both_native_builds_success(self):
        publish = job("github-release")
        condition = re.search(r"^    if:\s*(.*)$", publish, re.M).group(1)
        self.assertIn("needs.build-windows.result == 'success'", condition)
        self.assertIn("needs.build-macos.result == 'success'", condition)
        self.assertNotIn("||", condition)
        self.assertIn("publish_release == 'true'", condition)
        self.assertIn("needs.get-version.result == 'success'", condition)
        self.assertIn("needs.codeql.result == 'success'", condition)

    def test_real_codeql_job_preserves_required_name_permissions_and_no_build(self):
        analysis = job("codeql")
        self.assertRegex(analysis, r"(?m)^    name:\s*Analyze C\+\+\s*$")
        self.assertRegex(analysis, r"(?m)^    runs-on:\s*ubuntu-latest\s*$")
        self.assertEqual(len(re.findall(r"(?m)^    name:\s*Analyze C\+\+\s*$", WORKFLOW.read_text())), 1)
        self.assertNotRegex(analysis, r"(?m)^\s*(?:if|continue-on-error|strategy|run):")
        self.assertNotRegex(analysis, r"secrets[.\[]|github/codeql-action/autobuild")
        self.assertNotRegex(analysis, r"(?m)^\s*(?:ref|repository|token):")
        global_permissions = re.search(r"(?m)^permissions:\n((?:  [^\n]*\n)+)", WORKFLOW.read_text())
        self.assertIsNotNone(global_permissions)
        self.assertEqual(re.findall(r"(?m)^  ([\w-]+):\s*(\w+)\s*$", global_permissions.group(1)),
                         [("contents", "read")])
        permissions = re.search(r"(?m)^    permissions:\n((?:      [^\n]*\n)+)", analysis)
        self.assertIsNotNone(permissions)
        self.assertEqual(dict(re.findall(r"(?m)^      ([\w-]+):\s*(\w+)\s*$", permissions.group(1))),
                         {"contents": "read", "security-events": "write"})
        blocks = steps(analysis)
        expected = (
            "actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803",
            "github/codeql-action/init@33d7b85da1a04171e211befe2c739303b9a4c066",
            "github/codeql-action/analyze@33d7b85da1a04171e211befe2c739303b9a4c066",
        )
        actual = [re.findall(r"(?m)^        uses:\s*([^\s#]+)", block) for block in blocks]
        self.assertEqual(actual, [[action] for action in expected])
        init_inputs = dict(re.findall(r"(?m)^          ([\w-]+):\s*([^\s#]+)", blocks[1]))
        self.assertEqual(init_inputs, {"languages": "c-cpp", "build-mode": "none"})
        analyze_inputs = dict(re.findall(r"(?m)^          ([\w-]+):\s*([^\s#]+)", blocks[2]))
        self.assertIn(analyze_inputs, ({"category": "/language:c-cpp"},
                                     {"category": "/language:c-cpp", "upload": "always"}))
        for name in ("github-release", "ci-passed"):
            with self.subTest(required_by=name):
                prefix = job(name).split("    steps:", 1)[0]
                dependencies = re.search(r"(?m)^    needs:\s*(\[[^\]]+\])", prefix)
                self.assertIsNotNone(dependencies)
                self.assertIn("codeql", re.findall(r"[\w-]+", dependencies.group(1)))

    def test_workflow_does_not_mutate_source_or_push_main(self):
        paths = list((ROOT / ".github/workflows").glob("*.yml"))
        paths += list((ROOT / ".github/actions").glob("*/action.yml"))
        for path in paths:
            executable = "\n".join(line for line in path.read_text().splitlines()
                                   if not line.lstrip().startswith("#"))
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertNotRegex(executable, r"\bgit\s+(?:push|commit)\b")
                self.assertNotRegex(executable, r"\b(?:bash|python3?)\s+[^\n]*bump[-_]version")

    def test_committed_version_is_semantic_and_manifest_agrees(self):
        manifest = json.loads((ROOT / "vcpkg.json").read_text())
        self.assertRegex(manifest["version-string"], r"^\d+\.\d+\.\d+$")
        with tempfile.TemporaryDirectory(prefix="r3-version-") as temp:
            output = Path(temp) / "VERSION"
            result = subprocess.run([
                "cmake", "-DVERSION_FILE=" + str(output), "-DSYNERGY_VERSION_RELEASE=ON",
                "-DSYNERGY_VERSION_SNAPSHOT=OFF", "-P", str(ROOT / "cmake/SaveVersion.cmake")
            ], cwd=ROOT, capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(output.read_text().strip(), manifest["version-string"])

    def test_both_native_jobs_run_r3_and_retained_r2_contracts(self):
        for native in ("build-windows", "build-macos"):
            text = job(native)
            with self.subTest(job=native):
                for suite in R3_SUITES + R2_SUITES:
                    self.assertIn(suite, text)
                self.assertIn("--no-tests=error", text)
                self.assertIn("--output-on-failure", text)
                self.assertNotIn("ctest --test-dir build/src/unittests || true", text)

    def test_diagnostic_artifacts_preserve_raw_results_even_on_failure(self):
        for native in ("build-windows", "build-macos"):
            text = job(native)
            blocks = re.split(r"^      - ", text, flags=re.M)
            matching = [block for block in blocks if "actions/upload-artifact@" in block
                        and "r3-test-evidence" in block]
            with self.subTest(job=native):
                self.assertTrue(matching, "Missing R3 evidence upload")
                evidence = "\n".join(matching)
                self.assertRegex(evidence, SAFE_UPLOAD_IF)
                for expected in ("test-results", "LastTest.log", "r3-test-evidence", "R3StartupProbe"):
                    self.assertIn(expected, evidence)

    def test_probe_is_test_only_and_not_a_release_subject(self):
        cmake = (ROOT / "src/unittests/gui/CMakeLists.txt").read_text()
        self.assertIn("add_executable(R3StartupProbe EXCLUDE_FROM_ALL", cmake)
        self.assertIn("src/apps/synergy-gui/synergy-gui.cpp", cmake)
        self.assertIn("SYNERGY_EXTRA_HEADER SYNERGY_STARTUP_PROBE", cmake)
        self.assertNotRegex(cmake, r"install\s*\([^)]*R3StartupProbe")
        production = (ROOT / "src/apps/synergy-gui/CMakeLists.txt").read_text()
        self.assertNotIn("SYNERGY_STARTUP_PROBE", production)
        publish = job("github-release")
        self.assertIn("pattern: package-*", publish)
        self.assertNotIn("R3StartupProbe", publish)

    def test_publication_requires_existing_tag_bound_to_validated_sha(self):
        publish = job("github-release")
        self.assertIn('tag="v${VERSION}"', publish)
        self.assertIn('repos/${GH_REPO}/commits/${tag}', publish)
        # A failed lookup must enter a fatal branch; no implicit latest-branch tag creation.
        self.assertRegex(publish, r'if\s+!\s+existing_sha=\$\(gh api[^\n]*commits/\$\{tag\}')
        self.assertRegex(publish, r'\[\s*"\$existing_sha"\s*!=\s*"\$GITHUB_SHA"\s*\]')
        missing_branch = re.search(r'if\s+!\s+existing_sha=.*?\n(.*?)\n\s*fi', publish, re.S)
        self.assertIsNotNone(missing_branch)
        self.assertIn("exit 1", missing_branch.group(1))
        create = re.search(r'^\s*gh release create[^\n]+', publish, re.M)
        self.assertIsNotNone(create)
        self.assertIn("--verify-tag", create.group(0))


class GitHubDeliverySuccessorContract(unittest.TestCase):
    def test_version_ignores_environment_and_cmake_cache_patch_overrides(self):
        version = json.loads((ROOT / "vcpkg.json").read_text())["version-string"]
        for patch in (None, "", "99", "invalid"):
            with self.subTest(environment_patch=patch), tempfile.TemporaryDirectory(
                    prefix="gh-fixed-version-") as temporary:
                output = Path(temporary) / "VERSION"
                env = dict(os.environ)
                env.pop("SYNERGY_VERSION_PATCH", None)
                if patch is not None:
                    env["SYNERGY_VERSION_PATCH"] = patch
                result = subprocess.run([
                    "cmake", "-DVERSION_FILE=" + str(output), "-DSYNERGY_VERSION_RELEASE=ON",
                    "-DSYNERGY_VERSION_SNAPSHOT=OFF", "-DSYNERGY_VERSION_PATCH=88",
                    "-P", str(ROOT / "cmake/SaveVersion.cmake")
                ], cwd=ROOT, env=env, capture_output=True, text=True, timeout=30)
                print(json.dumps({"version_patch_fixture": patch, "exit": result.returncode,
                                  "stdout": result.stdout, "stderr": result.stderr}))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text().strip(), version)
                self.assertEqual((output.parent / "VERSION_BASE").read_text().strip(), version)

    def test_snapshot_and_dev_keep_committed_base(self):
        version = json.loads((ROOT / "vcpkg.json").read_text())["version-string"]
        for release, snapshot, suffix in (("OFF", "ON", "-snapshot+r"), ("OFF", "OFF", "-dev")):
            with self.subTest(suffix=suffix), tempfile.TemporaryDirectory(
                    prefix="gh-version-shape-") as temporary:
                output = Path(temporary) / "VERSION"
                env = dict(os.environ)
                env["SYNERGY_VERSION_PATCH"] = "99"
                result = subprocess.run([
                    "cmake", "-DVERSION_FILE=" + str(output),
                    "-DSYNERGY_VERSION_RELEASE=" + release,
                    "-DSYNERGY_VERSION_SNAPSHOT=" + snapshot,
                    "-P", str(ROOT / "cmake/SaveVersion.cmake")
                ], cwd=ROOT, env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue(output.read_text().startswith(version + suffix))
                self.assertEqual((output.parent / "VERSION_BASE").read_text().strip(), version)

    def test_no_ci_patch_computation_or_environment_propagation(self):
        for path in (WORKFLOW, ROOT / ".github/actions/get-version/action.yml",
                     ROOT / "cmake/Version.cmake"):
            text = "\n".join(line for line in path.read_text().splitlines()
                             if not line.lstrip().startswith("#"))
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertNotIn("SYNERGY_VERSION_AUTO_BUMP", text)
                self.assertNotIn("ENV{SYNERGY_VERSION_PATCH}", text)
                self.assertNotRegex(text, r"(?:export\s+|^\s*)SYNERGY_VERSION_PATCH[:=]")

    def test_publish_only_validated_tag_as_draft_then_public(self):
        result, calls = release_case()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        changed = mutations(calls)
        self.assertEqual([args[1] for args in changed], ["create", "edit"])
        create, publish = changed
        self.assertIn("v2.1.1", create)
        self.assertIn("--verify-tag", create)
        self.assertIn("--draft", create)
        self.assertIn(SOURCE_SHA, create)
        self.assertIn("--target", create)
        for package in ("dist/synergy-2.1.1-Windows-x64.msi", "dist/synergy package-2.1.1-macOS-arm64.dmg"):
            self.assertIn(package, create)
        self.assertIn("--draft=false", publish)
        self.assertTrue(any("repos/fixture/fixture/commits/main" in args for args in calls))
        self.assertNotIn("--clobber", [arg for args in calls for arg in args])

    def test_publication_rejects_invalid_identity_or_api_before_mutation(self):
        cases = (
            ("missing_tag", {"tag_status": 404}, {}),
            ("wrong_tag_sha", {"tag_sha": OTHER_SHA}, {}),
            ("empty_tag_sha", {"tag_sha": ""}, {}),
            ("tag_api_denied", {"tag_status": 403}, {}),
            ("tag_api_server_error", {"tag_status": 503}, {}),
            ("wrong_main_sha", {"main_sha": OTHER_SHA}, {}),
            ("main_api_denied", {"main_status": 403}, {}),
            ("main_api_server_error", {"main_status": 503}, {}),
            ("release_api_denied", {"release_status": 403}, {}),
            ("release_api_server_error", {"release_status": 503}, {}),
            ("release_no_http_status", {"missing_http_status": True}, {}),
            ("published_release_and_asset", {"release_status": 200}, {}),
            ("draft_release", {"release_status": 200, "release": {
                "draft": True, "tag_name": "v2.1.1", "target_commitish": SOURCE_SHA,
                "body": "Built-From: " + SOURCE_SHA, "assets": []}}, {}),
            ("wrong_ref", {}, {"GITHUB_REF": "refs/heads/unreviewed"}),
            ("published_event", {}, {"GITHUB_EVENT_NAME": "release"}),
        )
        for name, changes, env in cases:
            with self.subTest(case=name):
                result, calls = release_case(changes, env)
                self.assertNotEqual(result.returncode, 0, name + ": accepted invalid publication")
                self.assertEqual(mutations(calls), [], name + ": mutated remote before rejection")

    def test_both_native_jobs_explicitly_build_and_run_all_critical_suites(self):
        for native in ("build-windows", "build-macos"):
            text = job(native)
            for suite in CRITICAL_SUITES:
                with self.subTest(job=native, suite=suite):
                    self.assertIn(suite, text)
            build_steps = [block for block in steps(text)
                           if "cmake --build" in block and "--target" in block]
            build_commands = "\n".join(build_steps)
            for target in tuple(x for x in CRITICAL_SUITES if x != "R4VisualContractTests") + ("R4VisualProbe",):
                with self.subTest(job=native, built_target=target):
                    self.assertIn(target, build_commands)
            test_steps = [block for block in steps(text) if "ctest " in block]
            self.assertTrue(test_steps)
            for block in test_steps:
                self.assertIn("--no-tests=error", block)
                self.assertIn("--output-on-failure", block)
                timeout = re.search(r"^        timeout-minutes:\s*(\d+)\s*$", block, re.M)
                self.assertIsNotNone(timeout, native + ": critical step requires explicit timeout")
                self.assertGreaterEqual(int(timeout.group(1)), 10)
            unit = [block for block in steps(text) if "uses: ./.github/actions/run-tests" in block]
            self.assertEqual(len(unit), 1)
            timeout = re.search(r"^        timeout-minutes:\s*(\d+)\s*$", unit[0], re.M)
            self.assertIsNotNone(timeout)
            self.assertGreaterEqual(int(timeout.group(1)), 10)

    def test_native_critical_backend_scope_and_effective_child_environment(self):
        keys = r"\b(?:QT_QPA_PLATFORM|QT_STYLE_OVERRIDE)\b"
        workflow = WORKFLOW.read_text()
        self.assertNotRegex(workflow.split("\njobs:\n", 1)[0], keys)
        for path in ("src/unittests/CMakeLists.txt", "src/unittests/gui/CMakeLists.txt"):
            source = (ROOT / path).read_text()
            native_source = re.sub(r"(?ms)^(?P<i> *)if[ \t]*\(UNIX AND NOT APPLE\)\n.*?^(?P=i)endif\(\)", "", source)
            self.assertNotRegex(native_source, keys + r"|\bENVIRONMENT_MODIFICATION\b", path)
        startup = (ROOT / "src/unittests/gui/R3StartupContractTests.cpp").read_text()
        inherited = re.search(r"auto environment = QProcessEnvironment::systemEnvironment\(\);(.*?)"
                              r"process\.setProcessEnvironment\(environment\);", startup, re.S)
        self.assertIsNotNone(inherited)
        inserts = re.findall(r'\s*environment\.insert\("(SYNERGY_R3_PROBE_\w+)", [^\n]+\);', inherited.group(1))
        self.assertEqual(inserts, ["SYNERGY_R3_PROBE_ROOT", "SYNERGY_R3_PROBE_FIXTURE",
                                   "SYNERGY_R3_PROBE_KEY", "SYNERGY_R3_PROBE_AUTOMATED"])
        self.assertEqual(re.sub(r'\s*environment\.insert\("SYNERGY_R3_PROBE_\w+", [^\n]+\);', "",
                                inherited.group(1)).strip(), "")
        self.assertEqual(startup.count("process.setProcessEnvironment("), 1)
        for name in CRITICAL_SUITES + ("R3StartupProbeBootstrap",):
            self.assertNotRegex((ROOT / "src/unittests/gui" / (name + ".cpp")).read_text(),
                                keys + r"|--platform\b|--style\b", name)
        action = (ROOT / ".github/actions/run-tests/action.yml").read_text()
        remaining = [b for b in re.split(r"^    - ", action, flags=re.M) if b.startswith("name: Unit Tests\n")]
        self.assertEqual(len(remaining), 1)
        self.assertEqual(re.findall(r"(?m)^        QT_QPA_PLATFORM:\s*(\S+)\s*$", remaining[0]), ["offscreen"])
        self.assertNotIn("QT_STYLE_OVERRIDE", action)
        self.assertEqual(len(re.findall(keys, action)), 1, "remaining action must not reassign its backend")
        for native in ("build-windows", "build-macos"):
            with self.subTest(job=native):
                body = job(native)
                critical = [b for b in steps(body) if b.startswith("name: GUI configuration and critical contract tests\n")]
                self.assertEqual(len(critical), 1)
                block = critical[0]
                env = re.search(r"(?m)^        env:\n((?:          [^\n]+\n)+)", block)
                self.assertIsNotNone(env)
                pairs = [line.strip().split(":", 1) for line in env.group(1).splitlines()]
                expected = {"QT_QPA_PLATFORM": "windows", "QT_STYLE_OVERRIDE": "fusion"} if native == "build-windows" else {"QT_QPA_PLATFORM": "cocoa"}
                self.assertEqual(len(pairs), len(expected), "missing or duplicate backend/style")
                self.assertEqual({key: value.strip() for key, value in pairs}, expected)
                self.assertNotRegex(body.split("    steps:", 1)[0], keys)
                self.assertNotRegex("".join(b for b in steps(body) if b != block), keys)
                self.assertNotRegex(block, r"(?m)^\s*(?:if|continue-on-error):")
                script = actual_run(block)
                self.assertNotRegex(script, keys)
                suites = re.search(r"\bsuites=\((.*?)\)", script, re.S)
                self.assertIsNotNone(suites)
                self.assertEqual(tuple(suites.group(1).split()), CRITICAL_SUITES)
                units = [b for b in steps(body) if "uses: ./.github/actions/run-tests" in b]
                self.assertEqual(len(units), 1)
                self.assertEqual(re.findall(r"(?m)^          exclude-tests:\s*'([^']+)'\s*$", units[0]),
                                 ["^(" + "|".join(CRITICAL_SUITES) + ")$"])
                for mode in ("backend", "backend-environment", "backend-modification"):
                    with self.subTest(job=native, fixture=mode):
                        result, _, records = native_critical_case(native, mode)
                        if mode == "backend":
                            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                            self.assertEqual([r["suite"] for r in records], list(CRITICAL_SUITES))
                            self.assertTrue(all(r["ok"] for r in records))
                        else:
                            self.assertNotEqual(result.returncode, 0)
                            self.assertTrue(records and not records[0]["ok"], "CTest override was not observed")

    def test_actual_native_critical_steps_execute_exact_suites_and_propagate_errors(self):
        for native in ("build-windows", "build-macos"):
            for mode in ("success", "missing", "failed"):
                with self.subTest(job=native, fixture=mode):
                    result, calls = native_critical_case(native, mode)
                    if mode != "success":
                        self.assertNotEqual(result.returncode, 0,
                                            "actual critical step swallowed " + mode + " suite")
                        continue
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    ctest_calls = [args for args in calls if args[0] == "ctest"]
                    for suite in CRITICAL_SUITES:
                        matching = [args for args in ctest_calls if "-R" in args
                                    and args[args.index("-R") + 1] == "^" + suite + "$"]
                        self.assertTrue(matching, suite + ": no actual precisely scoped CTest invocation")
                        for args in matching:
                            self.assertIn("--no-tests=error", args)
                            self.assertIn("--output-on-failure", args)
                        self.assertRegex(result.stdout, r"(?m)^\s*Start\s+\d+: " + suite + r"\s*$")
                    built = [args for args in calls if args[0] == "cmake"
                             and "--build" in args and "--target" in args]
                    actual_targets = [arg for args in built for arg in args[args.index("--target") + 1:]]
                    for target in tuple(x for x in CRITICAL_SUITES if x != "R4VisualContractTests") + ("R4VisualProbe",):
                        self.assertIn(target, actual_targets,
                                      target + ": actual critical step did not request build target")

    def test_failure_upload_includes_junit_r4_and_original_raw(self):
        for native in ("build-windows", "build-macos"):
            uploads = [block for block in steps(job(native))
                       if "actions/upload-artifact@" in block and "r3-test-evidence" in block]
            with self.subTest(job=native):
                self.assertTrue(uploads)
                evidence = "\n".join(uploads)
                for item in ("test-results", "LastTest.log", "r3-test-evidence",
                             "R3StartupProbe", "R4VisualProbe", "ctest.xml"):
                    self.assertIn(item, evidence)
                self.assertRegex(evidence, SAFE_UPLOAD_IF)
                self.assertNotIn("continue-on-error: true", evidence)

    def test_actual_full_suite_action_rejects_zero_tests_and_preserves_failure(self):
        action = (ROOT / ".github/actions/run-tests/action.yml").read_text()
        match = re.search(r"^      run: \|\n((?:        .*\n|\n)+)", action, re.M)
        if not match:
            raise RuntimeError("EXTRACTION: missing actual Unit Tests action run block")
        script = textwrap.dedent(match.group(1))
        bash, cmake = shutil.which("bash"), shutil.which("cmake")
        if not bash or not cmake or not shutil.which("ctest"):
            raise RuntimeError("ENVIRONMENT: bash/cmake/ctest required for miniature CTest")
        for failure in (False, True):
            with self.subTest(failing_test=failure), tempfile.TemporaryDirectory(
                    prefix="gh-ctest-contract-") as temporary:
                temp = Path(temporary)
                copy_actual_diagnostic_helper(temp)
                tests = temp / "build/src/unittests"
                (tests / "test-results").mkdir(parents=True)
                command = 'add_test(ContractFailure "' + cmake.replace('\\', '/') + '" "-E" "false")\n'
                (tests / "CTestTestfile.cmake").write_text(command if failure else "# no tests\n")
                env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
                       if key in os.environ}
                env.update({"RUNNER_OS": "macOS", "GITHUB_OUTPUT": str(temp / "output")})
                result = subprocess.run([bash, "-eo", "pipefail", "-c", script], cwd=temp,
                                        env=env, capture_output=True, text=True, timeout=30)
                print(json.dumps({"mini_ctest_failure": failure, "exit": result.returncode,
                                  "stdout": result.stdout, "stderr": result.stderr}))
                self.assertNotEqual(result.returncode, 0, "native empty or failed suite succeeded")
                if failure:
                    self.assertTrue((tests / "Testing/Temporary/LastTest.log").is_file())
                    junit = tests / "test-results/ctest.xml"
                    self.assertTrue(junit.is_file(), "actual failed run did not retain JUnit")
                    self.assertIn("ContractFailure", junit.read_text())


    def test_actual_plan_and_native_wiring_only_select_windows_x64_and_macos_arm64(self):
        import shlex
        blocks = [block for block in steps(job("get-version"))
                  if block.startswith("name: Plan builds for this event\n") and "id: plan" in block]
        if len(blocks) != 1:
            raise RuntimeError("EXTRACTION: expected one actual native build planner")
        match = re.search(r"^        run: \|\n((?:          .*\n|\n)+)", blocks[0], re.M)
        if not match:
            raise RuntimeError("EXTRACTION: native build planner has no inline run block")
        script = textwrap.dedent(match.group(1))
        if "${{" in script:
            raise RuntimeError("EXTRACTION: unresolved planner expression")
        self.assertNotRegex(script, r"\b(?:gh|curl|wget|cmake|ctest)\s",
                            "planner fixture may only compute a local selection")
        bash = shutil.which("bash")
        if not bash:
            raise RuntimeError("ENVIRONMENT: bash required for actual build planner")
        cases = (
            ("pull_request", "", "", True, True, False),
            ("workflow_dispatch", "release", "all", True, True, True),
            ("workflow_dispatch", "release", "windows", True, True, True),
            ("workflow_dispatch", "release", "macos", True, True, True),
            ("workflow_dispatch", "snapshot", "all", True, True, False),
            ("workflow_dispatch", "snapshot", "", True, True, False),
            ("workflow_dispatch", "snapshot", "windows", True, False, False),
            ("workflow_dispatch", "snapshot", "macos", False, True, False),
        )
        rejected = tuple(("workflow_dispatch", "snapshot", selection)
                         for selection in ("linux", "flatpak", "macos-x86_64", "windows-arm64", "unsupported"))
        rejected += (("release", "release", "all"), ("push", "snapshot", "all"),
                     ("workflow_dispatch", "bogus", "all"), ("workflow_dispatch", "", "all"))
        with tempfile.TemporaryDirectory(prefix="gh-two-target-plan-") as temporary:
            temp = Path(temporary)
            for index, row in enumerate(cases + rejected):
                event, package, platform = row[:3]
                with self.subTest(event=event, package=package, platform=platform):
                    output = temp / (str(index) + ".output")
                    env = {key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TMPDIR", "PYTHONDONTWRITEBYTECODE")
                           if key in os.environ}
                    env.update({"GITHUB_EVENT_NAME": event, "PACKAGE_TYPE": package,
                                "PLATFORM": platform, "VERSION_BASE": "2.1.1",
                                "GITHUB_OUTPUT": str(output)})
                    result = subprocess.run([bash, "-euo", "pipefail", "-c", script], cwd=temp,
                                            env=env, capture_output=True, text=True, timeout=30)
                    values = dict(line.split("=", 1) for line in output.read_text().splitlines()) \
                        if output.exists() else {}
                    print(json.dumps({"actual_two_target_plan": {"event": event, "package": package,
                                      "platform": platform}, "exit": result.returncode,
                                      "outputs": values, "stdout": result.stdout, "stderr": result.stderr}))
                    if len(row) == 3:
                        self.assertNotEqual(result.returncode, 0,
                                            "unsupported route returned a successful empty build plan")
                        continue
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    for name, expected in zip(("build_windows", "build_macos", "publish_release"), row[3:]):
                        self.assertEqual(values.get(name), str(expected).lower())
                    windows = json.loads(values["windows_targets"])
                    macos = json.loads(values["macos_targets"])
                    self.assertEqual(len(windows), 1)
                    self.assertEqual(len(macos), 1)
                    self.assertIn(windows[0]["arch"], ("amd64", "x64"))
                    self.assertTrue(windows[0]["vcpkg-triplet"].startswith("x64-windows"))
                    self.assertEqual(macos[0]["expected-arch"], "arm64")
                    arch = [arg.split("=", 1)[1] for arg in shlex.split(macos[0]["config-args"])
                            if arg.startswith("-DCMAKE_OSX_ARCHITECTURES=")]
                    self.assertEqual(arch, ["arm64"])
                    for key in ("build_linux", "build_flatpak"):
                        self.assertNotEqual(values.get(key), "true")
                    for key in ("targets", "linux_targets", "flatpak_targets"):
                        if key in values:
                            self.assertEqual(json.loads(values[key]), [])
        windows = job("build-windows")
        vc = [block for block in steps(windows) if "uses: TheMrMilchmann/setup-msvc-dev@" in block]
        self.assertEqual(len(vc), 1)
        self.assertRegex(vc[0], r"(?m)^          arch:\s*(?:amd64|x64)\s*$")
        dependencies = [block for block in steps(windows)
                        if "uses: ./.github/actions/install-dependencies" in block]
        self.assertEqual(len(dependencies), 1)
        self.assertRegex(dependencies[0], r"(?m)^          vcpkg-triplet:\s*x64-windows[^\s]*\s*$")
        macos = job("build-macos")
        configure = [block for block in steps(macos) if block.startswith("name: Configure\n")]
        self.assertEqual(len(configure), 1)
        architecture = re.findall(r'-DCMAKE_OSX_ARCHITECTURES=(?:"([^"]+)"|\x27([^\x27]+)\x27|([^\s]+))',
                                  configure[0])
        self.assertEqual([next(value for value in row if value) for row in architecture], ["arm64"])
        text = WORKFLOW.read_text().split("\njobs:\n", 1)[1]
        names = re.findall(r"^  ([A-Za-z0-9_-]+):\n", text, re.M)
        for name in names:
            body = job(name)
            self.assertNotRegex(name.lower(), r"(?:linux|flatpak)")
            if "cmake --build" in body:
                self.assertIn(name, ("build-windows", "build-macos"),
                              "unexpected third product-build job")
        gate_dependencies = job("ci-passed").split("steps:", 1)[0].lower()
        self.assertNotRegex(gate_dependencies, r"linux|flatpak")


    def test_actual_final_gate_rejects_unexpected_skips_and_wrong_snapshot_plan(self):
        routes = (
            ("pull_request", "", "", (True, True, False)),
            ("workflow_dispatch", "release", "all", (True, True, True)),
            ("workflow_dispatch", "snapshot", "all", (True, True, False)),
            ("workflow_dispatch", "snapshot", "windows", (True, False, False)),
            ("workflow_dispatch", "snapshot", "macos", (False, True, False)),
            ("workflow_dispatch", "snapshot", "", (True, True, False)),
        )
        flag_names = ("build_windows", "build_macos", "publish_release")
        def statuses(flags):
            result = {name: "success" for name in ("get-version", "lint-clang", "lint-encoding", "codeql")}
            result.update({name: "success" if flags[key] == "true" else "skipped" for name, key in (
                ("build-windows", "build_windows"), ("build-macos", "build_macos"),
                ("github-release", "publish_release"))})
            return result
        for event, package, platform, selection in routes:
            flags = dict(zip(flag_names, (str(value).lower() for value in selection)))
            normal = statuses(flags)
            with self.subTest(event=event, package=package, platform=platform, fault="none"):
                result = gate_case(event, package, platform, flags, normal)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for name, expected in normal.items():
                for fault in (value for value in ("success", "skipped", "failure", "cancelled") if value != expected):
                    with self.subTest(event=event, package=package, platform=platform, job=name, result=fault):
                        mutated = dict(normal, **{name: fault})
                        result = gate_case(event, package, platform, flags, mutated)
                        self.assertNotEqual(result.returncode, 0,
                                            "actual final gate accepted unexpected " + name + ": " + fault)
        mismatches = (
            ("pull_request", "", "", ("true", "false", "false")),
            ("workflow_dispatch", "release", "all", ("true", "false", "true")),
            ("workflow_dispatch", "release", "all", ("true", "true", "false")),
            ("workflow_dispatch", "snapshot", "windows", ("true", "true", "false")),
            ("workflow_dispatch", "snapshot", "macos", ("true", "false", "false")),
            ("workflow_dispatch", "snapshot", "all", ("false", "false", "false")),
            ("workflow_dispatch", "snapshot", "all", ("", "true", "false")),
            ("workflow_dispatch", "snapshot", "all", ("bogus", "true", "false")),
            ("workflow_dispatch", "bogus", "all", ("true", "true", "false")),
            ("workflow_dispatch", "", "all", ("true", "true", "false")),
            ("release", "release", "all", ("true", "true", "true")),
            ("push", "snapshot", "all", ("true", "true", "false")),
        )
        for event, package, platform, selection in mismatches:
            with self.subTest(event=event, package=package, platform=platform, wrong_plan=selection):
                flags = dict(zip(flag_names, selection))
                result = gate_case(event, package, platform, flags, statuses(flags))
                self.assertNotEqual(result.returncode, 0, "final gate trusted a wrong or malformed plan")

    def test_actual_diagnostic_validation_precedes_reads_writes_and_failure_upload(self):
        self.assertTrue(DIAGNOSTIC_HELPER.is_file(), "required internal diagnostic path validator is missing")
        DIAGNOSTIC_HELPER.read_bytes().decode("ascii")
        self.assertRegex(DIAGNOSTIC_HELPER.read_text(), r"0x400|FILE_ATTRIBUTE_REPARSE_POINT")
        direct = (
            (("absent",), "missing", True), (("--recursive", "absent"), "missing", True),
            (("regular",), "regular", True), (("--recursive", "directory"), "regular", True),
            (("regular/child",), "regular", False), (("linked",), "link", False),
            (("linked",), "dangling", False), (("--recursive", "directory"), "nested", False),
            (("special",), "fifo", False), (("../outside",), "missing", False),
            (("/outside",), "missing", False), (("C:/outside",), "missing", False),
            (("//host/share",), "missing", False), (("directory\\child",), "missing", False),
            (("regular:stream",), "missing", False), (("line\nbreak",), "missing", False),
            (("",), "missing", False), (("linked/child",), "parent", False),
        )
        for arguments, kind, allowed in direct:
            with self.subTest(helper_args=arguments, fixture=kind), tempfile.TemporaryDirectory(
                    prefix="gh-path-validator-") as temporary:
                outer = Path(temporary)
                workspace = outer / "workspace"
                workspace.mkdir()
                sentinel = outer / "outside"
                sentinel.write_bytes(b"owned external sentinel\n")
                (workspace / "regular").write_text("owned regular file\n")
                (workspace / "directory").mkdir()
                (workspace / "directory/regular").write_text("owned descendant\n")
                if kind == "link":
                    (workspace / "linked").symlink_to(sentinel)
                elif kind == "dangling":
                    (workspace / "linked").symlink_to(outer / "never-created")
                elif kind == "nested":
                    (workspace / "directory/link").symlink_to(sentinel)
                elif kind == "parent":
                    target = outer / "outside-directory"
                    target.mkdir()
                    (target / "child").write_text("owned external descendant\n")
                    (workspace / "linked").symlink_to(target, target_is_directory=True)
                elif kind == "fifo":
                    if not hasattr(os, "mkfifo"):
                        raise RuntimeError("ENVIRONMENT: FIFO dynamic fixture unavailable on this host")
                    os.mkfifo(workspace / "special")
                result = subprocess.run([sys.executable, str(DIAGNOSTIC_HELPER), *arguments],
                                        cwd=workspace, capture_output=True, text=True, timeout=10)
                print(json.dumps({"actual_path_helper": arguments, "fixture": kind,
                                  "exit": result.returncode, "stdout": result.stdout, "stderr": result.stderr}))
                self.assertEqual(result.returncode == 0, allowed)
                self.assertNotIn("safe=true", result.stdout, "only the final step may emit safe")
                self.assertEqual(sentinel.read_bytes(), b"owned external sentinel\n")
        for native in ("build-windows", "build-macos"):
            blocks = steps(job(native))
            upload = [block for block in blocks if block.startswith("name: Upload raw contract diagnostics\n")]
            guard = [block for block in blocks if re.search(r"(?m)^        id:\s*diagnostics-safe\s*$", block)]
            self.assertEqual(len(upload), 1)
            self.assertEqual(len(guard), 1, "last upload validation stage is missing")
            self.assertEqual(blocks.index(guard[0]) + 1, blocks.index(upload[0]))
            self.assertRegex(guard[0], r"if:\s*\$\{\{\s*always\(\)\s*\}\}")
            self.assertRegex(upload[0], SAFE_UPLOAD_IF)
            self.assertIn("scripts/ci/check-diagnostic-paths.py", guard[0])
            paths = re.search(r"(?m)^          path: \|\n((?:            .*\n)+)", upload[0])
            self.assertIsNotNone(paths)
            self.assertEqual(tuple(line.strip() for line in paths.group(1).splitlines()), UPLOAD_PATHS)
            final_cases = (None, "missing", "ancestor") + tuple(("root-link", name) for name in UPLOAD_PATHS)
            final_cases += (("nested-link", UPLOAD_PATHS[0]), ("nested-link", UPLOAD_PATHS[3]),
                            ("dangling-link", UPLOAD_PATHS[4]),
                            ("fifo", "build/r3-test-evidence/special"))
            for unsafe in final_cases:
                with self.subTest(job=native, final_upload=unsafe):
                    result, proof = diagnostic_stage_case(native, "final", unsafe)
                    allowed = unsafe in (None, "missing")
                    self.assertEqual(result.returncode == 0, allowed)
                    self.assertEqual(proof["safe"], allowed,
                                     "failed-test upload must be eligible iff the actual validation succeeded")
                    self.assertTrue(proof["outside_unchanged"])
                    self.assertFalse(proof["outside_open_audit"])
                    self.assertFalse(proof["sentinel_in_evidence"])
            probe_cases = (None, "probe-parent", "identity") + tuple(("root-link", name) for name in PROBE_PATHS)
            probe_cases += (("root-link", "build/bin/"), ("root-link", "build/bin/R4VisualProbe.app/"),
                            ("root-link", UPLOAD_PATHS[2]))
            for unsafe in probe_cases:
                with self.subTest(job=native, before_probe_hash=unsafe):
                    result, proof = diagnostic_stage_case(native, "probe", unsafe)
                    self.assertEqual(result.returncode == 0, unsafe is None)
                    self.assertTrue(proof["outside_unchanged"])
                    self.assertFalse(proof["outside_open_audit"], "unsafe probe was opened before refusal")
                    self.assertFalse(proof["sentinel_in_evidence"])
            for unsafe in (None, "raw-read", ("root-link", UPLOAD_PATHS[-1])):
                with self.subTest(job=native, before_failure_console=unsafe):
                    result, proof = diagnostic_stage_case(native, "failure-read", unsafe)
                    self.assertEqual(result.returncode == 0, unsafe is None)
                    self.assertFalse(proof["sentinel_in_console"])
                    self.assertTrue(proof["outside_unchanged"])
            for unsafe in ("cache", "tee", "qt-output", "last-test", "copy-target", "last-copy-target"):
                with self.subTest(job=native, critical_pre_use=unsafe):
                    result, calls, proof = native_critical_case(native, "success", unsafe)
                    self.assertNotEqual(result.returncode, 0)
                    relative = {"cache": "build/CMakeCache.txt",
                                "tee": "build/r3-test-evidence/PageHostTests-build.log",
                                "qt-output": "build/src/unittests/test-results/PageHostTests.txt",
                                "last-test": UPLOAD_PATHS[-1],
                                "copy-target": "build/r3-test-evidence/PageHostTests-PageHostTests.txt",
                                "last-copy-target": "build/r3-test-evidence/PageHostTests-LastTest.log"}[unsafe]
                    guarded = False
                    for args in calls:
                        if "scripts/ci/check-diagnostic-paths.py" not in args:
                            continue
                        paths = args[args.index("scripts/ci/check-diagnostic-paths.py") + 1:]
                        guarded = guarded or any(path == relative or path.startswith(relative + "/")
                                                 or ("--recursive" in paths and relative.startswith(path.rstrip("/") + "/"))
                                                 for path in paths if not path.startswith("--"))
                    self.assertTrue(guarded, "actual helper did not cover the unsafe pre-use path")
                    self.assertTrue(proof["outside_unchanged"])
                    self.assertFalse(proof["outside_content_copied"])
                    if unsafe == "cache":
                        self.assertFalse(any(args[0] == "sed" for args in calls), "cache was read before validation")
                    if unsafe == "tee":
                        self.assertFalse(any(args[0] == "tee" or (args[0] == "cmake" and "--build" in args)
                                             for args in calls), "build/tee ran before validating its output")
                    if unsafe in ("qt-output", "last-test"):
                        self.assertFalse(any(args[0] == "ctest" for args in calls), "CTest ran with unsafe raw outputs")
                    if unsafe in ("copy-target", "last-copy-target"):
                        self.assertFalse(any(args[0] == "cp" for args in calls), "copy ran before destination validation")
        print("LIMITATION: Windows reparse/junction dynamic coverage and active concurrent replacement are not proven on this host.")


if __name__ == "__main__":
    unittest.main(verbosity=2)

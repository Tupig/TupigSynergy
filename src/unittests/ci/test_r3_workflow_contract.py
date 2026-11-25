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
RELEASE_ASSET_SUFFIXES = ("mac_arm64.dmg", "windows_x64-portable.7z", "windows_x64.msi")
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


def actual_step_run(block):
    """Extract one existing workflow step without interpreting YAML generally."""
    multi = re.search(r"^        run: \|\n((?:          .*\n|\n)+)", block, re.M)
    if multi:
        return textwrap.dedent(multi.group(1))
    single = re.search(r"^        run: (.+)$", block, re.M)
    if single:
        return single.group(1) + "\n"
    raise RuntimeError("EXTRACTION: missing actual step run command")


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
        if "--notes-file" in args:
            notes_path = Path(args[args.index("--notes-file") + 1])
            with Path(os.environ["GH_STUB_LOG"]).open("a") as output:
                output.write(json.dumps(["release", args[1] + "-notes", notes_path.read_text()]) + "\n")
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


def release_case(changes=None, environment=None, changed_package=False):
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
        asset_names = tuple("synergy_2.1.1_" + suffix for suffix in RELEASE_ASSET_SUFFIXES)
        manifest = temp / "release-subjects.txt"
        records = []
        for name in asset_names:
            payload = ("synthetic package boundary fixture: " + name).encode()
            (temp / "dist" / name).write_bytes(payload)
            records.append(f"{hashlib.sha256(payload).hexdigest()}  dist/{name}\n")
        manifest.write_text("".join(records))
        if changed_package:
            (temp / "dist" / asset_names[0]).write_bytes(b"different bytes after attestation")
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
                    "RELEASE_NOTES_ZH": "- 修复窗口显示。",
                    "RELEASE_NOTES_EN": "- Fixed window layout.",
                    "GITHUB_SHA": SOURCE_SHA, "GITHUB_REF": "refs/tags/v2.1.1",
                    "GITHUB_EVENT_NAME": "workflow_dispatch", "GH_TOKEN": "synthetic",
                    "GH_STUB_STATE": str(temp / "state.json"),
                    "GH_STUB_LOG": str(temp / "calls.jsonl"), "RUNNER_TEMP": str(temp),
                    "GITHUB_OUTPUT": str(temp / "output"),
                    "RELEASE_SUBJECTS": str(manifest),
                    # The actual native job's verified output is intentionally
                    # explicit. Retain the established unsigned-package
                    # contract for pre-signing fixtures.
                    "MACOS_PACKAGE_SIGNING": "ad-hoc"})
        for key, value in (environment or {}).items():
            if value is None:
                env.pop(key, None)
            else:
                env[key] = value
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
                    "SYNERGY_CI_CMAKE": str(temp / "bin" / "cmake"),
                    "SYNERGY_CI_CPACK": str(temp / "bin" / "cpack"),
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

    def test_native_package_uploads_fail_if_empty(self):
        for native in ("build-windows", "build-macos"):
            with self.subTest(native=native):
                upload = next(block for block in steps(job(native)) if block.startswith("name: Upload package\n"))
                self.assertRegex(upload, r"(?m)^          if-no-files-found: error$")

    def test_release_hashes_exact_package_inventory_before_attestation(self):
        publish = job("github-release")
        self.assertLess(publish.index("name: Hash release packages"), publish.index("name: Attest build provenance"))
        step = next(block for block in steps(publish) if block.startswith("name: Hash release packages\n"))
        script = actual_run(step)
        prefix, version = "fixture-package", "2.7.4"
        expected = tuple(f"{prefix}_{version}_{suffix}" for suffix in RELEASE_ASSET_SUFFIXES)
        cases = {
            "complete": expected,
            "no-mac": expected[1:],
            "no-portable": (expected[0], expected[2]),
            "no-msi": expected[:2],
            "wrong-version": (expected[0].replace(version, "2.7.3"), *expected[1:]),
            "wrong-architecture": (expected[0].replace("arm64", "x64"), *expected[1:]),
            "extra": (*expected, "extra.txt"),
            "nested": ("nested/" + expected[0], *expected[1:]),
            "empty-msi": expected,
            "symlink-msi": expected,
        }
        for case, files in cases.items():
            with self.subTest(case=case), tempfile.TemporaryDirectory(prefix="gh-release-inventory-") as directory:
                temp = Path(directory)
                dist = temp / "dist"
                dist.mkdir()
                for name in files:
                    path = dist / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(name.encode())
                if case == "empty-msi":
                    (dist / expected[-1]).write_bytes(b"")
                if case == "symlink-msi":
                    (dist / expected[-1]).unlink()
                    (dist / expected[-1]).symlink_to(expected[0])
                dest = temp / "output"
                env = {**os.environ, "PACKAGE_PREFIX": prefix, "VERSION": version,
                       "GITHUB_OUTPUT": str(dest), "RUNNER_TEMP": directory}
                result = subprocess.run(["bash", "-euo", "pipefail", "-c", script], cwd=temp,
                                        env=env, capture_output=True, text=True, timeout=30)
                manifest = temp / "release-subjects.txt"
                if case == "complete":
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(dest.read_text(), f"path={manifest}\n")
                    records = {f"{hashlib.sha256(name.encode()).hexdigest()}  dist/{name}" for name in expected}
                    self.assertEqual(set(manifest.read_text().splitlines()), records)
                else:
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("release package inventory", result.stdout)
                    self.assertFalse(manifest.exists())
                    self.assertFalse(dest.exists())

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

    def test_windows_absolute_tool_contract_when_present(self):
        """Execute the extracted Configure and Package glue against an old PATH."""
        workflow = WORKFLOW.read_text()
        windows = job("build-windows")
        configure = next(block for block in steps(windows) if block.startswith("name: Configure\n"))
        package = next(block for block in steps(windows) if block.startswith("name: Package\n"))
        configure_source = actual_step_run(configure)
        package_source = actual_step_run(package)
        helper = ROOT / "scripts/ci/verify-package-windows.bat"
        with tempfile.TemporaryDirectory(prefix="windows-absolute-tools-") as temporary:
            temp = Path(temporary)
            old = temp / "old"
            # Mirror the exact extracted wheel layout: GitHub Actions receives
            # absolute .exe paths rooted in RUNNER_TEMP, not a synthetic bin.
            verified = temp / "cmake-4.3.4" / "cmake" / "data" / "bin"
            old.mkdir()
            verified.mkdir(parents=True)
            log = temp / "argv.log"
            argv_json_log = temp / "argv.jsonl"

            def write_tool(directory, tool, label, version):
                path = directory / (tool + ".exe")
                path.write_text(
                    "#!/bin/sh\n"
                    "printf '%s\\t%s\\t%s\\n' " + repr(label) + " " + repr(tool) + " \"$*\" >> \"$SYNERGY_TOOL_LOG\"\n"
                    "python3 -c 'import json, os, sys; open(os.environ[\"SYNERGY_TOOL_ARGV_JSON_LOG\"], \"a\", encoding=\"utf-8\").write(json.dumps({\"label\": sys.argv[1], \"tool\": sys.argv[2], \"argv\": sys.argv[3:]}) + \"\\n\")' "
                    + repr(label) + " " + repr(tool) + " \"$@\"\n"
                    "if [ \"$1\" = --version ]; then printf '%s\\n' " + repr(tool + " version " + version) + "; fi\n"
                    "if [ " + repr(tool) + " = cmake ]; then\n"
                    "  for arg in \"$@\"; do\n"
                    "    if [ \"$arg\" = -Bbuild ]; then\n"
                    "      mkdir -p build\n"
                    "      if [ \"${SYNERGY_FIXTURE_NINJA_MODE:-verified}\" = stale-package ]; then\n"
                    "        printf 'build CMakeFiles/package.util: CUSTOM_COMMAND all\\n  COMMAND = /old/cpack --config ./CPackConfig.cmake\\nbuild package: phony CMakeFiles/package.util\\n# unrelated=%s\\n' \"$SYNERGY_CI_CPACK\" > build/build.ninja\n"
                    "      else\n"
                    "        printf 'build CMakeFiles/package.util: CUSTOM_COMMAND all\\n  COMMAND = %s --config ./CPackConfig.cmake\\nbuild package: phony CMakeFiles/package.util\\n' \"$SYNERGY_CI_CPACK\" > build/build.ninja\n"
                    "      fi\n"
                    "    fi\n"
                    "  done\n"
                    "fi\n"
                )
                path.chmod(0o700)
                # Git Bash resolves .exe names through PATH; the POSIX host
                # running this fixture does not, so retain only this adapter
                # for a deliberately selected old-PATH executable.
                compatibility_name = directory / tool
                if compatibility_name.exists() or compatibility_name.is_symlink():
                    compatibility_name.unlink()
                compatibility_name.symlink_to(path.name)
                return path

            for directory, label, version in ((old, "old", "3.31.6"), (verified, "verified", "4.3.4")):
                write_tool(directory, "cmake", label, version)
                write_tool(directory, "cpack", label, version)
            verified_cmake = verified / "cmake.exe"
            verified_cpack = verified / "cpack.exe"

            controlled_configure = ("cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DSKIP_BUILD_TESTS=ON "
                                   "-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON -DCMAKE_COMPILE_WARNING_AS_ERROR=ON")
            generic_vcpkg_config = (
                r"-DCMAKE_TOOLCHAIN_FILE=D:\ci\workspace\vcpkg\scripts\buildsystems\vcpkg.cmake "
                "-DVCPKG_TARGET_TRIPLET=x64-windows-release -DVCPKG_MANIFEST_MODE=OFF")
            expected_vcpkg_argv = generic_vcpkg_config.split()
            vcpkg_output = "${{ steps.get-deps.outputs.vcpkg-cmake-config }}"
            vcpkg_binding = re.search(
                r"^        env:\n(?:          [^\n]*\n)*?"
                r"          SYNERGY_CI_VCPKG_CONFIG: ([^\n]+)$", configure, re.M)
            vcpkg_binding_value = vcpkg_binding.group(1).strip() if vcpkg_binding else None
            bound_vcpkg_config = (generic_vcpkg_config if vcpkg_binding_value == vcpkg_output else None)
            replacements = {
                "${{ env.CMAKE_CONFIGURE }}": controlled_configure,
                vcpkg_output: generic_vcpkg_config,
                "${{ env.SYNERGY_VERSION_RELEASE }}": "false",
                "${{ env.SYNERGY_VERSION_SNAPSHOT }}": "false",
                "${{ steps.get-deps.outputs.qt-notice-root }}": str(temp / "notices"),
                "${{ steps.get-deps.outputs.qt-notice-sha256 }}": "fixture",
            }

            def fixture_script(source):
                script = source
                for expression, value in replacements.items():
                    script = script.replace(expression, value)
                if "${{" in script:
                    raise RuntimeError("EXTRACTION: unresolved Windows fixture expression")
                return script

            bash = shutil.which("bash")
            if not bash:
                raise RuntimeError("ENVIRONMENT: bash required for Windows shell-fixture check")

            def run_actual(source, cmake_path, cpack_path, ninja_mode="verified"):
                env = dict(os.environ, PATH=str(old) + os.pathsep + os.environ.get("PATH", ""),
                           CMAKE_CONFIGURE=controlled_configure, SYNERGY_CI_CPACK=str(cpack_path),
                           SYNERGY_TOOL_LOG=str(log), GITHUB_ACTIONS="true", RUNNER_TEMP=str(temp),
                           SYNERGY_FIXTURE_NINJA_MODE=ninja_mode,
                           SYNERGY_TOOL_ARGV_JSON_LOG=str(argv_json_log))
                if bound_vcpkg_config is not None:
                    env["SYNERGY_CI_VCPKG_CONFIG"] = bound_vcpkg_config
                else:
                    env.pop("SYNERGY_CI_VCPKG_CONFIG", None)
                if cmake_path is not None:
                    env["SYNERGY_CI_CMAKE"] = str(cmake_path)
                else:
                    env.pop("SYNERGY_CI_CMAKE", None)
                return subprocess.run([bash, "-o", "pipefail", "-c", fixture_script(source)], cwd=temp,
                                      env=env, capture_output=True, text=True)

            baseline_configure = run_actual(configure_source, verified_cmake, verified_cpack)
            baseline_package = run_actual(package_source, verified_cmake, verified_cpack)
            baseline_configure_argv = [
                record["argv"]
                for record in (json.loads(line) for line in argv_json_log.read_text().splitlines())
                if record["label"] == "verified" and record["tool"] == "cmake"
                and "-Bbuild" in record["argv"]
            ]
            missing_tool = run_actual(configure_source, None, verified_cpack)
            wrong_path = run_actual(configure_source, temp / "missing-cmake.exe", verified_cpack)
            stale_package_command = run_actual(configure_source, verified_cmake, verified_cpack, "stale-package")
            # A wrong version must fail even at the valid expected wheel path;
            # an outside-path rejection alone would not prove the version gate.
            write_tool(verified, "cmake", "wrong-version", "3.31.6")
            wrong_cmake_version = run_actual(configure_source, verified_cmake, verified_cpack)
            write_tool(verified, "cmake", "verified", "4.3.4")
            write_tool(verified, "cpack", "wrong-version", "3.31.6")
            wrong_cpack_version = run_actual(package_source, verified_cmake, verified_cpack)
            write_tool(verified, "cpack", "verified", "4.3.4")
            calls = [line.split("\t", 2) for line in log.read_text().splitlines()]
            problems = []
            expected_cmake = temp / "cmake-4.3.4" / "cmake" / "data" / "bin" / "cmake.exe"
            expected_cpack = temp / "cmake-4.3.4" / "cmake" / "data" / "bin" / "cpack.exe"
            if verified_cmake != expected_cmake or verified_cpack != expected_cpack:
                problems.append("fixture does not model the extracted absolute .exe pair")
            for text, name in ((windows, "Windows job"), (configure_source, "Configure"),
                               (package_source, "Package")):
                for variable in ("SYNERGY_CI_CMAKE", "SYNERGY_CI_CPACK"):
                    if variable not in text:
                        problems.append(name + " does not select " + variable)
            if "-DCMAKE_CPACK_COMMAND" in workflow:
                problems.append("workflow overrides CMAKE_CPACK_COMMAND")
            if not helper.is_file() or "SYNERGY_CI_CMAKE" not in helper.read_text(errors="replace"):
                problems.append("package verifier does not select verified CMake")
            if vcpkg_binding is None:
                problems.append("Configure does not bind vcpkg flags as data")
            elif vcpkg_binding_value != vcpkg_output:
                problems.append("Configure does not bind the vcpkg action output")
            if baseline_configure.returncode or baseline_package.returncode:
                problems.append("verified baseline did not execute actual workflow source")
            if any(label == "old" for label, _, _ in calls):
                problems.append("actual workflow source selected old PATH tool")
            configure_calls = [argv for label, tool, argv in calls if label == "verified" and tool == "cmake"
                               and "-Bbuild" in argv.split()]
            if not configure_calls or any(flag not in configure_calls[0] for flag in (
                    "-DSKIP_BUILD_TESTS=ON", "-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON",
                    "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON")):
                problems.append("verified Configure did not preserve controlled flags")
            if len(baseline_configure_argv) != 1:
                problems.append("fixture did not capture one baseline Configure argv")
            else:
                actual = baseline_configure_argv[0]
                expected_start = actual.index(expected_vcpkg_argv[0]) if expected_vcpkg_argv[0] in actual else -1
                if actual[expected_start:expected_start + len(expected_vcpkg_argv)] != expected_vcpkg_argv:
                    problems.append("Configure did not preserve generic Windows vcpkg argv")
            if not any(label == "verified" and tool == "cpack" and argv == "--version"
                       for label, tool, argv in calls):
                problems.append("Package did not invoke verified CPack version check")
            for name, result in (("missing CMake", missing_tool), ("wrong CMake path", wrong_path),
                                 ("wrong CMake version", wrong_cmake_version),
                                 ("wrong CPack version", wrong_cpack_version),
                                 ("stale package command with unrelated verified path", stale_package_command)):
                if result.returncode == 0:
                    problems.append(name + " did not fail closed through actual workflow source")
            self.assertEqual(problems, [])


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
        packages = {f"dist/synergy_2.1.1_{suffix}" for suffix in RELEASE_ASSET_SUFFIXES}
        self.assertEqual({arg for arg in create if arg.startswith("dist/")}, packages)
        self.assertIn("--draft=false", publish)
        self.assertNotIn("--generate-notes", create)
        notes = [args[2] for args in calls if args[:2] == ["release", "create-notes"]]
        self.assertEqual(len(notes), 1)
        self.assertTrue(notes[0].startswith("**本次更新**"))
        self.assertIn("**English**", notes[0])
        self.assertIn("- 修复窗口显示。", notes[0])
        self.assertIn("- Fixed window layout.", notes[0])
        self.assertNotIn("Generated release notes", notes[0])
        self.assertIn("Built-From: " + SOURCE_SHA, notes[0])
        self.assertTrue(any("repos/fixture/fixture/commits/main" in args for args in calls))
        self.assertNotIn("--clobber", [arg for args in calls for arg in args])

    def test_publish_rejects_package_changed_after_attestation(self):
        result, calls = release_case(changed_package=True)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(mutations(calls), [])

    def test_rejects_incomplete_or_hidden_release_notes_before_mutation(self):
        preparation = next(block for block in steps(job("get-version"))
                           if block.startswith("name: Require release notes for formal releases\n"))
        validate = actual_step_run(preparation)
        for var in ("RELEASE_NOTES_ZH", "RELEASE_NOTES_EN"):
            for notes in (None, "", " \n\t", "-", "- Text\n<details>",
                          "<!-- hidden -->", "- Text\n<!-- hides the footer",
                          "- Text\n```", "- Text\n~~~"):
                with self.subTest(var=var, notes=notes):
                    result, calls = release_case(environment={var: notes})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("provide visible Chinese and English release notes", result.stdout)
                    self.assertEqual(mutations(calls), [])

                    env = {"RELEASE_NOTES_ZH": "- 修复窗口显示。", "RELEASE_NOTES_EN": "- Fixed window layout."}
                    if notes is None:
                        del env[var]
                    else:
                        env[var] = notes
                    checked = subprocess.run(["bash", "-euo", "pipefail", "-c", validate],
                                             env={"PATH": os.environ["PATH"], **env},
                                             capture_output=True, text=True)
                    self.assertNotEqual(checked.returncode, 0)
                    self.assertIn("provide visible Chinese and English release notes", checked.stdout)

    def test_publish_uses_only_the_verified_macos_signing_mode(self):
        expected_notes = {
            "development": "Apple Development",
            "developer-id": "Developer ID",
            "ad-hoc": "ad-hoc signing",
        }
        for mode, expected_note in expected_notes.items():
            with self.subTest(mode=mode):
                result, calls = release_case(environment={"MACOS_PACKAGE_SIGNING": mode})
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                notes = [args[2] for args in calls if args[:2] == ["release", "create-notes"]]
                self.assertEqual(len(notes), 1)
                self.assertIn(expected_note, notes[0])

    def test_publish_rejects_missing_or_unknown_macos_signing_mode_before_mutation(self):
        for name, mode in (("missing", None), ("empty", ""), ("unknown", "unverified")):
            with self.subTest(mode=name):
                result, calls = release_case(environment={"MACOS_PACKAGE_SIGNING": mode})
                self.assertNotEqual(result.returncode, 0,
                                    name + ": accepted an unverified macOS signing mode")
                self.assertEqual(mutations(calls), [],
                                 name + ": mutated the release before mode validation")

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
                           if ("cmake --build" in block
                               or '"$SYNERGY_CI_CMAKE" --build' in block)
                           and "--target" in block]
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

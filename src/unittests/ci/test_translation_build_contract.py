#!/usr/bin/env python3
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
"""Exercise the real translation CMake module with real Qt tools, no app build.

Only copied inputs under TemporaryDirectory are built or updated. The caller
captures the printed command logs and identities as this run's raw evidence.
Environment/extraction errors are distinct from unittest assertion failures.
"""
from contextlib import contextmanager
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
LANGUAGES = ("en", "es", "it", "ja", "zh_CN", "ru", "ko")
SINGULAR = "GH delivery fixture singular marker"
PLURAL = "GH delivery fixture %n item(s)"
SOURCE = '''#include <QCoreApplication>
void fixture_only_not_compiled() {
  QCoreApplication::translate("DeliveryFixture", "GH delivery fixture singular marker");
  QCoreApplication::translate("DeliveryFixture", "GH delivery fixture %n item(s)", nullptr, 2);
}
'''


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def catalogues(directory):
    return {"synergy_" + lang + ".ts": digest(directory / ("synergy_" + lang + ".ts"))
            for lang in LANGUAGES}


def qt_prefix():
    # Reuse already installed dependencies; do not configure vcpkg or download.
    cache = ROOT / "build/CMakeCache.txt"
    if cache.is_file():
        match = re.search(r"^Qt6_DIR:[^=]*=(.+)$", cache.read_text(), re.M)
        if match:
            qt = Path(match.group(1))
            if (qt / "Qt6Config.cmake").is_file():
                return qt.parent.parent
    candidates = sorted((ROOT / "build/vcpkg_installed").glob("*/share/Qt6/Qt6Config.cmake"))
    if len(candidates) == 1:
        return candidates[0].parents[2]
    raise RuntimeError("ENVIRONMENT: one existing Qt6 installation must be discoverable")


class TranslationBuildContract(unittest.TestCase):
    def setUp(self):
        if not shutil.which("cmake") or not shutil.which("ninja"):
            raise RuntimeError("ENVIRONMENT: existing CMake and Ninja are required")
        self.prefix = qt_prefix()
        self.trace = []

    def tearDown(self):
        print(json.dumps({"translation_fixture_commands": self.trace}, sort_keys=True))

    def run_command(self, argv, cwd):
        result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True, timeout=120)
        self.trace.append({"argv": list(map(str, argv)), "cwd": str(cwd),
                           "exit": result.returncode, "stdout": result.stdout,
                           "stderr": result.stderr})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout + result.stderr

    @contextmanager
    def fixture(self):
        original = catalogues(ROOT / "translations")
        module = ROOT / "translations/CMakeLists.txt"
        module_hash = digest(module)
        floor = re.search(r'set\(REQUIRED_QT_VERSION\s+"?([0-9.]+)',
                          (ROOT / "CMakeLists.txt").read_text())
        if not floor:
            raise RuntimeError("EXTRACTION: project Qt version floor not found")
        with tempfile.TemporaryDirectory(prefix="gh-translation-contract-") as temporary:
            temp = Path(temporary).resolve()
            source = temp / "source"
            build = temp / "build"
            (source / "translations").mkdir(parents=True)
            (source / "src/apps/res").mkdir(parents=True)
            for name in original:
                shutil.copyfile(ROOT / "translations" / name, source / "translations" / name)
            shutil.copyfile(module, source / "translations/CMakeLists.txt")
            shutil.copyfile(ROOT / "src/apps/res/synergy.plist.in", source / "src/apps/res/synergy.plist.in")
            (source / "src/fixture.cpp").write_text(SOURCE)
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.24)\n'
                'project(TranslationContract LANGUAGES CXX)\n'
                'set(REQUIRED_QT_VERSION ' + floor.group(1) + ')\n'
                'set(UPSTREAM_PROJECT_NAME synergy)\n'
                'set(BUILD_OSX_BUNDLE OFF)\n'
                'set(CMAKE_INSTALL_I18N_DIR translations)\n'
                'find_package(Qt6 REQUIRED COMPONENTS Core LinguistTools)\n'
                'add_subdirectory(translations)\n'
                'get_target_property(_fixture_lconvert Qt6::lconvert IMPORTED_LOCATION)\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/lconvert-path.txt" "${_fixture_lconvert}")\n')
            self.trace.append({"module_sha256": module_hash, "repository_ts_sha256": original,
                               "fixture_source_sha256": digest(source / "src/fixture.cpp"),
                               "no_application_target": True})
            try:
                self.run_command(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                                  "-DCMAKE_PREFIX_PATH=" + str(self.prefix),
                                  "-DVCPKG_INSTALLED_DIR=" + str(self.prefix.parent),
                                  "-DVCPKG_TARGET_TRIPLET=" + self.prefix.name], temp)
                yield source, build
            finally:
                self.trace.append({"fixture_ts_sha256_after": catalogues(source / "translations"),
                                   "repository_ts_sha256_after": catalogues(ROOT / "translations"),
                                   "module_sha256_after": digest(module),
                                   "fixture_cleanup": "TemporaryDirectory context removes owned root"})
                self.assertEqual(catalogues(ROOT / "translations"), original,
                                 "repository catalogue changed outside disposable fixture")
                self.assertEqual(digest(module), module_hash)
        self.assertFalse(temp.exists(), "owned temporary fixture was not removed")

    def test_normal_all_and_translation_target_generate_qm_without_updating_ts(self):
        with self.fixture() as (source, build):
            expected = catalogues(source / "translations")
            for target in (None, "app_translations"):
                with self.subTest(target=target or "default ALL"):
                    argv = ["cmake", "--build", str(build), "--verbose"]
                    if target:
                        argv += ["--target", target]
                    log = self.run_command(argv, source)
                    self.assertEqual(catalogues(source / "translations"), expected,
                                     "normal translation build rewrote reviewed TS inputs")
                    self.assertNotRegex(log, r"(?:[/\\]|\b)lupdate(?:[. ]|$)")
                    tool = (build / "lconvert-path.txt").read_text()
                    if not Path(tool).is_file():
                        raise RuntimeError("ENVIRONMENT: actual Qt6::lconvert is unavailable: " + tool)
                    for lang in LANGUAGES:
                        qm = build / "translations" / ("synergy_" + lang + ".qm")
                        self.assertTrue(qm.is_file() and qm.stat().st_size > 0,
                                        "actual normal target did not create " + qm.name)
                        decoded = build / ("decoded_" + lang + ".ts")
                        self.run_command([tool, "-i", str(qm), "-o", str(decoded)], source)
                        self.assertEqual(ET.parse(decoded).getroot().tag, "TS")
                    self.trace.append({"normal_target": target or "ALL",
                                       "unchanged_ts_sha256": catalogues(source / "translations"),
                                       "qm_sha256": {p.name: digest(p) for p in sorted(
                                           (build / "translations").glob("synergy_*.qm"))}})

    def test_explicit_maintenance_updates_real_plural_and_other_catalogues(self):
        with self.fixture() as (source, build):
            self.run_command(["cmake", "--build", str(build), "--target", "update_translations",
                              "--verbose"], source)
            for lang in LANGUAGES:
                with self.subTest(language=lang):
                    path = source / "translations" / ("synergy_" + lang + ".ts")
                    messages = ET.parse(path).findall(".//message")
                    plurals = [message for message in messages
                               if message.findtext("source") == PLURAL]
                    self.assertTrue(plurals, "explicit update omitted real numerus source")
                    self.assertTrue(all(message.get("numerus") == "yes" for message in plurals))
                    singular = any(message.findtext("source") == SINGULAR for message in messages)
                    if lang == "en":
                        self.assertFalse(singular, "English maintenance must remain plural-only")
                    else:
                        self.assertTrue(singular, "explicit update omitted non-English source")


if __name__ == "__main__":
    unittest.main(verbosity=2)

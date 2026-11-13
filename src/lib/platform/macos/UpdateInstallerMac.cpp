/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>

namespace synergy::platform {

namespace {

/**
 * @brief The .app bundle that contains this executable.
 *
 * The executable lives at <bundle>/Contents/MacOS/<exe>; the bundle is three
 * levels up. QDir::cleanPath keeps the path in the form `open` and `cp` accept.
 */
QString bundlePath()
{
  return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../.."));
}

} // namespace

UpdateInstallResult launchUpdateInstaller(const QString &packagePath, QString *error)
{
  // The running bundle cannot be replaced in place. A detached shell waits for
  // this process (and its core helper) to exit, mounts the verified DMG,
  // replaces the bundle and relaunches it. The replacement needs write access
  // to /Applications, so fall back to one explicit administrator prompt when a
  // plain copy fails; the user still confirms a single time instead of dragging
  // the app out of the DMG themselves.
  QTemporaryFile script(QDir::tempPath() + QStringLiteral("/synergy-update-XXXXXX.sh"));
  script.setAutoRemove(false);
  if (!script.open()) {
    if (error != nullptr) {
      *error = QStringLiteral("could not create the update helper script");
    }
    return UpdateInstallResult::Failed;
  }

  const auto destination = bundlePath();
  const auto source = destination.section(QLatin1Char('/'), -1);
  const auto scriptText = QStringLiteral(
      "#!/bin/sh\n"
      "set -eu\n"
      "pid=\"$1\"; dmg=\"$2\"; dest=\"$3\"; name=\"$4\"\n"
      "# Give the GUI a moment to quit, then make sure nothing from the old\n"
      "# bundle keeps running: a surviving core helper makes the new instance\n"
      "# fail to bind its shared memory/socket.\n"
      "i=0\n"
      "while kill -0 \"$pid\" 2>/dev/null && [ \"$i\" -lt 50 ]; do sleep 0.2; i=$((i + 1)); done\n"
      "kill \"$pid\" 2>/dev/null || true\n"
      "pkill -f \"$dest/Contents/MacOS/\" 2>/dev/null || true\n"
      "sleep 1\n"
      "mount=$(mktemp -d)\n"
      "printf 'Y\\n' | hdiutil attach \"$dmg\" -nobrowse -readonly -mountpoint \"$mount\" >/dev/null\n"
      "copy() { rm -rf \"$dest\"; cp -R \"$mount/$name\" \"$dest\"; }\n"
      "if ! copy 2>/dev/null; then\n"
      "  /usr/bin/osascript -e \"do shell script \\\"rm -rf '$dest' && cp -R '$mount/$name' '$dest'\\\" with "
      "administrator privileges\"\n"
      "fi\n"
      "hdiutil detach \"$mount\" >/dev/null\n"
      "open \"$dest\"\n"
      "rm -f \"$0\"\n"
  );

  script.write(scriptText.toUtf8());
  script.flush();
  script.close();
  script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

  // startDetached() inherits no terminal; the helper outlives this process.
  if (!QProcess::startDetached(
          QStringLiteral("/bin/sh"),
          {script.fileName(), QString::number(QCoreApplication::applicationPid()), packagePath, destination, source}
      )) {
    if (error != nullptr) {
      *error = QStringLiteral("could not start the update helper");
    }
    return UpdateInstallResult::Failed;
  }

  return UpdateInstallResult::Launched;
}

} // namespace synergy::platform

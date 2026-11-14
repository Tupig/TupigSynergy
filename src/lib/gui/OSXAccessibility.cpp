/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "OSXHelpers.h"

#include "common/Constants.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUrl>

#include <ApplicationServices/ApplicationServices.h>

// Lives in a .cpp on purpose: lupdate only scans C++ sources, so keeping the
// user-visible strings here is what makes them translatable. Moving them into
// a .mm file silently drops the translations (lupdate marks them vanished).
bool checkOSXAccessibilityPermission(QWidget *parent)
{
  // Applications are trusted individually for the accessibility API, and the
  // grant is bound to the code signature: a local ad-hoc build is a different
  // identity from the installed copy and needs its own grant.
  if (AXIsProcessTrusted()) {
    return true;
  }

  // Startup and every "Start" click land here while the grant is missing, and
  // the core auto-restart can call this several times a second. Keep one
  // reminder: raise it instead of stacking another copy.
  static QPointer<QMessageBox> reminder;
  if (reminder) {
    reminder->raise();
    reminder->activateWindow();
    return false;
  }

  const auto appName = QString::fromUtf8(kAppName);

  // Register the app in the Accessibility list and let the system show its own
  // prompt first; the dialog below then points at the exact pane.
  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *trueValue[] = {kCFBooleanTrue};
  CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, trueValue, 1, nullptr, nullptr);
  AXIsProcessTrustedWithOptions(options);
  CFRelease(options);

  auto *box = new QMessageBox(
      QMessageBox::Warning, appName,
      QObject::tr("macOS requires Accessibility permission for %1 to control the keyboard and mouse.").arg(appName),
      QMessageBox::NoButton, parent
  );
  reminder = box;
  box->setAttribute(Qt::WA_DeleteOnClose);
  box->setInformativeText(
      QObject::tr(
          "Open System Settings → Privacy & Security → Accessibility, enable %1, then restart %1. "
          "You can configure %1 now; input sharing starts once the permission is granted."
      )
          .arg(appName)
  );
  auto *openSettings = box->addButton(QObject::tr("Open System Settings"), QMessageBox::ActionRole);
  box->addButton(QObject::tr("Later"), QMessageBox::RejectRole);
  QObject::connect(openSettings, &QPushButton::clicked, []() {
    QDesktopServices::openUrl(
        QUrl(QStringLiteral("x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"))
    );
  });
  // The reminder is modeless so configuration can continue, which means it
  // must raise itself: otherwise a click on "Start" looks like a no-op.
  box->show();
  box->raise();
  box->activateWindow();

  // TCC publishes the grant to the running process, so close the reminder as
  // soon as it takes effect instead of making the user dismiss a stale dialog.
  auto *timer = new QTimer(box);
  QObject::connect(timer, &QTimer::timeout, box, [box]() {
    if (AXIsProcessTrusted()) {
      box->accept();
    }
  });
  timer->start(1000);
  return false;
}

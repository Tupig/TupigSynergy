/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "common/Constants.h"
#include "common/RuntimeContext.h"
#include "common/Settings.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>
#include <QtTest>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace {
#if defined(Q_OS_WIN)
bool createDirectorySymbolicLink(const QString &target, const QString &link)
{
  const auto nativeTarget = QDir::toNativeSeparators(target);
  const auto nativeLink = QDir::toNativeSeparators(link);
  const auto targetName = reinterpret_cast<LPCWSTR>(nativeTarget.utf16());
  const auto linkName = reinterpret_cast<LPCWSTR>(nativeLink.utf16());
  bool created = CreateSymbolicLinkW(
                     linkName, targetName, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
                 ) != FALSE;
  DWORD firstError = ERROR_SUCCESS;
  DWORD nativeError = ERROR_SUCCESS;
  bool retried = false;
  if (!created) {
    firstError = GetLastError();
    nativeError = firstError;
    if (firstError == ERROR_INVALID_PARAMETER) {
      retried = true;
      created = CreateSymbolicLinkW(linkName, targetName, SYMBOLIC_LINK_FLAG_DIRECTORY) != FALSE;
      nativeError = created ? ERROR_SUCCESS : GetLastError();
    }
  }
  QTextStream(stderr) << "WL1 phase=native-creation created=" << created << " retried=" << retried
                      << " firstError=" << static_cast<quint32>(firstError)
                      << " nativeError=" << static_cast<quint32>(nativeError) << '\n';
  return created;
}
#endif

struct LinkTypeProof
{
  bool exists = false;
  bool directory = false;
  bool symbolic = false;
  bool shortcut = false;
  bool junction = false;
  bool targetMatches = false;
  bool nativeRequired = false;
  bool reparse = false;
  bool nativeTagRead = false;
  bool nativeSymlinkTag = false;
  bool nativeHandleClosed = false;
  quint32 nativeError = 0;
  bool accepted = false;
};

bool exactEntryAbsent(const QString &path, quint32 &error)
{
  error = 0;
#if defined(Q_OS_WIN)
  const auto native = QDir::toNativeSeparators(path);
  if (GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16())) != INVALID_FILE_ATTRIBUTES)
    return false;
  error = GetLastError();
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
#else
  const QFileInfo info(path);
  return !info.exists() && !info.isSymbolicLink() && !info.isShortcut() && !info.isJunction();
#endif
}

class OwnedLinkProof
{
public:
  OwnedLinkProof(QTemporaryDir &temp, QTemporaryDir &outside) : m_temp(temp), m_outside(outside)
  {
  }
  OwnedLinkProof(const OwnedLinkProof &) = delete;
  OwnedLinkProof &operator=(const OwnedLinkProof &) = delete;
  ~OwnedLinkProof()
  {
    if (m_active && !m_finished)
      cleanup();
  }

  bool prepare(const QString &link, const QString &target, bool rootCase)
  {
    m_rootCase = rootCase;
    m_link = link;
    m_target = QFileInfo(target).canonicalFilePath();
    m_tempRoot = QFileInfo(m_temp.path()).canonicalFilePath();
    m_outsideRoot = QFileInfo(m_outside.path()).canonicalFilePath();
    const QFileInfo tempInfo(m_temp.path()), outsideInfo(m_outside.path());
    const auto parent = QFileInfo(QFileInfo(link).absolutePath()).canonicalFilePath();
    const bool owned =
        !m_tempRoot.isEmpty() && !m_outsideRoot.isEmpty() && m_tempRoot != m_outsideRoot && tempInfo.isDir() &&
        !tempInfo.isSymbolicLink() && !tempInfo.isShortcut() && !tempInfo.isJunction() && outsideInfo.isDir() &&
        !outsideInfo.isSymbolicLink() && !outsideInfo.isShortcut() && !outsideInfo.isJunction() &&
        parent == (rootCase ? m_outsideRoot : m_tempRoot) && m_target == (rootCase ? m_tempRoot : m_outsideRoot) &&
        QFileInfo(link).fileName() == (rootCase ? "root-link" : "user");
    quint32 absentError = 0;
    if (!owned || !exactEntryAbsent(link, absentError) || !entries(m_tempRoot).isEmpty() ||
        !entries(m_outsideRoot).isEmpty()) {
      retain();
      QTextStream(stderr) << "WL1 phase=prepare owned=" << owned << " retained=1 code=92 nativeError=" << absentError
                          << '\n';
      return false;
    }
    m_sentinel = m_target + "/wl-target-sentinel";
    QFile sentinel(m_sentinel);
    if (!sentinel.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      retain();
      QTextStream(stderr) << "WL1 phase=sentinel ready=0 retained=1 code=92 fileError="
                          << static_cast<int>(sentinel.error()) << '\n';
      return false;
    }
    const auto bytes = sentinelBytes();
    const bool written = sentinel.write(bytes) == bytes.size() && sentinel.flush();
    sentinel.close();
    if (!written || sentinel.error() != QFileDevice::NoError) {
      retain();
      QTextStream(stderr) << "WL1 phase=sentinel ready=0 retained=1 code=92 fileError="
                          << static_cast<int>(sentinel.error()) << '\n';
      return false;
    }
    const auto negative = inspect(m_sentinel);
    reportType("negative", negative, false);
    if (negative.accepted) {
      retain();
      return false;
    }
    m_tempBefore = entries(m_tempRoot);
    m_outsideBefore = entries(m_outsideRoot);
    m_targetBefore = entries(m_target);
    m_active = true;
    return true;
  }

  void creationSucceeded()
  {
    m_creationReported = true;
    QTextStream(stderr) << "WL1 phase=creation rootCase=" << m_rootCase << " created=1 code=0\n";
  }

  bool verifyLink() const
  {
    if (!m_active)
      return true;
    const auto proof = inspect(m_link);
    reportType("type", proof, true);
    return proof.accepted;
  }

  bool afterConfigure(bool configured) const
  {
    if (!m_active)
      return true;
    const bool intact = rootsIntact() && targetIntact();
    QTextStream(stderr) << "WL1 phase=configuration rootCase=" << m_rootCase << " configured=" << configured
                        << " targetIntact=" << intact << " code=" << (intact ? 0 : 93) << '\n';
    return intact;
  }

  bool cleanup()
  {
    if (!m_active)
      return true;
    if (m_finished)
      return m_cleanupSucceeded;
    m_finished = true;
    if (!m_creationReported)
      QTextStream(stderr) << "WL1 phase=creation rootCase=" << m_rootCase << " created=0 code=91\n";
    const bool roots = rootsIntact();
    const bool beforeIntact = roots && targetIntact();
    quint32 nativeError = 0;
    const bool alreadyAbsent = roots && exactEntryAbsent(m_link, nativeError);
    bool removed = false;
    if (!alreadyAbsent && beforeIntact) {
      const auto proof = inspect(m_link);
      nativeError = proof.nativeError;
      if (proof.accepted) {
#if defined(Q_OS_WIN)
        const auto native = QDir::toNativeSeparators(m_link);
        removed = RemoveDirectoryW(reinterpret_cast<LPCWSTR>(native.utf16())) != FALSE;
        if (!removed)
          nativeError = GetLastError();
#else
        removed = QFile::remove(m_link);
#endif
      }
    }
    quint32 absenceError = 0;
    const bool absent = roots && exactEntryAbsent(m_link, absenceError);
    const bool afterIntact = roots && targetIntact();
    const bool restored = roots && entries(m_tempRoot) == m_tempBefore && entries(m_outsideRoot) == m_outsideBefore;
    m_cleanupSucceeded = (alreadyAbsent || removed) && absent && beforeIntact && afterIntact && restored;
    if (!m_cleanupSucceeded)
      retain();
    QTextStream(stderr) << "WL1 phase=cleanup rootCase=" << m_rootCase << " rootsIntact=" << roots
                        << " removed=" << removed << " absent=" << absent << " targetBefore=" << beforeIntact
                        << " targetAfter=" << afterIntact << " restored=" << restored
                        << " retained=" << !m_cleanupSucceeded << " code=" << (m_cleanupSucceeded ? 0 : 94)
                        << " nativeError=" << nativeError << " absenceError=" << absenceError << '\n';
    return m_cleanupSucceeded;
  }

private:
  void retain()
  {
    m_temp.setAutoRemove(false);
    m_outside.setAutoRemove(false);
  }

  static bool plainDirectory(const QString &path)
  {
    const QFileInfo info(path);
    return info.isDir() && !info.isSymbolicLink() && !info.isShortcut() && !info.isJunction() &&
           info.canonicalFilePath() == path;
  }

  bool rootsIntact() const
  {
    return plainDirectory(m_tempRoot) && plainDirectory(m_outsideRoot);
  }

  static QStringList entries(const QString &directory)
  {
    return QDir(directory).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
  }
  static QByteArray sentinelBytes()
  {
    return QByteArrayLiteral("WL owned target sentinel\n");
  }

  bool targetIntact() const
  {
    const QFileInfo target(m_target);
    const QFileInfo sentinel(m_sentinel);
    if (!target.isDir() || target.isSymbolicLink() || target.isShortcut() || target.isJunction() ||
        target.canonicalFilePath() != m_target || entries(m_target) != m_targetBefore || !sentinel.isFile() ||
        sentinel.isSymbolicLink() || sentinel.isShortcut() || sentinel.isJunction() ||
        sentinel.canonicalFilePath() != m_sentinel)
      return false;
    QFile file(m_sentinel);
    return file.open(QIODevice::ReadOnly) && file.readAll() == sentinelBytes() && file.error() == QFileDevice::NoError;
  }

  LinkTypeProof inspect(const QString &path) const
  {
    LinkTypeProof result;
    const QFileInfo info(path);
    result.exists = info.exists();
    result.directory = info.isDir();
    result.symbolic = info.isSymbolicLink();
    result.shortcut = info.isShortcut();
    result.junction = info.isJunction();
    result.targetMatches = info.canonicalFilePath() == m_target;
    const bool qtType = result.exists && result.directory && result.symbolic && !result.shortcut && !result.junction &&
                        result.targetMatches;
#if defined(Q_OS_WIN)
    result.nativeRequired = true;
    const auto native = QDir::toNativeSeparators(path);
    const auto wide = reinterpret_cast<LPCWSTR>(native.utf16());
    const DWORD attributes = GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      result.nativeError = GetLastError();
      return result;
    }
    result.reparse = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    if (!result.reparse)
      return result;
    HANDLE handle = CreateFileW(
        wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr
    );
    if (handle == INVALID_HANDLE_VALUE) {
      result.nativeError = GetLastError();
      return result;
    }
    FILE_ATTRIBUTE_TAG_INFO tag{};
    result.nativeTagRead =
        GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &tag, static_cast<DWORD>(sizeof(tag))) != FALSE;
    if (!result.nativeTagRead)
      result.nativeError = GetLastError();
    result.nativeSymlinkTag = result.nativeTagRead && tag.ReparseTag == IO_REPARSE_TAG_SYMLINK &&
                              (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    result.nativeHandleClosed = CloseHandle(handle) != FALSE;
    if (!result.nativeHandleClosed && !result.nativeError)
      result.nativeError = GetLastError();
    result.accepted =
        qtType && result.reparse && result.nativeTagRead && result.nativeSymlinkTag && result.nativeHandleClosed;
#else
    result.accepted = qtType;
#endif
    return result;
  }

  void reportType(const char *phase, const LinkTypeProof &proof, bool expectedLink) const
  {
    QTextStream(stderr) << "WL1 phase=" << phase << " rootCase=" << m_rootCase << " exists=" << proof.exists
                        << " directory=" << proof.directory << " symbolic=" << proof.symbolic
                        << " shortcut=" << proof.shortcut << " junction=" << proof.junction
                        << " targetMatches=" << proof.targetMatches << " nativeRequired=" << proof.nativeRequired
                        << " reparse=" << proof.reparse << " nativeTagRead=" << proof.nativeTagRead
                        << " nativeSymlinkTag=" << proof.nativeSymlinkTag
                        << " nativeHandleClosed=" << proof.nativeHandleClosed << " nativeError=" << proof.nativeError
                        << " accepted=" << proof.accepted << " expected=" << expectedLink
                        << " code=" << (proof.accepted == expectedLink ? 0 : 92) << '\n';
  }

  QTemporaryDir &m_temp;
  QTemporaryDir &m_outside;
  QString m_link;
  QString m_target;
  QString m_tempRoot;
  QString m_outsideRoot;
  QString m_sentinel;
  QStringList m_tempBefore;
  QStringList m_outsideBefore;
  QStringList m_targetBefore;
  bool m_rootCase = false;
  bool m_active = false;
  bool m_finished = false;
  bool m_creationReported = false;
  bool m_cleanupSucceeded = false;
};

int contextChild(const QString &mode)
{
  using namespace synergy::runtime;
  QTemporaryDir temp;
  QTemporaryDir outside;
  if (!temp.isValid() || !outside.isValid())
    return 90;
  const QString root = QFileInfo(temp.path()).canonicalFilePath();
  OwnedLinkProof linkProof(temp, outside);
  IsolatedRuntimeContext context{root, "tsr3-" + QUuid::createUuid().toString(QUuid::WithoutBraces)};
  if (mode == "empty")
    context.root.clear();
  if (mode == "relative")
    context.root = "relative-fixture";
  if (mode == "missing")
    context.root += "/does-not-exist";
  if (mode == "temp-root")
    context.root = QFileInfo(QDir::tempPath()).canonicalFilePath();
  if (mode == "key-slash")
    context.instanceKey = "invalid/key";
  if (mode == "key-control")
    context.instanceKey = "invalid\nkey";
  if (mode == "key-empty")
    context.instanceKey.clear();
  if (mode == "key-production")
    context.instanceKey = QStringLiteral("%1-gui").arg(kAppId);
  if (mode == "key-long")
    context.instanceKey = QString(100, QLatin1Char('a'));
  if (mode == "root-symlink") {
    const auto link = outside.filePath("root-link");
    if (!linkProof.prepare(link, root, true))
      return 92;
#if defined(Q_OS_WIN)
    if (!createDirectorySymbolicLink(root, link))
      return 91;
#else
    if (!QFile::link(root, link))
      return 91;
#endif
    linkProof.creationSucceeded();
    context.root = link;
  }
  if (mode == "descendant-symlink") {
    if (!linkProof.prepare(temp.filePath("user"), outside.path(), false))
      return 92;
#if defined(Q_OS_WIN)
    if (!createDirectorySymbolicLink(outside.path(), temp.filePath("user")))
      return 91;
#else
    if (!QFile::link(outside.path(), temp.filePath("user")))
      return 91;
#endif
    linkProof.creationSucceeded();
  }
  if (!linkProof.verifyLink())
    return 92;
  if (mode == "late")
    (void)Settings::userDir();
  const auto before = QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
  QString error;
  const bool configured = configureIsolatedRuntime(context, &error);
  const bool targetIntact = linkProof.afterConfigure(configured);
  const bool expected = mode == "valid" || mode == "repeat";
  if (configured != expected)
    return 10;
  if (QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot) != before)
    return 11;
  if (!targetIntact)
    return 93;
  if (!linkProof.cleanup())
    return 94;
  if (!expected)
    return !error.isEmpty() && !isIsolatedRuntime() ? 0 : 12;
  if (mode == "repeat") {
    error.clear();
    return !configureIsolatedRuntime(context, &error) && !error.isEmpty() ? 0 : 13;
  }
  const QString user = root + "/user";
  const QString system = root + "/system";
  if (Settings::userDir() != user || Settings::systemDir() != system)
    return 20;
  if (Settings::userSettingsFile() != user + "/" + kAppName + ".conf")
    return 21;
  if (Settings::systemSettingsFile() != system + "/" + kAppName + ".conf")
    return 22;
  if (runtimeStateFile() != root + "/state/app.state")
    return 23;
  if (runtimeTestSettingsFile() != root + "/test-settings.ini")
    return 24;
  if (runtimeInstanceKey("production-fallback") != context.instanceKey)
    return 25;
  auto legacyUser = legacySettings(QSettings::UserScope);
  auto legacySystem = legacySettings(QSettings::SystemScope);
  if (!legacyUser || legacyUser->format() != QSettings::IniFormat ||
      legacyUser->fileName() != root + "/legacy/user.ini")
    return 26;
  if (!legacySystem || legacySystem->format() != QSettings::IniFormat ||
      legacySystem->fileName() != root + "/legacy/system.ini")
    return 27;
  // Exact empty fixture stores may not inherit production values through QSettings fallbacks.
  if (!legacyUser->allKeys().isEmpty() || !legacySystem->allKeys().isEmpty())
    return 28;
  const QString log = Settings::defaultValue(Settings::Log::File).toString();
  if (log != root + "/logs/app.log" || Settings::value(Settings::Log::ToFile).toBool())
    return 29;
  error.clear();
  if (configureIsolatedRuntime(context, &error) || error.isEmpty())
    return 30;
  QJsonObject receipt{
      {"user", user},
      {"system", system},
      {"state", runtimeStateFile()},
      {"legacyUser", legacyUser->fileName()},
      {"legacySystem", legacySystem->fileName()},
      {"testSettings", runtimeTestSettingsFile()},
      {"instanceKey", context.instanceKey},
      {"log", log}
  };
  QTextStream(stdout) << QJsonDocument(receipt).toJson(QJsonDocument::Compact) << '\n';
  return 0;
}
} // namespace
class R3RuntimeContextTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void contextIsFailClosed_data()
  {
    QTest::addColumn<QString>("mode");
    for (const char *mode :
         {"valid", "repeat", "late", "empty", "relative", "missing", "temp-root", "root-symlink", "descendant-symlink",
          "key-empty", "key-slash", "key-control", "key-production", "key-long"})
      QTest::newRow(mode) << QString::fromLatin1(mode);
  }
  void contextIsFailClosed()
  {
    QFETCH(QString, mode);
    QProcess process;
    process.start(QCoreApplication::applicationFilePath(), {"--context-child", mode});
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(15000));
    if (mode == "root-symlink" || mode == "descendant-symlink") {
      const auto channel = process.readChannel();
      process.setReadChannel(QProcess::StandardError);
      const auto metadata = process.peek(8192);
      process.setReadChannel(channel);
      int forwarded = 0;
      for (const auto &line : metadata.split('\n')) {
        if (line.startsWith("WL1 ") && forwarded < 8) {
          qInfo().noquote() << QString::fromUtf8(line);
          ++forwarded;
        }
      }
    }
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        process.exitCode() == 0, qPrintable(QStringLiteral("mode=%1 code=%2 stderr=%3")
                                                .arg(mode)
                                                .arg(process.exitCode())
                                                .arg(QString::fromUtf8(process.readAllStandardError())))
    );
    if (mode == "valid") {
      const auto object = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
      QVERIFY(object.value("instanceKey").toString().startsWith("tsr3-"));
      QCOMPARE(object.size(), 8);
    }
  }
};
int main(int argc, char **argv)
{
  if (argc == 3 && QByteArray(argv[1]) == "--context-child")
    return contextChild(QString::fromLocal8Bit(argv[2]));
  QCoreApplication app(argc, argv);
  R3RuntimeContextTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "R3RuntimeContextTests.moc"

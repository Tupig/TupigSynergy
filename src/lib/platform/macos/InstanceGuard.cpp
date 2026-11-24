/*
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#include "InstanceGuard.h"

#include "common/Constants.h"
#include "common/RuntimeContext.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <libproc.h>
#include <limits>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace synergy::platform::macos {
int detail::activationConnectError(int socket, int initialError)
{
  if (initialError != EINPROGRESS)
    return initialError;
  pollfd pending{socket, POLLOUT, 0};
  const QDeadlineTimer deadline(100);
  int ready = 0;
  do {
    ready = ::poll(&pending, 1, static_cast<int>(deadline.remainingTime()));
  } while (ready < 0 && errno == EINTR && !deadline.hasExpired());
  if (ready <= 0)
    return ready == 0 ? ETIMEDOUT : errno;
  int error = 0;
  socklen_t length = sizeof(error);
  return ::getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &length) == 0 ? error : errno;
}

namespace {
QString guardRoot()
{
  const auto isolated = runtime::runtimeRoot();
  if (!isolated.isEmpty()) {
    const QFileInfo info(isolated);
    return info.isDir() && !info.isSymLink() && info.canonicalFilePath() == isolated ? isolated : QString();
  }

  const auto base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (base.isEmpty() || !QDir().mkpath(base))
    return {};
  const auto parent = QFileInfo(base).canonicalFilePath();
  if (parent.isEmpty())
    return {};
  const auto root = QDir(parent).filePath(QString::fromLatin1(kRevFqdnName) + QStringLiteral(".instances"));
  const auto path = QFile::encodeName(root);
  if (::mkdir(path.constData(), 0700) != 0 && errno != EEXIST)
    return {};
  struct stat info{};
  if (::lstat(path.constData(), &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != ::geteuid() ||
      (info.st_mode & 0077) != 0)
    return {};
  return root;
}

bool confirmsLiveOwner(const QLockFile &lock)
{
  qint64 pid = 0;
  QString host;
  QString process;
  if (!lock.getLockInfo(&pid, &host, &process) || pid <= 0 || pid > std::numeric_limits<pid_t>::max() ||
      host.isEmpty() || host != QSysInfo::machineHostName() || process.isEmpty())
    return false;
  const auto owner = static_cast<pid_t>(pid);
  char name[PROC_PIDPATHINFO_MAXSIZE]{};
  char expected[PROC_PIDPATHINFO_MAXSIZE]{};
  return ::kill(owner, 0) == 0 && ::proc_name(owner, name, sizeof(name)) > 0 &&
         ::proc_name(::getpid(), expected, sizeof(expected)) > 0 && process == QFile::decodeName(name) &&
         process == QFile::decodeName(expected);
}

InstanceGuard acquireLock(const QString &fileName, bool verifyOwner)
{
  const auto root = guardRoot();
  if (root.isEmpty())
    return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "instance lock directory is unavailable")};
  const auto path = QDir(root).filePath(fileName);
  struct stat info{};
  const int found = ::lstat(QFile::encodeName(path).constData(), &info);
  if ((found == 0 && (!S_ISREG(info.st_mode) || info.st_uid != ::geteuid())) || (found != 0 && errno != ENOENT))
    return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "instance lock path is unavailable")};

  auto lock = std::make_unique<QLockFile>(path);
  lock->setStaleLockTime(0);
  if (lock->tryLock(0))
    return {InstanceStatus::Primary, {}, std::move(lock)};
  if (lock->error() == QLockFile::LockFailedError) {
    if (!verifyOwner || confirmsLiveOwner(*lock))
      return {InstanceStatus::Duplicate};
    return {
        InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "unable to verify the instance lock owner")
    };
  }
  if (lock->error() == QLockFile::PermissionError)
    return {
        InstanceStatus::Failure,
        QT_TRANSLATE_NOOP("EngineLogMessages", "permission denied while acquiring the instance lock")
    };
  return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "cannot acquire the instance lock")};
}

enum class SocketState
{
  Absent,
  Active,
  Stale,
  Unavailable
};

SocketState probeSocket(const QByteArray &path, const sockaddr_un &address, struct stat &identity)
{
  if (::lstat(path.constData(), &identity) != 0)
    return errno == ENOENT ? SocketState::Absent : SocketState::Unavailable;
  if (!S_ISSOCK(identity.st_mode) || identity.st_uid != ::geteuid())
    return SocketState::Unavailable;
  const int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket < 0)
    return SocketState::Unavailable;
  if (::fcntl(socket, F_SETFL, O_NONBLOCK) != 0 || ::fcntl(socket, F_SETFD, FD_CLOEXEC) != 0) {
    ::close(socket);
    return SocketState::Unavailable;
  }
  const int connected = ::connect(socket, reinterpret_cast<const sockaddr *>(&address), address.sun_len);
  const int error = detail::activationConnectError(socket, connected == 0 ? 0 : errno);
  ::close(socket);
  if (error == 0)
    return SocketState::Active;
  // Qt also maps EINVAL to ConnectionRefusedError. Only native refusal
  // demonstrates that this exact socket can be considered for retirement.
  return error == ECONNREFUSED ? SocketState::Stale : SocketState::Unavailable;
}

bool retireSocket(const QByteArray &path, const struct stat &observed)
{
  struct stat current{};
  if (::lstat(path.constData(), &current) != 0)
    return errno == ENOENT;
  return S_ISSOCK(current.st_mode) && current.st_uid == ::geteuid() && current.st_dev == observed.st_dev &&
         current.st_ino == observed.st_ino && ::unlink(path.constData()) == 0;
}
} // namespace

InstanceGuard acquireGuiInstance(const QString &socketName)
{
  auto guard = acquireLock(QStringLiteral("gui.lock"), false);
  if (guard.status == InstanceStatus::Failure)
    return guard;

  const auto temp = QFileInfo(QDir::tempPath()).canonicalFilePath();
  if (temp.isEmpty() || socketName.isEmpty() || QFileInfo(socketName).fileName() != socketName)
    return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "activation socket path is unavailable")};
  struct stat directory{};
  if (::lstat(QFile::encodeName(temp).constData(), &directory) != 0 || !S_ISDIR(directory.st_mode) ||
      directory.st_uid != ::geteuid() || (directory.st_mode & 0022) != 0)
    return {
        InstanceStatus::Failure,
        QT_TRANSLATE_NOOP("EngineLogMessages", "activation socket directory is not privately owned")
    };
  const auto path = QDir(temp).filePath(socketName);
  const auto encoded = QFile::encodeName(path);
  struct sockaddr_un address{};
  if (encoded.contains('\0') || encoded.size() >= static_cast<qsizetype>(sizeof(address.sun_path)))
    return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "activation socket path is too long")};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, encoded.constData(), static_cast<size_t>(encoded.size()));
  address.sun_len = static_cast<decltype(address.sun_len)>(SUN_LEN(&address));

  struct stat identity{};
  if (guard.status == InstanceStatus::Duplicate) {
    QElapsedTimer timer;
    timer.start();
    do {
      if (probeSocket(encoded, address, identity) == SocketState::Active)
        return {InstanceStatus::Duplicate};
      QThread::msleep(20);
    } while (timer.elapsed() < 450);
    return {InstanceStatus::Busy, QT_TRANSLATE_NOOP("EngineLogMessages", "owned activation socket did not respond")};
  }

  const auto candidate = probeSocket(encoded, address, identity);
  if (candidate == SocketState::Active)
    return {InstanceStatus::Duplicate};
  if (candidate == SocketState::Unavailable || (candidate == SocketState::Stale && !retireSocket(encoded, identity)))
    return {
        InstanceStatus::Failure,
        QT_TRANSLATE_NOOP("EngineLogMessages", "cannot use the existing activation socket path")
    };

  auto server = std::make_unique<QLocalServer>();
  if (!server->listen(path)) {
    if (server->serverError() == QAbstractSocket::SocketAccessError)
      return {
          InstanceStatus::Failure,
          QT_TRANSLATE_NOOP("EngineLogMessages", "permission denied while opening the activation socket")
      };
    if (server->serverError() == QAbstractSocket::AddressInUseError)
      return {
          InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "activation socket address is already in use")
      };
    return {InstanceStatus::Failure, QT_TRANSLATE_NOOP("EngineLogMessages", "cannot listen on the activation socket")};
  }
  guard.activationServer = std::move(server);
  return guard;
}

InstanceGuard acquireCoreInstance(bool singleInstanceOnly)
{
  if (!singleInstanceOnly)
    return {InstanceStatus::Bypassed};
  return acquireLock(QStringLiteral("core.lock"), true);
}
} // namespace synergy::platform::macos

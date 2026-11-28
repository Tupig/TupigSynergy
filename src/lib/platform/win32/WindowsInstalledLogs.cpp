/* SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception */
#include "WindowsInstalledLogs.h"

#include <QDir>
#include <QFileInfo>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <aclapi.h>
#include <sddl.h>

#include <array>
#include <cstddef>
#include <vector>

namespace synergy::platform {
namespace {
DWORD attrs(const QString &path)
{
  return GetFileAttributesW(QDir::toNativeSeparators(path).toStdWString().c_str());
}
bool plainDirectory(const QString &path)
{
  const auto value = attrs(path);
  return value != INVALID_FILE_ATTRIBUTES && (value & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
         (value & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}
bool administrativeSid(PSID sid)
{
  return sid && (IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
}

bool hasProtectedServiceAcl(PACL acl)
{
  if (!acl) {
    return false;
  }
  constexpr DWORD writeRights = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES |
                                FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL;
  for (DWORD i = 0; i < acl->AceCount; ++i) {
    void *entry = nullptr;
    if (!GetAce(acl, i, &entry)) {
      return false;
    }
    const auto *header = static_cast<const ACE_HEADER *>(entry);
    if (header->AceType == ACCESS_DENIED_ACE_TYPE) {
      continue;
    }
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
      return false;
    }
    auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(entry);
    if ((ace->Mask & writeRights) && !administrativeSid(&ace->SidStart)) {
      return false;
    }
  }
  return true;
}

bool trustedExistingDirectory(const QString &path, const QString &sid, bool service = false)
{
  const auto native = QDir::toNativeSeparators(path).toStdWString();
  HANDLE handle = CreateFileW(
      native.c_str(), READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr
  );
  if (handle == INVALID_HANDLE_VALUE)
    return false;
  BY_HANDLE_FILE_INFORMATION info{};
  PSID owner = nullptr;
  PACL acl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  const auto securityInformation =
      OWNER_SECURITY_INFORMATION | (service ? DACL_SECURITY_INFORMATION : SECURITY_INFORMATION{0});
  const bool ok =
      GetFileInformationByHandle(handle, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
      !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
      GetSecurityInfo(handle, SE_FILE_OBJECT, securityInformation, &owner, nullptr, &acl, nullptr, &descriptor) ==
          ERROR_SUCCESS;
  LPWSTR text = nullptr;
  const bool trusted = ok && owner &&
                       (service ? administrativeSid(owner) && hasProtectedServiceAcl(acl)
                                : administrativeSid(owner) ||
                                      (ConvertSidToStringSidW(owner, &text) && QString::fromWCharArray(text) == sid));
  if (text)
    LocalFree(text);
  if (descriptor)
    LocalFree(descriptor);
  CloseHandle(handle);
  return trusted;
}
QString executablePath()
{
  std::array<wchar_t, 32768> buffer{};
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  return length == 0 || length >= buffer.size() ? QString() : QString::fromWCharArray(buffer.data(), length);
}
QString processSid()
{
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    return {};
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<std::byte> data(size);
  QString result;
  if (size && GetTokenInformation(token, TokenUser, data.data(), size, &size)) {
    LPWSTR text = nullptr;
    if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &text)) {
      result = QString::fromWCharArray(text);
      LocalFree(text);
    }
  }
  CloseHandle(token);
  return result;
}
} // namespace

bool isWindowsMsiLayout()
{
  const auto path = executablePath();
  const QFileInfo info(path);
  if (info.fileName().isEmpty() || info.dir().dirName().compare(QStringLiteral("bin"), Qt::CaseInsensitive) != 0 ||
      !plainDirectory(info.dir().path()) || !plainDirectory(info.dir().filePath(QStringLiteral(".."))))
    return false;
  const auto marker = info.dir().filePath(QStringLiteral("synergy.install"));
  const auto markerAttrs = attrs(marker);
  return markerAttrs != INVALID_FILE_ATTRIBUTES && (markerAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
         (markerAttrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

QString windowsInstalledLogDirectory(bool forService)
{
  if (!isWindowsMsiLayout())
    return {};
  const auto bin = QFileInfo(executablePath()).absoluteDir();
  const auto root = QDir::cleanPath(bin.filePath(QStringLiteral("..")));
  const auto logs = QDir(root).filePath(QStringLiteral("logs"));
  const auto branch = QDir(logs).filePath(forService ? QStringLiteral("service") : QStringLiteral("accounts"));
  if (!plainDirectory(logs) || !plainDirectory(branch))
    return {};
  if (forService)
    return trustedExistingDirectory(logs, {}, true) && trustedExistingDirectory(branch, {}, true) ? branch : QString();
  const auto sid = processSid();
  if (sid.isEmpty())
    return {};
  const auto account = QDir(branch).filePath(sid);
  if (plainDirectory(account)) {
    return trustedExistingDirectory(account, sid) ? account : QString();
  }
  {
    const auto native = QDir::toNativeSeparators(account).toStdWString();
    const auto sddl = QStringLiteral("D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;%1)").arg(sid).toStdWString();
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
      return {};
    SECURITY_ATTRIBUTES security{sizeof(security), descriptor, FALSE};
    const BOOL created = CreateDirectoryW(native.c_str(), &security);
    const DWORD error = created ? ERROR_SUCCESS : GetLastError();
    LocalFree(descriptor);
    if (!created && error != ERROR_ALREADY_EXISTS)
      return {};
    if (!created && !trustedExistingDirectory(account, sid))
      return {};
  }
  return plainDirectory(account) ? account : QString();
}
} // namespace synergy::platform

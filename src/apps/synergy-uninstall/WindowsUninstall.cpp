/*
 * SPDX-License-Identifier: MIT
 */

#include <windows.h>

#include <msi.h>

#include <array>
#include <optional>
#include <string>

namespace {

const wchar_t *localized(const wchar_t *zh, const wchar_t *en)
{
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE ? zh : en;
}

void showError(const wchar_t *message)
{
  MessageBoxW(nullptr, message, L"TuPig Synergy", MB_OK | MB_ICONERROR);
}

std::wstring installationRoot()
{
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) {
    return {};
  }
  path.resize(length);

  const auto executable = path.find_last_of(L'\\');
  if (executable == std::wstring::npos) {
    return {};
  }
  path.resize(executable);
  const auto binaryDirectory = path.find_last_of(L'\\');
  if (binaryDirectory == std::wstring::npos ||
      CompareStringOrdinal(path.c_str() + binaryDirectory + 1, -1, L"bin", -1, TRUE) != CSTR_EQUAL) {
    return {};
  }
  return path.substr(0, binaryDirectory);
}

std::wstring canonicalDirectory(const std::wstring &directory)
{
  if (directory.empty()) {
    return {};
  }
  const HANDLE handle = CreateFileW(
      directory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr
  );
  if (handle == INVALID_HANDLE_VALUE) {
    return {};
  }
  std::wstring result(32768, L'\0');
  const DWORD length =
      GetFinalPathNameByHandleW(handle, result.data(), static_cast<DWORD>(result.size()), VOLUME_NAME_DOS);
  CloseHandle(handle);
  if (length == 0 || length >= result.size()) {
    return {};
  }
  result.resize(length);
  return result;
}

std::optional<std::wstring> installedProduct(const std::wstring &root)
{
  if (root.empty()) {
    return std::nullopt;
  }

  std::optional<std::wstring> product;
  for (DWORD index = 0;; ++index) {
    std::array<wchar_t, 39> candidate{};
    const UINT status = MsiEnumRelatedProductsW(SYNERGY_WINDOWS_UPGRADE_CODE, 0, index, candidate.data());
    if (status == ERROR_NO_MORE_ITEMS) {
      return product;
    }
    if (status != ERROR_SUCCESS) {
      return std::nullopt;
    }

    std::wstring installLocation(32768, L'\0');
    DWORD length = static_cast<DWORD>(installLocation.size());
    if (MsiGetProductInfoExW(
            candidate.data(), nullptr, MSIINSTALLCONTEXT_MACHINE, INSTALLPROPERTY_INSTALLLOCATION,
            installLocation.data(), &length
        ) != ERROR_SUCCESS ||
        length == 0 || length >= installLocation.size()) {
      continue;
    }
    installLocation.resize(length);
    const auto registered = canonicalDirectory(installLocation);
    if (CompareStringOrdinal(root.c_str(), -1, registered.c_str(), -1, TRUE) == CSTR_EQUAL) {
      if (product.has_value()) {
        return std::nullopt;
      }
      product = candidate.data();
    }
  }
}

bool startUninstall(const std::wstring &product)
{
  std::wstring systemDir(32768, L'\0');
  const UINT length = GetSystemDirectoryW(systemDir.data(), static_cast<UINT>(systemDir.size()));
  if (length == 0 || length >= systemDir.size()) {
    return false;
  }
  systemDir.resize(length);
  const auto installer = systemDir + L"\\msiexec.exe";
  auto command = L"\"" + installer + L"\" /x " + product;

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(
          installer.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, systemDir.c_str(), &startup, &process
      )) {
    return false;
  }
  // Exit so Windows Installer can remove this executable with the other files.
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
  const auto product = installedProduct(canonicalDirectory(installationRoot()));
  if (!product) {
    showError(localized(
        L"没有找到与此目录匹配的 TuPig Synergy 安装。请通过 Windows 的「已安装的应用」卸载。",
        L"No registered TuPig Synergy installation matches this folder. Uninstall from Windows Installed Apps."
    ));
    return 1;
  }
  if (!startUninstall(*product)) {
    showError(localized(L"无法启动 Windows 安装程序。", L"Windows Installer could not be started."));
    return 1;
  }
  return 0;
}

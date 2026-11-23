/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/UpdateInstaller.h"

#include "common/Constants.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace synergy::platform {

namespace {

// This transaction runs unchanged with ordinary or explicitly authorized privileges.
// Keep the outer bundle inode stable so the Dock bookmark still identifies the same app.
constexpr auto kInstallBundle = R"SH(
set -eu
export LC_ALL=C

[ "$#" -eq 5 ] || exit 64
source=$1
dest=$2
expected_id=$3
image_work=$4
expected_root=$5
gui=${SYNERGY_MAC_UPDATE_GUI:?}
core=${SYNERGY_MAC_UPDATE_CORE:?}
[ -n "$expected_id" ] && [ -n "$expected_root" ] || exit 64

identity() { /usr/bin/stat -f '%d:%i' "$1"; }

bundle_matches() {
  [ -d "$1" ] && [ ! -L "$1" ] &&
    [ -d "$1/Contents" ] && [ ! -L "$1/Contents" ] &&
    [ -f "$1/Contents/Info.plist" ] && [ ! -L "$1/Contents/Info.plist" ] &&
    [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$1/Contents/Info.plist")" = "$expected_id" ]
}

verify_bundle() {
  bundle_matches "$1" && /usr/bin/codesign --verify --deep --strict "$1"
}

bundle_matches "$dest" && verify_bundle "$source" || exit 65
dest=$(cd -P "$dest" && pwd -P) || exit 65
source=$(cd -P "$source" && pwd -P) || exit 65
[ "$source" != "$dest" ] || exit 65
[ "${gui%/*}" = "$dest/Contents/MacOS" ] && [ "${core%/*}" = "$dest/Contents/MacOS" ] || exit 65
parent=${dest%/*}
[ -n "$parent" ] || parent=/
root_id=$(identity "$dest")
[ "$root_id" = "$expected_root" ] || exit 65

# Status 77 alone requests an administrator retry. It precedes any exchange.
[ -w "$dest" ] && [ -w "$parent" ] || exit 77
exec 9< "$dest"
/usr/bin/lockf -s -t 0 9 || exit 75
[ "$(/usr/bin/stat -L -f '%i' /dev/fd/9)" = "${root_id#*:}" ] || exit 65
[ "$(identity "$dest")" = "$root_id" ] || exit 65

stage_parent="$parent"
effective_uid=$(/usr/bin/id -u)
if [ "$effective_uid" -eq 0 ]; then
  # A writable parent such as /Applications could allow another user to
  # rename even a root-owned sibling directory. Reuse the private image work.
  case "$image_work" in
    /private/tmp/synergy-update-image.*) ;;
    *) exit 65 ;;
  esac
  [ -d "$image_work" ] && [ ! -L "$image_work" ] &&
    [ "$(/usr/bin/stat -f '%u:%Lp' "$image_work")" = "0:700" ] || exit 65
  stage_parent="$image_work"
fi
[ "$(/usr/bin/stat -f '%d' "$stage_parent")" = "${root_id%%:*}" ] || exit 73

umask 077
if ! stage=$(/usr/bin/mktemp -d "$stage_parent/.synergy-update.XXXXXXXX" 2>&1); then
  case "$stage" in
    *'Permission denied'*|*'Operation not permitted'*) exit 77 ;;
    *) printf '%s\n' "$stage" >&2; exit 73 ;;
  esac
fi
stage_id=$(/usr/bin/stat -f '%d:%i' "$stage")
keep_backup=0
cleanup() {
  if [ "$keep_backup" -eq 1 ]; then
    printf 'Update recovery bundle retained at %s\n' "$stage/incoming.app" >&2
  elif [ "$(/usr/bin/stat -f '%d:%i' "$stage" 2>/dev/null)" = "$stage_id" ]; then
    /bin/rm -rf "$stage"
  fi
}
trap cleanup EXIT
trap 'exit 74' HUP INT TERM

staged="$stage/incoming.app"
/usr/bin/ditto --noacl "$source" "$staged" || exit 73
if [ "$effective_uid" -eq 0 ]; then
  /usr/sbin/chown -Rh 0:0 "$staged" || exit 73
  /usr/bin/find -P "$staged" ! -type l -perm +022 -exec /bin/chmod go-w {} + || exit 73
fi
verify_bundle "$staged" || exit 65
# Clear only the verified incoming app's download marker before the exchange.
/usr/bin/xattr -dr com.apple.quarantine "$staged" || exit 65
bundle_matches "$dest" || exit 65
[ "$(identity "$dest")" = "$root_id" ] || exit 65
exec 8< "$staged"

# Open directory descriptors pin both roots during the swap. RENAME_NOFOLLOW_ANY
# rejects symlinks in the final paths. Both flags come from Darwin sys/stdio.h.
exchange_contents() {
  /usr/bin/osascript -l JavaScript -e '
    ObjC.import("Foundation");
    ObjC.bindFunction("renameatx_np", ["int", ["int", "char *", "int", "char *", "unsigned int"]]);
    function run() {
      const renameSwap = 0x00000002;
      const renameNoFollowAny = 0x00000010;
      if ($.renameatx_np(9, "Contents", 8, "Contents", renameSwap | renameNoFollowAny) !== 0)
        throw Error("Could not atomically exchange the application contents.");
    }
  '
}

old_id=$(identity "$dest/Contents")
new_id=$(identity "$staged/Contents")
[ "$old_id" != "$new_id" ] || exit 65

# Staging can take time. Use UTF-8 so ps reports localized paths verbatim.
commands=$(LC_ALL=en_US.UTF-8 /bin/ps -Aww -o comm=) || exit 75
match_status=0
printf '%s\n' "$commands" | /usr/bin/grep -Fqx -e "$gui" -e "$core" || match_status=$?
[ "$match_status" -eq 1 ] || exit 75

# From here on, even an ambiguous interruption must retain the staged copy.
keep_backup=1
if ! exchange_contents; then
  if [ "$(identity "$dest")" = "$root_id" ] &&
     [ "$(identity "$dest/Contents")" = "$old_id" ] &&
     [ "$(identity "$staged/Contents")" = "$new_id" ]; then
    keep_backup=0
    exit 73
  fi
  exit 74
fi

[ "$(identity "$dest")" = "$root_id" ] &&
  [ "$(identity "$dest/Contents")" = "$new_id" ] &&
  [ "$(identity "$staged/Contents")" = "$old_id" ] || exit 74

if ! verify_bundle "$dest"; then
  if exchange_contents &&
     [ "$(identity "$dest")" = "$root_id" ] &&
     [ "$(identity "$dest/Contents")" = "$old_id" ] &&
     [ "$(identity "$staged/Contents")" = "$new_id" ]; then
    keep_backup=0
    exit 65
  fi
  exit 74
fi

keep_backup=0
)SH";

// Run the digest check and the mount with the same identity that installs the
// bundle. An administrator retry cannot inherit a user-controlled mount.
constexpr auto kInstallFromDiskImage = R"SH(
set -eu
umask 077

dmg=$1
expected_sha=$2
dest=$3
source_name=$4
bundle_id=$5
install_bundle=$6
expected_root=$7
gui=$8
core=$9
case "$expected_sha" in
  ''|*[!0-9a-f]*) exit 65 ;;
esac
[ "${#expected_sha}" -eq 64 ] || exit 65
[ -f "$dmg" ] && [ ! -L "$dmg" ] || exit 65

work=$(/usr/bin/mktemp -d /private/tmp/synergy-update-image.XXXXXXXX) || exit 73
private_dmg="$work/update.dmg"
mount_path="$work/mounted"

detach_image() {
  if [ -d "$mount_path" ] && ! /bin/rmdir "$mount_path" 2>/dev/null; then
    if ! /usr/bin/hdiutil detach "$mount_path" >/dev/null; then
      printf 'Update image remains mounted at %s\n' "$mount_path" >&2
      return
    fi
    /bin/rmdir "$mount_path" || return
  fi
  /bin/rm -f "$private_dmg" || :
  # A retained rollback bundle intentionally keeps this work directory alive.
  /bin/rmdir "$work" 2>/dev/null || :
}
trap detach_image EXIT
trap 'exit 74' HUP INT TERM

/bin/cat "$dmg" > "$private_dmg" || exit 73
sha_result=$(/usr/bin/shasum -a 256 < "$private_dmg") || exit 65
[ "${sha_result%% *}" = "$expected_sha" ] || exit 65
/bin/mkdir "$mount_path"
printf 'Y\n' | /usr/bin/hdiutil attach "$private_dmg" -nobrowse -readonly -mountpoint "$mount_path" >/dev/null ||
  exit 65
SYNERGY_MAC_UPDATE_GUI="$gui" SYNERGY_MAC_UPDATE_CORE="$core" \
  /bin/sh -c "$install_bundle" mac-update-install "$mount_path/$source_name" "$dest" "$bundle_id" "$work" "$expected_root"
)SH";

constexpr auto kRunUpdate = R"SH(
set -eu

pid=$1
dmg=$2
expected_sha=$3
dest=$4
source_name=$5
bundle_id=$6
gui=$7
core=$8
install_image=$9
install_bundle=${10}
expected_root=$(/usr/bin/stat -f '%d:%i' "$dest") || exit 65

bundle_is_idle() {
  # The initiating GUI normally exits while this helper is starting.
  [ "$(LC_ALL=en_US.UTF-8 /bin/ps -ww -p "$pid" -o comm=)" != "$gui" ] || return 1
  commands=$(LC_ALL=en_US.UTF-8 /bin/ps -Aww -o comm=) || return 1
  match_status=0
  printf '%s\n' "$commands" | /usr/bin/grep -Fqx -e "$gui" -e "$core" || match_status=$?
  [ "$match_status" -eq 1 ]
}

# The GUI shuts down its core during normal exit. Do not exchange its files
# until every GUI or core at these exact paths has stopped.
count=0
while ! bundle_is_idle && [ "$count" -lt 50 ]; do
  /bin/sleep 0.2
  count=$((count + 1))
done
bundle_is_idle || exit 75

result=0
/bin/sh -c "$install_image" mac-update-image "$dmg" "$expected_sha" "$dest" "$source_name" "$bundle_id" "$install_bundle" "$expected_root" "$gui" "$core" || result=$?
if [ "$result" -eq 77 ]; then
  # Pass code inline and quote every argument in AppleScript. The privileged
  # command never executes a user-writable helper script or app executable.
  if admin_result=$(/usr/bin/osascript - "$install_image" "$dmg" "$expected_sha" "$dest" "$source_name" "$bundle_id" "$install_bundle" "$expected_root" "$gui" "$core" <<'APPLESCRIPT'
on run argv
  set commandLine to "/bin/sh -c " & quoted form of (item 1 of argv) & " mac-update-image"
  repeat with argumentIndex from 2 to count argv
    set commandLine to commandLine & " " & quoted form of (item argumentIndex of argv)
  end repeat
  return do shell script (commandLine & " 2>&1; /usr/bin/printf '\\nSYNERGY_UPDATE_RESULT=%s' \"$?\"") with administrator privileges without altering line endings
end run
APPLESCRIPT
); then
    printf '%s\n' "${admin_result%SYNERGY_UPDATE_RESULT=*}" >&2
    case "${admin_result##*SYNERGY_UPDATE_RESULT=}" in
      0|74|75) result=${admin_result##*SYNERGY_UPDATE_RESULT=} ;;
      *) result=1 ;;
    esac
  else
    result=1
  fi
fi

case "$result" in
  0) /usr/bin/open "$dest" ;;
  74|75) exit "$result" ;;
  *)
    if /usr/bin/codesign --verify --deep --strict "$dest"; then
      /usr/bin/open "$dest"
    fi
    exit "$result"
    ;;
esac
)SH";

} // namespace

UpdateInstallResult launchUpdateInstaller(const QString &packagePath, const QString &expectedSha256, QString *error)
{
  // The GUI is inside <bundle>/Contents/MacOS. Use its canonical bundle path,
  // but locate the downloaded payload by its build name even if this app was renamed.
  const auto appDirectory = QDir(QCoreApplication::applicationDirPath());
  const auto destination = QFileInfo(appDirectory.absoluteFilePath("../..")).canonicalFilePath();
  if (destination.isEmpty() || !destination.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not locate the installed application bundle");
    }
    return UpdateInstallResult::Failed;
  }

  const auto executableDirectory = QDir(destination + QStringLiteral("/Contents/MacOS"));
  const auto gui = executableDirectory.filePath(QFileInfo(QCoreApplication::applicationFilePath()).fileName());
  const auto core = executableDirectory.filePath(QString::fromLatin1(kCoreBinName));
  const auto sourceName = QString::fromUtf8(kAppName) + QStringLiteral(".app");

  if (!QProcess::startDetached(
          QStringLiteral("/bin/sh"),
          {QStringLiteral("-c"), QString::fromLatin1(kRunUpdate), QStringLiteral("mac-update"),
           QString::number(QCoreApplication::applicationPid()), packagePath, expectedSha256, destination, sourceName,
           QString::fromLatin1(kRevFqdnName), gui, core, QString::fromLatin1(kInstallFromDiskImage),
           QString::fromLatin1(kInstallBundle)}
      )) {
    if (error != nullptr) {
      *error = QStringLiteral("could not start the update helper");
    }
    return UpdateInstallResult::Failed;
  }

  return UpdateInstallResult::Launched;
}

} // namespace synergy::platform

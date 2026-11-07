#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
payload=$(realpath "${1:?offline payload directory required}")
script=$(realpath "${BASH_SOURCE[0]}")

if [[ ${2:-} != --inside-network-namespace ]]; then
  # Fail rather than falling back to an online installation if namespaces are
  # unavailable. The CI container grants the capability explicitly.
  exec unshare --net -- bash "$script" "$payload" --inside-network-namespace
fi

work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
export HOME="$work/home" FLATPAK_USER_DIR="$work/installation" FLATPAK_SYSTEM_DIR="$work/system"
export FLATPAK_CONFIG_DIR="$work/config" XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data"
export XDG_CACHE_HOME="$work/home/cache" XDG_RUNTIME_DIR="$work/runtime" FLATPAK_GL_DRIVERS=default
mkdir -p "$HOME" "$FLATPAK_CONFIG_DIR" "$XDG_RUNTIME_DIR" "$FLATPAK_SYSTEM_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
[[ -z $(flatpak --user list --columns=ref) ]]
bash "$payload/install-offline.sh"
app_ref=$(cat "$payload/app.ref")
runtime_ref=$(cat "$payload/runtime.ref")
actual_count=$(flatpak --user list --columns=ref | wc -l)
expected_count=$(wc -l < "$payload/refs.tsv")
[[ $actual_count -eq $expected_count ]]

# Every setting used below lives inside sandbox /tmp, including the legacy
# migration marker that would otherwise inspect system-scope preferences.
dbus-run-session -- flatpak --user run --unshare=network --command=sh "$app_ref" -eu -c '
  task_home=$(mktemp -d)
  trap '\''rm -rf -- "$task_home"'\'' EXIT
  export HOME="$task_home" XDG_CONFIG_HOME="$task_home/.config" XDG_CONFIG_DIRS="$task_home/system"
  export XDG_STATE_HOME="$task_home/state" XDG_CACHE_HOME="$task_home/cache"
  mkdir -p "$XDG_CONFIG_HOME/TuPig Synergy" "$XDG_CONFIG_DIRS"
  printf "[migration]\nschemaVersion=2147483647\nnotifiedFor=2147483647\n[scope]\npreferSystem=false\n" > "$XDG_CONFIG_HOME/TuPig Synergy/TuPig Synergy.extra.conf"
  printf "[core]\ncomputerName=synthetic-package-check\n" > "$XDG_CONFIG_HOME/TuPig Synergy/TuPig Synergy.conf"
  test -x /app/bin/synergy-core
  test -x /app/bin/synergy
  ldd /app/bin/synergy-core /app/bin/synergy > "$task_home/dependencies"
  if grep -q "not found" "$task_home/dependencies"; then exit 1; fi
  if grep -E " => /" "$task_home/dependencies" | grep -Ev " => /(app|usr)/"; then exit 1; fi
  /app/bin/synergy-core --version
  QT_QPA_PLATFORM=offscreen QT_DEBUG_PLUGINS=1 LD_DEBUG=libs /app/bin/synergy --help > "$task_home/gui.out" 2> "$task_home/gui.err"
  grep -E "libqoffscreen.*loaded library|loaded library.*libqoffscreen" "$task_home/gui.err" > /dev/null
  if grep "calling init:" "$task_home/gui.err" | grep -Ev "calling init: /(app|usr)/"; then exit 1; fi
  test -d /app/share/licenses/libei
  test -d /app/share/licenses/libportal
'

# The negative probe uses another empty installation. Removing the declared
# runtime ref from a private local repo must make dependency-aware install fail.
negative="$work/missing-runtime"
mkdir -p "$negative"
ostree --repo="$negative/repo" init --mode=archive-z2
app_commit=$(awk -F '\t' -v ref="$app_ref" '$1 == ref {print $2}' "$payload/refs.tsv")
ostree --repo="$negative/repo" pull-local --untrusted "$payload/repo" "$app_commit"
ostree --repo="$negative/repo" refs --create="$app_ref" "$app_commit"
flatpak build-update-repo "$negative/repo"
export FLATPAK_USER_DIR="$work/negative-installation"
flatpak --user remote-add --no-gpg-verify missing-runtime "file://$negative/repo"
if flatpak --user install --noninteractive --no-related missing-runtime "$app_ref" > "$work/negative.log" 2>&1; then
  echo 'ERROR: missing-runtime negative probe unexpectedly installed.' >&2
  exit 1
fi
runtime_id=${runtime_ref#runtime/}
runtime_id=${runtime_id%%/*}
grep -F "$runtime_id" "$work/negative.log" > /dev/null
(cd "$payload" && sha256sum --check --quiet SHA256SUMS)
echo 'PASS: offline empty-installation closure, exact commits, smoke and missing-runtime rejection.'

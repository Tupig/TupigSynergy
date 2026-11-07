#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
repo=$(realpath "${1:?built Flatpak repository required}")
archive=$(realpath -m "${2:?output tar.zst required}")
arch=${3:?Flatpak architecture required}
source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
export HOME="$work/home" FLATPAK_USER_DIR="$work/installation" FLATPAK_SYSTEM_DIR="$work/system"
export FLATPAK_CONFIG_DIR="$work/config" XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data"
export XDG_CACHE_HOME="$work/home/cache" XDG_RUNTIME_DIR="$work/runtime" FLATPAK_GL_DRIVERS=default
mkdir -p "$HOME" "$FLATPAK_CONFIG_DIR" "$XDG_RUNTIME_DIR" "$FLATPAK_SYSTEM_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
payload="$work/tupig-flatpak-offline"
mkdir -p "$payload/repo"
flatpak --user config --set languages '*'
flatpak --user config --set report-os-info false
flatpak --user remote-add --no-gpg-verify built-app "file://$repo"
flatpak --user remote-add flathub https://dl.flathub.org/repo/flathub.flatpakrepo
app_ref=$(ostree --repo="$repo" refs | awk -v arch="$arch" '$0 ~ "^app/com.tupig.synergy/" arch "/" {print}')
[[ $(printf '%s\n' "$app_ref" | wc -l) -eq 1 && -n "$app_ref" ]]
# Install from the build repository so its app Locale extension is included too.
flatpak --user install --noninteractive --arch="$arch" built-app "$app_ref"
runtime_ref="runtime/$(flatpak --user info --show-runtime "$app_ref")"
printf '%s\n' "$app_ref" > "$payload/app.ref"
printf '%s\n' "$runtime_ref" > "$payload/runtime.ref"
ostree --repo="$payload/repo" init --mode=archive-z2
flatpak --user list --columns=ref | LC_ALL=C sort > "$work/refs"
while IFS= read -r ref; do
  ref=$(flatpak --user info --show-ref "$ref")
  [[ $ref == app/* || $ref == runtime/* ]]
  [[ $ref != *.Sdk/* && $ref != *.Debug/* && $ref != *.Sources/* ]]
  commit=$(flatpak --user info --show-commit "$ref")
  origin=$(flatpak --user info --show-origin "$ref")
  # Extra-data commits omit externally downloaded payloads. Do not claim such
  # a closure is offline-complete merely because this build machine has it.
  extra=$(ostree --repo="$FLATPAK_USER_DIR/repo" show --print-metadata-key=xa.extra-data-sources "$commit" 2>/dev/null || true)
  if [[ -n "$extra" && "$extra" != '@a(ayaytt) []' ]]; then
    echo "ERROR: $ref requires external extra-data; cannot export a complete unchanged offline commit." >&2
    exit 1
  fi
  ostree --repo="$payload/repo" pull-local --untrusted "$FLATPAK_USER_DIR/repo" "$commit"
  ostree --repo="$payload/repo" refs --create="$ref" "$commit"
  printf '%s\t%s\t%s\n' "$ref" "$commit" "$origin" >> "$payload/refs.tsv"
done < "$work/refs"
flatpak build-update-repo "$payload/repo"
cp "$source_root/deploy/linux/flatpak/install-offline.sh" "$payload/install-offline.sh"
cp "$source_root/deploy/linux/flatpak/offline.md" "$payload/README.md"
(cd "$payload" && find . -type f ! -name SHA256SUMS -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > SHA256SUMS)
bash "$source_root/scripts/ci/verify-package-flatpak.sh" "$payload"
tar --zstd -cf "$archive" -C "$work" tupig-flatpak-offline

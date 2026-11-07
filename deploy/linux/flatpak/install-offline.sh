#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set -euo pipefail
bundle=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$bundle"
sha256sum --check --quiet SHA256SUMS
app_ref=$(cat app.ref)
runtime_ref=$(cat runtime.ref)
app_commit=$(awk -F '\t' -v ref="$app_ref" '$1 == ref {print $2}' refs.tsv)
[[ $app_commit =~ ^[a-f0-9]{64}$ ]]
remote="tupig-offline-${app_commit:0:12}"
# Trust is scoped to this generated local snapshot repository, never a network
# remote. The archive checksum/provenance must be verified before installation.
flatpak --user remote-add --if-not-exists --no-gpg-verify "$remote" "file://$bundle/repo"
flatpak --user remote-modify --url="file://$bundle/repo" "$remote"
flatpak --user install --noninteractive --no-related "$remote" "$runtime_ref"
while IFS=$'\t' read -r ref commit origin; do
  [[ $ref == runtime/* && $ref != "$runtime_ref" ]] || continue
  flatpak --user install --noninteractive --no-related "$remote" "$ref"
done < refs.tsv
flatpak --user install --noninteractive --no-related "$remote" "$app_ref"
while IFS=$'\t' read -r ref commit origin; do
  actual=$(flatpak --user info --show-commit "$ref")
  [[ $actual == "$commit" ]] || { echo "ERROR: installed commit differs for $ref" >&2; exit 1; }
done < refs.tsv
echo 'Installed the complete local Flatpak snapshot and runtime closure.'

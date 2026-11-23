#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set +x
set -euo pipefail
script_directory=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
exec python3 "$script_directory/setup-macos-signing.py" "$@"

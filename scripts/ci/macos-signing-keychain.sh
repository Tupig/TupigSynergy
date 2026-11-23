#!/usr/bin/env bash
# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT
set +x
set -euo pipefail
umask 077

action=${1:-}
runner_directory=${RUNNER_TEMP:?}
keychain="$runner_directory/tupig-signing.keychain-db"
password_file="$runner_directory/tupig-signing.keychain-password"
certificate_file="$runner_directory/tupig-signing.p12"
search_file="$runner_directory/tupig-signing.search-list"

cleanup() {
  local status=0
  if [[ -e "$keychain" ]]; then
    /usr/bin/security delete-keychain "$keychain" >/dev/null 2>&1 || status=1
  fi
  if [[ -f "$search_file" ]]; then
    python3 - "$search_file" <<'PY' || status=1
import json
from pathlib import Path
import subprocess
import sys
previous = json.loads(Path(sys.argv[1]).read_text())
subprocess.run(['/usr/bin/security', 'list-keychains', '-d', 'user', '-s', *previous], check=True)
PY
  fi
  python3 - "$password_file" "$certificate_file" "$search_file" <<'PY' || status=1
from pathlib import Path
import sys
for name in sys.argv[1:]:
    Path(name).unlink(missing_ok=True)
PY
  return "$status"
}

finish_setup() {
  local result=$?
  if (( result != 0 )); then
    cleanup || result=1
  fi
  exit "$result"
}

case "$action" in
  setup)
    : "${APPLE_P12_CERTIFICATE:?}"
    : "${APPLE_P12_PASSWORD:?}"
    : "${GITHUB_ENV:?}"
    [[ ! -e "$keychain" && ! -L "$keychain" && ! -e "$password_file" && ! -e "$search_file" ]] || {
      echo 'Signing temporary paths are already in use.' >&2
      exit 1
    }
    # Always remove partial imports. Successful setup is cleaned by the job's
    # always() step after the package has been signed.
    trap finish_setup EXIT
    trap 'exit 1' HUP INT TERM
    python3 - "$search_file" <<'PY'
import json
from pathlib import Path
import shlex
import subprocess
import sys
output = subprocess.check_output(['/usr/bin/security', 'list-keychains', '-d', 'user'], text=True)
Path(sys.argv[1]).write_text(json.dumps(shlex.split(output)))
PY
    password=$(openssl rand -hex 32)
    printf '::add-mask::%s\n' "$password"
    printf '%s' "$password" > "$password_file"
    printf '%s' "$APPLE_P12_CERTIFICATE" | /usr/bin/base64 -D > "$certificate_file"
    /usr/bin/security create-keychain -p "$password" "$keychain" >/dev/null 2>&1
    /usr/bin/security set-keychain-settings -lut 3600 "$keychain" >/dev/null 2>&1
    /usr/bin/security unlock-keychain -p "$password" "$keychain" >/dev/null 2>&1
    /usr/bin/security import "$certificate_file" -k "$keychain" -P "$APPLE_P12_PASSWORD" -T /usr/bin/codesign >/dev/null 2>&1
    /usr/bin/security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$password" "$keychain" >/dev/null 2>&1
    # codesign/Security also resolve private keys through the user search list.
    # Include this per-run keychain temporarily, retaining every existing entry.
    python3 - "$search_file" "$keychain" <<'PYCODE'
import json
from pathlib import Path
import subprocess
import sys
previous = json.loads(Path(sys.argv[1]).read_text())
subprocess.run(['/usr/bin/security', 'list-keychains', '-d', 'user', '-s', sys.argv[2], *previous], check=True)
PYCODE
    python3 - "$certificate_file" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).unlink()
PY
    {
      echo "SIGN_KEYCHAIN=$keychain"
      echo "SIGN_KEYCHAIN_PASSWORD_FILE=$password_file"
    } >> "$GITHUB_ENV"
    ;;
  unlock)
    [[ -f "$password_file" && -f "$keychain" ]] || exit 1
    /usr/bin/security unlock-keychain -p "$(cat "$password_file")" "$keychain" >/dev/null 2>&1
    ;;
  cleanup)
    cleanup
    ;;
  *)
    echo "usage: $0 setup|unlock|cleanup" >&2
    exit 2
    ;;
esac

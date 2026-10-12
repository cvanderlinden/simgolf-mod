#!/usr/bin/env bash
# tools/modctl/modctl: manage SimGolf mods (modctl.py; docs/modding/CORE.md).
# Needs build/native/modctl_core.exe (bash tools/build_native.sh).
here=$(cd "$(dirname "$0")" && pwd)
exec "${PYTHON:-python}" "$here/modctl.py" "$@"

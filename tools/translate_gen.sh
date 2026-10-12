#!/usr/bin/env bash
# tools/translate_gen.sh [out]: re-translates the existing Ghidra exports into
# out (default build/gen of this checkout) so new @override functions and
# translator patches take effect. Reads the exports only (PORT_EXPORTS,
# default the main tree's build/ghidra_work); no Ghidra run, about a minute.
# In a worktree whose build/gen is a junction to the main tree, replace the
# junction with a real folder first: the main tree's build/gen is shared.
set -euo pipefail
cd "$(dirname "$0")/.."
. tools/env.sh
export PORT_EXPORTS=${PORT_EXPORTS_OVERRIDE:-build/ghidra_work}
export PORT_GEN=${1:-$SIMGOLF_ROOT/build/gen}
if [ "$(cd "$PORT_GEN" 2>/dev/null && pwd -W)" = "build/gen" ] && [ "$SIMGOLF_ROOT" != "." ]; then
  echo "translate_gen: $PORT_GEN is the main tree's shared build/gen; use a folder of this worktree"; exit 3
fi
sh port/tools/translate/run.sh

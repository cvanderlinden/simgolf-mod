# tools/env.sh: toolchain locations for the port's build scripts (Git Bash on Windows).
# Source it: `. tools/env.sh`. Every value can be overridden from the environment.
# Paths are Windows-style with forward slashes (F:/...), which both Git Bash and
# the Windows-native tools (Java, Python, CMake) understand.

SIMGOLF_ROOT=${SIMGOLF_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd -W)}
TOOLS_EXT=${TOOLS_EXT:-C:/simgolf-tools}
export SIMGOLF_ROOT TOOLS_EXT

# Ghidra 12.1 + JDK 21. GHIDRA_HEADLESS is our wrapper (tools/ghidra_headless.sh):
# analyzeHeadless.bat re-splits arguments on ',' and '=', which breaks the
# port's script arguments (purges=a,b, from=1).
export GHIDRA_HOME=${GHIDRA_HOME:-$TOOLS_EXT/ghidra_12.1_PUBLIC}
export JAVA_HOME=${JAVA_HOME_GHIDRA:-$TOOLS_EXT/jdk21}
export GHIDRA_HEADLESS=${GHIDRA_HEADLESS:-$SIMGOLF_ROOT/tools/ghidra_headless.sh}
export GHIDRA_HEADLESS_MAXMEM=${GHIDRA_HEADLESS_MAXMEM:-6G}

# Ghidra projects and exports (decompiler output: never in git).
export W=${W:-$SIMGOLF_ROOT/build/ghidra_work}
export PORT_EXPORTS=${PORT_EXPORTS:-$W}
# Translator output and the game install the build reads.
export PORT_GEN=${PORT_GEN:-$SIMGOLF_ROOT/build/gen}
export PORT_WEB=${PORT_WEB:-$SIMGOLF_ROOT/build/web}
export SIMGOLF_GAME=${SIMGOLF_GAME:-$SIMGOLF_ROOT/game}

# Python 3 (the WindowsApps python3 is a store stub, so prefer `python`).
if [ -z "${PYTHON:-}" ]; then
  if python -c 'import sys; sys.exit(sys.version_info < (3, 8))' >/dev/null 2>&1; then PYTHON=python; else PYTHON=python3; fi
fi
export PYTHON

# Emscripten (emsdk) and its environment.
export EMSDK_DIR=${EMSDK_DIR:-$TOOLS_EXT/emsdk}
if ! command -v emcc >/dev/null 2>&1 && [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
  EMSDK_QUIET=1 . "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
fi

# CMake (VS 2022 BuildTools copy) and Ninja.
CMAKE_DIR=${CMAKE_DIR:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"}
NINJA_DIR=${NINJA_DIR:-$TOOLS_EXT/ninja}
command -v cmake >/dev/null 2>&1 || PATH="$(cygpath -u "$CMAKE_DIR"):$PATH"
command -v ninja >/dev/null 2>&1 || PATH="$(cygpath -u "$NINJA_DIR"):$PATH"
export PATH

# Chrome with JSPI (137+) for headless runs (tools/port/run.mjs).
if [ -z "${CHROME_PATH:-}" ]; then
  for c in "C:/Program Files/Google/Chrome/Application/chrome.exe" \
           "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"; do
    [ -f "$c" ] && { CHROME_PATH=$c; break; }
  done
fi
export CHROME_PATH

# The translator reads and writes UTF-8 (Ghidra exports with -Dfile.encoding=UTF8).
export PYTHONUTF8=1

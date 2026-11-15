#!/usr/bin/env bash
# Builds ResearchNet with g++ (C++17). Works on Linux, macOS and Windows (Git Bash + MSYS2).
#
#   ./build.sh          build the server  -> ./researchnet  (researchnet.exe on Windows)
#   ./build.sh test     build and run the unit tests (no database needed)
#
# On Windows set PG_HOME if PostgreSQL is not in "C:/Program Files/PostgreSQL/18",
# and put MSYS2's ucrt64 g++ first in PATH (or set CXX).
set -euo pipefail
cd "$(dirname "$0")"

CXX="${CXX:-g++}"
BUILD="${BUILD_DIR:-build}"            # object files and test binaries
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Iinclude -Ithird_party"
CORE="src/TextUtils.cpp src/Trie.cpp src/InvertedIndex.cpp src/SearchEngine.cpp src/Recommender.cpp"

if [[ "${OS:-}" == "Windows_NT" ]]; then
    PG_HOME="${PG_HOME:-C:/Program Files/PostgreSQL/18}"
    if [[ -z "${CXX_SET:-}" && -x /c/msys64/ucrt64/bin/g++ ]]; then
        export PATH="/c/msys64/ucrt64/bin:$PATH"     # needs a C++17 compiler (GCC 8+)
    fi
    PG_FLAGS=(-I "$PG_HOME/include")
    # Link the DLL directly (MinGW) and the GCC runtime statically, so the program does not
    # pick up the older libwinpthread-1.dll that ships with PostgreSQL.
    PG_LIBS=("$PG_HOME/bin/libpq.dll" -lws2_32 -static)
    EXE=.exe
else
    PG_FLAGS=(-I "$(pg_config --includedir 2>/dev/null || echo /usr/include/postgresql)")
    PG_LIBS=(-lpq -pthread)
    EXE=
fi

if [[ "${1:-}" == "test" ]]; then
    mkdir -p "$BUILD"
    status=0
    for test in tests/test_*.cpp; do
        name="$BUILD/$(basename "${test%.cpp}")$EXE"
        echo "== $test"
        # shellcheck disable=SC2086
        $CXX $CXXFLAGS -Itests $CORE "$test" -o "$name" && "$name" || status=1
    done
    exit $status
fi

mkdir -p "$BUILD"
OBJECTS=()
for source in src/*.cpp; do
    object="$BUILD/$(basename "${source%.cpp}").o"
    OBJECTS+=("$object")
    # Recompile only when the source or any header is newer than the object file.
    if [[ ! -f "$object" || "$source" -nt "$object" || -n "$(find include -newer "$object" -print -quit)" ]]; then
        echo "compiling $source"
        # shellcheck disable=SC2086
        $CXX $CXXFLAGS "${PG_FLAGS[@]}" -c "$source" -o "$object" &
    fi
done
wait
# shellcheck disable=SC2086
$CXX $CXXFLAGS "${OBJECTS[@]}" -o "researchnet$EXE" "${PG_LIBS[@]}"
echo "built ./researchnet$EXE"

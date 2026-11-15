#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CXX="${CXX:-g++}"
BUILD="${BUILD_DIR:-build}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Iinclude -Ithird_party"
CORE="src/TextUtils.cpp src/Trie.cpp src/InvertedIndex.cpp src/SearchEngine.cpp src/Recommender.cpp"

if [[ "${OS:-}" == "Windows_NT" ]]; then
    PG_HOME="${PG_HOME:-C:/Program Files/PostgreSQL/18}"
    if [[ -z "${CXX_SET:-}" && -x /c/msys64/ucrt64/bin/g++ ]]; then
        export PATH="/c/msys64/ucrt64/bin:$PATH"
    fi
    PG_FLAGS=(-I "$PG_HOME/include")
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
        $CXX $CXXFLAGS -Itests $CORE "$test" -o "$name" && "$name" || status=1
    done
    exit $status
fi

mkdir -p "$BUILD"
JOBS="${JOBS:-2}"
OBJECTS=()
PIDS=()
failed=0
for source in src/*.cpp; do
    object="$BUILD/$(basename "${source%.cpp}").o"
    OBJECTS+=("$object")
    if [[ ! -f "$object" || "$source" -nt "$object" || -n "$(find include -newer "$object" -print -quit)" ]]; then
        echo "compiling $source"
        rm -f "$object"
        $CXX $CXXFLAGS "${PG_FLAGS[@]}" -c "$source" -o "$object" &
        PIDS+=($!)
        if (( ${#PIDS[@]} >= JOBS )); then
            wait "${PIDS[0]}" || failed=1
            PIDS=("${PIDS[@]:1}")
        fi
    fi
done
for pid in "${PIDS[@]}"; do
    wait "$pid" || failed=1
done
if (( failed )); then
    echo "build failed" >&2
    exit 1
fi
$CXX $CXXFLAGS "${OBJECTS[@]}" -o "researchnet$EXE" "${PG_LIBS[@]}"
echo "built ./researchnet$EXE"

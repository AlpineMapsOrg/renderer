#!/bin/sh
# Type-check the MLS-MPM C++ files without building the app.
#
# A full build needs Qt, Dawn and the whole dependency tree; this only needs headers and runs
# `g++ -fsyntax-only` per translation unit, which catches everything short of link errors.
# Written for containers without a GPU or a Qt install (used for 09-performance-analysis.md).
#
#   sh mpm-mls-doc/scripts/syntax_check.sh [file.cpp ...]
#
# Needs: g++ (C++20), git, curl, unzip, Qt 6 and glm headers (apt: qt6-base-dev libglm-dev).
# Fetches into $MPM_CHECK_CACHE (default ~/.cache/webigeo-syntax-check): the emdawnwebgpu
# package of the Dawn release webgpu/base/CMakeLists.txt pins, and the header-only deps.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CACHE=${MPM_CHECK_CACHE:-$HOME/.cache/webigeo-syntax-check}
mkdir -p "$CACHE"

DAWN=$(sed -n 's/^set(ALP_DAWN_VERSION "\([^"]*\)".*/\1/p' "$ROOT/webgpu/base/CMakeLists.txt")
if [ ! -f "$CACHE/emdawn-$DAWN/emdawnwebgpu_pkg/webgpu/include/webgpu/webgpu.h" ]; then
    curl -sSL -o "$CACHE/emdawn.zip" "https://github.com/google/dawn/releases/download/v$DAWN/emdawnwebgpu_pkg-v$DAWN.zip"
    unzip -q -o "$CACHE/emdawn.zip" -d "$CACHE/emdawn-$DAWN"
fi

fetch() { # name url commit
    if [ ! -d "$CACHE/$1" ]; then
        git clone -q "$2" "$CACHE/$1"
        git -C "$CACHE/$1" checkout -q "$3"
    fi
}
fetch radix https://github.com/AlpineMapsOrg/radix.git e939e10c5a40866950b68a0bc04c851bcfcf5dad
fetch expected https://github.com/TartanLlama/expected.git v1.1.0
fetch imgui https://github.com/AlpineMapsOrgDependencies/imgui_slim.git 4d8ecbf58ebf32e757ff56a60f1fb0446033edb8
fetch imnodes https://github.com/AlpineMapsOrgDependencies/imnodes_slim.git 324355e64ed8b5bea02fe1439cdcb5c7773b4436
fetch icons https://github.com/juliettef/IconFontCppHeaders.git f30b1e73b2d71eb331d77619c3f1de34199afc38
fetch filedialog https://github.com/AlpineMapsOrgDependencies/ImGuiFileDialog_slim.git 39f98eb131ee00d476f1002336fc7e9e1795ccc5

# webgpu_interface.hpp includes SDL for one declaration; a stub is enough for type checking.
mkdir -p "$CACHE/stubs/SDL2"
[ -f "$CACHE/stubs/SDL2/SDL.h" ] || echo 'typedef struct SDL_Window SDL_Window;' > "$CACHE/stubs/SDL2/SDL.h"

QT=$(ls -d /usr/include/x86_64-linux-gnu/qt6 /usr/include/qt6 2>/dev/null | head -n 1)
[ -n "$QT" ] || { echo "Qt 6 headers not found (apt install qt6-base-dev)" >&2; exit 2; }

if [ $# -eq 0 ]; then
    set -- webgpu/compute/nodes/MpmSolverNode.cpp apps/webgpu_app/avalanche/AvalanchePanel.cpp apps/webgpu_app/compute/nodes/MpmSolverNodeRenderer.cpp
fi

cd "$ROOT"
status=0
for file in "$@"; do
    printf '%s: ' "$file"
    if g++ -std=c++20 -fsyntax-only -fPIC -Wall -Wextra -Wno-unused-parameter \
        -I. -Iwebgpu -Iwebgpu/base -Iwebgpu/base/raii -Iwebgpu/compute -Iwebgpu/compute/nodes \
        -Iapps/webgpu_app -Iapps/webgpu_app/compute -Iapps/webgpu_app/compute/nodes -Iapps/webgpu_app/ui -Iapps/webgpu_app/avalanche \
        -I"$CACHE/emdawn-$DAWN/emdawnwebgpu_pkg/webgpu/include" -I"$CACHE/radix/src" -I"$CACHE/expected/include" \
        -I"$CACHE/imgui" -I"$CACHE/imnodes" -I"$CACHE/icons" -I"$CACHE/filedialog" -I"$CACHE/stubs" \
        -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -I"$QT/QtNetwork" "$file"; then
        echo ok
    else
        status=1
    fi
done
exit $status

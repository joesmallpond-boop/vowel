#!/usr/bin/env bash
# VowelWorm — macOS build script (no Homebrew required).
#
#   ./build_mac.sh              build Release VST3 + AU + Standalone for this Mac, run tests
#   ./build_mac.sh --install    ... and copy the plug-ins into ~/Library/Audio/Plug-Ins
#   ./build_mac.sh --universal  build for Intel + Apple Silicon
#   JUCE_DIR=/path/to/JUCE ./build_mac.sh   use an existing JUCE checkout instead of downloading
#
# Checks for: Xcode Command Line Tools, CMake (downloads a private copy into .tools/ if missing), JUCE.
set -euo pipefail
cd "$(dirname "$0")"

INSTALL=0; UNIVERSAL=0
for a in "$@"; do
  case "$a" in
    --install) INSTALL=1 ;;
    --universal) UNIVERSAL=1 ;;
    *) echo "unknown option $a"; exit 1 ;;
  esac
done

echo "== VowelWorm build"

# 1. Xcode Command Line Tools
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools are not installed."
  echo "Run:  xcode-select --install   then re-run this script."
  exit 1
fi
echo "Xcode CLT: $(xcode-select -p)"

# 2. CMake (>= 3.22): use the system one if new enough, otherwise a private download
CMAKE=""
if command -v cmake >/dev/null 2>&1; then
  ver=$(cmake --version | head -1 | awk '{print $3}')
  major=${ver%%.*}; rest=${ver#*.}; minor=${rest%%.*}
  if [ "$major" -gt 3 ] || { [ "$major" -eq 3 ] && [ "$minor" -ge 22 ]; }; then CMAKE=$(command -v cmake); fi
fi
if [ -z "$CMAKE" ] && [ -x "/Applications/CMake.app/Contents/bin/cmake" ]; then CMAKE="/Applications/CMake.app/Contents/bin/cmake"; fi
if [ -z "$CMAKE" ]; then
  CMV=3.30.5
  if [ ! -x ".tools/cmake/CMake.app/Contents/bin/cmake" ]; then
    echo "CMake not found - downloading CMake $CMV into .tools/ (no system changes)"
    mkdir -p .tools
    curl -fsSL -o .tools/cmake.tar.gz "https://github.com/Kitware/CMake/releases/download/v$CMV/cmake-$CMV-macos-universal.tar.gz"
    tar -xzf .tools/cmake.tar.gz -C .tools
    rm -rf .tools/cmake && mv ".tools/cmake-$CMV-macos-universal" .tools/cmake
  fi
  CMAKE="$PWD/.tools/cmake/CMake.app/Contents/bin/cmake"
fi
echo "CMake: $CMAKE ($("$CMAKE" --version | head -1))"

# 3. JUCE: explicit checkout, ./JUCE, or download via CMake FetchContent (needs git)
JUCE_ARG=()
if [ -n "${JUCE_DIR:-}" ]; then JUCE_ARG=(-DVW_JUCE_DIR="$JUCE_DIR")
elif [ -d "JUCE/modules" ]; then JUCE_ARG=(-DVW_JUCE_DIR="$PWD/JUCE")
else
  command -v git >/dev/null 2>&1 || { echo "git is required to download JUCE (comes with Xcode CLT)"; exit 1; }
  echo "JUCE: will be downloaded (8.0.10) on first configure"
fi

ARCH_ARG=()
if [ "$UNIVERSAL" = 1 ]; then ARCH_ARG=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"); fi

JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release ${JUCE_ARG[@]+"${JUCE_ARG[@]}"} ${ARCH_ARG[@]+"${ARCH_ARG[@]}"}
"$CMAKE" --build build --config Release -j "$JOBS"

echo "== Running engine tests"
./build/vw_tests

# ad-hoc signature so macOS / Ableton will load locally built bundles
for b in build/*_artefacts/Release/VST3/*.vst3 build/*_artefacts/Release/AU/*.component build/*_artefacts/Release/Standalone/*.app; do
  [ -e "$b" ] && codesign --force --deep --sign - "$b" >/dev/null 2>&1 || true
done

if [ "$INSTALL" = 1 ]; then
  mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
  for b in build/*_artefacts/Release/VST3/*.vst3; do rm -rf ~/Library/Audio/Plug-Ins/VST3/"$(basename "$b")"; cp -R "$b" ~/Library/Audio/Plug-Ins/VST3/; done
  for b in build/*_artefacts/Release/AU/*.component; do rm -rf ~/Library/Audio/Plug-Ins/Components/"$(basename "$b")"; cp -R "$b" ~/Library/Audio/Plug-Ins/Components/; done
  echo "Installed to ~/Library/Audio/Plug-Ins (VST3 + Components). Rescan plug-ins in Ableton (Settings > Plug-Ins)."
fi

echo "== Done"
ls -d build/*_artefacts/Release/*/* 2>/dev/null

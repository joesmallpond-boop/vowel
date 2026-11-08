#!/bin/bash
# VowelWorm Max for Live installer: puts the vowelworm~ external + display script where Max finds them,
# and the device into your Ableton User Library.
cd "$(dirname "$0")"
PKG="$PWD"
xattr -dr com.apple.quarantine "$PKG" 2>/dev/null
installed=0
for M in "$HOME/Documents/Max 9" "$HOME/Documents/Max 8"; do
  if [ -d "$M" ] || [ "$M" = "$HOME/Documents/Max 9" ]; then
    mkdir -p "$M/Packages"
    rm -rf "$M/Packages/VowelWorm"
    mkdir -p "$M/Packages/VowelWorm"
    cp -R "$PKG/externals" "$PKG/javascript" "$PKG/package-info.json" "$M/Packages/VowelWorm/"
    echo "Installed Max package to: $M/Packages/VowelWorm"
    installed=1
  fi
done
LIB="$HOME/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect"
mkdir -p "$LIB"
cp "$PKG/VowelWorm.amxd" "$LIB/"
echo "Installed device to: $LIB/VowelWorm.amxd"
echo "Restart Live, then find VowelWorm under Places > User Library > Presets > Audio Effects > Max Audio Effect."

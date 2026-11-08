#!/usr/bin/env bash
# Builds VowelWorm-Installer.pkg (macOS) from the packaged build in dist/VowelWorm.
#
#   scripts/make_pkg.sh [dist/VowelWorm] [version]
#
# Components (each can be deselected in the installer's "Customise" step):
#   VST3         -> /Library/Audio/Plug-Ins/VST3
#   AU           -> /Library/Audio/Plug-Ins/Components   (Audio Unit cache refreshed after install)
#   Standalone   -> /Applications
#   Max for Live -> /Users/Shared/Max 9/Packages/VowelWorm (+ Max 8), device copied into your Ableton User Library
#
# Unsigned (ad-hoc) build: first launch of the .pkg needs right-click > Open. Files installed by the
# installer are not quarantined, so Ableton loads the plug-ins without any Terminal commands.
set -euo pipefail
SRC="${1:-dist/VowelWorm}"
VERSION="${2:-0.2.0}"
WORK="$(mktemp -d)"
OUT="dist/VowelWorm-Installer.pkg"
ID=com.joecaple.vowelworm

nonrelocatable () { # $1 root dir, $2 plist out
  pkgbuild --analyze --root "$1" "$2" >/dev/null
  n=$(plutil -convert json -o - "$2" | python3 -c "import json,sys;print(len(json.load(sys.stdin)))")
  for ((i=0; i<n; i++)); do plutil -replace "$i.BundleIsRelocatable" -bool NO "$2"; done
}

mkdir -p "$WORK/pkgs" "$WORK/scripts/au" "$WORK/scripts/m4l" "$WORK/res"

# --- VST3
mkdir -p "$WORK/vst3"; cp -R "$SRC"/*.vst3 "$WORK/vst3/"
nonrelocatable "$WORK/vst3" "$WORK/vst3.plist"
pkgbuild --root "$WORK/vst3" --component-plist "$WORK/vst3.plist" --identifier $ID.vst3 --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/pkgs/vst3.pkg"

# --- AU
mkdir -p "$WORK/au"; cp -R "$SRC"/*.component "$WORK/au/"
nonrelocatable "$WORK/au" "$WORK/au.plist"
cat > "$WORK/scripts/au/postinstall" <<'EOF'
#!/bin/bash
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
exit 0
EOF
chmod +x "$WORK/scripts/au/postinstall"
pkgbuild --root "$WORK/au" --component-plist "$WORK/au.plist" --scripts "$WORK/scripts/au" --identifier $ID.au --version "$VERSION" \
  --install-location "/Library/Audio/Plug-Ins/Components" "$WORK/pkgs/au.pkg"

# --- Standalone
mkdir -p "$WORK/app"; cp -R "$SRC"/VowelWorm.app "$WORK/app/"
nonrelocatable "$WORK/app" "$WORK/app.plist"
pkgbuild --root "$WORK/app" --component-plist "$WORK/app.plist" --identifier $ID.app --version "$VERSION" \
  --install-location "/Applications" "$WORK/pkgs/app.pkg"

# --- Max for Live: shared Max package + device into the logged-in user's User Library
M4L="$SRC/Max for Live"
mkdir -p "$WORK/m4l/VowelWorm"
cp -R "$M4L/externals" "$M4L/javascript" "$M4L/package-info.json" "$M4L/VowelWorm.amxd" "$M4L/README.md" "$WORK/m4l/VowelWorm/"
cat > "$WORK/scripts/m4l/postinstall" <<'EOF'
#!/bin/bash
PKG="/Users/Shared/Max 9/Packages/VowelWorm"
# Live versions that embed Max 8 look in the Max 8 folder
mkdir -p "/Users/Shared/Max 8/Packages"
rm -rf "/Users/Shared/Max 8/Packages/VowelWorm"
cp -R "$PKG" "/Users/Shared/Max 8/Packages/VowelWorm"
chmod -R a+rX "/Users/Shared/Max 9/Packages/VowelWorm" "/Users/Shared/Max 8/Packages/VowelWorm"
# device into the console user's Ableton User Library
U=$(stat -f%Su /dev/console)
if [ -n "$U" ] && [ "$U" != "root" ]; then
  H=$(dscl . -read "/Users/$U" NFSHomeDirectory | awk '{print $2}')
  LIB="$H/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect"
  mkdir -p "$LIB"
  cp "$PKG/VowelWorm.amxd" "$LIB/VowelWorm.amxd"
  chown -R "$U" "$H/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect"
fi
exit 0
EOF
chmod +x "$WORK/scripts/m4l/postinstall"
pkgbuild --root "$WORK/m4l" --scripts "$WORK/scripts/m4l" --identifier $ID.m4l --version "$VERSION" \
  --install-location "/Users/Shared/Max 9/Packages" "$WORK/pkgs/m4l.pkg"

# --- installer UI text
cat > "$WORK/res/welcome.html" <<EOF
<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2>VowelWorm $VERSION</h2>
<p>A MIDI-controlled vocal harmoniser and voice-following synth.</p>
<p>This installs the <b>VST3</b> and <b>Audio Unit</b> plug-ins, the <b>Standalone</b> app and the
<b>Max for Live device</b>. Use <i>Customise</i> to choose.</p>
<p><b>Ableton setup:</b> put VowelWorm on your vocal track (Monitor = In). On a MIDI track set
<i>MIDI To</i> = the vocal track, then choose VowelWorm. Rescan plug-ins in Live's settings after installing.</p>
<p>The Max for Live device appears in Places &gt; User Library &gt; Presets &gt; Audio Effects &gt; Max Audio Effect.</p>
</body></html>
EOF

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>VowelWorm $VERSION</title>
  <welcome file="welcome.html" mime-type="text/html"/>
  <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="vst3"/><line choice="au"/><line choice="app"/><line choice="m4l"/>
  </choices-outline>
  <choice id="vst3" title="VST3 plug-ins" description="VowelWorm (effect + MIDI in) and VowelWorm Inst (instrument + sidechain) for Ableton Live and other VST3 hosts.">
    <pkg-ref id="$ID.vst3"/></choice>
  <choice id="au" title="Audio Unit plug-ins" description="For Logic, GarageBand and other AU hosts.">
    <pkg-ref id="$ID.au"/></choice>
  <choice id="app" title="Standalone app" description="VowelWorm.app in Applications - play with a mic and MIDI keyboard without a DAW.">
    <pkg-ref id="$ID.app"/></choice>
  <choice id="m4l" title="Max for Live device" description="vowelworm~ external + displays (Max package in /Users/Shared) and the VowelWorm device in your Ableton User Library.">
    <pkg-ref id="$ID.m4l"/></choice>
  <pkg-ref id="$ID.vst3" version="$VERSION">vst3.pkg</pkg-ref>
  <pkg-ref id="$ID.au" version="$VERSION">au.pkg</pkg-ref>
  <pkg-ref id="$ID.app" version="$VERSION">app.pkg</pkg-ref>
  <pkg-ref id="$ID.m4l" version="$VERSION">m4l.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p dist
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$WORK/res" "$OUT"
pkgutil --check-signature "$OUT" || true
echo "built $OUT"
pkgutil --payload-files "$WORK/pkgs/vst3.pkg" | head -5

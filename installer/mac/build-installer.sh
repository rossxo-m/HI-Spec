#!/usr/bin/env bash
# HI-Spec — macOS installer build script (pkgbuild + productbuild).
set -euo pipefail

VERSION="0.1.0"
IDENT="com.hi.hispec"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARTEFACTS="$PROJECT_ROOT/build/HISpec_artefacts/Release"
WORK="$SCRIPT_DIR/work"
OUTPUT="$SCRIPT_DIR/output"

VST3_SRC="$ARTEFACTS/VST3/HI-Spec.vst3"

if [ ! -d "$VST3_SRC" ]; then
    echo "error: $VST3_SRC not found. Build the plugin first." >&2
    exit 1
fi

rm -rf "$WORK" "$OUTPUT"
mkdir -p "$WORK/vst3-root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$OUTPUT"

cp -R "$VST3_SRC" "$WORK/vst3-root/Library/Audio/Plug-Ins/VST3/"

VST3_PKG="$WORK/HI-Spec-vst3.pkg"
pkgbuild --root "$WORK/vst3-root" \
         --identifier "$IDENT.vst3" \
         --version "$VERSION" \
         --install-location "/" \
         "$VST3_PKG"

DIST_XML="$WORK/distribution.xml"
{
    echo '<?xml version="1.0" encoding="utf-8"?>'
    echo '<installer-gui-script minSpecVersion="2">'
    echo "    <title>HI-Spec</title>"
    echo "    <organization>com.hi</organization>"
    echo "    <domains enable_localSystem=\"true\"/>"
    echo "    <options customize=\"never\" require-scripts=\"false\"/>"
    echo "    <choices-outline>"
    echo "        <line choice=\"default\"><line choice=\"$IDENT.vst3\"/></line>"
    echo "    </choices-outline>"
    echo "    <choice id=\"default\"/>"
    echo "    <choice id=\"$IDENT.vst3\" visible=\"false\"><pkg-ref id=\"$IDENT.vst3\"/></choice>"
    echo "    <pkg-ref id=\"$IDENT.vst3\">HI-Spec-vst3.pkg</pkg-ref>"
    echo '</installer-gui-script>'
} > "$DIST_XML"

productbuild --distribution "$DIST_XML" \
             --package-path "$WORK" \
             "$OUTPUT/HI-Spec-$VERSION-macos.pkg"

echo "Built: $OUTPUT/HI-Spec-$VERSION-macos.pkg"

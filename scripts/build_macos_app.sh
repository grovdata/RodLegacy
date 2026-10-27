#!/bin/bash
# Build a distributable RodLegacy.app (+ .dmg) for macOS.
# Universal (arm64 11.0 + x86_64 10.15), everything linked statically from source, nothing to bundle.
# 10.15 is the floor because the code uses std::filesystem.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD=build-macos
APP=$BUILD/RodLegacy.app

# Homebrew's pkg-config is hidden so only the subprojects are used, whatever is installed.
export PKG_CONFIG_LIBDIR=$(mktemp -d)
FALLBACK=sdl2,ogg,vorbis,sdl2_image,sdl2_mixer

build_arch() {
    local arch=$1 min=$2 dir=$BUILD/$1
    if [ ! -d "$dir" ]; then
        mkdir -p "$BUILD"
        cat > "$BUILD/$arch.ini" <<INI
[built-in options]
c_args = ['-arch', '$arch', '-mmacosx-version-min=$min']
cpp_args = ['-arch', '$arch', '-mmacosx-version-min=$min']
objc_args = ['-arch', '$arch', '-mmacosx-version-min=$min']
c_link_args = ['-arch', '$arch', '-mmacosx-version-min=$min']
cpp_link_args = ['-arch', '$arch', '-mmacosx-version-min=$min']
INI
        meson setup "$dir" --native-file "$BUILD/$arch.ini" --buildtype=release --force-fallback-for=$FALLBACK
    fi
    ninja -C "$dir" rodlegacy
}

build_arch arm64 11.0
build_arch x86_64 10.15
VERSION=$(python3 -c "import json;print(json.load(open('$BUILD/arm64/meson-info/intro-projectinfo.json'))['version'])")

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
lipo -create "$BUILD/arm64/rodlegacy" "$BUILD/x86_64/rodlegacy" -output "$APP/Contents/MacOS/rodlegacy"
strip -x "$APP/Contents/MacOS/rodlegacy"

iconset=$(mktemp -d)/RodLegacy.iconset
mkdir -p "$iconset"
for s in 16 32 64 128 256 512; do
    sips -z $s $s data/res/icon.png --out "$iconset/icon_${s}x${s}.png" >/dev/null
done
# Retina slots reuse the next size up.
for s in 16 32 128 256; do cp "$iconset/icon_$((s*2))x$((s*2)).png" "$iconset/icon_${s}x${s}@2x.png"; done
iconutil -c icns "$iconset" -o "$APP/Contents/Resources/RodLegacy.icns"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>RodLegacy</string>
    <key>CFBundleDisplayName</key>       <string>RodLegacy</string>
    <key>CFBundleIdentifier</key>        <string>se.grovdata.rodlegacy</string>
    <key>CFBundleExecutable</key>        <string>rodlegacy</string>
    <key>CFBundleIconFile</key>          <string>RodLegacy.icns</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key>           <string>$VERSION</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>LSMinimumSystemVersion</key>    <string>10.15</string>
    <key>NSHighResolutionCapable</key>   <true/>
</dict>
</plist>
PLIST

# Ad-hoc signature: unsigned arm64 binaries are killed on launch.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

DMG="$BUILD/RodLegacy-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -quiet -volname RodLegacy -srcfolder "$APP" -ov -format UDZO "$DMG"

echo "built $APP"
echo "built $DMG"

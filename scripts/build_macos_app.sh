#!/bin/bash
# Build a distributable RodLegacy.app (+ .dmg) for macOS.
# Bundles the Homebrew dylibs, so the result runs on a machine without brew.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD=build-macos
APP=$BUILD/RodLegacy.app

command -v dylibbundler >/dev/null || { echo "need: brew install dylibbundler"; exit 1; }

[ -d "$BUILD" ] || meson setup "$BUILD" --buildtype=release
ninja -C "$BUILD" rodlegacy
VERSION=$(python3 -c "import json;print(json.load(open('$BUILD/meson-info/intro-projectinfo.json'))['version'])")

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"
install -m755 "$BUILD/rodlegacy" "$APP/Contents/MacOS/rodlegacy"
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
    <key>LSMinimumSystemVersion</key>    <string>11.0</string>
    <key>NSHighResolutionCapable</key>   <true/>
</dict>
</plist>
PLIST

dylibbundler -of -b -x "$APP/Contents/MacOS/rodlegacy" \
    -d "$APP/Contents/Frameworks" -p @executable_path/../Frameworks

# Homebrew's sdl2 is sdl2-compat, a shim that dlopens SDL3 at run time. That
# load is invisible to dylibbundler, and the shim aborts in its initializer
# when it cannot find it. It looks beside itself first, so put it there.
SDL2_LIB=$APP/Contents/Frameworks/libSDL2-2.0.0.dylib
if [ -f "$SDL2_LIB" ] && strings -a "$SDL2_LIB" | grep -q '@loader_path/libSDL3.dylib'; then
    SDL3_LIB=$(pkg-config --variable=libdir sdl3)/libSDL3.0.dylib
    [ -f "$SDL3_LIB" ] || { echo "sdl2-compat needs SDL3: brew install sdl3"; exit 1; }
    install -m644 "$SDL3_LIB" "$APP/Contents/Frameworks/libSDL3.dylib"
    install_name_tool -id @loader_path/libSDL3.dylib "$APP/Contents/Frameworks/libSDL3.dylib"
fi

# Ad-hoc signature: unsigned arm64 binaries are killed on launch.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

DMG="$BUILD/RodLegacy-$VERSION.dmg"
rm -f "$DMG"
hdiutil create -quiet -volname RodLegacy -srcfolder "$APP" -ov -format UDZO "$DMG"

echo "built $APP"
echo "built $DMG"

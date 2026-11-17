#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "==> Building CopyHeap (Release)..."
swift build -c release

APP_DIR="build/CopyHeap.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"

rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR" "$RESOURCES_DIR"

echo "==> Copying binary..."
cp .build/release/CopyHeap "$MACOS_DIR/CopyHeap"

echo "==> Creating Info.plist..."
cat << 'EOF' > "$CONTENTS_DIR/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>CopyHeap</string>
    <key>CFBundleIdentifier</key>
    <string>com.copyheap.macos</string>
    <key>CFBundleName</key>
    <string>CopyHeap</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.1.0</string>
    <key>CFBundleVersion</key>
    <string>1.1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>LSUIElement</key>
    <true/>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

echo "==> Packaging icon..."
if [ -f "../res/icon.png" ]; then
    ICONSET="build/CopyHeap.iconset"
    rm -rf "$ICONSET"
    mkdir -p "$ICONSET"
    sips -z 16 16     ../res/icon.png --out "$ICONSET/icon_16x16.png" > /dev/null
    sips -z 32 32     ../res/icon.png --out "$ICONSET/icon_16x16@2x.png" > /dev/null
    sips -z 32 32     ../res/icon.png --out "$ICONSET/icon_32x32.png" > /dev/null
    sips -z 64 64     ../res/icon.png --out "$ICONSET/icon_32x32@2x.png" > /dev/null
    sips -z 128 128   ../res/icon.png --out "$ICONSET/icon_128x128.png" > /dev/null
    sips -z 256 256   ../res/icon.png --out "$ICONSET/icon_128x128@2x.png" > /dev/null
    sips -z 256 256   ../res/icon.png --out "$ICONSET/icon_256x256.png" > /dev/null
    sips -z 512 512   ../res/icon.png --out "$ICONSET/icon_256x256@2x.png" > /dev/null
    sips -z 512 512   ../res/icon.png --out "$ICONSET/icon_512x512.png" > /dev/null
    sips -z 1024 1024 ../res/icon.png --out "$ICONSET/icon_512x512@2x.png" > /dev/null
    iconutil -c icns "$ICONSET" -o "$RESOURCES_DIR/AppIcon.icns"
    rm -rf "$ICONSET"
fi

if [ -f "../res/StatusBarIcon.png" ]; then
    cp ../res/StatusBarIcon.png "$RESOURCES_DIR/"
    cp ../res/StatusBarIcon@2x.png "$RESOURCES_DIR/"
fi

echo "==> Done! App bundle created at macos/build/CopyHeap.app"

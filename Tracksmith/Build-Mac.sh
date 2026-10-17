#!/bin/bash
# Build a local development app on the Mac that will run it.
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -s)" != Darwin ]]; then
  echo "Run this script on macOS. Windows cannot produce the native Mac app." >&2; exit 1
fi
for tool in xcrun cmake node npm; do
  if ! command -v "$tool" >/dev/null; then
    echo "Missing $tool. Install Xcode Command Line Tools, CMake and Node.js 22.12+ first." >&2; exit 1
  fi
done
xcrun --find clang++ >/dev/null
architecture="${TRACKSMITH_MAC_ARCH:-$(uname -m)}"
case "$architecture" in arm64|x86_64) ;; *) echo "Use arm64 (M-series) or x86_64 (Intel)." >&2; exit 1;; esac
node -e 'const [a,b]=process.versions.node.split(".").map(Number);if(a<22||(a===22&&b<12)){console.error("Node.js 22.12 or newer is required");process.exit(1)}'
(cd ui && npm ci && npm run build)
build_dir="${TRACKSMITH_MAC_BUILD_DIR:-$PWD/build-mac-$architecture}"
cmake -S . -B "$build_dir" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$architecture" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DTRACKSMITH_ENABLE_ASIO=OFF \
  -DTRACKSMITH_BUILD_STOCK_PLUGINS=ON -DTRACKSMITH_STUDIO_TESTS=ON \
  -DTRACKSMITH_TONE3000_TESTS=OFF
cmake --build "$build_dir" --target Tracksmith StudioSessionTests StudioClipTests StudioMixerTests --parallel "${TRACKSMITH_BUILD_JOBS:-2}"
# Core non-GUI tests do not need a recording device or third-party plugins.
"$build_dir/StudioSessionTests_artefacts/Release/StudioSessionTests"
"$build_dir/StudioClipTests"
"$build_dir/StudioMixerTests"
app="$build_dir/Tracksmith_artefacts/Release/Tracksmith.app"
test -x "$app/Contents/MacOS/Tracksmith"
lipo -info "$app/Contents/MacOS/Tracksmith"
# Include the native dependency licenses with the development app.
notices="$app/Contents/Resources/ThirdPartyNotices"
mkdir -p "$notices"
cp t3k/LICENSE "$notices/TONE3000-LICENSE.txt"
cp t3k/TRACKSMITH-UPSTREAM.md "$notices/TONE3000-SOURCE.md"
cp ../BUILD30-TONE3000.md "$notices/TONE3000-INTEGRATION.md"
# Copy source license notices while preserving paths and avoiding model content.
while IFS= read -r license; do
  relative="${license#t3k/}"
  mkdir -p "$notices/TONE3000/$(dirname "$relative")"
  cp "$license" "$notices/TONE3000/$relative"
done < <(find t3k -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'NOTICE*' \))
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$app" "$build_dir/Tracksmith-Mac-$architecture.zip"
printf '\nBuilt: %s\nDevelopment ZIP: %s\n' "$app" "$build_dir/Tracksmith-Mac-$architecture.zip"
echo "This is a local development build; launch, recording, plugin loading and signing/notarization are not verified by compilation."

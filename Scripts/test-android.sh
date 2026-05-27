#!/usr/bin/env bash
# Package the TenjinSample app for Android, install on a connected device,
# launch it, and tail logcat filtered for Tenjin output. Useful for fast
# round-trip testing.
#
# Usage:
#   ./Scripts/test-android.sh
#   ./Scripts/test-android.sh --no-package        # skip build, only install/launch
#   ./Scripts/test-android.sh --device <serial>   # target a specific adb device
#
# Requirements:
#   - UE_ROOT env var pointing at your Unreal install
#     (e.g. /Users/Shared/Epic\ Games/UE_5.3). Falls back to common defaults.
#   - Android SDK paths configured in <Project>/Config/DefaultEngine.ini OR
#     via the editor's Android SDK page.
#   - A device with USB debugging enabled, or an emulator running.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${REPO_ROOT}/Sample/TenjinSample/TenjinSample.uproject"
PACKAGE_NAME="com.tenjin.unreal.sample"
ANDROID_ACTIVITY="${PACKAGE_NAME}/com.epicgames.unreal.GameActivity"

DEVICE_FLAG=""
DO_PACKAGE=1
for arg in "$@"; do
	case "$arg" in
		--no-package) DO_PACKAGE=0 ;;
		--device) shift; DEVICE_FLAG="-s $1"; shift || true ;;
		--help|-h) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	esac
done

if [[ -z "${UE_ROOT:-}" ]]; then
	for candidate in \
		"/Users/Shared/Epic Games/UE_5.7" \
		"/Users/Shared/Epic Games/UE_5.6" \
		"/Users/Shared/Epic Games/UE_5.5" \
		"/Users/Shared/Epic Games/UE_5.4" \
		"/Users/Shared/Epic Games/UE_5.3"; do
		if [[ -d "$candidate" ]]; then UE_ROOT="$candidate"; break; fi
	done
fi

# adb usually sits in ~/Library/Android/sdk/platform-tools — add it to PATH
# if it's not already there.
if ! command -v adb >/dev/null; then
	for candidate in \
		"$HOME/Library/Android/sdk/platform-tools" \
		"$HOME/Android/Sdk/platform-tools" \
		"${ANDROID_HOME:-}/platform-tools"; do
		if [[ -x "$candidate/adb" ]]; then PATH="$candidate:$PATH"; break; fi
	done
fi
if [[ -z "${UE_ROOT:-}" ]]; then
	echo "Error: set UE_ROOT to your Unreal Engine install (e.g. /Users/Shared/Epic\ Games/UE_5.3)." >&2
	exit 1
fi

UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
[[ -x "$UAT" ]] || { echo "Error: RunUAT.sh not found at $UAT"; exit 1; }

if [[ $DO_PACKAGE -eq 1 ]]; then
	STAGING="$REPO_ROOT/Sample/TenjinSample/Saved/StagedBuilds/Android_ASTC"
	echo "==> Packaging Android (this takes ~5-15 min on a clean build)..."
	rm -rf "$STAGING"
	"$UAT" BuildCookRun \
		-project="$PROJECT" \
		-platform=Android \
		-cookflavor=ASTC \
		-clientconfig=Development \
		-build -cook -stage -package -pak \
		-archive -archivedirectory="$REPO_ROOT/Sample/TenjinSample/Saved/Archive"
fi

APK="$(find "$REPO_ROOT/Sample/TenjinSample/Binaries/Android" "$REPO_ROOT/Sample/TenjinSample/Saved/Archive" \
		-name '*.apk' -not -name '*-symbols.apk' -print 2>/dev/null | head -n 1)"
if [[ -z "$APK" ]]; then
	echo "Error: no APK found. Check the package log above." >&2
	exit 2
fi
echo "==> APK: $APK"

command -v adb >/dev/null || { echo "Error: adb not on PATH. Add Android SDK platform-tools to PATH."; exit 1; }
adb $DEVICE_FLAG install -r "$APK"
adb $DEVICE_FLAG shell am start -n "$ANDROID_ACTIVITY"

echo "==> Tailing logcat (Ctrl-C to stop)..."
adb $DEVICE_FLAG logcat -c
exec adb $DEVICE_FLAG logcat \
	TenjinSDKBridge:V TenjinSDK:V Tenjin:V \
	LogTenjin:V LogTemp:V UE:V \
	AndroidRuntime:E *:S

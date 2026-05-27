#!/usr/bin/env bash
# Package the TenjinSample app for iOS, and optionally deploy + run it on a
# tethered device.
#
# UE 5.x uses the "Modern Xcode" workflow: there is NO separate iOS .xcodeproj
# to open and Run — UnrealBuildTool drives signing and packaging directly and
# produces a signed .ipa. (The only .xcodeproj files UE generates are the
# "(Mac)" host-editor projects; opening those just gives you the Mac build,
# which is why this script no longer calls `open` on anything.)
#
# Before first use, configure signing. With UE 5.x "Modern Xcode" this lives
# in Config/DefaultEngine.ini under [/Script/MacTargetPlatform.XcodeProjectSettings]
# (NOT the IOSRuntimeSettings section, which is the legacy Xcode path):
#   [/Script/MacTargetPlatform.XcodeProjectSettings]
#   bUseAutomaticCodeSigning=True
#   CodeSigningTeam=<your 10-char Apple Team ID>
#   CodeSigningPrefix=com.yourcompany
# The bundle id becomes CodeSigningPrefix + "." + project name.
# After changing it, delete Intermediate/ProjectFilesIOS so the .xcconfig
# regenerates.
#
# Usage:
#   ./Scripts/test-ios.sh                       # package a .ipa (no device needed)
#   ./Scripts/test-ios.sh --deploy              # install + launch on a tethered device
#   ./Scripts/test-ios.sh --deploy --device <UDID>
#                                               # explicit device (hardware UDID,
#                                               # 8-4-12 hex format from
#                                               # `xcrun devicectl list devices --json-output -`).
#                                               # Auto-detected if omitted.
#   ./Scripts/test-ios.sh --no-package          # skip build, just print next steps
#
# Requirements:
#   - UE_ROOT env var, or a default UE install in /Users/Shared/Epic Games/.
#   - The iOS engine component installed (Epic Games Launcher -> UE -> Options).
#   - TenjinSDK.xcframework staged in TenjinSDK/ThirdParty/iOS/
#     (run Scripts/install-ios-spm.sh first).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${REPO_ROOT}/Sample/TenjinSample/TenjinSample.uproject"
XCFRAMEWORK="${REPO_ROOT}/TenjinSDK/ThirdParty/iOS/TenjinSDK.xcframework"
ARCHIVE_DIR="${REPO_ROOT}/Sample/TenjinSample/Saved/Archive"

DO_PACKAGE=1
DO_DEPLOY=0
IOS_DEVICE_ID="${IOS_DEVICE_ID:-}"
while [[ $# -gt 0 ]]; do
	case "$1" in
		--no-package) DO_PACKAGE=0; shift ;;
		--deploy)     DO_DEPLOY=1; shift ;;
		--device)     IOS_DEVICE_ID="$2"; shift 2 ;;
		--help|-h)    grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) echo "Unknown arg: $1" >&2; exit 2 ;;
	esac
done

if [[ ! -d "$XCFRAMEWORK" ]]; then
	echo "Error: $XCFRAMEWORK not found." >&2
	echo "       Run ./Scripts/install-ios-spm.sh first to fetch it via SPM." >&2
	exit 1
fi

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
if [[ -z "${UE_ROOT:-}" ]]; then
	echo "Error: set UE_ROOT to your Unreal Engine install." >&2
	exit 1
fi

UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
[[ -x "$UAT" ]] || { echo "Error: RunUAT.sh not found at $UAT"; exit 1; }

# Auto-detect a connected device when deploying (auto-pick the first connected
# iOS device unless --device / IOS_DEVICE_ID was provided).
if [[ $DO_DEPLOY -eq 1 && -z "$IOS_DEVICE_ID" ]]; then
	IOS_DEVICE_ID="$(xcrun devicectl list devices --json-output - 2>/dev/null \
		| /usr/bin/python3 -c '
import json, sys
try:
    data = json.load(sys.stdin)
    for d in data.get("result", {}).get("devices", []):
        hw = d.get("hardwareProperties", {})
        conn = d.get("connectionProperties", {})
        if hw.get("platform") == "iOS" and conn.get("tunnelState") == "connected":
            print(hw.get("udid", ""))
            sys.exit(0)
except Exception:
    pass
' 2>/dev/null || true)"
fi
if [[ $DO_DEPLOY -eq 1 && -z "$IOS_DEVICE_ID" ]]; then
	echo "Error: --deploy requested but no connected iOS device found." >&2
	echo "       Plug the device in and unlock it, or pass --device <UDID>." >&2
	echo "       List devices: xcrun devicectl list devices --json-output -" >&2
	exit 3
fi

if [[ $DO_PACKAGE -eq 1 ]]; then
	UAT_ARGS=(
		BuildCookRun
		-project="$PROJECT"
		-platform=IOS
		-clientconfig=Development
		-build -cook -stage -pak -package
		-archive -archivedirectory="$ARCHIVE_DIR"
	)
	if [[ $DO_DEPLOY -eq 1 ]]; then
		UAT_ARGS+=( -deploy -run "-device=IOS@$IOS_DEVICE_ID" )
		echo "==> Deploy + run on iOS device: $IOS_DEVICE_ID"
	fi
	echo "==> Packaging iOS (first clean build ~10-15 min)..."
	"$UAT" "${UAT_ARGS[@]}"
fi

IPA="$(find "$ARCHIVE_DIR" "$REPO_ROOT/Sample/TenjinSample/Binaries/IOS" \
	-name '*.ipa' -print 2>/dev/null | head -n 1)"

echo
if [[ -n "$IPA" ]]; then
	echo "==> Packaged: $IPA"
else
	echo "==> No .ipa located. If the package step failed, it is almost always"
	echo "    signing — configure it in Project Settings -> Platforms -> iOS."
fi
echo
echo "To run on a device, pick one:"
echo "  1. Re-run this script with --deploy   (device plugged in, signing configured)"
echo "  2. Drag the .ipa onto your device in Xcode -> Window -> Devices and Simulators"
echo
echo "Do NOT use the editor's 'Launch' button for SDK testing: it streams cooked"
echo "content from your Mac's Zen server instead of baking it into the app, so the"
echo "app aborts on launch if the device can't reach the Mac. This script's"
echo "-pak -package build is fully self-contained and has no such dependency."

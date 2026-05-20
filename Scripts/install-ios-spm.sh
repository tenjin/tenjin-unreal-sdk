#!/usr/bin/env bash
# Resolve Tenjin's iOS SDK via Swift Package Manager and stage the resulting
# xcframework where Build.cs expects it. Run from anywhere; this script
# self-locates the repo root.
#
# Usage:
#   ./Scripts/install-ios-spm.sh                  # latest tagged version
#   ./Scripts/install-ios-spm.sh 1.16.1           # pin to a version
#   ./Scripts/install-ios-spm.sh --force          # re-resolve even if already present
#
# Why a script (and not Build.cs)?
#   UnrealBuildTool has no Swift Package primitive. The Build.cs links a
#   .xcframework from a fixed path; this script populates that path from SPM.
set -euo pipefail

REPO_URL="https://github.com/tenjin/tenjin-ios-spm.git"
PACKAGE_NAME="TenjinSDK"
DEFAULT_VERSION="1.16.1"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="${REPO_ROOT}/TenjinSDK/ThirdParty/iOS"
DEST_XCFRAMEWORK="${DEST_DIR}/TenjinSDK.xcframework"

VERSION=""
FORCE=0
for arg in "$@"; do
	case "$arg" in
		--force) FORCE=1 ;;
		--help|-h)
			grep '^#' "$0" | sed 's/^# \{0,1\}//'
			exit 0 ;;
		*) VERSION="$arg" ;;
	esac
done
VERSION="${VERSION:-$DEFAULT_VERSION}"

if [[ -d "$DEST_XCFRAMEWORK" && $FORCE -ne 1 ]]; then
	echo "[install-ios-spm] $DEST_XCFRAMEWORK already exists. Pass --force to refresh."
	exit 0
fi

command -v swift >/dev/null || { echo "Error: 'swift' not found. Install Xcode command-line tools."; exit 1; }

WORK_DIR="$(mktemp -d -t tenjin-spm-XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "[install-ios-spm] Resolving $REPO_URL @ $VERSION in $WORK_DIR ..."

cat > "$WORK_DIR/Package.swift" <<EOF
// swift-tools-version:5.7
import PackageDescription

let package = Package(
    name: "TenjinBootstrap",
    platforms: [.iOS(.v15)],
    dependencies: [
        .package(url: "$REPO_URL", from: "$VERSION"),
    ],
    targets: [
        .target(
            name: "TenjinBootstrap",
            dependencies: [
                .product(name: "$PACKAGE_NAME", package: "tenjin-ios-spm"),
            ],
            path: "Sources"
        ),
    ]
)
EOF

mkdir -p "$WORK_DIR/Sources/TenjinBootstrap"
touch "$WORK_DIR/Sources/TenjinBootstrap/Bootstrap.swift"

(cd "$WORK_DIR" && swift package resolve)

# SPM places binary-target artifacts under .build/artifacts/<pkg>/<target>/<name>.xcframework.
# The exact directory name varies by SPM version (some include the host arch),
# so we search rather than hard-code.
RESOLVED=$(find "$WORK_DIR/.build" -type d -name "${PACKAGE_NAME}.xcframework" -print -quit || true)

# Some versions of the Tenjin SPM repo ship the xcframework checked into the
# source tree rather than as a remote binary target. Fall back to checkouts/.
if [[ -z "$RESOLVED" ]]; then
	RESOLVED=$(find "$WORK_DIR/.build/checkouts" -type d -name "${PACKAGE_NAME}.xcframework" -print -quit || true)
fi

if [[ -z "$RESOLVED" || ! -d "$RESOLVED" ]]; then
	echo "Error: could not locate ${PACKAGE_NAME}.xcframework after swift package resolve." >&2
	echo "       Inspect $WORK_DIR/.build/ to debug." >&2
	exit 2
fi

mkdir -p "$DEST_DIR"
rm -rf "$DEST_XCFRAMEWORK"
cp -R "$RESOLVED" "$DEST_XCFRAMEWORK"

echo "[install-ios-spm] Installed $PACKAGE_NAME.xcframework ($VERSION) into:"
echo "                  $DEST_XCFRAMEWORK"

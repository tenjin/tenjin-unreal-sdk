# iOS TenjinSDK xcframework

`TenjinSDK.Build.cs` links `TenjinSDK.xcframework` from this folder via UE's
`PublicAdditionalFrameworks`. The framework is **committed to the repository**
so cloning + building the plugin is one step — no fetch needed.

## Current bundled version

The xcframework currently committed here is **TenjinSDK 1.17.0**.

## Bumping the version

1. Download the new `.xcframework` from
   [tenjin-ios-sdk releases](https://github.com/tenjin/tenjin-ios-sdk/releases)
   (or extract it from a CocoaPods install with `pod 'TenjinSDK', '<new-version>'`
   in a scratch project).
2. Replace the entire `TenjinSDK.xcframework/` folder here.
3. Verify the iOS build still compiles (`./Scripts/test-ios.sh` will do a
   full clean iOS package).
4. Bump the version reference in this README and in
   [`CHANGELOG.md`](../../../CHANGELOG.md).
5. Commit the new framework binary alongside the version-bump docs.

That's it — the plugin source already imports the eight ILRD category
headers; new networks bundled by a future TenjinSDK release will need a
small addition to `Private/iOS/TenjinSDK_iOS.mm` (the `#import` line) and
`Public/TenjinBPLibrary.h` (a new `EventAdImpressionXXX` method).

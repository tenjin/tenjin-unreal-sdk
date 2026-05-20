# iOS TenjinSDK xcframework

`TenjinSDK.Build.cs` links whatever sits at
`TenjinSDK/ThirdParty/iOS/TenjinSDK.xcframework`. There are three ways to
populate it; pick one.

## 1. Swift Package Manager (recommended)

```bash
./Scripts/install-ios-spm.sh             # latest pinned version
./Scripts/install-ios-spm.sh 1.16.1      # pin a specific version
./Scripts/install-ios-spm.sh --force     # re-resolve over an existing one
```

The script runs `swift package resolve` against
<https://github.com/tenjin/tenjin-ios-spm>, locates the resolved
`TenjinSDK.xcframework` in SPM's build cache, and copies it here.

Why a script rather than a `Build.cs` integration? UnrealBuildTool has no
`SwiftPackage` primitive — every iOS UE plugin that depends on a vendored
SDK uses either a script (Adjust) or a checked-in xcframework (AppsFlyer).
The script approach gets SPM's checksum verification and one-line version
bumps without fighting UBT.

## 2. Direct download

Grab the latest release from
<https://github.com/tenjin/tenjin-ios-sdk/releases>, extract
`TenjinSDK.xcframework/` here so the layout is:

```
TenjinSDK/ThirdParty/iOS/TenjinSDK.xcframework/
    Info.plist
    ios-arm64/...
    ios-arm64_x86_64-simulator/...
```

## 3. CocoaPods (manual)

Run `pod install` against this Podfile snippet in a scratch dir, then copy
`TenjinSDK.xcframework` out of `Pods/TenjinSDK/`:

```ruby
platform :ios, '15.0'
target 'Scratch' do
  use_frameworks!
  pod 'TenjinSDK', '1.16.1'
end
```

---

The folder is `.gitignore`d so the resolved framework is not committed.
Each developer (or CI job) re-runs the install step.

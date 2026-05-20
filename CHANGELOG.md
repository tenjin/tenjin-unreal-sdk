# Changelog

All notable changes to the Tenjin Unreal Engine SDK are documented here.
This project follows [semantic versioning](https://semver.org/).

## [1.0.0] — Unreleased

First release.

### Plugin
- Public `UTenjinBPLibrary` (Blueprint + C++) with 37 static methods mirroring
  the React Native / Unity / Flutter SDKs: initialize/connect, custom events,
  IAP transactions (iOS + Android), StoreKit 2 subscriptions, attribution
  info, deep-link handler, user profile / LiveOps metrics, customer + analytics
  installation IDs, opt-in/opt-out (CMP, Google DMA), SKAdNetwork postbacks,
  and ILRD passthroughs for AppLovin / IronSource / AdMob / TopOn / HyperBid
  / TradPlus.
- `UTenjinDelegates` singleton with `BlueprintAssignable` multicast events
  for deep-link payloads and attribution updates.
- `UTenjinSettings` (`UDeveloperSettings`) — Project-Settings page for API
  key, ATT prompt copy, Android app store, and auto-initialize toggle.

### Native dependencies
- **iOS**: TenjinSDK `1.16.1` via Swift Package Manager
  ([tenjin-ios-spm](https://github.com/tenjin/tenjin-ios-spm)). Resolved by
  `Scripts/install-ios-spm.sh` into `TenjinSDK/ThirdParty/iOS/`.
- **Android**: `com.tenjin:android-sdk:1.18.0` from Maven Central, injected
  via UPL XML at package time.

### Tooling
- `Scripts/install-ios-spm.sh` — fetches the iOS xcframework via SPM.
- `Scripts/test-ios.sh` — packages and (optionally) deploys + runs on a
  tethered device with live console streaming. Auto-detects the device's
  hardware UDID via `xcrun devicectl`.
- `Scripts/test-android.sh` — packages, installs, launches, and tails a
  Tenjin-filtered `logcat`. Auto-finds `adb` in the standard Android SDK
  location.

### Sample
- `Sample/TenjinSample/` — UE 5.7 C++ project demonstrating every SDK
  method via an on-screen Slate panel. iOS / Android only; the Mac editor
  remains compilable for development (calls are no-ops on desktop).

### Engine support
- Primary: UE 5.3 – 5.7.
- Best-effort: UE 5.0 – 5.2.
- Not supported: UE 4.27.

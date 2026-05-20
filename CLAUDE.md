# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Unreal Engine plugin for [Tenjin](https://www.tenjin.com) mobile marketing
analytics. The plugin wraps the native Tenjin iOS and Android SDKs and
exposes them as a `UBlueprintFunctionLibrary` (`UTenjinBPLibrary`) callable
from both C++ and Blueprints.

The repo holds two things:

- `TenjinSDK/` — the standalone plugin (drop into `<project>/Plugins/`)
- `Sample/TenjinSample/` — a tiny UE 5.3 sample app that drives every public
  method from an on-screen Slate panel. `Sample/TenjinSample/Plugins/TenjinSDK`
  is a symlink to the canonical plugin at the repo root.

## Architecture

### Plugin layout
```
TenjinSDK/
├── TenjinSDK.uplugin
├── Source/TenjinSDK/
│   ├── TenjinSDK.Build.cs            # adds xcframework + UPL XMLs
│   ├── TenjinSDK_UPL_iOS.xml         # ATT + SKAdNetwork plist entries
│   ├── TenjinSDK_UPL_Android.xml     # Maven dep + Java helper installation
│   ├── Public/
│   │   ├── TenjinSDK.h               # IModuleInterface
│   │   └── TenjinBPLibrary.h         # public BP/C++ API
│   └── Private/
│       ├── TenjinSDK.cpp
│       ├── TenjinBPLibrary.cpp       # dispatcher: PLATFORM_IOS / PLATFORM_ANDROID
│       ├── iOS/
│       │   ├── TenjinSDK_iOS.h
│       │   └── TenjinSDK_iOS.mm      # Objective-C++ — talks to TenjinSDK pod
│       └── Android/
│           └── TenjinSDKBridge.java  # Java helper — wraps com.tenjin.android.TenjinSDK
└── ThirdParty/iOS/                   # TenjinSDK.xcframework drop-in
```

### Native bridge pattern

1. `UTenjinBPLibrary` is a thin, platform-agnostic dispatcher. Every method
   ends in either `#if PLATFORM_IOS / TenjinIOS::Foo(...)` or
   `#if PLATFORM_ANDROID / JNI call into TenjinSDKBridge.foo(...)`. Editor
   and desktop targets compile to no-ops.

2. **iOS** — `TenjinSDK_iOS.mm` is Objective-C++ delegating to the
   `TenjinSDK` pod that lives in `ThirdParty/iOS/TenjinSDK.xcframework`. The
   framework must be downloaded into that folder before packaging an iOS
   build (see `ThirdParty/iOS/README.md`).

3. **Android** — `TenjinSDKBridge.java` owns a single static `TenjinSDK`
   instance. UPL XML (`TenjinSDK_UPL_Android.xml`) copies that Java file
   into the packaged APK at build time and adds a Maven dependency on
   `com.tenjin:android-sdk` so the Tenjin JAR ships in the APK. C++ calls
   the bridge via JNI; the activity reference is obtained through
   `AndroidJavaEnv::GetGameActivityThis()` and passed at initialization.

### Async results

Methods that need callbacks (`GetAttributionInfo`, `GetCustomerUserId`,
`GetUserProfileDictionary`, `SubscriptionWithStoreKit`) take a
`DECLARE_DYNAMIC_DELEGATE_*` parameter and re-dispatch the result onto the
game thread via `AsyncTask(ENamedThreads::GameThread, ...)` before invoking.

## Common commands

There are no scripts in this repo. Standard Unreal workflows apply:

- **Generate project files** — run `Build.sh -projectfiles` on macOS or
  `GenerateProjectFiles.bat` on Windows from your engine install, against
  `Sample/TenjinSample/TenjinSample.uproject`.
- **Build the sample for iOS** — open `TenjinSample.uproject` in the editor,
  then `Platforms → iOS → Package Project`. Requires Xcode and a valid
  provisioning profile.
- **Build the sample for Android** — same flow under `Platforms → Android`.
  Requires Android Studio SDK paths configured in `Edit → Project Settings →
  Platforms → Android SDK`.

## Decisions taken from UNREAL-RESEARCH.md

- **LoadingPhase = Default** in `.uplugin`. `PostConfigInit` was tried first
  (research suggests attribution SDKs load early) but `StartupModule()` reads
  `GetDefault<UTenjinSettings>()`, and the UObject system isn't up that early —
  it crashed in `UClass::InternalCreateDefaultObjectWrapper`. Auto-initialize
  is instead deferred to an `FCoreDelegates::OnPostEngineInit` handler
  registered from `StartupModule()`.
- **Module `PlatformAllowList: ["IOS", "Android"]`** in the `.uplugin`. This
  is safe — `PlatformAllowList` restricts what's compiled into a packaged
  standalone game, not what the editor can build, so the Mac/Win/Linux editor
  still compiles + loads the plugin (verified). The UE4-era `WhitelistPlatforms`
  key is not shipped — the plugin targets UE 5.3+.
- **ProGuard `-keep` rules** are shipped in `TenjinSDK_UPL_Android.xml`. R8
  would otherwise strip the JNI-reachable bridge class and Tenjin's listener
  inner classes in shipping builds (the bug class that bit AppsFlyer's plugin
  in production).
- **`UDeveloperSettings`-backed `UTenjinSettings`** exposes SdkKey / ATT
  description / Android app-store / auto-init under
  *Project Settings → Plugins → Tenjin SDK*. Avoids hardcoded keys in game
  code; values land in `DefaultEngine.ini`.
- **`UTenjinDelegates` singleton** hosts `BlueprintAssignable` multicast
  delegates for deeplink + attribution lifecycle events. Per-call
  `FTenjin*Delegate` callbacks are still used for one-shot fetches
  (Adjust's dual-pattern API).
- **`AsyncTask(ENamedThreads::GameThread, ...)`** marshals every native
  callback onto the game thread before invoking BP delegates. Avoids GC
  races and crashes Singular's plugin warns its users about.
- **`AndroidJavaEnv::GetGameActivityThis()`** is used everywhere instead of
  `FindClass("com/epicgames/ue4/GameActivity")` — that string broke when UE5
  renamed the package to `com.epicgames.unreal`.

## iOS gotchas (learned the hard way)

- **Code signing under Modern Xcode** (`bUseModernXcode=true`, UE 5.x default)
  reads from `[/Script/MacTargetPlatform.XcodeProjectSettings]` —
  `CodeSigningTeam`, `CodeSigningPrefix`, `bUseAutomaticCodeSigning`. The
  `[/Script/IOSRuntimeSettings.IOSRuntimeSettings]` `IOSTeamID` /
  `bAutomaticSigning` keys are the *legacy* Xcode path and are ignored. UBT
  bakes these into `Intermediate/ProjectFilesIOS/XcconfigsIOS/*.xcconfig`;
  if you change them, delete `Intermediate/ProjectFilesIOS/` to force a
  regenerate (a plain `Build.sh` reuses the stale xcconfig).
- **Test on-device with a packaged build, not the editor "Launch" button.**
  Editor Launch (and `BuildCookRun` without `-pak -package`) produces a
  streaming build — content stays on the Mac's Zen server and the device
  pulls it over the network on launch. If the device can't reach the Mac
  (different subnet, firewall, Zen port 8558 closed) the app aborts with
  signal 6 before any Tenjin code runs. `Scripts/test-ios.sh` uses
  `-pak -package` so the `.app` is fully self-contained.
- **The native iOS API does not match the React Native SDK's selectors
  one-for-one.** Verified against TenjinSDK 1.16.1 headers:
  `connectWithDeferredDeeplink:` takes an `NSURL*`; the ILRD passthroughs
  live in per-network category headers (`TenjinSDK+AdMobILRD.h` etc.) that
  must be `#import`ed separately; `registerDeepLinkHandler:` is an instance
  method on `[TenjinSDK sharedInstance]`; `subscriptionWithStoreKitForProductId:`
  is iOS 16+ and fire-and-forget (no completion handler). Always check the
  staged xcframework's `Headers/` before adding an iOS bridge call.

## Conscious divergences from the research

- **No Adjust-style `USTRUCT` per feature.** Flat-parameter methods preserve
  one-to-one parity with our Unity/RN/Flutter SDKs. Re-shaping the 36-method
  surface to typed structs is a v2 task if studios actually ask.
- **No Helium/Chartboost ILRD passthroughs in v1.** The native Tenjin SDKs
  don't expose them either — would be a parallel addition there first.
- **UE 4.27 is not a primary target.** Research recommends best-effort; we
  ship UE 5.3+ as primary and don't compile-time gate for 4.27.
- **Subscriptions on Android remain a stub.** Tenjin's native Android
  subscription path is "coming soon"; we keep the symmetric method but it
  no-ops until the native side lands.

## Conventions

- The public API in `TenjinBPLibrary.h` mirrors our React Native SDK
  one-for-one (see `/Users/diego/Documents/Development/tenjin-react-native-sdk`
  for the canonical method list). When adding a new method:
  1. Add it to `TenjinBPLibrary.h` with appropriate `UFUNCTION` metadata.
  2. Implement the dispatcher in `TenjinBPLibrary.cpp` with platform
     branches.
  3. Add the iOS impl to `TenjinSDK_iOS.h/.mm`.
  4. Add the Android impl to `TenjinSDKBridge.java` and verify the JNI
     signature in the dispatcher.
- Editor/desktop targets must remain compilable; they should fall through
  to no-ops or log lines.
- Do not bypass `Sample/TenjinSample/Plugins/TenjinSDK` — it is a symlink to
  the canonical plugin so changes flow back to the source of truth.
- Naming: keep parity with the RN SDK (e.g. `EventWithName`, not `LogEvent`).
- The Android `_activity` reflection trick is intentionally NOT used; we
  pass the activity from C++ via `AndroidJavaEnv::GetGameActivityThis()`.

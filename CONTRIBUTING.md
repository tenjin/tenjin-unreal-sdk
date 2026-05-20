# Contributing

This document explains the plugin's internals — read it if you're adding
methods, debugging the bridge layer, or bumping native SDK versions.
End-user integration docs live in [README.md](README.md).

## Table of contents

* [Repository layout](#layout)
* [Architecture](#architecture)
* [Dependency management](#deps)
* [Packaging and on-device testing](#packaging)
* [Adding a new SDK method](#adding-method)
* [Known dependency conflicts](#conflicts)
* [Releasing](#releasing)

## <a id="layout"></a>Repository layout

```
tenjin-unreal-sdk/
├── README.md                       # user-facing docs
├── CONTRIBUTING.md                 # this file
├── CLAUDE.md                       # guidance for AI coding agents
├── CHANGELOG.md                    # version history
├── LICENSE                         # MIT
├── Scripts/                        # install + test helpers
│   ├── test-android.sh
│   └── test-ios.sh
├── TenjinSDK/                      # The plugin — drop into <Project>/Plugins/
│   ├── TenjinSDK.uplugin
│   ├── ThirdParty/iOS/             # TenjinSDK.xcframework (gitignored)
│   └── Source/TenjinSDK/
│       ├── TenjinSDK.Build.cs
│       ├── TenjinSDK_UPL_iOS.xml
│       ├── TenjinSDK_UPL_Android.xml
│       ├── Public/                 # API headers (Blueprint-visible)
│       │   ├── TenjinSDK.h
│       │   ├── TenjinBPLibrary.h
│       │   ├── TenjinDelegates.h
│       │   └── TenjinSettings.h
│       └── Private/
│           ├── TenjinSDK.cpp
│           ├── TenjinBPLibrary.cpp # platform dispatcher
│           ├── TenjinDelegates.cpp
│           ├── TenjinSettings.cpp
│           ├── iOS/
│           │   ├── TenjinSDK_iOS.h
│           │   └── TenjinSDK_iOS.mm
│           └── Android/
│               └── TenjinSDKBridge.java
└── Sample/TenjinSample/            # Working example project
    ├── TenjinSample.uproject
    ├── Source/...
    └── Plugins/TenjinSDK -> ../../../TenjinSDK    (symlink to canonical plugin)
```

The sample app's `Plugins/TenjinSDK` is a symlink to the repo-root plugin, so
edits to the canonical plugin source flow into the sample without copying.

## <a id="architecture"></a>Architecture

### Native bridge pattern

1. **`UTenjinBPLibrary`** is a thin, platform-agnostic dispatcher. Every
   method is `static`, exposed to Blueprints via `UFUNCTION`, and ends in a
   `#if PLATFORM_IOS / TenjinIOS::Foo(...)` or `#if PLATFORM_ANDROID / JNI
   call into TenjinSDKBridge.foo(...)` branch. Editor and desktop targets
   compile to no-ops.

2. **iOS** — `Private/iOS/TenjinSDK_iOS.mm` is Objective-C++ delegating to
   the `TenjinSDK` pod that lives in `ThirdParty/iOS/TenjinSDK.xcframework`.

3. **Android** — `Private/Android/TenjinSDKBridge.java` owns a single static
   `TenjinSDK` instance. UPL XML (`TenjinSDK_UPL_Android.xml`) copies that
   Java file into the packaged APK at build time and injects the Maven
   dependency on `com.tenjin:android-sdk` so the Tenjin JAR ships in the
   APK. C++ calls the bridge via JNI; the activity reference is obtained
   through `AndroidJavaEnv::GetGameActivityThis()` and passed at
   initialization.

### Threading and callbacks

Methods that need results (`GetAttributionInfo`, `GetCustomerUserId`,
`GetUserProfileDictionary`, `SubscriptionWithStoreKit`) accept a
`DECLARE_DYNAMIC_DELEGATE_*` parameter and re-dispatch the result onto the
game thread via `AsyncTask(ENamedThreads::GameThread, ...)` before invoking.
**Never broadcast a `BlueprintAssignable` delegate from a background
thread** — UObject mutation off the game thread crashes.

For lifecycle events (deeplinks, attribution-info changes) the plugin uses
a single `UTenjinDelegates` singleton (`Public/TenjinDelegates.h`) with
`BlueprintAssignable` multicast delegates. The singleton is held in
`RF_MarkAsRootSet` to survive GC across async callbacks.

### Loading phase

The plugin's `.uplugin` declares `LoadingPhase: Default`. An earlier phase
(`PostConfigInit`) was tried first — the research suggests attribution SDKs
load early — but `FTenjinSDKModule::StartupModule()` reads
`GetDefault<UTenjinSettings>()`, and the UObject system / CDOs do not exist
yet at `PostConfigInit`, so it crashed in `UClass::InternalCreateDefaultObjectWrapper`.
`Default` is the earliest phase at which a module can safely touch UObjects
in `StartupModule`.

To keep auto-initialize as early as possible without that fragility,
`StartupModule()` registers an `FCoreDelegates::OnPostEngineInit` handler
rather than doing the work inline; that fires once the engine is fully up
but still well before any gameplay. A plugin that needs to register native
lifecycle observers truly early would do that in the iOS `.mm` /
Android UPL layer, not by moving the UE module's `LoadingPhase`.

The module declares `PlatformAllowList: ["IOS", "Android"]`. This is safe
even though the editor runs on Mac/Windows/Linux: `PlatformAllowList`
restricts what gets compiled into a *packaged standalone game*, not what the
*editor* can build — the editor still compiles runtime modules for any
platform it can cook for. Verified: the Mac editor builds, loads, and mounts
the plugin with this list. (We don't ship the UE4-era `WhitelistPlatforms`
key — the plugin targets UE 5.3+.)

## <a id="deps"></a>Dependency management

Unreal Engine has no first-class package manager. Each layer handles a
different slice:

| Concern | Mechanism | Where |
|---------|-----------|-------|
| UE plugin enable/disable | `.uplugin` JSON | `TenjinSDK/TenjinSDK.uplugin` |
| C++ module deps | `Build.cs` | `Source/TenjinSDK/TenjinSDK.Build.cs` |
| iOS native framework | `PublicAdditionalFrameworks` reference to a `.xcframework` | `Build.cs` reads `ThirdParty/iOS/TenjinSDK.xcframework` |
| iOS plist + SKAdNetwork entries | UPL XML | `TenjinSDK_UPL_iOS.xml` |
| Android Maven deps | UPL XML `<buildGradleAdditions>` | `TenjinSDK_UPL_Android.xml` |
| Android manifest + ProGuard | UPL XML | `TenjinSDK_UPL_Android.xml` |
| Android Java helper | UPL `<resourceCopies>` from `Private/Android/` | `TenjinSDK_UPL_Android.xml` |

### Bumping the Android SDK version

Edit the Maven coordinate in `TenjinSDK/Source/TenjinSDK/TenjinSDK_UPL_Android.xml`:

```xml
<buildGradleAdditions>
  <insert>
    dependencies {
      implementation 'com.tenjin:android-sdk:1.18.0'
    }
  </insert>
</buildGradleAdditions>
```

Next package-build will re-resolve from Maven Central — no other change
needed.

### Bumping the iOS SDK version

The iOS xcframework is committed to the repo at
`TenjinSDK/ThirdParty/iOS/TenjinSDK.xcframework/` so cloning + building is
one step (no fetch script). To bump:

1. Download the new `TenjinSDK.xcframework` from
   [tenjin-ios-sdk releases][ios-releases] (or extract it from a CocoaPods
   install with `pod 'TenjinSDK', '<new-version>'` in a scratch project).
2. Replace the entire `TenjinSDK.xcframework/` folder.
3. Run `./Scripts/test-ios.sh` to verify the iOS package still builds and
   signs cleanly.
4. Bump the version string in
   [`TenjinSDK/ThirdParty/iOS/README.md`](TenjinSDK/ThirdParty/iOS/README.md)
   and [`CHANGELOG.md`](CHANGELOG.md).
5. Commit the new framework binary alongside the doc changes. `.gitattributes`
   already marks `*.xcframework/**` as binary so git won't try to LF-normalize.

Why static vs SPM? Earlier the iOS dep was resolved via
[`tenjin-ios-spm`](https://github.com/tenjin/tenjin-ios-spm), but that repo
lags behind the main iOS SDK's release cadence (latest SPM tag at time of
writing is `1.16.1` while the iOS SDK has shipped `1.17.0+`). Committing
the framework lets us pin to any version that has a release on
[`tenjin-ios-sdk`](https://github.com/tenjin/tenjin-ios-sdk/releases); the
cost is a ~21 MB binary in the repo.

[ios-releases]: https://github.com/tenjin/tenjin-ios-sdk/releases

## <a id="packaging"></a>Packaging and on-device testing

Both helper scripts wrap UnrealBuildTool's `RunUAT BuildCookRun` with the
device/log glue UE doesn't ship. They auto-locate the engine under
`/Users/Shared/Epic Games/UE_5.7…5.3` (override with `UE_ROOT`) and default
to the `Development` client config.

### iOS — `Scripts/test-ios.sh`

```bash
./Scripts/test-ios.sh                      # package a signed .ipa (no device needed)
./Scripts/test-ios.sh --deploy             # also install + launch on a tethered device
./Scripts/test-ios.sh --deploy --device <hardware-UDID>
./Scripts/test-ios.sh --no-package         # skip the build, just print next steps
```

Packages with `-build -cook -stage -pak -package -archive`, so the resulting
`.app` is fully self-contained (no Zen content streaming). With `--deploy`
the script appends `-deploy -run -device=IOS@<UDID>`, which deploys via
`xcrun devicectl device process launch --terminate-existing --console`
— the `--console` flag streams the device log to your terminal until you
Ctrl-C. The UDID is auto-detected from
`xcrun devicectl list devices --json-output -` (filtering for
`platform == "iOS"` and `tunnelState == "connected"`); pass `--device` to
override.

Signing is configured in `Sample/TenjinSample/Config/DefaultEngine.ini`
under `[/Script/MacTargetPlatform.XcodeProjectSettings]` (Modern Xcode
workflow). After editing signing values, delete
`Sample/TenjinSample/Intermediate/ProjectFilesIOS/` so the generated
`.xcconfig` is rebuilt — a plain rebuild reuses the cached xcconfig.

### Android — `Scripts/test-android.sh`

```bash
./Scripts/test-android.sh                     # package, install, launch, tail logcat
./Scripts/test-android.sh --device <serial>   # pick a specific adb device
./Scripts/test-android.sh --no-package        # skip build, only install/launch
```

Packages with `-pak -package` (so content is baked into the APK and the
device doesn't depend on a Zen server), then `adb install -r`,
`adb shell am start -n com.tenjin.unreal.sample/com.epicgames.unreal.GameActivity`,
then `adb logcat` filtered to `TenjinSDKBridge:V TenjinSDK:V Tenjin:V
LogTenjin:V LogTemp:V UE:V AndroidRuntime:E *:S`. The script
prepends `~/Library/Android/sdk/platform-tools` (and a couple of fallbacks)
to PATH if `adb` isn't already there.

UE 5.7's Android build needs NDK `r27c` (= `27.2.12479018`), JDK 17, and
the `cmdline-tools/latest/bin/sdkmanager` binary; run
`Engine/Extras/Android/SetupAndroid.command` once with
`STUDIO_SDK_PATH=$HOME/Library/Android/sdk` to install/align all of those
and write `ANDROID_HOME`/`NDKROOT`/`JAVA_HOME` exports into your shell
profile.

## <a id="adding-method"></a>Adding a new SDK method

The API surface mirrors our React Native, Unity, and Flutter SDKs one-for-one.
When upstream adds a method:

1. **Declare it** in `Public/TenjinBPLibrary.h` with appropriate
   `UFUNCTION(BlueprintCallable, Category = "Tenjin|...")` metadata. For
   async results, declare a `DECLARE_DYNAMIC_DELEGATE_*` if no compatible
   one exists yet.

2. **Implement the dispatcher** in `Private/TenjinBPLibrary.cpp` with the
   `#if PLATFORM_IOS / #elif PLATFORM_ANDROID / #else` branches. Marshal
   async results to the game thread via `AsyncTask(ENamedThreads::GameThread, ...)`
   before invoking the delegate.

3. **Add the iOS impl** to `Private/iOS/TenjinSDK_iOS.h` (forward declare in
   the `TenjinIOS` namespace) and `TenjinSDK_iOS.mm` (Objective-C++
   delegating to the SDK class methods).

4. **Add the Android impl** to `Private/Android/TenjinSDKBridge.java` as a
   `public static` method. The JNI signature in the C++ dispatcher must
   match exactly — keep the parameter types simple (`String`, `int`,
   `double`, `boolean`, `String[]`). For callbacks back to C++, declare a
   `private static native` method in Java and define
   `Java_com_tenjin_unreal_TenjinSDKBridge_nativeFoo` in `TenjinBPLibrary.cpp`.

5. **Add a button** to `Sample/TenjinSample/Source/TenjinSample/Widgets/STenjinTestPanel.cpp`
   so the new method is exercised end-to-end on device.

6. **Document it** in `README.md` under the matching *Additional features*
   subsection. Show the `UFUNCTION` signature and a usage snippet.

### Conventions

* Keep method names verbatim from the RN / Unity SDKs (`EventWithName`, not
  `LogEvent`).
* Flat parameters — do **not** introduce `USTRUCT` wrappers (e.g.
  `FTenjinTransaction`) just for ergonomics. That breaks one-for-one parity
  with the other Tenjin SDKs.
* Editor / desktop targets must remain compilable. The default branch
  should be a no-op or a `UE_LOG` line, never an `unimplemented()`.
* Do not edit `Sample/TenjinSample/Plugins/TenjinSDK` — it is a symlink to
  the canonical plugin at the repo root. Edit the source of truth instead.

## <a id="conflicts"></a>Known dependency conflicts

### Android

* **Maven dedup** — the Tenjin Android SDK is pulled from Maven Central,
  so Gradle dedupes against host-app deps automatically. If another plugin
  embeds Tenjin via a vendored `.aar`, Gradle will report a `Duplicate
  class` error — remove the dupe `implementation` from the other plugin or
  pin both to the same version.
* **AndroidX + Jetifier** are assumed enabled. If the host project sets
  `android.useAndroidX=false`, enable it in `Config/AndroidEngine.ini`.
* **R8 stripping** — `TenjinSDK_UPL_Android.xml` emits `-keep` rules for
  the bridge class and the Tenjin SDK package. R8 would otherwise strip
  the JNI-reachable bridge class in shipping builds (the bug class that
  bit AppsFlyer's plugin in production).

### iOS

* The bundled `TenjinSDK.xcframework` ships its own `PrivacyInfo.xcprivacy`
  (iOS 17 privacy manifest). Keep the xcframework intact when packaging.
* If another plugin in the host project also embeds AppTrackingTransparency,
  UE5's Xcode project resolves it once — no action needed.

## <a id="releasing"></a>Releasing

1. Bump the version in `TenjinSDK/TenjinSDK.uplugin` (`Version` and
   `VersionName`).
2. Bump the native SDK versions:
   * Android: edit the `implementation 'com.tenjin:android-sdk:X.Y.Z'`
     line in `TenjinSDK/Source/TenjinSDK/TenjinSDK_UPL_Android.xml`.
   * iOS: replace `TenjinSDK/ThirdParty/iOS/TenjinSDK.xcframework/` with
     the new release (see `ThirdParty/iOS/README.md`) and verify a
     Sample build links.
3. Tag the release on GitHub matching the underlying native SDK
   `major.minor`.
4. Smoke-test the Sample app on at least one iOS and one Android device.

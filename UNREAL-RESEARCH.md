# Building a Tenjin Unreal plugin: feasibility, effort, and design

## Executive summary

**Recommendation: build it, but scope it as a defensive 6–8 engineer-week v1 and a thin ongoing-maintenance commitment — not a strategic bet.** A production-grade Tenjin Unreal Engine plugin covering init, custom events, IAP/transactions, attribution, deep linking, ad-revenue passthroughs, and GDPR/CCPA/ATT controls is **well-understood engineering** with two excellent open-source references (AppsFlyer and Adjust). The work is mostly mechanical glue between the existing Tenjin iOS/Android natives and Unreal's `UBlueprintFunctionLibrary` + UPL (Unreal Plugin Language) build system. **There are no novel research problems.** The hardest parts are not coding — they are (a) the cross-engine-version test matrix (UE 4.27 + UE 5.0–5.6), (b) deduping native dependencies against other plugins users have installed, and (c) keeping pace with App Store / Play Store / SDK churn.

**Effort estimate**: **6–8 engineer-weeks for v1** (one senior mobile SDK engineer who already knows Tenjin iOS/Android, with part-time help from a second engineer for QA and engine-version validation). Add **~0.25 FTE ongoing** to track Tenjin SDK releases, new Unreal versions, and toolchain churn. AppsFlyer ships ~12–18 releases per year on their Unreal plugin, which is the right pace expectation.

**Market read**: The addressable market is **real but narrow and off-ICP for Tenjin**. Unreal is ~5–10% of mobile games by title, concentrated in Korean MMORPGs (NCSoft, Netmarble, Wemade, Krafton, Kakao), Chinese open-world/shooter (Kuro Games' Wuthering Waves, Tencent's Arena Breakout, PUBG Mobile), and battle royales (Fortnite, PUBG, BGMI). These large publishers are **locked into AppsFlyer/Adjust/Singular enterprise contracts** and unlikely to switch on plugin parity alone. The accessible slice for Tenjin is the long tail of indie/mid-market Unreal mobile studios — a small but defensible niche. Treat the plugin as **(1) a churn shield for existing Tenjin customers diversifying into Unreal, (2) RFP feature-parity vs AppsFlyer/Adjust, and (3) a positioning hedge if UE5's mobile improvements pull more mid-tier studios in over 2027–28.** Do not expect it to drive material new revenue inside 12 months.

**Strategic call-outs**:
- **Ship MIT-licensed and GitHub-first** (`github.com/tenjin/tenjin-unreal-sdk`). Adjust's MIT license is the studio-friendly default; AppsFlyer's unlicensed proprietary stance is a soft negative. Fab/Marketplace is optional and none of the major MMPs use it.
- **Adopt Adjust's architecture, not AppsFlyer's.** Adjust uses typed `USTRUCT`s per feature, a clean `UTenjinDelegates` singleton for async events, dual Blueprint-delegate + C++ `TFunction` APIs, and ships explicit ProGuard `-keep` rules. AppsFlyer's monolithic `#if PLATFORM_IOS`/`#elif PLATFORM_ANDROID` mega-file and `TObjectIterator` callback fan-out are pragmatic but uglier, and they've shipped real bugs because of it (locale-dependent `NSNumberFormatter` dropping `af_revenue`, ProGuard strip-outs in shipping builds).
- **Resolve native deps via Maven (Android) and CocoaPods/embedded xcframework (iOS).** Embedding `.aar`s causes duplicate-class Gradle failures when users also have AppsFlyer/Firebase/AdMob. Tenjin's iOS SDK is already on CocoaPods and SPM; Android is on Maven Central — perfect for UPL `<buildGradleAdditions>` injection.
- **There is no existing Tenjin Unreal plugin worth treating as a baseline.** A single third-party paid Marketplace listing ("Tenjin Mobile Analytics" by Volihan Games / Andrey Kharitonov) exists, but it is **Android-only** by the author's own admission, unaffiliated with Tenjin, and has a single rating. Tenjin's FAQ acknowledges an "unofficial community plugin" without endorsing it. Building the official version is uncontested.

---

## 1. Unreal Engine plugin architecture for mobile

### 1.1 Plugin fundamentals

A mobile-SDK wrapper plugin in Unreal is a directory at `<Project>/Plugins/TenjinSDK/` (or `Engine/Plugins/Marketplace/...`) containing a JSON `.uplugin` manifest, one or more C++ modules under `Source/`, and per-module `Build.cs` files compiled by **UnrealBuildTool (UBT)**. The canonical layout used by AppsFlyer is:

```
TenjinSDK/
├── TenjinSDK.uplugin
├── Resources/Icon128.png
└── Source/
    └── TenjinSDK/
        ├── TenjinSDK.Build.cs
        ├── TenjinSDK_UPL_iOS.xml
        ├── TenjinSDK_UPL_Android.xml
        ├── Public/                  ← headers auto-exposed to dependents
        │   ├── TenjinSDKBlueprint.h
        │   └── TenjinSDKCallbacks.h
        └── Private/                  ← .cpp, .mm, vendored libs
            ├── TenjinSDKBlueprint.cpp
            └── ios/TenjinSDK.embeddedframework.zip
```

The `.uplugin` is a JSON manifest UBT reads at project load. The fields that matter for an MMP plugin:

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0.0",
  "FriendlyName": "Tenjin SDK",
  "Category": "Mobile",
  "CreatedBy": "Tenjin",
  "EnabledByDefault": true,
  "CanContainContent": false,
  "Modules": [
    {
      "Name": "TenjinSDK",
      "Type": "Runtime",
      "LoadingPhase": "PostConfigInit",
      "WhitelistPlatforms": [ "IOS", "Android" ],
      "PlatformAllowList":  [ "IOS", "Android" ]
    }
  ]
}
```

**Two critical gotchas in the manifest**: First, `WhitelistPlatforms` was renamed to `PlatformAllowList` in UE5; UE5 silently ignores the old key without error, and UE4 ignores the new one. Ship **both** to remain dual-compatible across 4.27 and 5.x. Second, `LoadingPhase` must be `PostConfigInit` or `PreDefault` for attribution SDKs — analytics and lifecycle observers need to register before `IOSAppDelegate.application:didFinishLaunchingWithOptions:` and `GameActivity.onCreate` complete, or you'll miss install events on first launch.

`Build.cs` is a C# file compiled per-module by UBT; it declares dependencies, additional frameworks, and the path to the UPL XML. The standard mobile-wrapper shape:

```csharp
public class TenjinSDK : ModuleRules
{
    public TenjinSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableObjCAutomaticReferenceCounting = true;

        PublicDependencyModuleNames.AddRange(new[] { "Core","CoreUObject","Engine" });

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicFrameworks.AddRange(new[] {
                "AdSupport", "AppTrackingTransparency", "AdServices",
                "StoreKit", "iAd", "SystemConfiguration", "CoreTelephony"
            });
            PublicAdditionalFrameworks.Add(new Framework("TenjinSDK",
                "../ThirdParty/iOS/TenjinSDK.embeddedframework.zip"));
            AdditionalPropertiesForReceipt.Add("IOSPlugin",
                Path.Combine(ModuleDirectory, "TenjinSDK_UPL_iOS.xml"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "Launch" });
            AdditionalPropertiesForReceipt.Add("AndroidPlugin",
                Path.Combine(ModuleDirectory, "TenjinSDK_UPL_Android.xml"));
        }
    }
}
```

`PublicAdditionalFrameworks` is how a vendored iOS `.framework` or `.xcframework` gets embedded — UBT unzips the archive into `Intermediate/IOS/...` and adds it to the Xcode link + embed phases. `AdditionalPropertiesForReceipt` with the magic keys `IOSPlugin` and `AndroidPlugin` is how UBT discovers UPL XML files.

### 1.2 iOS native bridging via UPL + Objective-C++

Unreal compiles `.mm` (Objective-C++) files transparently, so an iOS bridge is just C++ that imports `<TenjinSDK/TenjinSDK.h>` inside `#if PLATFORM_IOS`. The minimum useful UPL_iOS.xml looks like:

```xml
<root xmlns:android="http://schemas.android.com/apk/res/android">
  <iosPListUpdates>
    <key>NSUserTrackingUsageDescription</key>
    <string>Used to deliver a personalized ad experience.</string>
    <key>NSAdvertisingAttributionReportEndpoint</key>
    <string>https://tenjin-skan.com</string>
    <key>SKAdNetworkItems</key>
    <array>
      <dict><key>SKAdNetworkIdentifier</key><string>v9wttpbfk9.skadnetwork</string></dict>
      <!-- + AppLovin/IronSource/AdMob/Liftoff/Vungle SKAN IDs -->
    </array>
  </iosPListUpdates>
  <iosLinkLibraries>
    <Library name="AdSupport.framework" weak="false"/>
    <Library name="AppTrackingTransparency.framework" weak="true"/>
    <Library name="AdServices.framework" weak="true"/>
    <Library name="StoreKit.framework" weak="false"/>
  </iosLinkLibraries>
</root>
```

The bridge file pattern (this is a 40-line example showing the canonical patterns — dispatch to main, marshal to game thread):

```cpp
#if PLATFORM_IOS
#import <TenjinSDK/TenjinSDK.h>
#include "IOS/IOSAppDelegate.h"
#include "Async/Async.h"

void UTenjinSDKBlueprint::Connect()
{
    dispatch_async(dispatch_get_main_queue(), ^{ [TenjinSDK connect]; });
}

void UTenjinSDKBlueprint::GetAttributionInfo()
{
    [TenjinSDK getAttributionInfo:^(NSDictionary* info, NSError* err) {
        TMap<FString,FString> Out = NSDictToTMap(info);
        AsyncTask(ENamedThreads::GameThread, [Out]() {
            UTenjinSDKDelegates::Get()->OnAttributionInfo.Broadcast(Out);
        });
    }];
}
#endif
```

The two non-obvious details: **(1)** Objective-C delegate callbacks fire on whatever queue the SDK chose (often a background thread). You must marshal to the Unreal **game thread** before broadcasting a `BlueprintAssignable` delegate, otherwise you crash on UObject mutation. `AsyncTask(ENamedThreads::GameThread, lambda)` from `Async/Async.h` is the modern primitive; older plugins use `FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(..., ENamedThreads::GameThread)`. **(2)** Engine app lifecycle is accessed via `FIOSCoreDelegates::OnOpenURL` (multicast, for deep links) and `NSNotificationCenter` observers on `UIApplicationWillEnterForegroundNotification` (to call `[TenjinSDK connect]` on foreground). Unreal's iOS app delegate is `IOSAppDelegate` from `Runtime/ApplicationCore/Public/IOS/IOSAppDelegate.h` — don't subclass, observe via notifications.

Apple's iOS 17 **privacy manifest** requirement (effective spring 2024) adds one more step: ship `PrivacyInfo.xcprivacy` inside the framework and copy it via UPL `<iosCopyResources>`. Tenjin's xcframework already includes one; the plugin just needs to make sure it survives packaging.

### 1.3 Android native bridging via UPL + JNI

The Android story lives entirely in **UPL XML** processed by `UEDeployAndroid.cs`. UPL is Unreal's mini-language for injecting verbatim text into the auto-generated `build.gradle`, `AndroidManifest.xml`, and `GameActivity.java`. The canonical structure:

```xml
<root xmlns:android="http://schemas.android.com/apk/res/android">
  <gradleProperties>
    <insert>
      android.useAndroidX=true
      android.enableJetifier=true
    </insert>
  </gradleProperties>

  <buildGradleAdditions>
    <insert>
      dependencies {
        implementation 'com.tenjin:android-sdk:1.18.0'
        implementation 'com.google.android.gms:play-services-ads-identifier:18.0.1'
        implementation 'com.google.android.gms:play-services-appset:16.0.2'
        implementation 'com.android.installreferrer:installreferrer:2.2'
      }
    </insert>
  </buildGradleAdditions>

  <androidManifestUpdates>
    <addPermission android:name="android.permission.INTERNET"/>
    <addPermission android:name="com.google.android.gms.permission.AD_ID"/>
    <addElements tag="application">
      <meta-data android:name="TENJIN_APP_STORE" android:value="googleplay"/>
    </addElements>
  </androidManifestUpdates>

  <gameActivityImportAdditions>
    <insert>
      import com.tenjin.android.TenjinSDK;
      import com.tenjin.android.AttributionInfoCallback;
    </insert>
  </gameActivityImportAdditions>

  <gameActivityClassAdditions>
    <insert>
      public void tenjinConnect(String apiKey) {
        TenjinSDK.getInstance(this, apiKey).connect();
      }
      public void tenjinEventWithValue(String name, int value) {
        TenjinSDK.getInstance(this, null).eventWithNameAndValue(name, value);
      }
      public class TenjinAttributionListener implements AttributionInfoCallback {
        public void onSuccess(java.util.Map&lt;String,String&gt; data) {
          nativeTenjinAttributionInfo(new org.json.JSONObject(data).toString());
        }
        public void onFailure(String error) { /* ... */ }
      }
      public native void nativeTenjinAttributionInfo(String json);
    </insert>
  </gameActivityClassAdditions>

  <proguardAdditions>
    <insert>
      -keep class com.tenjin.** { *; }
      -keep class com.epicgames.unreal.GameActivity$TenjinAttributionListener { *; }
      -keepattributes Signature
    </insert>
  </proguardAdditions>
</root>
```

JNI calls from C++ use Unreal's `FJavaWrapper` and `FAndroidApplication::GetJavaEnv()` helpers (from `Runtime/Launch/Public/Android/AndroidJNI.h`):

```cpp
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"

void UTenjinSDKBlueprint::EventWithValue(const FString& Name, int32 Value)
{
    JNIEnv* Env = FAndroidApplication::GetJavaEnv();
    static jmethodID Mid = FJavaWrapper::FindMethod(Env,
        FJavaWrapper::GameActivityClassID,
        "tenjinEventWithValue", "(Ljava/lang/String;I)V", false);
    jstring jName = Env->NewStringUTF(TCHAR_TO_UTF8(*Name));
    FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Mid, jName, Value);
    Env->DeleteLocalRef(jName);
}
#endif
```

Java-to-C++ callbacks use the JNI-naming convention. A method declared in `GameActivity` as `public native void nativeTenjinAttributionInfo(String json)` resolves to:

```cpp
extern "C" JNIEXPORT void JNICALL
Java_com_epicgames_unreal_GameActivity_nativeTenjinAttributionInfo(
    JNIEnv* Env, jobject /*thiz*/, jstring jJson)
{
    const char* utf = Env->GetStringUTFChars(jJson, nullptr);
    FString Json = UTF8_TO_TCHAR(utf);
    Env->ReleaseStringUTFChars(jJson, utf);
    AsyncTask(ENamedThreads::GameThread, [Json](){
        UTenjinSDKDelegates::Get()->OnAttributionInfo.Broadcast(Json);
    });
}
```

The **single most important UE4→UE5 portability bug** is the Java package rename from `com.epicgames.ue4.GameActivity` to `com.epicgames.unreal.GameActivity` in 5.0. Plugins that hard-code the path via `Env->FindClass("com/epicgames/ue4/GameActivity")` break on UE5. The fix is to always use `FJavaWrapper::GameActivityClassID` (a `jclass` the engine pre-populates) and to use the `Java_com_epicgames_unreal_...` symbol name (UE5 default) while shipping a UE4-symbol variant guarded by a version macro if you need UE 4.27 support.

### 1.4 Blueprint exposure

The Blueprint surface is a single `UBlueprintFunctionLibrary` for static-style calls, plus a `UObject` singleton for async callbacks. Following Adjust's pattern:

```cpp
UCLASS()
class TENJINSDK_API UTenjin : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Tenjin")
    static void Connect(const FString& ApiKey);

    UFUNCTION(BlueprintCallable, Category="Tenjin|Events")
    static void SendEvent(const FString& EventName);

    UFUNCTION(BlueprintCallable, Category="Tenjin|Events")
    static void SendEventWithValue(const FString& EventName, int32 Value);

    UFUNCTION(BlueprintCallable, Category="Tenjin|IAP")
    static void Transaction(const FTenjinTransaction& Tx);

    UFUNCTION(BlueprintCallable, Category="Tenjin|AdRevenue")
    static void EventAdImpression(ETenjinAdNetwork Network, const FString& Json);

    // C++ lambda overload for power users
    static void GetAttributionInfo(TFunction<void(const TMap<FString,FString>&)> Cb);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnTenjinAttributionInfo, const TMap<FString,FString>&, Info);

UCLASS(BlueprintType)
class TENJINSDK_API UTenjinDelegates : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="Tenjin") FOnTenjinAttributionInfo OnAttributionInfo;
    UPROPERTY(BlueprintAssignable, Category="Tenjin") FOnTenjinDeeplinkReceived OnDeeplinkReceived;
    static UTenjinDelegates* Get(); // singleton accessor
};
```

USTRUCTs carry typed payloads. The transaction struct mirrors Tenjin's native API:

```cpp
USTRUCT(BlueprintType)
struct FTenjinTransaction
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FString ProductId;
    UPROPERTY(BlueprintReadWrite) FString CurrencyCode;
    UPROPERTY(BlueprintReadWrite) int32 Quantity = 1;
    UPROPERTY(BlueprintReadWrite) float UnitPrice = 0.0f;
    UPROPERTY(BlueprintReadWrite) FString TransactionId;
    UPROPERTY(BlueprintReadWrite) FString Receipt;
    UPROPERTY(BlueprintReadWrite) FString Signature;
};
```

For one-shot async results (attribution fetch, ATT prompt) the right pattern is `UBlueprintAsyncActionBase` — auto-generates a Blueprint node with success/error exec pins. Adjust uses this. AppsFlyer uses an actor-component fan-out via `TObjectIterator<UAppsFlyerSDKCallbacks>` which works but is harder to discover.

SDK key, app store type, ATT description, and ILRD toggles belong in a **`UDeveloperSettings`** subclass that registers a Project Settings page — so the developer never writes code to bootstrap. AppsFlyer's `defaultSettings->appsFlyerDevKey` shows the pattern; copy it.

### 1.5 UE4 vs UE5 differences

| Concern | UE 4.27 | UE 5.0–5.6 |
|---|---|---|
| GameActivity Java package | `com.epicgames.ue4.GameActivity` | `com.epicgames.unreal.GameActivity` (renamed in 5.0) |
| `.uplugin` platform key | `WhitelistPlatforms` | `PlatformAllowList` (ship both) |
| `IncludeOrderVersion` | n/a | New in 5.1; IWYU tightened across 5.2/5.4 |
| iOS bitcode | Required by App Store ≤ 4/2022 | Apple deprecated; UE5 stripped support |
| iOS minimum | iOS 12 | iOS 14 (5.0) → iOS 15 (5.2) → iOS 16 (5.3) → iOS 17 (5.4+) |
| Android minSdk / targetSdk | 21 / 30 | 26+ (5.0) / 33 (5.2) / 34 (5.4) |
| Android Gradle Plugin | AGP 4.0 | AGP 7.x (5.1) → AGP 8.x (5.3+); needs Java 17 |
| NDK | NDK 21/23 | NDK 25.1+ (5.2), 25/26 (5.4) |
| Xcode | 13 | 14.1+ (5.1) → 15+ (5.3) → 15.3+ (5.4) |
| UPL syntax | Stable | Same DSL, more directives added |

UPL syntax itself is **stable across UE4 and UE5** — the same XML works in both. Vendors typically use compile-time guards on `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` (from `Runtime/Launch/Resources/Version.h`) for the small number of C++ shifts, and resolve the GameActivity class via `FJavaWrapper::GameActivityClassID` instead of `FindClass(...)` to handle the package rename.

### 1.6 Async callback patterns and pitfalls

The thread-safety pattern is identical on both platforms: **native callback (any thread) → marshal to game thread via `AsyncTask(ENamedThreads::GameThread, ...)` → broadcast UE dynamic-multicast delegate**. Hold a `TWeakObjectPtr` (or do a `TObjectIterator` walk like AppsFlyer) so GC during the marshal doesn't crash you. Singular's plugin **does not do this** and explicitly warns users in docs that callbacks may arrive off the game thread — that is a real footgun, and Tenjin's plugin should not copy it.

**The pitfalls that bite production plugins**, in rough order of how often they surface:

1. **Manifest merger conflicts** when host app and SDK both declare `android:allowBackup`, `android:fullBackupContent`, etc. Mitigation: `tools:replace="android:allowBackup"` via UPL `<androidManifestUpdates>`.
2. **ProGuard/R8 stripping** of UPL-injected Java callback inner classes in shipping builds. Adjust ships explicit `-keep` rules; AppsFlyer learned this in issues #6/#7 the hard way.
3. **Duplicate-class Gradle failures** when two plugins each embed `play-services-ads-identifier` or `androidx.appcompat`. Mitigation: prefer Maven dependencies (Gradle dedupes) over vendored `.aar`s.
4. **AndroidX / Jetifier** mismatches on older engine builds; fix with `android.useAndroidX=true` and `android.enableJetifier=true` in `<gradleProperties>`.
5. **iOS deployment-target mismatches** — embedded framework built against iOS 14 SDK while Unreal sets `MinimumiOSVersion=IOS_12` causes App Store validation failures.
6. **Editor vs packaged behavior** — `PLATFORM_IOS` / `PLATFORM_ANDROID` are 0 in PIE; provide editor no-op stubs or BP calls crash the editor.
7. **`FindClass("com/epicgames/ue4/GameActivity")`** failing on UE5 — covered above.
8. **Locale-dependent number parsing** — AppsFlyer's bridge uses `NSNumberFormatter` to parse string revenue, which is locale-aware and silently drops values in `,`-decimal locales. Use `NSDecimalNumber` with `NSLocaleIdentifier="en_US_POSIX"`, or pass numerics as `double`/`int32` from the BP side instead of strings.

### Key references

- Unreal Plugin Language reference: `dev.epicgames.com/documentation/en-us/unreal-engine/using-the-unreal-plugin-language-for-mobile-projects-in-unreal-engine`
- Plugins in Unreal Engine: `dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine`
- AppsFlyer plugin (canonical example): `github.com/AppsFlyerSDK/appsflyer-unreal-plugin`
- Adjust plugin (cleaner reference): `github.com/adjust/unreal_sdk`

---

## 2. Survey of competitor MMP Unreal plugins

The competitive landscape is essentially **two mature plugins (AppsFlyer, Adjust), one mid-tier (Singular), and four no-shows (Branch, Kochava, Firebase, Meta)**. The two mature ones are both open-source on GitHub, both are GitHub-only (not on Fab/Marketplace), and both ship sample UE projects. **Tenjin's competitive bar is set by Adjust** — not AppsFlyer.

### 2.1 AppsFlyer — github.com/AppsFlyerSDK/appsflyer-unreal-plugin

Latest **v6.17.8 (Jan 2026)**, 26 stars, 11 forks, 18 releases, no explicit license (proprietary). Supports UE4 and UE5 broadly; demo project is UE 5.1. Language mix per GitHub: C++ 89.9%, C# 5.9%, Objective-C++ 3.0%. Single Runtime module with a monolithic `AppsFlyerSDKBlueprint.cpp` (~3,000–4,500 LOC) using merged `#if PLATFORM_IOS / #elif PLATFORM_ANDROID` blocks. **iOS bridge**: embeds `AppsFlyerLib.framework`, dispatches to main queue, hooks `UIApplicationWillEnterForegroundNotification`. **Android bridge**: Maven dependency injected via UPL (`com.appsflyer:af-android-sdk:6.17.x` + `installreferrer`), Java methods injected into `GameActivity` (`afStart`, `afSetConsentData`, `appsflyer_set_consent_v2`), JNI-back via a static export from `com.appsflyer.AppsFlyer2dXConversionCallback`. **Callbacks**: `UAppsFlyerSDKCallbacks` actor-component the user attaches to an actor, with multicast delegates fanned out via `TObjectIterator`. **Config via `UDeveloperSettings`** — Project Settings panel with `appsFlyerDevKey`, `appleAppID`, `disableSKAdNetwork`, etc.

**Event API**: one generic `logEvent(eventName, TMap<FString,FString>)` plus a special `ValidateAndLogInAppPurchase(FAFSDKPurchaseDetails, ...)` added in v6.17.8. **Ad revenue**: no per-network helpers — developer calls `logEvent("af_ad_revenue", {...})`. This is a notable gap vs Adjust. **Deep linking**: developer manually edits manifests; OneLink resolution via `setResolveDeepLinkURLs`. **Privacy**: ATT timeout, `SetConsentForGDPRUser`, `SetConsentForNonGDPRUser`, TCF v2 via `appsflyer_set_consent_v2` (uses boxed `java.lang.Boolean` for tri-state).

**Known issues**: locale-dependent `NSNumberFormatter` drops `af_revenue` in `,`-decimal locales (issue #22, unresolved); ProGuard/AndroidX/Firebase conflicts (#6/#7) that forced community forks; manifest merger failures common.

### 2.2 Adjust — github.com/adjust/unreal_sdk

Latest **v5.5.0 (Jan 2026)**, 17 stars, MIT license, tested on UE 4.27 + UE 5.x. Language mix: C++ 93.6%, Objective-C 3.5% — Adjust keeps iOS in **separate `.m`/`.mm` files** rather than the merged-`#if` approach. Estimated **~5,000–7,000 LOC** in the runtime module spread across multiple USTRUCT-per-feature headers: `FAdjustConfig`, `FAdjustEvent`, `FAdjustAdRevenue`, `FAdjustAttribution`, `FAdjustThirdPartySharing`, `FAdjustAppStoreSubscription`, `FAdjustPlayStoreSubscription`, `FAdjustAppStorePurchase`, `FAdjustPlayStorePurchase`, `FAdjustSkanConversionDataMap`.

**Android bridge** injects ~20 typed Java callback inner classes via UPL — one per Adjust SDK listener (`AdjustUeAttributionGetterCallback`, `AdjustUeSkanConversionValueUpdatedCallback`, etc.) — with **explicit ProGuard `-keep` rules** for each. This is the right pattern. Maven dependency `com.adjust.sdk:adjust-android:5.x` via `<buildGradleAdditions>`.

**Dual public API** — Blueprint-friendly `UAdjustDelegates` with `BlueprintAssignable` dynamic multicast delegates, plus C++-only `TFunction` lambda overloads for power users:
```cpp
static void GetAttribution(TFunction<void(const FAdjustAttribution&)> Cb);
static void RequestAppTrackingAuthorization(TFunction<void(int)> Cb);
static void ProcessAndResolveDeeplink(const FAdjustDeeplink&, TFunction<void(const FString&)>);
```
This is **the design pattern Tenjin should adopt**.

**Ad revenue modeling is richer** than AppsFlyer: typed `FAdjustAdRevenue` struct with an explicit `Source` field accepting documented strings (`applovin_max_sdk`, `admob_sdk`, `ironsource_sdk`, `admost_sdk`, `unity_sdk`, `helium_chartboost_sdk`, `adx_sdk`, `tradplus_sdk`, `topon_sdk`, `publisher_sdk`) plus `Revenue`, `Currency`, `AdRevenueNetwork`, `AdRevenueUnit`, `AdRevenuePlacement`, `AdImpressionsCount`.

**Privacy controls are the most comprehensive**: `GdprForgetMe`, `TrackThirdPartySharing` with `GranularOptions` (Facebook LDU CCPA, Google DMA `eea`/`ad_personalization`/`ad_user_data`), `TrackMeasurementConsent`, COPPA, Play Store Kids, ATT, AdServices, URL strategy with EU/Turkey/US/China/India data residency. Documentation is extraordinary — single-page README is encyclopedic.

### 2.3 Singular — `support.singular.net` (no GitHub)

Closed-source ZIP distribution, **v2.6.0 (July 2025)**, proprietary. Single static `USingularSDKBPLibrary`. The plugin has a known brittleness: docs explicitly tell users to **move `Singular.framework` to `/Users/Shared/Epic Games/UE_5.5/Engine/Intermediate/UnzippedFrameworks/Singular`** to resolve a `'Singular/Singular.h' file not found` first-build error. Deep-link callback docs warn users that **handlers may execute on a background thread** — Singular doesn't marshal for you, a clear footgun. Sample project not bundled.

### 2.4 Branch, Kochava, Firebase, Meta — no official Unreal plugins

**Branch** — `github.com/BranchMetrics` enumerates Android, iOS, Unity, RN, Cordova/Ionic, Xamarin, Capacitor, Flutter, macOS, Windows, tvOS, Roku, Web — **no Unreal**.

**Kochava** — `github.com/Kochava` ships Apple SwiftPM, Android AAR, Unity, Web, .NET, React Native — **no Unreal**. The Kochava integration guide does not list Unreal.

**Firebase** — Google's official position (from a firebase-talk Google Group response): "We don't provide an Unreal blueprint SDK for Firebase ... use the Firebase C++ SDK directly from Unreal." Third-party paid plugins fill the gap: **Pandores' "Firebase Features"** on Fab (most mature, UE 5.x, BP-first), `nprudnikov/PrFirebase` (open-source UE4), and several legacy Marketplace entries.

**Meta (Facebook)** — Meta ships Unreal SDKs only for Meta Horizon / Quest VR (`developers.meta.com/horizon/...`). No mobile app-attribution plugin. The historical `Apsalar` plugin (now Singular's predecessor) shipped inside UE4.27 source under `/Engine/Plugins/Analytics/Apsalar` via the `IAnalyticsProvider` interface but is abandoned.

### 2.5 Tenjin's current state — no plugin exists

Confirmed via `github.com/tenjin` (40 repos, **no `tenjin-unreal-*` repo**) and `tenjin.com/docs`. The only thing in market is a single third-party paid plugin on the legacy Marketplace (Volihan Games / Andrey Kharitonov, "Tenjin Mobile Analytics") which is **Android-only** by the author's own admission and unaffiliated with Tenjin. Tenjin's FAQ acknowledges "some customers have used this plug-in" without endorsement.

### 2.6 Comparison table

| Vendor | Repo | UE Versions | LOC | Marketplace/Fab | License | Last release | Notable architecture |
|---|---|---|---|---|---|---|---|
| AppsFlyer | github.com/AppsFlyerSDK/appsflyer-unreal-plugin | UE4 + UE5 | ~3–5k | No | Proprietary | Jan 2026 (6.17.8) | Single-file merged-`#if`; `TObjectIterator` fan-out; `UDeveloperSettings` |
| Adjust | github.com/adjust/unreal_sdk | UE 4.27 + UE 5.x | ~5–7k | No | **MIT** | Jan 2026 (5.5.0) | Modular USTRUCTs; dual BP/lambda API; explicit ProGuard rules; rich ad-revenue typing |
| Singular | support.singular.net (ZIP) | UE 4.x + UE 5.5 | unknown | No | Proprietary | Jul 2025 (2.6.0) | Manual framework relocation needed; callbacks NOT marshalled to game thread |
| Branch | — | — | — | No | — | — | No Unreal SDK |
| Kochava | — | — | — | No | — | — | No Unreal SDK |
| Firebase (Google) | — | — | — | No (3rd-party Pandores on Fab) | — | — | Google says "use C++ SDK directly" |
| Meta | — | — | — | No | — | — | Only Quest/Horizon VR Unreal SDK |
| Tenjin | — | — | — | 3rd-party Android-only on Marketplace | — | — | **No first-party plugin** |

---

## 3. Tenjin SDK specifics

### 3.1 GitHub presence and supported SDKs

Tenjin's org (`github.com/tenjin`) has 40 public repos. The SDK-relevant ones, with observed mid-2026 state:

| Repo | Latest | Notes |
|---|---|---|
| `tenjin-ios-sdk` | **1.16.0** (Mar 2026) | 57 releases, Obj-C 92% + C/Swift, distributed via CocoaPods/SPM/xcframework |
| `tenjin-android-sdk` | **1.18.0** (Apr 2026) | 52 releases, Java/Kotlin, Maven Central `com.tenjin:android-sdk` |
| `tenjin-unity-sdk` | **1.16.4** (Apr 2026) | 63 releases, C# 84% + Obj-C++ 9%, UPM via Git URL + .unitypackage |
| `tenjin-ios-spm` | active | SPM-only wrapper |
| `tenjin-react-native-sdk` | **1.3.1** (Mar 2026) | bundles iOS 1.15.1 / Android 1.17.2 |
| `flutter-sdk` | active | Dart + native |
| `tenjin-ionic-sdk` | Feb 2026 | Capacitor/Ionic |
| `TenjinGMExtension` | infrequent | GameMaker |
| `sdk-llm-guides` | active | LLM-prompt-ready integration guides per SDK |

**No `tenjin-unreal-*` repo exists.** No archived repo, no WIP branch, no Unreal entry in `sdk-llm-guides/`. Tenjin's docs portal (`docs.tenjin.com`, `tenjin.com/docs/add-the-tenjin-sdk`) lists iOS, Android, Unity, React Native, Flutter, Ionic, GameMaker, "Other Plugins" (Defold community + Server-To-Server). Unreal is not listed anywhere. The "Other Plugins" page invites devs to email `support@tenjin.com` for unsupported engines — consistent with Unreal being on the "ask us" tier.

### 3.2 Tenjin Unity SDK — the closest analog to design from

The Unity SDK is structured as a thin C# façade (`BaseTenjin` abstract, `IosTenjin : BaseTenjin`, `AndroidTenjin : BaseTenjin`, selected at compile time by `#if UNITY_IOS/UNITY_ANDROID`) over the unmodified native iOS/Android SDKs. Layout:

```
Runtime/                — BaseTenjin.cs, Tenjin.cs (factory), AndroidTenjin.cs, IosTenjin.cs
Plugins/iOS/            — TenjinSDKWrapper.mm (Obj-C++ bridge with extern "C" functions)
Plugins/Android/        — resolves com.tenjin:android-sdk via EDM4U (External Dep Manager)
Editor/                 — TenjinEditorPrefs.cs
package.json            — UPM manifest
```

iOS bridging is via `[DllImport("__Internal")]` against the `extern "C"` shims in `TenjinSDKWrapper.mm`. Android bridging is via `AndroidJavaClass`/`AndroidJavaObject` JNI on `com.tenjin.android.TenjinSDK`. Async results (deeplink, attribution) flow back via `UnitySendMessage("Tenjin", "DeferredDeeplinkCallback", ...)` to a hidden GameObject, then fan out to user-supplied `Action`s. The Unreal plugin will skip the `UnitySendMessage` hop — Unreal uses `AsyncTask(ENamedThreads::GameThread, ...)` directly to a `UTenjinDelegates` singleton.

### 3.3 Full Tenjin native API surface (iOS + Android, parity)

The iOS `TenjinSDK.h` exposes all functionality as **class methods** on `TenjinSDK : NSObject`. The Android `com.tenjin.android.TenjinSDK` uses a `getInstance(Context, String)` factory. The APIs are intentionally near-identical:

**Lifecycle**: `initialize(apiKey)` / `getInstance(ctx, apiKey)`, optional `andSharedSecret:`, `connect()`, `connectWithDeferredDeeplink(url)`, `setAppStore(AppStoreType)` (Android only — `googleplay`/`amazon`/`other`/`unspecified`), `appendAppSubversion(int)`, `debugLogs()`.

**iOS-only**: `requestTrackingAuthorizationWithCompletionHandler:` (ATT), `updatePostbackConversionValue:` with three overloads (SKAN 4.0 coarse value + lock window).

**Privacy** (both): `optIn`, `optOut`, `optInParams(list)`, `optOutParams(list)`, `optInOutUsingCMP() : bool` (reads IAB TCF v2), `setGoogleDMAParameters(adPersonalization, adUserData)`, `optInGoogleDMA`, `optOutGoogleDMA`.

**Events**: `sendEventWithName:` / `eventWithName`, `sendEventWithName:andValue:` / `eventWithNameAndValue` (integer value, hard limits: ≤80 chars name, ≤500 unique names per app).

**Purchases**:
- iOS: `transaction:andReceipt:` (SK1 SKPaymentTransaction + ASN.1 receipt) or `transactionWithProductName:andCurrencyCode:andQuantity:andUnitPrice:andTransactionId:andBase64Receipt:` (also accepts SK2 JWS as receipt).
- Android: `transaction(productId, currency, qty, price, purchaseDataJson, signature)`.
- Amazon Android: `transactionAmazon(productId, currency, qty, price, receiptId, userId, receipt)`.

**Subscriptions** (newer, 2025): `Subscription(...)` and `SubscriptionWithStoreKit(productId, currency, price)` — iOS first-class, Android documented as "coming soon" in the Unity README. **Flag this as a moving target**.

**Attribution & deeplinks**: `getAttributionInfo(callback)` returns dictionary with `ad_network`, `campaign_id`, `campaign_name`, `site_id`, `creative_name`, etc. `registerDeepLinkHandler(handler)` returns `clicked_tenjin_link`, `is_first_session`, `deferred_deeplink_url`, attribution context.

**Identifiers**: `setCustomerUserId`, `getCustomerUserId`, `getAnalyticsInstallationId` (random GUID stored locally).

**User Profile / LiveOps Metrics** (added late 2025): `getUserProfile()` returning `TJNUserProfileData` (iOS) / `UserProfileData` (Android) with `sessionCount`, `totalSessionTime`, `averageSessionLength`, `lastSessionLength`, `iapTransactionCount`, `totalILRDRevenueUSD`, `iapRevenueByCurrency`, `purchasedProductIDs`, `ilrdRevenueByNetwork`. Also exposed as a flat `Map<String,String>`. `resetUserProfile()`.

**Cache**: `setCacheEventSetting(bool)` enables retry-on-failure.

**Impression-Level Ad Revenue (ILRD) passthroughs** — paid Tenjin feature. **Crucially, Tenjin does NOT pull any ad-network SDK transitively**. The host app wires the ad SDK's impression callback and calls Tenjin's per-network passthrough method, which serializes a JSON dictionary. Methods exist for AppLovin MAX, Unity LevelPlay (IronSource), AdMob, TopOn, HyperBid, CAS, TradPlus. Helium/Chartboost mediation is not currently in Tenjin's native ILRD lists — skip for v1.

### 3.4 Feature parity matrix for the Unreal plugin

The Unreal plugin must mirror everything above. Concretely:

**Must ship in v1** (covers Tenjin's ICP and the AppsFlyer/Adjust feature bar): init + shared secret, connect, ATT prompt with timeout, SKAN postback updates (all 3 overloads), GDPR/CCPA opt-in/opt-out + params variants, Google DMA, `optInOutUsingCMP`, custom events (named + with value), iOS transaction (SK1 + SK2 paths), Android transaction (Google Play), Amazon transaction, attribution callback, deeplink callback, customer user ID, analytics installation ID, cache setting, user profile struct + dict + reset, `setAppStore`, `appendAppSubversion`, `debugLogs`, ILRD passthrough methods for AppLovin MAX / LevelPlay / AdMob / TopOn / HyperBid / CAS / TradPlus, automatic Info.plist injection (ATT description, SKAN endpoint, SKAdNetworkItems), automatic AndroidManifest injection (permissions, `TENJIN_APP_STORE` meta, Meta install-referrer queries), automatic Maven/Cocoapods dep injection.

**Defer to v1.1**: Subscriptions (Android side documented as "coming soon"), Huawei/MSA OAID variants (China stores), Helium/Chartboost ILRD.

### 3.5 Existing Unreal integration

None worth using as a baseline. Tenjin's FAQ vaguely references an "unofficial community plugin" but provides no URL, no endorsement, and no repo exists under the Tenjin org or as a discoverable fork. A single third-party paid Marketplace plugin ("Tenjin Mobile Analytics" by Andrey Kharitonov / Volihan Games) is **Android-only** by the author's own product page and has a single rating. **The market is uncontested.**

---

## 4. Estimated complexity and effort

### 4.1 V1 effort estimate

For a senior mobile SDK engineer who already knows Tenjin's iOS/Android natives, with part-time QA support from a second engineer, the breakdown is:

| Workstream | Engineer-days |
|---|---|
| Plugin scaffolding (.uplugin, Build.cs, UDeveloperSettings) | 2 |
| UPL_Android.xml (Gradle deps, manifest injections, GameActivity Java, ProGuard) | 3–4 |
| UPL_iOS.xml (Info.plist injections, framework embedding, SKAN list) | 2–3 |
| iOS Obj-C++ bridge (every API method, ATT, SKAN, transaction, attribution, ILRD) | 5–7 |
| Android JNI bridge (every API method, JNI marshalling of maps, Java listener inner classes) | 5–7 |
| Blueprint surface (UBlueprintFunctionLibrary, USTRUCTs, UTenjinDelegates, async nodes) | 3–4 |
| Async callback plumbing (game-thread marshalling, deeplink + attribution flows) | 2–3 |
| Sample project (UE 5.4 + simple BP scene exercising every API) | 3–5 |
| Documentation (README, Installation, BasicIntegration, API, Deeplink, Privacy, ILRD per network) | 4–6 |
| Cross-engine-version validation (4.27, 5.1, 5.3, 5.5, 5.6) | 5–8 |
| QA on real iOS + Android devices, App Store/Play upload smoke test | 3–5 |
| **Total** | **37–54 days ≈ 6–8 engineer-weeks** |

The mean estimate aligns with industry experience: AppsFlyer's plugin is ~3–5k LOC of C++/Obj-C++, Adjust is ~5–7k. Both have evolved over multiple years with frequent releases — but the **first shippable version** is on the order of 6–8 engineer-weeks. Allow up to 10 weeks if shipping subscriptions and full ILRD parity from day one.

### 4.2 Hardest parts (ranked)

1. **Cross-engine-version test matrix.** UE 4.27 → 5.0 introduces the GameActivity Java package rename, the `WhitelistPlatforms`→`PlatformAllowList` change, AGP/NDK/Java toolchain bumps. UE 5.0 → 5.6 brings ongoing iOS deployment-target bumps (12 → 17), AGP 7 → AGP 8 (Java 17), tightened IWYU, and Apple privacy-manifest requirements (iOS 17). Each minor version is a potential break.
2. **Dependency deconfliction.** Users will have AppsFlyer or Firebase or AdMob plugins installed alongside Tenjin's, and the resulting Gradle classpath frequently produces `Duplicate class` errors. Maven-based deps mitigate but don't eliminate. Documenting known conflicts is unavoidable.
3. **Async callback lifetime + threading.** Getting attribution/deeplink callbacks reliably from background-thread native dispatch onto the UE game thread with no race-on-GC requires careful `TWeakObjectPtr` + `AsyncTask` discipline. Bugs here are sporadic and very hard to reproduce.
4. **iOS provisioning + framework embedding.** First-time package-for-iOS frequently breaks codesign in ways that don't surface in editor. UE handles "Embed & Sign" via the Xcode project but vendored xcframeworks with mixed signatures (e.g., debug build artifacts from a different machine) trip up automation.
5. **ProGuard/R8 strip-outs in shipping builds.** UPL-injected Java callback inner classes (`GameActivity$TenjinAttributionListener`) are stripped by R8 unless `-keep` rules are present. This was the root cause of multiple AppsFlyer issues; Adjust learned the lesson and ships explicit rules.
6. **Subscription parity on Android.** Tenjin's native Android subscription path is documented as "coming soon" — committing to subscription parity in v1 means racing the native team.

### 4.3 Ongoing maintenance burden

The realistic ongoing cost is **~0.25 FTE = ~1 engineer-day per week**, distributed roughly as:

- **Tenjin SDK release tracking** — Tenjin iOS releases ~6×/year, Android ~5×/year, each requiring at least a version bump in UPL Gradle/CocoaPods coordinates and a smoke test. Most are 0.5-day touches.
- **Unreal Engine major release** — Once per ~12 months Epic ships a new minor (5.5 → 5.6). Each requires a package-build smoke test across 2–3 device classes, occasional UPL fix for toolchain changes. Budget 1 week per UE release.
- **iOS / App Store policy churn** — privacy manifests (2024), ATT enforcement, new SKAN versions, StoreKit 2 migration, new SKAN/AdAttributionKit transitions. Each is a multi-day item every 6–12 months.
- **Google Play policy churn** — targetSdk bumps annually (Aug deadline), Data Safety form updates, AD_ID permission changes. 2–3 day items.
- **Issue triage on GitHub** — AppsFlyer averages ~1 substantive issue per month; Adjust fewer. Budget a couple hours per week.

AppsFlyer ships ~12–18 releases per year on their Unreal plugin. That's a reasonable pace expectation if Tenjin wants to be competitive.

### 4.4 Support and QA implications

The plugin will surface support tickets that the existing Tenjin Unity/iOS/Android support team probably can't fully handle: UE-specific build errors, BP-specific integration patterns, IWYU/PCH issues. Plan for **one Tenjin engineer (probably the plugin author) to own L2 support for the first 6–12 months**, with FAQ/docs investments to drive down repetitive Tier-1 tickets. The single biggest QA-cost driver is the **engine version × OS version × device matrix** — supporting UE 4.27 + UE 5.1/5.3/5.5/5.6 against iOS 15/16/17/18 and Android 10/11/12/13/14/15 is ~30+ combinations. Pragmatically, **pick 2 engine versions (UE 5.3 + UE 5.6) and the latest iOS/Android as the supported tier**, with best-effort on others.

---

## 5. Distribution considerations

### 5.1 Marketplace, Fab, GitHub — what MMPs actually do

**None of AppsFlyer, Adjust, or Singular publishes through the Unreal Marketplace or Epic's newer Fab unified marketplace.** All three are **GitHub or vendor-ZIP downloads**. The pattern is industry-wide: studios prefer to clone or fork SDK plugins for version pinning, source-code visibility, and easier internal patching. Marketplace pre-compiled binaries per engine version are a maintenance tax that vendors avoid.

Fab (Epic's unified marketplace replacing Marketplace, Quixel, and Sketchfab) launched late 2024. It is a sensible place for paid third-party SDKs (Pandores' Firebase Features is there) but not for free vendor-distributed MMP plugins. **Tenjin should ship GitHub-first under MIT** at `github.com/tenjin/tenjin-unreal-sdk` and only consider Fab if a specific go-to-market reason emerges (e.g., a paid premium tier).

### 5.2 Engine version support strategy

The pragmatic support tier (matching what Adjust ships) is:

- **Primary**: UE 5.6 (current), UE 5.5, UE 5.4 (LTS-ish)
- **Best-effort**: UE 5.3, UE 5.2, UE 5.1, UE 5.0
- **Legacy**: UE 4.27 if there's strong customer demand; otherwise deprecate

Compile-time guards on `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` handle the shifts. Ship one source plugin; do not pre-compile per-version unless going to Fab. Use `FJavaWrapper::GameActivityClassID` (engine-populated) rather than `FindClass(...)` so the Android side is automatically forward-compatible with the package rename.

### 5.3 Licensing

**MIT.** Match Adjust. AppsFlyer's unlicensed proprietary stance is a small but real friction in studio legal review. MIT removes that friction at no real cost — the plugin is glue code, not core IP.

### 5.4 Release cadence

Mirror the iOS/Android SDK release pace — every Tenjin iOS or Android release should trigger an Unreal plugin patch release within 1–2 weeks. Pin the underlying SDK versions explicitly in UPL (`com.tenjin:android-sdk:1.18.0`, not `1.18.+`) so studios can audit and CI-pin. Tag releases on GitHub matching the underlying SDK major.minor.

---

## 6. The Unreal mobile games market

### 6.1 Engine share — Unity dominates count, Unreal punches up in revenue

The cleanest defensible claim: **Unity powers ~70% of top-1,000 mobile games by title count; Unreal is ~5–10% of titles but disproportionately represented in high-grossing 3D titles**, particularly Korean MMORPGs and Chinese open-world / shooter genres. Proprietary engines (Tencent TQuant/in-house, NetEase Messiah, miHoYo's customized Unity stack, Pearl Abyss BlackSpace, King) hold the rest of the absolute top-grossing chart.

Public data is fragmented and PC-skewed:
- Astute Analytica / Argentics: Unity ~71% of top-1,000 mobile games.
- Sensor Tower / Video Game Insights "Big Game Engine Report 2025": custom engines collapsed from 71% (2012) to 13% (2024) on Steam; Unreal share rising since 2015, Unity declining since 2021. *PC-only, but directionally informative.*
- devtodev / GDC dev survey 2024–25: Unity 32% / Unreal 32% / proprietary 13% by developer count, all platforms.
- 6sense corporate adoption: Unity 25% / Unreal 15% across game studios.

**The trend on mobile is modestly up for Unreal.** UE 5.6 shipped meaningful mobile improvements (Lumen on mobile experimental, smaller build sizes, better Vulkan/Metal). Krafton is moving PUBG Mobile to UE5 (target 2028). Most new Korean tentpole MMORPGs and Chinese open-world games are choosing Unreal. The Unity 2023 runtime-fee fallout pushed some marginal projects toward Unreal. But the **high-volume long tail** — casual, hyper-casual, hybrid-casual, midcore — remains overwhelmingly Unity and is not switching.

### 6.2 Notable Unreal mobile titles

| Title | Publisher | Genre | Engine | Mobile revenue tier |
|---|---|---|---|---|
| Fortnite Mobile | Epic Games | Battle Royale | UE5 | Returned to iOS US May 2025; cross-platform $1B+/yr |
| PUBG Mobile | Krafton/Tencent | BR | UE4 → UE5 by 2028 | **$1.1B (2024), ~$1.6B (2025)**; $15B lifetime; 113M MAU |
| Game for Peace (CN PUBG) | Tencent | BR | UE4 | >50% of PUBG franchise revenue |
| BGMI | Krafton (India) | BR | UE4 | Krafton mobile +35.7% YoY 2024 |
| Wuthering Waves | Kuro Games | Open-world ARPG | UE4 (heavily modified) | $233M+ lifetime mobile by May 2025 |
| Tower of Fantasy | Hotta / Level Infinite | Open-world MMO | UE 4.26 | ~$200M+ lifetime mobile |
| Arena Breakout | MoreFun (Tencent) | Extraction FPS | UE 4.26 | 100M+ downloads; top-20 grossing FPS |
| Solo Leveling: Arise | Netmarble | Action RPG | Unreal | Top-grossing globally for several months in 2024 |
| Night Crows | MADNGINE/Wemade | MMORPG | UE5 | Top-grossing Korean launch 2024 |
| MIR4 | Wemade | MMORPG | UE4 | Top-10 Korean in 2021–22 |
| Lineage 2: Revolution | Netmarble | MMORPG | UE4 | $924M first 11 months |
| Lineage 2M / Lineage W | NCSoft | MMORPG | UE4 | Korean top-5 since launch |
| Blade & Soul Revolution | Netmarble | MMORPG | UE4 | Top-10 Korea at launch |
| Ni no Kuni: Cross Worlds | Netmarble/Level-5 | MMORPG | UE4 | #1 Korea/Japan/Taiwan launch (2021) |
| Aion 2 (Q4 2025) | NCSoft | MMORPG | Unreal | Expected major Korean launch |
| Gran Saga | NPIXEL | MMORPG | UE4 | Korean top-10 |

**Engines commonly confused (NOT Unreal):** Genshin Impact, Honkai: Star Rail, Zenless Zone Zero — all Unity (miHoYo). Diablo Immortal — NetEase Messiah. Black Desert Mobile — Pearl Abyss BlackSpace. Honor of Kings — Tencent proprietary. Call of Duty Mobile / Warzone Mobile — Activision proprietary. Marvel Rivals is Unreal 5 but has **no mobile version**.

### 6.3 Where Unreal dominates on mobile

- **Korean MMORPGs** (NCSoft, Netmarble, Wemade, Kakao, NPIXEL, Smilegate, Com2uS) — estimated 60–80% of Korean top-grossing mobile MMORPGs run on Unreal.
- **Chinese open-world / cross-platform** (Kuro Games, Tencent's newer shooters)
- **Battle royales and extraction shooters globally** (Fortnite, PUBG Mobile, BGMI, Arena Breakout, Game for Peace)

Unity dominates everywhere else: casual, midcore, gacha (despite UE's MetaHuman appeal, miHoYo and Nikke and Blue Archive are Unity), match-3, idle, simulation, SLG. The long tail of mobile titles — where MMPs do most of their volume — is overwhelmingly Unity.

### 6.4 Addressable market for Tenjin specifically — honest assessment

**The math doesn't support a heavy-resourced Unreal-first push.** Tenjin's customer profile is indie/mid-market, $200/month entry pricing, hyper-casual/hybrid-casual content focus, LATAM/SEA/Turkey/India geographic skew. The Unreal mobile market is the **opposite** profile: a small number of very large publishers (NCSoft, Netmarble, Krafton, Tencent, Kuro Games) on enterprise MMP contracts with AppsFlyer/Adjust/Singular, primarily Korea and China. **The overlap with Tenjin's ICP is small.**

The accessible slice is the **long tail of indie/mid-market Unreal mobile studios** — smaller Korean studios, Southeast Asian and Indian shooter/BR clone makers, indie devs porting Unreal projects to mobile — probably numbering in the low thousands of studios globally. Defensively meaningful, offensively limited.

**The case for building anyway** rests on three arguments:

1. **Churn defense.** Existing Tenjin customers occasionally need to ship an Unreal title (e.g., a Unity studio diversifying into a 3D project). Not having a plugin is a churn risk on those specific accounts. This is the strongest argument.
2. **RFP feature parity.** When a prospect asks "do you support Unreal?" the answer needs to be "yes," not "we have a community workaround." AppsFlyer and Adjust have made this a default expectation.
3. **Optionality hedge.** UE5's mobile improvements lower the barrier; if Unreal's mobile share doubles among indies over 2027–28, Tenjin should already have a plugin in market.

**The case against** is straightforward: the math doesn't justify it from new-revenue alone, and the engineering team has higher-leverage projects.

**Recommendation framing**: treat the Unreal plugin as a **low-cost defensive investment** at the 6–8 engineer-week + 0.25 FTE ongoing level, with the expectation that it pays back through (a) defending current accounts, (b) ticking RFP boxes, and (c) hedging the UE5 mobile trend. Do not stand up an Unreal-specific GTM motion, don't expect material new MRR inside 12 months, and don't try to win Krafton or NCSoft against AppsFlyer. Build it serviceable, ship it MIT on GitHub, mirror Adjust's architecture, and move on.

---

## Conclusion: what to actually do

Tenjin's Unreal plugin is **buildable in 6–8 engineer-weeks** by one strong mobile SDK engineer with light QA backup, then **sustainable at ~0.25 FTE ongoing** to keep up with Tenjin native releases, UE versions, and store policy churn. The technical risk is low — Adjust and AppsFlyer have laid the trail and their plugins are open-source references. The unknowns are operational (test matrix, dependency conflicts, support load) rather than algorithmic.

**Three concrete design calls to lock in before kickoff:**

First, **clone Adjust's architecture, not AppsFlyer's**: modular USTRUCTs per feature, MIT license, separate .mm files for iOS, explicit ProGuard `-keep` rules, dual BP-delegate + C++ `TFunction` APIs, a clean `UTenjinDelegates` singleton with `AsyncTask(ENamedThreads::GameThread, ...)` marshalling for every callback, and `UDeveloperSettings`-based Project Settings configuration. Avoid AppsFlyer's `TObjectIterator` callback fan-out and locale-dependent `NSNumberFormatter` revenue parsing.

Second, **scope v1 to feature parity with the Unity SDK minus subscriptions and Helium/Chartboost ILRD**: init, connect, ATT, SKAN, GDPR/CCPA/DMA, custom events, iOS+Android+Amazon transactions, attribution callback, deeplink callback, customer user ID, analytics installation ID, user profile, and ILRD passthroughs for the seven supported networks (AppLovin MAX, LevelPlay, AdMob, TopOn, HyperBid, CAS, TradPlus). Ship subscriptions in v1.1 once Tenjin's Android native subscription path leaves "coming soon."

Third, **commit to a narrow support tier** (UE 5.3, 5.4, 5.5, 5.6 primary; UE 5.0–5.2 and 4.27 best-effort) and ship GitHub-first with no Fab/Marketplace listing initially. This matches what Adjust and AppsFlyer do and minimizes maintenance tax.

The market read justifies the build as a defensive investment but not as a strategic bet. **Build it for churn defense and RFP completeness, not for new-logo growth.** If Tenjin's Korean or Chinese enterprise pipeline starts producing serious Unreal-only opportunities, revisit the GTM investment level then. For now: ship it, keep it boring, keep it MIT, and don't over-invest.
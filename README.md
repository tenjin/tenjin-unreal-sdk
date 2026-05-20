# Tenjin Unreal Engine SDK

## Summary

The Unreal Engine SDK for Tenjin. To learn more about Tenjin and our product
offering, please visit <https://www.tenjin.com>.

* The Tenjin Unreal SDK supports both iOS and Android.
* Review the [iOS][tenjin-ios] and [Android][tenjin-android] documentation
  and apply the proper platform settings to your builds.
* See the [CHANGELOG](CHANGELOG.md) for detailed version history.
* For any issues or support, please contact <support@tenjin.com>.

## Table of contents

* [Requirements](#requirements)
* [Basic integration](#basic-integration)
  * [Get the SDK](#sdk-get)
  * [Add the SDK to your project](#sdk-add)
  * [Configure the plugin](#sdk-configure)
  * [Initialize and connect](#sdk-init)
* [Additional features](#additional-features)
  * [Custom events](#send-events)
  * [Purchase events](#track-iap)
  * [Subscription tracking](#track-subscriptions)
  * [LiveOps Campaigns (attribution)](#get-attribution-info)
  * [Deep links](#deep-links)
  * [User Profile — LiveOps Metrics](#user-profile)
  * [Customer User ID](#customer-user-id)
  * [Analytics Installation ID](#analytics-installation-id)
  * [App Subversion](#append-app-subversion)
  * [Retry/cache events](#cache-encrypt)
  * [GDPR (opt-in / opt-out)](#privacy-opt)
  * [Google DMA parameters](#privacy-dma)
  * [Opt in/out using CMP](#privacy-cmp)
  * [SKAdNetwork and Conversion Values (iOS)](#skadnetwork)
  * [Impression-Level Ad Revenue (ILRD)](#ilrd)
  * [App Store (Google Play / Amazon / Other)](#app-store)
* [Sample app](#sample-app)
* [Testing](#testing)
* [Engine version support](#engine-support)
* [Support](#support)
* [License](#license)

## <a id="requirements"></a>Requirements

* Unreal Engine **5.3 or newer**
* A **C++ project** (Blueprint-only projects must be converted to a code project
  before installing the plugin — UE compiles the plugin's C++ at package time)
* iOS 15+ deployment target
* Android `minSdkVersion` 23+, `compileSdkVersion` / `targetSdkVersion` 34
* A Tenjin SDK key from the [Tenjin dashboard][tenjin-dashboard]

## <a id="basic-integration"></a>Basic integration

### <a id="sdk-get"></a>Get the SDK

Clone or download this repository, or add it to your project as a git
submodule. Releases are tagged on GitHub.

### <a id="sdk-add"></a>Add the SDK to your project

1. Copy (or `git submodule add`) the `TenjinSDK/` folder into your project's
   `Plugins/` directory:

   ```
   <YourProject>/Plugins/TenjinSDK/
   ```

2. **No manual native-dep setup needed.** Both native SDKs are wired up
   automatically:
   * **iOS** — `TenjinSDK.xcframework` is committed under
     `Plugins/TenjinSDK/ThirdParty/iOS/` and linked via
     `PublicAdditionalFrameworks` in `Build.cs`. Bundled version is
     **TenjinSDK 1.17.0**. To bump, replace the framework folder per
     [`ThirdParty/iOS/README.md`][thirdparty-readme].
   * **Android** — the plugin's UPL XML injects
     `implementation 'com.tenjin:android-sdk:1.18.0'` into the generated
     Gradle build at package time; Gradle pulls from Maven Central during
     the build.

3. Add `TenjinSDK` to your game module's `Build.cs`:

   ```cs
   PublicDependencyModuleNames.AddRange(new string[] {
       "Core", "CoreUObject", "Engine", "TenjinSDK"
   });
   ```

4. Right-click your `.uproject` and choose *Generate project files*, then
   build the project once. The plugin compiles alongside your game module.

5. Enable the plugin in *Edit → Plugins → Analytics → Tenjin SDK*. It
   auto-enables when listed in your `.uproject`.

### <a id="sdk-configure"></a>Configure the plugin

The plugin registers a settings page at *Edit → Project Settings → Plugins →
Tenjin SDK*:

| Setting | Purpose |
|---------|---------|
| `SdkKey` | Your dashboard SDK key |
| `AttUsageDescription` | iOS ATT prompt text (injected into `Info.plist`) |
| `AndroidAppStore` | `googleplay` / `amazon` / `other` |
| `bAutoInitialize` | If `true`, the plugin auto-initializes during module startup |

Values are persisted in `Config/DefaultEngine.ini`:

```ini
[/Script/TenjinSDK.TenjinSettings]
SdkKey="YOUR_TENJIN_SDK_KEY"
AttUsageDescription="This identifier will be used to deliver personalized ads to you."
AndroidAppStore=GooglePlay
bAutoInitialize=False
```

#### iOS code signing

UE 5.x uses the "Modern Xcode" workflow. iOS signing is **not** under the
iOS Runtime Settings page — it lives in `Config/DefaultEngine.ini` under
`[/Script/MacTargetPlatform.XcodeProjectSettings]`:

```ini
[/Script/MacTargetPlatform.XcodeProjectSettings]
bUseAutomaticCodeSigning=True
CodeSigningTeam=XXXXXXXXXX        ; your 10-char Apple Team ID
CodeSigningPrefix=com.yourcompany ; bundle id = prefix + "." + project name
```

Find your Team ID in Xcode → Settings → Accounts, or decode a provisioning
profile. After editing, delete `Intermediate/ProjectFilesIOS/` so the
generated `.xcconfig` is rebuilt from the new values.

### <a id="sdk-init"></a>Initialize and connect

The two minimum calls required to record an install.

**From C++:**

```cpp
#include "TenjinBPLibrary.h"

UTenjinBPLibrary::Initialize(TEXT("YOUR_TENJIN_SDK_KEY"));
UTenjinBPLibrary::Connect();
```

**From Blueprints:** drag the nodes *Tenjin → Initialize* and *Tenjin →
Connect* into your game's startup graph.

The relevant function signatures:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin")
static void Initialize(const FString& SdkKey);

UFUNCTION(BlueprintCallable, Category = "Tenjin")
static void Connect();
```

> **iOS note:** if your app prompts the user for App Tracking Transparency,
> call your ATT request *before* `Connect()` so the IDFA decision is settled
> before Tenjin makes its first network call.

## <a id="additional-features"></a>Additional features

Once the SDK is initialized you can use any of the methods below.

### <a id="send-events"></a>Custom events

Track a named event:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Events")
static void EventWithName(const FString& Name);
```

```cpp
UTenjinBPLibrary::EventWithName(TEXT("level_complete"));
```

Track an event with an integer value (typed numeric is enforced — passing
strings is deprecated):

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Events")
static void EventWithNameAndValue(const FString& Name, int32 Value);
```

```cpp
UTenjinBPLibrary::EventWithNameAndValue(TEXT("coins_earned"), 250);
```

Limits: event name ≤ 80 characters, ≤ 500 unique event names per app.

### <a id="track-iap"></a>Purchase events

Generic transaction (no receipt):

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
static void Transaction(const FString& ProductName,
                        const FString& CurrencyCode,
                        int32 Quantity, float UnitPrice);
```

iOS — pass the StoreKit transaction id and base64-encoded receipt:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
static void TransactionWithReceipt(const FString& ProductName,
                                   const FString& CurrencyCode,
                                   int32 Quantity, float UnitPrice,
                                   const FString& TransactionId,
                                   const FString& Base64Receipt);
```

Android — pass the Play Billing purchase data JSON and signature:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
static void TransactionWithDataSignature(const FString& ProductName,
                                         const FString& CurrencyCode,
                                         int32 Quantity, float UnitPrice,
                                         const FString& PurchaseData,
                                         const FString& DataSignature);
```

### <a id="track-subscriptions"></a>Subscription tracking (iOS only)

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
static void Subscription(const FString& ProductId,
                         const FString& CurrencyCode, float UnitPrice,
                         const FString& IosTransactionId,
                         const FString& IosOriginalTransactionId,
                         const FString& IosReceipt,
                         const FString& IosSKTransaction,
                         const FString& AndroidPurchaseToken,
                         const FString& AndroidPurchaseData,
                         const FString& AndroidDataSignature);
```

iOS 15+ — fetches the latest StoreKit 2 transaction natively, then sends it
to Tenjin. Use this when your IAP library (e.g. RevenueCat) doesn't expose
SK2 data:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
static void SubscriptionWithStoreKit(const FString& ProductId,
                                     const FString& CurrencyCode,
                                     float UnitPrice,
                                     const FTenjinSubscriptionResultDelegate& Callback);
```

> **Android:** the native Tenjin Android SDK does not yet expose a
> subscription API. `Subscription(...)` is a no-op on Android until upstream
> support lands.

### <a id="get-attribution-info"></a>LiveOps Campaigns (attribution)

Tenjin supports retrieving user attribution information — sourcing ad
network, campaign, creative, etc. — directly from the SDK. This lets you
tie attribution to in-game data per device, or show different content
depending on the install source. Bind a UFUNCTION on a UObject and pass it
to `GetAttributionInfo` (dynamic delegates can't bind to lambdas):

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Attribution")
static void GetAttributionInfo(const FTenjinAttributionInfoDelegate& Callback);
```

```cpp
FTenjinAttributionInfoDelegate Cb;
Cb.BindUFunction(Listener, FName(TEXT("HandleAttributionInfo")));
UTenjinBPLibrary::GetAttributionInfo(Cb);
```

`Json` is the raw payload — `ad_network`, `campaign_id`, `campaign_name`,
`site_id`, `creative_name`, etc. **LiveOps Campaigns is a paid Tenjin
feature** — contact your Tenjin account manager to enable it.

### <a id="deep-links"></a>Deep links

Bind the multicast delegate from any `UObject` (or from a Blueprint via
*Get Tenjin Delegates → On Deep Link Received → Bind Event*), then install
Tenjin's handler:

```cpp
UTenjinDelegates::Get()->OnDeepLinkReceived.AddDynamic(this, &UMyClass::HandleDeepLink);
UTenjinBPLibrary::RegisterDeepLinkHandler();
```

```cpp
UFUNCTION()
void UMyClass::HandleDeepLink(const FString& JsonPayload)
{
    // keys include clicked_tenjin_link, is_first_session,
    // deferred_deeplink_url, plus the standard attribution context.
}
```

### <a id="user-profile"></a>User Profile — LiveOps Metrics

Tenjin automatically tracks per-user engagement metrics (session count,
session length, IAP totals by currency, ad revenue by network).

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|UserProfile")
static void GetUserProfileDictionary(const FTenjinStringDelegate& Callback);

UFUNCTION(BlueprintCallable, Category = "Tenjin|UserProfile")
static void ResetUserProfile();
```

```cpp
FTenjinStringDelegate Cb;
Cb.BindLambda([](const FString& Json)
{
    // Parse Json for keys:
    //   session_count, total_session_time, average_session_length,
    //   last_session_length, iap_transaction_count, total_ilrd_revenue_usd,
    //   first_session_date, last_session_date, current_session_length,
    //   iap_revenue_by_currency, purchased_product_ids, ilrd_revenue_by_network
});
UTenjinBPLibrary::GetUserProfileDictionary(Cb);
```

### <a id="customer-user-id"></a>Customer User ID

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
static void SetCustomerUserId(const FString& UserId);

UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
static void GetCustomerUserId(const FTenjinStringDelegate& Callback);
```

### <a id="analytics-installation-id"></a>Analytics Installation ID

A random GUID that Tenjin assigns at first launch. Useful as a stable
identifier when advertising ids are unavailable.

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
static void GetAnalyticsInstallationId(const FTenjinStringDelegate& Callback);
```

### <a id="append-app-subversion"></a>App Subversion

Track sub-versions of your app — useful for A/B testing builds within the
same store version.

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
static void AppendAppSubversion(int32 Subversion);
```

### <a id="cache-encrypt"></a>Retry/cache events

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
static void SetCacheEventSetting(bool bSetting);

UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
static void SetEncryptRequestsSetting(bool bSetting);
```

`SetCacheEventSetting(true)` enables retry-on-failure for events sent while
the device is offline.

### <a id="privacy-opt"></a>GDPR (opt-in / opt-out)

```cpp
UTenjinBPLibrary::OptIn();
UTenjinBPLibrary::OptOut();
UTenjinBPLibrary::OptInParams(TArray<FString>{TEXT("country"), TEXT("device_id")});
UTenjinBPLibrary::OptOutParams(TArray<FString>{TEXT("country"), TEXT("device_id")});
```

### <a id="privacy-dma"></a>Google DMA parameters

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
static void SetGoogleDMAParameters(bool bAdPersonalization, bool bAdUserData);

UTenjinBPLibrary::OptInGoogleDMA();
UTenjinBPLibrary::OptOutGoogleDMA();
```

### <a id="privacy-cmp"></a>Opt in/out using CMP

Drive opt-in/opt-out from the IAB TCF v2 consent string emitted by your CMP:

```cpp
UTenjinBPLibrary::OptInOutUsingCMP();
```

### <a id="skadnetwork"></a>SKAdNetwork and Conversion Values (iOS)

iOS SKAdNetwork postback updates. Pre-iOS-16.1 SDKs ignore `CoarseValue` and
`bLockWindow`.

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|SKAdNetwork")
static void UpdatePostbackConversionValue(int32 ConversionValue);

UFUNCTION(BlueprintCallable, Category = "Tenjin|SKAdNetwork")
static void UpdatePostbackConversionValueWithCoarseValue(
    int32 ConversionValue,
    ETenjinCoarseConversionValue CoarseValue,
    bool bLockWindow);
```

`ConversionValue` is a 0–63 integer (6 bits). `CoarseValue` is one of
`Low` / `Medium` / `High` / `None`.

### <a id="ilrd"></a>Impression-Level Ad Revenue (ILRD)

ILRD passthroughs are exposed only for ad mediation platforms that ship
an Unreal Engine plugin (otherwise there's no realistic way for a UE app
to obtain the impression JSON). Serialise the network's impression
dictionary to JSON in your app code and hand it to the matching method:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
static void EventAdImpressionAdMob(const FString& JsonPayload);

UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
static void EventAdImpressionAppLovin(const FString& JsonPayload);

UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
static void EventAdImpressionCAS(const FString& JsonPayload);
```

Supported networks and their UE plugins:

| Network | UE plugin |
|---------|-----------|
| **AppLovin MAX** | [AppLovin/AppLovin-MAX-Unreal](https://github.com/AppLovin/AppLovin-MAX-Unreal) |
| **Google AdMob** | Community / marketplace UE plugins (no official Google plugin) |
| **CAS** (Clever Ads Solutions) | [cleveradssolutions/CAS-Unreal](https://github.com/cleveradssolutions/CAS-Unreal) |

The other ILRD networks supported by Tenjin's native iOS/Android SDKs
(IronSource / Unity LevelPlay, HyperBid, TopOn, TradPlus, CloudX) ship
only Unity SDKs at this time, so they're intentionally omitted here. If
upstream publishes a UE plugin for any of them, the corresponding method
is one drop-in addition — the Tenjin native side already supports it.

> ILRD is a paid Tenjin feature — please contact your Tenjin account
> manager before sending ILRD events in production.

### <a id="app-store"></a>App Store (Google Play / Amazon / Other)

Tenjin supports three Android app-store values: `googleplay`, `amazon`, and
`other` (for Huawei AppGallery and other stores). The plugin defaults the
project to `googleplay` via `UTenjinSettings::AndroidAppStore`. To switch
at runtime:

```cpp
UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
static void SetAppStore(ETenjinAppStore Type);

// ETenjinAppStore::GooglePlay | Amazon | Other
```

> If you are publishing in an Android store other than Google Play, update
> the value before calling `Connect()` so the install is attributed to the
> right store.

## <a id="sample-app"></a>Sample app

A working example project lives under `Sample/TenjinSample/`. It mounts an
on-screen panel with one button per SDK method.

```bash
cd Sample/TenjinSample
open TenjinSample.uproject     # macOS — opens in Unreal Editor
```

Set your SDK key under *Project Settings → Plugins → Tenjin SDK* (or paste
it into `Config/DefaultEngine.ini`) before tapping **Initialize**.

For one-shot device testing the repo ships two helper scripts:

```bash
./Scripts/test-ios.sh --deploy   # packages a signed .ipa + deploys + streams device console
./Scripts/test-android.sh        # packages, installs, launches, tails logcat
```

Both auto-locate Unreal under `/Users/Shared/Epic Games/UE_5.*` — set
`UE_ROOT` if your install is elsewhere. Pass `--no-package` to skip the
build step on repeat runs. See [CONTRIBUTING.md](CONTRIBUTING.md#packaging)
for the full flag set.

## <a id="testing"></a>Testing

You can verify your integration is working through Tenjin's
[Live Test Device Data Tool][tenjin-sdk-diagnostics]. Add your
`advertising_id` (Android) or `IDFA` (iOS) under
*Support → [Test Devices][tenjin-test-devices]*, then send the test events
from your app. You should see live events come in on the SDK Live page.

The sample app's on-screen panel exercises every public method in
`UTenjinBPLibrary`, so it's the fastest way to confirm Initialize +
Connect + a custom event are flowing end-to-end.

## <a id="engine-support"></a>Engine version support

| UE version | Status |
|------------|--------|
| 5.7 | Primary |
| 5.6 | Primary |
| 5.5 | Primary |
| 5.4 | Primary |
| 5.3 | Primary |
| 5.0 – 5.2 | Best-effort |
| 4.27 | Not supported in v1 |

The sample app's `*.Target.cs` files are pinned to
`BuildSettingsVersion.V6` / `EngineIncludeOrderVersion.Unreal5_7`. If you
open the sample in an older engine, lower those two values to match (e.g.
`V4` / `Unreal5_3` for UE 5.3).

## <a id="support"></a>Support

If you have any issues with the plugin integration or usage, please
contact us at <support@tenjin.com>.

## <a id="license"></a>License

The Tenjin Unreal Engine SDK is licensed under the MIT License. See
[LICENSE](LICENSE).

[tenjin]: https://www.tenjin.com
[tenjin-dashboard]: https://www.tenjin.com/dashboard/apps
[tenjin-ios]: https://github.com/tenjin/tenjin-ios-sdk
[tenjin-android]: https://github.com/tenjin/tenjin-android-sdk
[tenjin-sdk-diagnostics]: https://www.tenjin.com/dashboard/sdk_diagnostics
[tenjin-test-devices]: https://www.tenjin.com/dashboard/debug_app_users
[thirdparty-readme]: TenjinSDK/ThirdParty/iOS/README.md

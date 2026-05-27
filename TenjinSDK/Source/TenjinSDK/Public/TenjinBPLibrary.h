// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TenjinBPLibrary.generated.h"

UENUM(BlueprintType)
enum class ETenjinAppStore : uint8
{
	GooglePlay UMETA(DisplayName = "Google Play"),
	Amazon     UMETA(DisplayName = "Amazon"),
	Other      UMETA(DisplayName = "Other"),
};

UENUM(BlueprintType)
enum class ETenjinCoarseConversionValue : uint8
{
	None   UMETA(DisplayName = "None"),
	Low    UMETA(DisplayName = "Low"),
	Medium UMETA(DisplayName = "Medium"),
	High   UMETA(DisplayName = "High"),
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FTenjinAttributionInfoDelegate, bool, bSuccess, const FString&, JsonResult);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTenjinStringDelegate, const FString&, Result);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FTenjinSubscriptionResultDelegate, bool, bSuccess, const FString&, Error);

/**
 * Blueprint-callable API for the Tenjin SDK.
 *
 * All methods are static and thread-safe to call from the game thread.
 * On platforms other than iOS and Android the methods are no-ops.
 *
 * Typical flow:
 *   1. Tenjin::Initialize("YOUR_API_KEY")
 *   2. Tenjin::Connect()                          // call after granting/denying ATT on iOS
 *   3. Tenjin::EventWithName("level_complete")
 */
UCLASS()
class TENJINSDK_API UTenjinBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------
	// Initialization
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin")
	static void Initialize(const FString& SdkKey);

	UFUNCTION(BlueprintCallable, Category = "Tenjin")
	static void Connect();

	UFUNCTION(BlueprintCallable, Category = "Tenjin")
	static void ConnectWithDeferredDeeplink(const FString& DeferredDeeplink);

	// ------------------------------------------------------------------
	// Privacy / consent
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptIn();

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptOut();

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptInParams(const TArray<FString>& Params);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptOutParams(const TArray<FString>& Params);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptInOutUsingCMP();

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptInGoogleDMA();

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void OptOutGoogleDMA();

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Privacy")
	static void SetGoogleDMAParameters(bool bAdPersonalization, bool bAdUserData);

	// ------------------------------------------------------------------
	// Platform options
	// ------------------------------------------------------------------

	/** Android only — has no effect on iOS. */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
	static void SetAppStore(ETenjinAppStore Type);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
	static void SetCacheEventSetting(bool bSetting);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
	static void SetEncryptRequestsSetting(bool bSetting);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Config")
	static void AppendAppSubversion(int32 Subversion);

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Events")
	static void EventWithName(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Events")
	static void EventWithNameAndValue(const FString& Name, int32 Value);

	// ------------------------------------------------------------------
	// Transactions / subscriptions
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
	static void Transaction(
		const FString& ProductName,
		const FString& CurrencyCode,
		int32 Quantity,
		float UnitPrice);

	/** iOS — pass StoreKit transactionId and base64-encoded receipt. */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
	static void TransactionWithReceipt(
		const FString& ProductName,
		const FString& CurrencyCode,
		int32 Quantity,
		float UnitPrice,
		const FString& TransactionId,
		const FString& Base64Receipt);

	/** Android — pass Play Billing purchaseData (JSON) and dataSignature. */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
	static void TransactionWithDataSignature(
		const FString& ProductName,
		const FString& CurrencyCode,
		int32 Quantity,
		float UnitPrice,
		const FString& PurchaseData,
		const FString& DataSignature);

	/**
	 * Track a subscription. Pass iOS-only or Android-only params depending on platform;
	 * leave the others empty. The native bridge ignores unsupported params.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
	static void Subscription(
		const FString& ProductId,
		const FString& CurrencyCode,
		float UnitPrice,
		const FString& IosTransactionId,
		const FString& IosOriginalTransactionId,
		const FString& IosReceipt,
		const FString& IosSKTransaction,
		const FString& AndroidPurchaseToken,
		const FString& AndroidPurchaseData,
		const FString& AndroidDataSignature);

	/** iOS 16+ only. Fetches the latest StoreKit 2 transaction natively, then sends. */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Transactions")
	static void SubscriptionWithStoreKit(
		const FString& ProductId,
		const FString& CurrencyCode,
		float UnitPrice,
		const FTenjinSubscriptionResultDelegate& Callback);

	// ------------------------------------------------------------------
	// SKAdNetwork (iOS)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|SKAdNetwork")
	static void UpdatePostbackConversionValue(int32 ConversionValue);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|SKAdNetwork")
	static void UpdatePostbackConversionValueWithCoarseValue(
		int32 ConversionValue,
		ETenjinCoarseConversionValue CoarseValue,
		bool bLockWindow);

	// ------------------------------------------------------------------
	// Customer / installation IDs
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
	static void SetCustomerUserId(const FString& UserId);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
	static void GetCustomerUserId(const FTenjinStringDelegate& Callback);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|User")
	static void GetAnalyticsInstallationId(const FTenjinStringDelegate& Callback);

	// ------------------------------------------------------------------
	// Attribution
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|Attribution")
	static void GetAttributionInfo(const FTenjinAttributionInfoDelegate& Callback);

	/**
	 * Register for Tenjin deep-link click resolutions. Once installed, results
	 * arrive on the UTenjinDelegates::OnDeepLinkReceived multicast delegate.
	 * Call after Initialize() and before (or right after) Connect().
	 */
	UFUNCTION(BlueprintCallable, Category = "Tenjin|Attribution")
	static void RegisterDeepLinkHandler();

	// ------------------------------------------------------------------
	// User profile (LiveOps metrics)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Tenjin|UserProfile")
	static void GetUserProfileDictionary(const FTenjinStringDelegate& Callback);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|UserProfile")
	static void ResetUserProfile();

	// ------------------------------------------------------------------
	// Impression-Level Ad Revenue (ILRD)
	//
	// JsonPayload is the raw network JSON (e.g. the AdMob impression dict
	// serialised to a string). The native SDKs parse it.
	// ------------------------------------------------------------------

	// Only ad mediation platforms that ship an Unreal Engine plugin:
	//   - AppLovin MAX → github.com/AppLovin/AppLovin-MAX-Unreal
	//   - AdMob        → Google Mobile Ads (community/marketplace UE plugins)
	//   - CAS          → github.com/cleveradssolutions/CAS-Unreal
	// The other networks Tenjin supports natively (IronSource, HyperBid,
	// TopOn, TradPlus, CloudX) ship only Unity SDKs, so passing impression
	// JSON from a UE app isn't realistic — methods omitted.

	UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
	static void EventAdImpressionAdMob(const FString& JsonPayload);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
	static void EventAdImpressionAppLovin(const FString& JsonPayload);

	UFUNCTION(BlueprintCallable, Category = "Tenjin|ILRD")
	static void EventAdImpressionCAS(const FString& JsonPayload);
};

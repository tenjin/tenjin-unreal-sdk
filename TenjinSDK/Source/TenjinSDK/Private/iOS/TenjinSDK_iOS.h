// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#if PLATFORM_IOS

#include "CoreMinimal.h"

namespace TenjinIOS
{
	void Initialize(const FString& SdkKey);
	void Connect();
	void ConnectWithDeferredDeeplink(const FString& DeferredDeeplink);

	void OptIn();
	void OptOut();
	void OptInParams(const TArray<FString>& Params);
	void OptOutParams(const TArray<FString>& Params);
	void OptInOutUsingCMP();
	void OptInGoogleDMA();
	void OptOutGoogleDMA();
	void SetGoogleDMAParameters(bool bAdPersonalization, bool bAdUserData);

	void SetCacheEventSetting(bool bSetting);
	void SetEncryptRequestsSetting(bool bSetting);
	void AppendAppSubversion(int32 Subversion);

	void EventWithName(const FString& Name);
	void EventWithNameAndValue(const FString& Name, int32 Value);

	void Transaction(const FString& ProductName, const FString& CurrencyCode,
	                 int32 Quantity, float UnitPrice);
	void TransactionWithReceipt(const FString& ProductName, const FString& CurrencyCode,
	                            int32 Quantity, float UnitPrice,
	                            const FString& TransactionId, const FString& Base64Receipt);
	void Subscription(const FString& ProductId, const FString& CurrencyCode, float UnitPrice,
	                  const FString& TransactionId, const FString& OriginalTransactionId,
	                  const FString& Receipt, const FString& SKTransaction);

	using FSubscriptionCompletion = TFunction<void(bool /*bSuccess*/, const FString& /*Error*/)>;
	void SubscriptionWithStoreKit(const FString& ProductId, const FString& CurrencyCode,
	                              float UnitPrice, FSubscriptionCompletion Completion);

	void UpdatePostbackConversionValue(int32 ConversionValue);
	void UpdatePostbackConversionValueWithCoarseValue(int32 ConversionValue,
	                                                  const FString& CoarseValue,
	                                                  bool bLockWindow);

	void SetCustomerUserId(const FString& UserId);
	FString GetCustomerUserId();
	FString GetAnalyticsInstallationId();

	using FAttributionCompletion = TFunction<void(bool /*bSuccess*/, const FString& /*Json*/)>;
	void GetAttributionInfo(FAttributionCompletion Completion);

	using FDeepLinkBroadcast = TFunction<void(const FString& /*Json*/)>;
	void RegisterDeepLinkHandler(FDeepLinkBroadcast Broadcast);

	FString GetUserProfileDictionaryJson();
	void ResetUserProfile();

	void EventAdImpressionAdMob(const FString& Json);
	void EventAdImpressionAppLovin(const FString& Json);
	void EventAdImpressionCAS(const FString& Json);
}

#endif

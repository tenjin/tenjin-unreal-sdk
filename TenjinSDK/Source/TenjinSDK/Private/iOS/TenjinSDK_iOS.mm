// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinSDK_iOS.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#import <TenjinSDK/TenjinSDK.h>
// ILRD passthroughs live in per-network category headers, not the main
// class. Only networks that ship an Unreal Engine plugin are exposed.
#import <TenjinSDK/TenjinSDK+AdMobILRD.h>
#import <TenjinSDK/TenjinSDK+AppLovinILRD.h>
#import <TenjinSDK/TenjinSDK+CASILRD.h>

namespace
{
	NSString* ToNSString(const FString& S)
	{
		return S.GetNSString();
	}

	FString FromNSString(NSString* S)
	{
		return S ? FString(S) : FString();
	}

	NSArray<NSString*>* ToNSArray(const TArray<FString>& Items)
	{
		NSMutableArray<NSString*>* Arr = [NSMutableArray arrayWithCapacity:Items.Num()];
		for (const FString& Item : Items)
		{
			[Arr addObject:ToNSString(Item)];
		}
		return Arr;
	}

	NSString* JsonStringFromDictionary(NSDictionary* Dict)
	{
		if (!Dict) return @"{}";
		NSError* Error = nil;
		NSData* Data = [NSJSONSerialization dataWithJSONObject:Dict options:0 error:&Error];
		if (!Data) return @"{}";
		return [[NSString alloc] initWithData:Data encoding:NSUTF8StringEncoding];
	}
}

namespace TenjinIOS
{

void Initialize(const FString& SdkKey)
{
	[TenjinSDK getInstance:ToNSString(SdkKey)];
}

void Connect()
{
	[TenjinSDK connect];
}

void ConnectWithDeferredDeeplink(const FString& DeferredDeeplink)
{
	NSURL* Url = [NSURL URLWithString:ToNSString(DeferredDeeplink)];
	[TenjinSDK connectWithDeferredDeeplink:Url];
}

void OptIn()  { [TenjinSDK optIn]; }
void OptOut() { [TenjinSDK optOut]; }

void OptInParams(const TArray<FString>& Params)
{
	[TenjinSDK optInParams:ToNSArray(Params)];
}

void OptOutParams(const TArray<FString>& Params)
{
	[TenjinSDK optOutParams:ToNSArray(Params)];
}

void OptInOutUsingCMP()   { [TenjinSDK optInOutUsingCMP]; }
void OptInGoogleDMA()     { [TenjinSDK optInGoogleDMA]; }
void OptOutGoogleDMA()    { [TenjinSDK optOutGoogleDMA]; }

void SetGoogleDMAParameters(bool bAdPersonalization, bool bAdUserData)
{
	[[TenjinSDK sharedInstance] setGoogleDMAParametersWithAdPersonalization:bAdPersonalization
	                                                             adUserData:bAdUserData];
}

void SetCacheEventSetting(bool bSetting)       { [TenjinSDK setCacheEventSetting:bSetting]; }
void SetEncryptRequestsSetting(bool bSetting)  { [TenjinSDK setEncryptRequestsSetting:bSetting]; }

void AppendAppSubversion(int32 Subversion)
{
	[TenjinSDK appendAppSubversion:[NSNumber numberWithInt:Subversion]];
}

void EventWithName(const FString& Name)
{
	[TenjinSDK sendEventWithName:ToNSString(Name)];
}

void EventWithNameAndValue(const FString& Name, int32 Value)
{
	[TenjinSDK sendEventWithName:ToNSString(Name) andValue:(NSInteger)Value];
}

void Transaction(const FString& ProductName, const FString& CurrencyCode,
                 int32 Quantity, float UnitPrice)
{
	NSDecimalNumber* Price = [[NSDecimalNumber alloc] initWithDouble:UnitPrice];
	[TenjinSDK transactionWithProductName:ToNSString(ProductName)
	                      andCurrencyCode:ToNSString(CurrencyCode)
	                          andQuantity:(NSInteger)Quantity
	                         andUnitPrice:Price];
}

void TransactionWithReceipt(const FString& ProductName, const FString& CurrencyCode,
                            int32 Quantity, float UnitPrice,
                            const FString& TransactionId, const FString& Base64Receipt)
{
	NSDecimalNumber* Price = [[NSDecimalNumber alloc] initWithDouble:UnitPrice];
	[TenjinSDK transactionWithProductName:ToNSString(ProductName)
	                      andCurrencyCode:ToNSString(CurrencyCode)
	                          andQuantity:(NSInteger)Quantity
	                         andUnitPrice:Price
	                     andTransactionId:ToNSString(TransactionId)
	                     andBase64Receipt:ToNSString(Base64Receipt)];
}

void Subscription(const FString& ProductId, const FString& CurrencyCode, float UnitPrice,
                  const FString& TransactionId, const FString& OriginalTransactionId,
                  const FString& Receipt, const FString& SKTransaction)
{
	NSDecimalNumber* Price = [[NSDecimalNumber alloc] initWithDouble:UnitPrice];
	[TenjinSDK subscriptionWithProductName:ToNSString(ProductId)
	                       andCurrencyCode:ToNSString(CurrencyCode)
	                          andUnitPrice:Price
	                      andTransactionId:ToNSString(TransactionId)
	              andOriginalTransactionId:ToNSString(OriginalTransactionId)
	                      andBase64Receipt:ToNSString(Receipt)
	                      andSKTransaction:ToNSString(SKTransaction)];
}

void SubscriptionWithStoreKit(const FString& ProductId, const FString& CurrencyCode,
                              float UnitPrice, FSubscriptionCompletion Completion)
{
	// TenjinSDK's StoreKit 2 helper fetches the latest SK2 transaction natively.
	// The native API is fire-and-forget (no completion handler) and requires
	// iOS 16+, so we report success once the call is dispatched.
	if (@available(iOS 16.0, *))
	{
		NSDecimalNumber* Price = [[NSDecimalNumber alloc] initWithDouble:UnitPrice];
		[TenjinSDK subscriptionWithStoreKitForProductId:ToNSString(ProductId)
		                               andCurrencyCode:ToNSString(CurrencyCode)
		                                  andUnitPrice:Price];
		Completion(true, FString());
	}
	else
	{
		Completion(false, FString(TEXT("subscriptionWithStoreKit requires iOS 16.0 or later")));
	}
}

void UpdatePostbackConversionValue(int32 ConversionValue)
{
	[TenjinSDK updatePostbackConversionValue:(int)ConversionValue];
}

void UpdatePostbackConversionValueWithCoarseValue(int32 ConversionValue,
                                                  const FString& CoarseValue,
                                                  bool bLockWindow)
{
	[TenjinSDK updatePostbackConversionValue:(int)ConversionValue
	                             coarseValue:ToNSString(CoarseValue)
	                              lockWindow:bLockWindow];
}

void SetCustomerUserId(const FString& UserId)
{
	[TenjinSDK setCustomerUserId:ToNSString(UserId)];
}

FString GetCustomerUserId()
{
	return FromNSString([TenjinSDK getCustomerUserId]);
}

FString GetAnalyticsInstallationId()
{
	return FromNSString([TenjinSDK getAnalyticsInstallationId]);
}

void GetAttributionInfo(FAttributionCompletion Completion)
{
	[[TenjinSDK sharedInstance] getAttributionInfo:^(NSDictionary* Info, NSError* Error) {
		if (Error || !Info)
		{
			Completion(false, FromNSString(Error.localizedDescription ?: @"No attribution info"));
			return;
		}
		Completion(true, FromNSString(JsonStringFromDictionary(Info)));
	}];
}

void RegisterDeepLinkHandler(FDeepLinkBroadcast Broadcast)
{
	// registerDeepLinkHandler: is an instance method on the shared instance.
	[[TenjinSDK sharedInstance] registerDeepLinkHandler:^(NSDictionary* Params, NSError* Error)
	{
		if (Error || !Params) return;
		Broadcast(FromNSString(JsonStringFromDictionary(Params)));
	}];
}

FString GetUserProfileDictionaryJson()
{
	NSDictionary* Dict = [TenjinSDK getUserProfileAsDictionary];
	return FromNSString(JsonStringFromDictionary(Dict));
}

void ResetUserProfile()
{
	[TenjinSDK resetUserProfile];
}

void EventAdImpressionAdMob(const FString& Json)    { [TenjinSDK adMobImpressionFromJSON:ToNSString(Json)]; }
void EventAdImpressionAppLovin(const FString& Json) { [TenjinSDK appLovinImpressionFromJSON:ToNSString(Json)]; }
void EventAdImpressionCAS(const FString& Json)      { [TenjinSDK casImpressionFromJSON:ToNSString(Json)]; }

} // namespace TenjinIOS

#endif // PLATFORM_IOS

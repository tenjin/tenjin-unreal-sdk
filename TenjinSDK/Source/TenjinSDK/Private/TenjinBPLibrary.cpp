// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinBPLibrary.h"
#include "TenjinDelegates.h"
#include "Async/Async.h"

#if PLATFORM_IOS
#include "iOS/TenjinSDK_iOS.h"
#endif

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidJNI.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTenjin, Log, All);

namespace
{
	FString CoarseValueToString(ETenjinCoarseConversionValue V)
	{
		switch (V)
		{
			case ETenjinCoarseConversionValue::Low:    return TEXT("low");
			case ETenjinCoarseConversionValue::Medium: return TEXT("medium");
			case ETenjinCoarseConversionValue::High:   return TEXT("high");
			default:                                   return FString();
		}
	}

	FString AppStoreToString(ETenjinAppStore S)
	{
		switch (S)
		{
			case ETenjinAppStore::GooglePlay: return TEXT("googleplay");
			case ETenjinAppStore::Amazon:     return TEXT("amazon");
			default:                          return TEXT("other");
		}
	}
}

// =========================================================================
// Android JNI helpers
// =========================================================================
#if PLATFORM_ANDROID

namespace TenjinAndroid
{
	// Java helper class injected via TenjinSDK_UPL_Android.xml. It owns the
	// native TenjinSDK instance and exposes a stable API the JNI code calls.
	const char* kBridgeClass = "com/tenjin/unreal/TenjinSDKBridge";

	jclass GetBridgeClass(JNIEnv* Env)
	{
		jclass LocalRef = AndroidJavaEnv::FindJavaClass(kBridgeClass);
		if (!LocalRef)
		{
			UE_LOG(LogTenjin, Error, TEXT("Failed to find Java class %hs"), kBridgeClass);
		}
		return LocalRef;
	}

	void CallStaticVoid(const char* Method, const char* Sig, ...)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env) return;
		jclass Cls = GetBridgeClass(Env);
		if (!Cls) return;
		jmethodID Id = Env->GetStaticMethodID(Cls, Method, Sig);
		if (!Id) { Env->DeleteLocalRef(Cls); return; }

		va_list Args;
		va_start(Args, Sig);
		Env->CallStaticVoidMethodV(Cls, Id, Args);
		va_end(Args);

		Env->DeleteLocalRef(Cls);
	}

	jstring NewJString(JNIEnv* Env, const FString& S)
	{
		return Env->NewStringUTF(TCHAR_TO_UTF8(*S));
	}

	FString JStringToFString(JNIEnv* Env, jstring JStr)
	{
		if (!JStr) return FString();
		const char* Chars = Env->GetStringUTFChars(JStr, nullptr);
		FString Out = FString(UTF8_TO_TCHAR(Chars));
		Env->ReleaseStringUTFChars(JStr, Chars);
		return Out;
	}

	jstring CallStaticString(const char* Method)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env) return nullptr;
		jclass Cls = GetBridgeClass(Env);
		if (!Cls) return nullptr;
		jmethodID Id = Env->GetStaticMethodID(Cls, Method, "()Ljava/lang/String;");
		if (!Id) { Env->DeleteLocalRef(Cls); return nullptr; }
		jstring Result = (jstring)Env->CallStaticObjectMethod(Cls, Id);
		Env->DeleteLocalRef(Cls);
		return Result;
	}
}

#endif // PLATFORM_ANDROID

// =========================================================================
// Public API
// =========================================================================

void UTenjinBPLibrary::Initialize(const FString& SdkKey)
{
#if PLATFORM_IOS
	TenjinIOS::Initialize(SdkKey);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jobject Activity = AndroidJavaEnv::GetGameActivityThis();
	if (!Activity)
	{
		UE_LOG(LogTenjin, Error, TEXT("Tenjin.Initialize: GameActivity not available yet"));
		return;
	}
	jstring JKey = TenjinAndroid::NewJString(Env, SdkKey);
	TenjinAndroid::CallStaticVoid("initialize", "(Landroid/app/Activity;Ljava/lang/String;)V", Activity, JKey);
	Env->DeleteLocalRef(JKey);
#else
	UE_LOG(LogTenjin, Verbose, TEXT("Initialize(%s) ignored on this platform"), *SdkKey);
#endif
}

void UTenjinBPLibrary::Connect()
{
#if PLATFORM_IOS
	TenjinIOS::Connect();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("connect", "()V");
#endif
}

void UTenjinBPLibrary::ConnectWithDeferredDeeplink(const FString& DeferredDeeplink)
{
#if PLATFORM_IOS
	TenjinIOS::ConnectWithDeferredDeeplink(DeferredDeeplink);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring J = TenjinAndroid::NewJString(Env, DeferredDeeplink);
	TenjinAndroid::CallStaticVoid("connectWithDeferredDeeplink", "(Ljava/lang/String;)V", J);
	Env->DeleteLocalRef(J);
#endif
}

void UTenjinBPLibrary::OptIn()
{
#if PLATFORM_IOS
	TenjinIOS::OptIn();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("optIn", "()V");
#endif
}

void UTenjinBPLibrary::OptOut()
{
#if PLATFORM_IOS
	TenjinIOS::OptOut();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("optOut", "()V");
#endif
}

void UTenjinBPLibrary::OptInParams(const TArray<FString>& Params)
{
#if PLATFORM_IOS
	TenjinIOS::OptInParams(Params);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jclass StringCls = Env->FindClass("java/lang/String");
	jobjectArray JArr = Env->NewObjectArray(Params.Num(), StringCls, nullptr);
	for (int32 i = 0; i < Params.Num(); ++i)
	{
		jstring Item = TenjinAndroid::NewJString(Env, Params[i]);
		Env->SetObjectArrayElement(JArr, i, Item);
		Env->DeleteLocalRef(Item);
	}
	TenjinAndroid::CallStaticVoid("optInParams", "([Ljava/lang/String;)V", JArr);
	Env->DeleteLocalRef(JArr);
	Env->DeleteLocalRef(StringCls);
#endif
}

void UTenjinBPLibrary::OptOutParams(const TArray<FString>& Params)
{
#if PLATFORM_IOS
	TenjinIOS::OptOutParams(Params);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jclass StringCls = Env->FindClass("java/lang/String");
	jobjectArray JArr = Env->NewObjectArray(Params.Num(), StringCls, nullptr);
	for (int32 i = 0; i < Params.Num(); ++i)
	{
		jstring Item = TenjinAndroid::NewJString(Env, Params[i]);
		Env->SetObjectArrayElement(JArr, i, Item);
		Env->DeleteLocalRef(Item);
	}
	TenjinAndroid::CallStaticVoid("optOutParams", "([Ljava/lang/String;)V", JArr);
	Env->DeleteLocalRef(JArr);
	Env->DeleteLocalRef(StringCls);
#endif
}

void UTenjinBPLibrary::OptInOutUsingCMP()
{
#if PLATFORM_IOS
	TenjinIOS::OptInOutUsingCMP();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("optInOutUsingCMP", "()V");
#endif
}

void UTenjinBPLibrary::OptInGoogleDMA()
{
#if PLATFORM_IOS
	TenjinIOS::OptInGoogleDMA();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("optInGoogleDMA", "()V");
#endif
}

void UTenjinBPLibrary::OptOutGoogleDMA()
{
#if PLATFORM_IOS
	TenjinIOS::OptOutGoogleDMA();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("optOutGoogleDMA", "()V");
#endif
}

void UTenjinBPLibrary::SetGoogleDMAParameters(bool bAdPersonalization, bool bAdUserData)
{
#if PLATFORM_IOS
	TenjinIOS::SetGoogleDMAParameters(bAdPersonalization, bAdUserData);
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("setGoogleDMAParameters", "(ZZ)V",
		(jboolean)bAdPersonalization, (jboolean)bAdUserData);
#endif
}

void UTenjinBPLibrary::SetAppStore(ETenjinAppStore Type)
{
#if PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring J = TenjinAndroid::NewJString(Env, AppStoreToString(Type));
	TenjinAndroid::CallStaticVoid("setAppStore", "(Ljava/lang/String;)V", J);
	Env->DeleteLocalRef(J);
#else
	(void)Type;
#endif
}

void UTenjinBPLibrary::SetCacheEventSetting(bool bSetting)
{
#if PLATFORM_IOS
	TenjinIOS::SetCacheEventSetting(bSetting);
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("setCacheEventSetting", "(Z)V", (jboolean)bSetting);
#endif
}

void UTenjinBPLibrary::SetEncryptRequestsSetting(bool bSetting)
{
#if PLATFORM_IOS
	TenjinIOS::SetEncryptRequestsSetting(bSetting);
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("setEncryptRequestsSetting", "(Z)V", (jboolean)bSetting);
#endif
}

void UTenjinBPLibrary::AppendAppSubversion(int32 Subversion)
{
#if PLATFORM_IOS
	TenjinIOS::AppendAppSubversion(Subversion);
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("appendAppSubversion", "(I)V", (jint)Subversion);
#endif
}

void UTenjinBPLibrary::EventWithName(const FString& Name)
{
#if PLATFORM_IOS
	TenjinIOS::EventWithName(Name);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring J = TenjinAndroid::NewJString(Env, Name);
	TenjinAndroid::CallStaticVoid("eventWithName", "(Ljava/lang/String;)V", J);
	Env->DeleteLocalRef(J);
#else
	UE_LOG(LogTenjin, Verbose, TEXT("EventWithName(%s) ignored on this platform"), *Name);
#endif
}

void UTenjinBPLibrary::EventWithNameAndValue(const FString& Name, int32 Value)
{
#if PLATFORM_IOS
	TenjinIOS::EventWithNameAndValue(Name, Value);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring J = TenjinAndroid::NewJString(Env, Name);
	TenjinAndroid::CallStaticVoid("eventWithNameAndValue", "(Ljava/lang/String;I)V", J, (jint)Value);
	Env->DeleteLocalRef(J);
#endif
}

void UTenjinBPLibrary::Transaction(
	const FString& ProductName,
	const FString& CurrencyCode,
	int32 Quantity,
	float UnitPrice)
{
#if PLATFORM_IOS
	TenjinIOS::Transaction(ProductName, CurrencyCode, Quantity, UnitPrice);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring JP = TenjinAndroid::NewJString(Env, ProductName);
	jstring JC = TenjinAndroid::NewJString(Env, CurrencyCode);
	TenjinAndroid::CallStaticVoid("transaction",
		"(Ljava/lang/String;Ljava/lang/String;ID)V",
		JP, JC, (jint)Quantity, (jdouble)UnitPrice);
	Env->DeleteLocalRef(JP);
	Env->DeleteLocalRef(JC);
#endif
}

void UTenjinBPLibrary::TransactionWithReceipt(
	const FString& ProductName,
	const FString& CurrencyCode,
	int32 Quantity,
	float UnitPrice,
	const FString& TransactionId,
	const FString& Base64Receipt)
{
#if PLATFORM_IOS
	TenjinIOS::TransactionWithReceipt(ProductName, CurrencyCode, Quantity, UnitPrice, TransactionId, Base64Receipt);
#elif PLATFORM_ANDROID
	(void)ProductName; (void)CurrencyCode; (void)Quantity;
	(void)UnitPrice; (void)TransactionId; (void)Base64Receipt;
#endif
}

void UTenjinBPLibrary::TransactionWithDataSignature(
	const FString& ProductName,
	const FString& CurrencyCode,
	int32 Quantity,
	float UnitPrice,
	const FString& PurchaseData,
	const FString& DataSignature)
{
#if PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring JP = TenjinAndroid::NewJString(Env, ProductName);
	jstring JC = TenjinAndroid::NewJString(Env, CurrencyCode);
	jstring JData = TenjinAndroid::NewJString(Env, PurchaseData);
	jstring JSig = TenjinAndroid::NewJString(Env, DataSignature);
	TenjinAndroid::CallStaticVoid("transactionWithDataSignature",
		"(Ljava/lang/String;Ljava/lang/String;IDLjava/lang/String;Ljava/lang/String;)V",
		JP, JC, (jint)Quantity, (jdouble)UnitPrice, JData, JSig);
	Env->DeleteLocalRef(JP);
	Env->DeleteLocalRef(JC);
	Env->DeleteLocalRef(JData);
	Env->DeleteLocalRef(JSig);
#else
	(void)ProductName; (void)CurrencyCode; (void)Quantity;
	(void)UnitPrice; (void)PurchaseData; (void)DataSignature;
#endif
}

void UTenjinBPLibrary::Subscription(
	const FString& ProductId,
	const FString& CurrencyCode,
	float UnitPrice,
	const FString& IosTransactionId,
	const FString& IosOriginalTransactionId,
	const FString& IosReceipt,
	const FString& IosSKTransaction,
	const FString& AndroidPurchaseToken,
	const FString& AndroidPurchaseData,
	const FString& AndroidDataSignature)
{
#if PLATFORM_IOS
	TenjinIOS::Subscription(
		ProductId, CurrencyCode, UnitPrice,
		IosTransactionId, IosOriginalTransactionId,
		IosReceipt, IosSKTransaction);
#elif PLATFORM_ANDROID
	// The Android Tenjin SDK does not currently expose a subscription tracking API;
	// kept here for symmetry. If/when it does, wire through the bridge.
	(void)ProductId; (void)CurrencyCode; (void)UnitPrice;
	(void)AndroidPurchaseToken; (void)AndroidPurchaseData; (void)AndroidDataSignature;
#endif
}

void UTenjinBPLibrary::SubscriptionWithStoreKit(
	const FString& ProductId,
	const FString& CurrencyCode,
	float UnitPrice,
	const FTenjinSubscriptionResultDelegate& Callback)
{
#if PLATFORM_IOS
	TenjinIOS::SubscriptionWithStoreKit(ProductId, CurrencyCode, UnitPrice,
		[Callback](bool bSuccess, const FString& Error)
		{
			AsyncTask(ENamedThreads::GameThread, [Callback, bSuccess, Error]()
			{
				Callback.ExecuteIfBound(bSuccess, Error);
			});
		});
#else
	(void)ProductId; (void)CurrencyCode; (void)UnitPrice;
	AsyncTask(ENamedThreads::GameThread, [Callback]()
	{
		Callback.ExecuteIfBound(false, TEXT("subscriptionWithStoreKit is only available on iOS"));
	});
#endif
}

void UTenjinBPLibrary::UpdatePostbackConversionValue(int32 ConversionValue)
{
#if PLATFORM_IOS
	TenjinIOS::UpdatePostbackConversionValue(ConversionValue);
#else
	(void)ConversionValue;
#endif
}

void UTenjinBPLibrary::UpdatePostbackConversionValueWithCoarseValue(
	int32 ConversionValue,
	ETenjinCoarseConversionValue CoarseValue,
	bool bLockWindow)
{
#if PLATFORM_IOS
	const FString Coarse = CoarseValueToString(CoarseValue);
	if (CoarseValue == ETenjinCoarseConversionValue::None)
	{
		TenjinIOS::UpdatePostbackConversionValue(ConversionValue);
	}
	else
	{
		TenjinIOS::UpdatePostbackConversionValueWithCoarseValue(ConversionValue, Coarse, bLockWindow);
	}
#else
	(void)ConversionValue; (void)CoarseValue; (void)bLockWindow;
#endif
}

void UTenjinBPLibrary::SetCustomerUserId(const FString& UserId)
{
#if PLATFORM_IOS
	TenjinIOS::SetCustomerUserId(UserId);
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return;
	jstring J = TenjinAndroid::NewJString(Env, UserId);
	TenjinAndroid::CallStaticVoid("setCustomerUserId", "(Ljava/lang/String;)V", J);
	Env->DeleteLocalRef(J);
#endif
}

void UTenjinBPLibrary::GetCustomerUserId(const FTenjinStringDelegate& Callback)
{
#if PLATFORM_IOS
	const FString Result = TenjinIOS::GetCustomerUserId();
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FString Result;
	if (Env)
	{
		jstring J = TenjinAndroid::CallStaticString("getCustomerUserId");
		Result = TenjinAndroid::JStringToFString(Env, J);
		if (J) Env->DeleteLocalRef(J);
	}
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#else
	AsyncTask(ENamedThreads::GameThread, [Callback]()
	{
		Callback.ExecuteIfBound(FString());
	});
#endif
}

void UTenjinBPLibrary::GetAnalyticsInstallationId(const FTenjinStringDelegate& Callback)
{
#if PLATFORM_IOS
	const FString Result = TenjinIOS::GetAnalyticsInstallationId();
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FString Result;
	if (Env)
	{
		jstring J = TenjinAndroid::CallStaticString("getAnalyticsInstallationId");
		Result = TenjinAndroid::JStringToFString(Env, J);
		if (J) Env->DeleteLocalRef(J);
	}
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#else
	AsyncTask(ENamedThreads::GameThread, [Callback]()
	{
		Callback.ExecuteIfBound(FString());
	});
#endif
}

void UTenjinBPLibrary::GetAttributionInfo(const FTenjinAttributionInfoDelegate& Callback)
{
#if PLATFORM_IOS
	TenjinIOS::GetAttributionInfo([Callback](bool bSuccess, const FString& Json)
	{
		AsyncTask(ENamedThreads::GameThread, [Callback, bSuccess, Json]()
		{
			Callback.ExecuteIfBound(bSuccess, Json);
		});
	});
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FString Json;
	if (Env)
	{
		jstring J = TenjinAndroid::CallStaticString("getAttributionInfoJson");
		Json = TenjinAndroid::JStringToFString(Env, J);
		if (J) Env->DeleteLocalRef(J);
	}
	const bool bSuccess = !Json.IsEmpty();
	AsyncTask(ENamedThreads::GameThread, [Callback, bSuccess, Json]()
	{
		Callback.ExecuteIfBound(bSuccess, Json);
	});
#else
	AsyncTask(ENamedThreads::GameThread, [Callback]()
	{
		Callback.ExecuteIfBound(false, TEXT("{}"));
	});
#endif
}

void UTenjinBPLibrary::GetUserProfileDictionary(const FTenjinStringDelegate& Callback)
{
#if PLATFORM_IOS
	const FString Result = TenjinIOS::GetUserProfileDictionaryJson();
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#elif PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	FString Result;
	if (Env)
	{
		jstring J = TenjinAndroid::CallStaticString("getUserProfileJson");
		Result = TenjinAndroid::JStringToFString(Env, J);
		if (J) Env->DeleteLocalRef(J);
	}
	AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
	{
		Callback.ExecuteIfBound(Result);
	});
#else
	AsyncTask(ENamedThreads::GameThread, [Callback]()
	{
		Callback.ExecuteIfBound(TEXT("{}"));
	});
#endif
}

void UTenjinBPLibrary::ResetUserProfile()
{
#if PLATFORM_IOS
	TenjinIOS::ResetUserProfile();
#elif PLATFORM_ANDROID
	TenjinAndroid::CallStaticVoid("resetUserProfile", "()V");
#endif
}

namespace
{
#if PLATFORM_ANDROID
	void AndroidImpressionJson(const char* Method, const FString& Json)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env) return;
		jstring J = TenjinAndroid::NewJString(Env, Json);
		TenjinAndroid::CallStaticVoid(Method, "(Ljava/lang/String;)V", J);
		Env->DeleteLocalRef(J);
	}
#endif
}

void UTenjinBPLibrary::RegisterDeepLinkHandler()
{
	auto Broadcast = [](const FString& Json)
	{
		AsyncTask(ENamedThreads::GameThread, [Json]()
		{
			if (UTenjinDelegates* D = UTenjinDelegates::Get())
			{
				D->OnDeepLinkReceived.Broadcast(Json);
			}
		});
	};

#if PLATFORM_IOS
	TenjinIOS::RegisterDeepLinkHandler(Broadcast);
#elif PLATFORM_ANDROID
	(void)Broadcast; // Java side calls nativeOnDeepLink directly.
	TenjinAndroid::CallStaticVoid("registerDeepLinkHandler", "()V");
#else
	(void)Broadcast;
#endif
}

#if PLATFORM_ANDROID
extern "C" JNIEXPORT void JNICALL
Java_com_tenjin_unreal_TenjinSDKBridge_nativeOnDeepLink(
	JNIEnv* Env, jclass /*cls*/, jstring JJson)
{
	if (!JJson) return;
	const char* Utf = Env->GetStringUTFChars(JJson, nullptr);
	const FString Json(UTF8_TO_TCHAR(Utf));
	Env->ReleaseStringUTFChars(JJson, Utf);
	AsyncTask(ENamedThreads::GameThread, [Json]()
	{
		if (UTenjinDelegates* D = UTenjinDelegates::Get())
		{
			D->OnDeepLinkReceived.Broadcast(Json);
		}
	});
}
#endif

void UTenjinBPLibrary::EventAdImpressionAdMob(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionAdMob(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionAdMob", JsonPayload);
#endif
}

void UTenjinBPLibrary::EventAdImpressionAppLovin(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionAppLovin(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionAppLovin", JsonPayload);
#endif
}

void UTenjinBPLibrary::EventAdImpressionHyperBid(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionHyperBid(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionHyperBid", JsonPayload);
#endif
}

void UTenjinBPLibrary::EventAdImpressionIronSource(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionIronSource(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionIronSource", JsonPayload);
#endif
}

void UTenjinBPLibrary::EventAdImpressionTopOn(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionTopOn(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionTopOn", JsonPayload);
#endif
}

void UTenjinBPLibrary::EventAdImpressionTradPlus(const FString& JsonPayload)
{
#if PLATFORM_IOS
	TenjinIOS::EventAdImpressionTradPlus(JsonPayload);
#elif PLATFORM_ANDROID
	AndroidImpressionJson("eventAdImpressionTradPlus", JsonPayload);
#endif
}

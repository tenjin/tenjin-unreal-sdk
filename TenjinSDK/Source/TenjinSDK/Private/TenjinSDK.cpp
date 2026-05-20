// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinSDK.h"
#include "TenjinBPLibrary.h"
#include "TenjinSettings.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FTenjinSDKModule"

DEFINE_LOG_CATEGORY_STATIC(LogTenjin, Log, All);

void FTenjinSDKModule::StartupModule()
{
	// Auto-initialize must wait until the UObject system is fully up — reading
	// GetDefault<UTenjinSettings>() during module startup crashes, since the
	// module can load before CDOs and packages exist. OnPostEngineInit fires
	// once the engine is ready, which is still well before any gameplay.
	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([]()
	{
		const UTenjinSettings* Settings = UTenjinSettings::Get();
		if (!Settings || !Settings->bAutoInitialize)
		{
			return;
		}
		if (Settings->SdkKey.IsEmpty())
		{
			UE_LOG(LogTenjin, Warning, TEXT("Tenjin: AutoInitialize is enabled but SdkKey is empty"));
			return;
		}
		UTenjinBPLibrary::SetAppStore(Settings->AndroidAppStore);
		UTenjinBPLibrary::Initialize(Settings->SdkKey);
		UTenjinBPLibrary::Connect();
	});
}

void FTenjinSDKModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTenjinSDKModule, TenjinSDK)

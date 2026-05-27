// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TenjinBPLibrary.h"
#include "TenjinSettings.generated.h"

/**
 * Project-settings page for the Tenjin plugin.
 *
 * Accessible via Edit → Project Settings → Plugins → Tenjin SDK.
 * Values written here are stored in DefaultEngine.ini and can be read at
 * runtime via UTenjinSettings::Get().
 */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Tenjin SDK"))
class TENJINSDK_API UTenjinSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTenjinSettings();

	/** Your Tenjin SDK key from https://www.tenjin.com (Apps → SDK Keys). */
	UPROPERTY(Config, EditAnywhere, Category = "Tenjin", meta = (DisplayName = "SDK Key"))
	FString SdkKey;

	/**
	 * iOS — string shown in the AppTrackingTransparency prompt.
	 * Surfaces into Info.plist as NSUserTrackingUsageDescription at package time.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Tenjin|iOS",
		meta = (DisplayName = "ATT Usage Description"))
	FString AttUsageDescription =
		TEXT("This identifier will be used to deliver personalized ads to you.");

	/** Android — sets the install-referrer source. Ignored on iOS. */
	UPROPERTY(Config, EditAnywhere, Category = "Tenjin|Android",
		meta = (DisplayName = "Android App Store"))
	ETenjinAppStore AndroidAppStore = ETenjinAppStore::GooglePlay;

	/**
	 * If true, the plugin auto-calls Initialize() with SdkKey and Connect()
	 * during PostConfigInit. Disable to take manual control of the lifecycle.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Tenjin",
		meta = (DisplayName = "Auto Initialize on Startup"))
	bool bAutoInitialize = false;

	static const UTenjinSettings* Get();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

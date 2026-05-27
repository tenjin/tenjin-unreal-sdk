// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TenjinDelegates.generated.h"

/**
 * Fired when a Tenjin deep link click resolves. Payload is the raw JSON
 * Tenjin's native SDKs hand back — keys include:
 *   "clicked_tenjin_link" (bool string)
 *   "is_first_session"    (bool string)
 *   "deferred_deeplink_url" (string)
 *   campaign_id / campaign_name / site_id / creative_name etc.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTenjinDeepLinkBroadcast, const FString&, JsonPayload);

/**
 * Singleton object hosting BlueprintAssignable multicast delegates. Bind
 * once during BeginPlay (Blueprints) or module startup (C++) to receive
 * lifecycle events the SDK pushes asynchronously.
 */
UCLASS(BlueprintType)
class TENJINSDK_API UTenjinDelegates : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Tenjin|Events")
	FTenjinDeepLinkBroadcast OnDeepLinkReceived;

	UFUNCTION(BlueprintPure, Category = "Tenjin")
	static UTenjinDelegates* Get();
};

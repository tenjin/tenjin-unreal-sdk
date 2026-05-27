// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TenjinSampleListener.generated.h"

class STenjinTestPanel;

/**
 * UObject relay for Tenjin's async callbacks.
 *
 * Tenjin's callback delegates are DYNAMIC delegates (so they can be bound
 * from Blueprints), which means they can only bind to UFUNCTIONs on a
 * UObject — not to lambdas. The sample's UI is an SCompoundWidget (not a
 * UObject), so this relay receives the callbacks and forwards them to the
 * on-screen log panel.
 *
 * This is the same pattern a game would use to route Tenjin callbacks into
 * non-UObject systems.
 */
UCLASS()
class UTenjinSampleListener : public UObject
{
	GENERATED_BODY()

public:
	/** Set by STenjinTestPanel; forwarded log lines land in the panel. */
	TWeakPtr<STenjinTestPanel> Panel;

	UFUNCTION()
	void HandleAttributionInfo(bool bSuccess, const FString& Json);

	UFUNCTION()
	void HandleCustomerUserId(const FString& Result);

	UFUNCTION()
	void HandleInstallationId(const FString& Result);

	UFUNCTION()
	void HandleUserProfile(const FString& Result);

	UFUNCTION()
	void HandleDeepLink(const FString& JsonPayload);

private:
	void Log(const FString& Line);
};

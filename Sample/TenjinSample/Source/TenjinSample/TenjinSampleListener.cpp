// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinSampleListener.h"
#include "Widgets/STenjinTestPanel.h"

void UTenjinSampleListener::HandleAttributionInfo(bool bSuccess, const FString& Json)
{
	Log(FString::Printf(TEXT("Attribution (success=%s): %s"),
		bSuccess ? TEXT("true") : TEXT("false"), *Json));
}

void UTenjinSampleListener::HandleCustomerUserId(const FString& Result)
{
	Log(FString::Printf(TEXT("Customer User ID: \"%s\""), *Result));
}

void UTenjinSampleListener::HandleInstallationId(const FString& Result)
{
	Log(FString::Printf(TEXT("Analytics Installation ID: \"%s\""), *Result));
}

void UTenjinSampleListener::HandleUserProfile(const FString& Result)
{
	Log(FString::Printf(TEXT("User profile: %s"), *Result));
}

void UTenjinSampleListener::HandleDeepLink(const FString& JsonPayload)
{
	Log(FString::Printf(TEXT("Deep link received: %s"), *JsonPayload));
}

void UTenjinSampleListener::Log(const FString& Line)
{
	if (TSharedPtr<STenjinTestPanel> Pinned = Panel.Pin())
	{
		Pinned->AppendLog(Line);
	}
}

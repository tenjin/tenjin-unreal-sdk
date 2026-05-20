// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/StrongObjectPtr.h"
#include "TenjinSampleListener.h"

/**
 * On-screen control panel for exercising the Tenjin SDK from a packaged build.
 *
 * The widget intentionally does not depend on UMG or asset content so the
 * sample app remains a single .uproject + source dir with no .uasset files.
 */
class STenjinTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STenjinTestPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Appends a timestamped line to the on-screen log pane. */
	void AppendLog(const FString& Line);

private:
	FReply OnInitializeClicked();
	FReply OnConnectClicked();
	FReply OnOptInClicked();
	FReply OnOptOutClicked();
	FReply OnSendEventClicked();
	FReply OnSendEventWithValueClicked();
	FReply OnTransactionClicked();
	FReply OnGetAttributionInfoClicked();
	FReply OnGetCustomerUserIdClicked();
	FReply OnGetInstallationIdClicked();
	FReply OnSetCustomerUserIdClicked();
	FReply OnGetUserProfileClicked();
	FReply OnResetUserProfileClicked();
	FReply OnUpdatePostbackConversionValueClicked();
	FReply OnSetGoogleDMAParamsClicked();
	FReply OnRegisterDeepLinkHandlerClicked();

	FString ResolveSdkKey() const;

	TSharedPtr<STextBlock> LogText;
	FString LogBuffer;
	int32 EventCounter = 0;

	/** UObject relay for Tenjin's dynamic-delegate callbacks. */
	TStrongObjectPtr<UTenjinSampleListener> Listener;
};

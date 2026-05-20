// Copyright (c) Tenjin. All Rights Reserved.

#include "STenjinTestPanel.h"
#include "TenjinBPLibrary.h"
#include "TenjinDelegates.h"
#include "TenjinSettings.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "STenjinTestPanel"

namespace
{
	TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked)
	{
		// Generous padding so each button hits Apple's 44pt minimum touch
		// target on phones, and a larger font so labels are readable.
		const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
		return SNew(SButton)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(16.f, 14.f))
			.OnClicked(OnClicked)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Font(ButtonFont)
				.Text(Label)
			];
	}
}

void STenjinTestPanel::Construct(const FArguments& InArgs)
{
	const FSlateBrush* PanelBrush = FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder");

	// The UObject relay receives Tenjin's dynamic-delegate callbacks and
	// forwards them to AppendLog. TStrongObjectPtr keeps it alive without a
	// UPROPERTY (this widget is not a UObject).
	Listener = TStrongObjectPtr<UTenjinSampleListener>(NewObject<UTenjinSampleListener>());
	Listener->Panel = SharedThis(this);

	// Surface global deep-link broadcasts in the log pane.
	if (UTenjinDelegates* Delegates = UTenjinDelegates::Get())
	{
		Delegates->OnDeepLinkReceived.AddDynamic(Listener.Get(), &UTenjinSampleListener::HandleDeepLink);
	}

	const FSlateFontInfo LogFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	const FMargin SafeArea(20.f, 40.f, 20.f, 20.f); // top inset clears the notch

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.Padding(SafeArea)
	[
		SNew(SHorizontalBox)

		// ----- Button column (fixed width so labels and touch targets are comfy) -----
		+ SHorizontalBox::Slot()
		.FillWidth(0.42f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush)
			.Padding(8.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Init", "Initialize"),                     FOnClicked::CreateSP(this, &STenjinTestPanel::OnInitializeClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Connect", "Connect"),                     FOnClicked::CreateSP(this, &STenjinTestPanel::OnConnectClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("OptIn", "Opt In"),                        FOnClicked::CreateSP(this, &STenjinTestPanel::OnOptInClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("OptOut", "Opt Out"),                      FOnClicked::CreateSP(this, &STenjinTestPanel::OnOptOutClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Event", "Send Event"),                    FOnClicked::CreateSP(this, &STenjinTestPanel::OnSendEventClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("EventVal", "Send Event w/ Value"),        FOnClicked::CreateSP(this, &STenjinTestPanel::OnSendEventWithValueClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Tx", "Send Transaction"),                 FOnClicked::CreateSP(this, &STenjinTestPanel::OnTransactionClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Attr", "Get Attribution Info"),           FOnClicked::CreateSP(this, &STenjinTestPanel::OnGetAttributionInfoClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("SetUid", "Set Customer User ID"),         FOnClicked::CreateSP(this, &STenjinTestPanel::OnSetCustomerUserIdClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("GetUid", "Get Customer User ID"),         FOnClicked::CreateSP(this, &STenjinTestPanel::OnGetCustomerUserIdClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("GetIid", "Get Installation ID"),          FOnClicked::CreateSP(this, &STenjinTestPanel::OnGetInstallationIdClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Profile", "Get User Profile"),            FOnClicked::CreateSP(this, &STenjinTestPanel::OnGetUserProfileClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("ResetProfile", "Reset User Profile"),     FOnClicked::CreateSP(this, &STenjinTestPanel::OnResetUserProfileClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("Postback", "SKAdNetwork Postback (iOS)"), FOnClicked::CreateSP(this, &STenjinTestPanel::OnUpdatePostbackConversionValueClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("DMA", "Set Google DMA Params"),           FOnClicked::CreateSP(this, &STenjinTestPanel::OnSetGoogleDMAParamsClicked)) ]
				+ SScrollBox::Slot().Padding(4.f)[ MakeButton(LOCTEXT("DeepLink", "Register Deep Link Handler"),  FOnClicked::CreateSP(this, &STenjinTestPanel::OnRegisterDeepLinkHandlerClicked)) ]
			]
		]

		// ----- Log pane -----
		+ SHorizontalBox::Slot()
		.FillWidth(0.58f)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush)
			.Padding(8.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(LogText, STextBlock)
					.Font(LogFont)
					.Text(FText::FromString(TEXT("Tenjin sample — tap a button to begin.\n")))
					.AutoWrapText(true)
				]
			]
		]
	];
}

void STenjinTestPanel::AppendLog(const FString& Line)
{
	const FString Stamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	LogBuffer = FString::Printf(TEXT("[%s] %s\n%s"), *Stamp, *Line, *LogBuffer);
	if (LogText.IsValid())
	{
		LogText->SetText(FText::FromString(LogBuffer));
	}
	UE_LOG(LogTemp, Display, TEXT("Tenjin sample: %s"), *Line);
}

// ---------------------------------------------------------------------------

FString STenjinTestPanel::ResolveSdkKey() const
{
	if (const UTenjinSettings* Settings = UTenjinSettings::Get())
	{
		if (!Settings->SdkKey.IsEmpty())
		{
			return Settings->SdkKey;
		}
	}
	return TEXT("YOUR_TENJIN_SDK_KEY");
}

FReply STenjinTestPanel::OnInitializeClicked()
{
	const FString SdkKey = ResolveSdkKey();
	UTenjinBPLibrary::Initialize(SdkKey);
	AppendLog(FString::Printf(TEXT("Initialize(%s)"), *SdkKey));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnConnectClicked()
{
	UTenjinBPLibrary::Connect();
	AppendLog(TEXT("Connect()"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnOptInClicked()
{
	UTenjinBPLibrary::OptIn();
	AppendLog(TEXT("OptIn()"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnOptOutClicked()
{
	UTenjinBPLibrary::OptOut();
	AppendLog(TEXT("OptOut()"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnSendEventClicked()
{
	const FString Name = FString::Printf(TEXT("sample_event_%d"), ++EventCounter);
	UTenjinBPLibrary::EventWithName(Name);
	AppendLog(FString::Printf(TEXT("EventWithName(\"%s\")"), *Name));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnSendEventWithValueClicked()
{
	const FString Name = TEXT("sample_event_value");
	const int32 Value = FMath::RandRange(1, 100);
	UTenjinBPLibrary::EventWithNameAndValue(Name, Value);
	AppendLog(FString::Printf(TEXT("EventWithNameAndValue(\"%s\", %d)"), *Name, Value));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnTransactionClicked()
{
	UTenjinBPLibrary::Transaction(TEXT("com.tenjin.unreal.sample.gem_pack_small"), TEXT("USD"), 1, 0.99f);
	AppendLog(TEXT("Transaction(\"gem_pack_small\", USD, 1, $0.99)"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnGetAttributionInfoClicked()
{
	FTenjinAttributionInfoDelegate Cb;
	Cb.BindUFunction(Listener.Get(), FName(TEXT("HandleAttributionInfo")));
	UTenjinBPLibrary::GetAttributionInfo(Cb);
	AppendLog(TEXT("GetAttributionInfo() — awaiting callback"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnGetCustomerUserIdClicked()
{
	FTenjinStringDelegate Cb;
	Cb.BindUFunction(Listener.Get(), FName(TEXT("HandleCustomerUserId")));
	UTenjinBPLibrary::GetCustomerUserId(Cb);
	return FReply::Handled();
}

FReply STenjinTestPanel::OnGetInstallationIdClicked()
{
	FTenjinStringDelegate Cb;
	Cb.BindUFunction(Listener.Get(), FName(TEXT("HandleInstallationId")));
	UTenjinBPLibrary::GetAnalyticsInstallationId(Cb);
	return FReply::Handled();
}

FReply STenjinTestPanel::OnSetCustomerUserIdClicked()
{
	const FString Uid = FString::Printf(TEXT("unreal_user_%lld"), FDateTime::UtcNow().ToUnixTimestamp());
	UTenjinBPLibrary::SetCustomerUserId(Uid);
	AppendLog(FString::Printf(TEXT("SetCustomerUserId(\"%s\")"), *Uid));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnGetUserProfileClicked()
{
	FTenjinStringDelegate Cb;
	Cb.BindUFunction(Listener.Get(), FName(TEXT("HandleUserProfile")));
	UTenjinBPLibrary::GetUserProfileDictionary(Cb);
	return FReply::Handled();
}

FReply STenjinTestPanel::OnResetUserProfileClicked()
{
	UTenjinBPLibrary::ResetUserProfile();
	AppendLog(TEXT("ResetUserProfile()"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnUpdatePostbackConversionValueClicked()
{
	const int32 Value = FMath::RandRange(0, 63);
	UTenjinBPLibrary::UpdatePostbackConversionValue(Value);
	AppendLog(FString::Printf(TEXT("UpdatePostbackConversionValue(%d)"), Value));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnSetGoogleDMAParamsClicked()
{
	UTenjinBPLibrary::SetGoogleDMAParameters(true, true);
	AppendLog(TEXT("SetGoogleDMAParameters(true, true)"));
	return FReply::Handled();
}

FReply STenjinTestPanel::OnRegisterDeepLinkHandlerClicked()
{
	UTenjinBPLibrary::RegisterDeepLinkHandler();
	AppendLog(TEXT("RegisterDeepLinkHandler() — deep link payloads will appear here"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

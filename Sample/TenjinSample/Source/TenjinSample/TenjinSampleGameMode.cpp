// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinSampleGameMode.h"
#include "Widgets/STenjinTestPanel.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

ATenjinSampleGameMode::ATenjinSampleGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATenjinSampleGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine && GEngine->GameViewport)
	{
		TSharedRef<STenjinTestPanel> Panel = SNew(STenjinTestPanel);
		GEngine->GameViewport->AddViewportWidgetForPlayer(
			GetWorld()->GetFirstLocalPlayerFromController(),
			Panel,
			10
		);
	}
}

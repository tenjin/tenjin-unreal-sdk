// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TenjinSampleGameMode.generated.h"

UCLASS()
class ATenjinSampleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATenjinSampleGameMode();

	virtual void BeginPlay() override;
};

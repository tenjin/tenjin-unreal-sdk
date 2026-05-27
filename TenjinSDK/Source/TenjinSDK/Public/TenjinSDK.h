// Copyright (c) Tenjin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTenjinSDKModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handle for the deferred OnPostEngineInit auto-initialize hook. */
	FDelegateHandle PostEngineInitHandle;
};

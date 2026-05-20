// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinSettings.h"

UTenjinSettings::UTenjinSettings()
{
	CategoryName = TEXT("Plugins");
}

const UTenjinSettings* UTenjinSettings::Get()
{
	return GetDefault<UTenjinSettings>();
}

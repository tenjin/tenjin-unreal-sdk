// Copyright (c) Tenjin. All Rights Reserved.

#include "TenjinDelegates.h"
#include "UObject/Package.h"

namespace
{
	TWeakObjectPtr<UTenjinDelegates> GSingleton;
}

UTenjinDelegates* UTenjinDelegates::Get()
{
	if (UTenjinDelegates* Existing = GSingleton.Get())
	{
		return Existing;
	}

	UTenjinDelegates* New = NewObject<UTenjinDelegates>(
		GetTransientPackage(),
		UTenjinDelegates::StaticClass(),
		TEXT("TenjinDelegates"),
		RF_MarkAsRootSet);
	GSingleton = New;
	return New;
}

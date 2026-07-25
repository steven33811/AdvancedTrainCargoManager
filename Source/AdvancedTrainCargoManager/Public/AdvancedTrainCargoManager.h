// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAdvancedTrainCargoManager, Log, All);

ADVANCEDTRAINCARGOMANAGER_API
void RegisterATCMTranslations();

class FAdvancedTrainCargoManagerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

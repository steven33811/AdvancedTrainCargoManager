#include "ATCMGameInstanceModule.h"

#include "AdvancedTrainCargoManager.h"
#include "ATCMModConfiguration.h"
#include "ATCMRemoteCallObject.h"
#include "ATCMUserInterface.h"

UATCMGameInstanceModule::UATCMGameInstanceModule()
{
	bRootModule = true;

	RemoteCallObjects.Add(
		UATCMRemoteCallObject::StaticClass());

	ModConfigurations.Add(
		UATCMModConfiguration::StaticClass());
}

void UATCMGameInstanceModule::DispatchLifecycleEvent(
	ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::POST_INITIALIZATION)
	{
		// StartupModule runs before the final localization refresh.
		// Register again here so mod UI and keybinding texts follow
		// the player's selected language.
		RegisterATCMTranslations();

		FATCMUserInterface::Initialize();

		UE_LOG(
			LogAdvancedTrainCargoManager,
			Log,
			TEXT("ATCM-Netzwerk- und UI-Modul "
			     "wurde registriert."));
	}
}

void UATCMGameInstanceModule::BeginDestroy()
{
	FATCMUserInterface::Shutdown();

	Super::BeginDestroy();
}

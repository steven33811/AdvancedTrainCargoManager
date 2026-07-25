#include "ATCMGameWorldModule.h"

#include "AdvancedTrainCargoManager.h"
#include "ATCMSubsystem.h"

UATCMGameWorldModule::UATCMGameWorldModule()
{
	bRootModule = true;

	ModSubsystems.Add(AATCMSubsystem::StaticClass());
}

void UATCMGameWorldModule::DispatchLifecycleEvent(
	ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::POST_INITIALIZATION)
	{
		AATCMSubsystem* Subsystem = AATCMSubsystem::Get(this);

		if (Subsystem)
		{
			UE_LOG(
				LogAdvancedTrainCargoManager,
				Log,
				TEXT("ATCM-Subsystem wurde erfolgreich erstellt.")
			);
		}
		else
		{
			UE_LOG(
				LogAdvancedTrainCargoManager,
				Error,
				TEXT("ATCM-Subsystem konnte nicht gefunden werden.")
			);
		}
	}
}
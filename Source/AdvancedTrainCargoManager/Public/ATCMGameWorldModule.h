#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "ATCMGameWorldModule.generated.h"

/**
 * Registriert die weltgebundenen Bestandteile der Mod.
 */
UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMGameWorldModule final
	: public UGameWorldModule
{
	GENERATED_BODY()

public:
	UATCMGameWorldModule();

	virtual void DispatchLifecycleEvent(
		ELifecyclePhase Phase) override;
};
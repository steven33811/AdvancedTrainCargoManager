#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "ATCMGameInstanceModule.generated.h"

UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMGameInstanceModule final
	: public UGameInstanceModule
{
	GENERATED_BODY()

public:
	UATCMGameInstanceModule();

	virtual void DispatchLifecycleEvent(
		ELifecyclePhase Phase) override;

	virtual void BeginDestroy() override;
};

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "ATCMTypes.h"
#include "ATCMRemoteCallObject.generated.h"

class AFGTrain;
class AFGTrainStationIdentifier;

/**
 * Überträgt Fahrplanänderungen vom Client zum Server.
 */
UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMRemoteCallObject final
	: public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SaveSchedule(
		AFGTrain* Train,
		const TArray<AFGTrainStationIdentifier*>& RouteStations,
		const TArray<FATCMStopRules>& Stops);

private:
	/**
	 * Ein repliziertes Feld ist erforderlich, damit Unreal
	 * das RCO für Netzwerkaufrufe registriert.
	 */
	UPROPERTY(Replicated)
	bool ForceNetField = false;
};
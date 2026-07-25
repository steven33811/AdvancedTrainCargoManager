#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "ATCMTypes.h"
#include "ATCMSubsystem.generated.h"

class AFGRailroadTimeTable;
class AFGTrainStationIdentifier;
class AFGTrain;

DECLARE_MULTICAST_DELEGATE(FATCMOnSchedulesChanged);

/**
 * Speichert und repliziert die erweiterten Zugfahrpläne.
 * Änderungen dürfen später ausschließlich auf dem Server vorgenommen werden.
 */
UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API AATCMSubsystem final
	: public AModSubsystem
	, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AATCMSubsystem();

	/**
	 * Sucht das Subsystem in der aktuellen Spielwelt.
	 */
	static AATCMSubsystem* Get(const UObject* WorldContext);

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IFGSaveInterface
	virtual void PreSaveGame_Implementation(
		int32 SaveVersion,
		int32 GameVersion) override;

	virtual void PostSaveGame_Implementation(
		int32 SaveVersion,
		int32 GameVersion) override;

	virtual void PreLoadGame_Implementation(
		int32 SaveVersion,
		int32 GameVersion) override;

	virtual void PostLoadGame_Implementation(
		int32 SaveVersion,
		int32 GameVersion) override;

	virtual void GatherDependencies_Implementation(
		TArray<UObject*>& OutDependentObjects) override;

	virtual bool NeedTransform_Implementation() override;
	virtual bool ShouldSave_Implementation() const override;

	/**
	 * Sucht die gespeicherten Regeln eines Fahrplans.
	 */
	const FATCMTrainSchedule* FindSchedule(
		const AFGRailroadTimeTable* TimeTable) const;

	/**
	 * Sucht die Regeln eines bestimmten Halts.
	 */
	const FATCMStopRules* FindStopRules(
		const AFGRailroadTimeTable* TimeTable,
		const AFGTrainStationIdentifier* Station,
		int32 StopIndex) const;
	
/**
 * Übernimmt einen Fahrplan und seine erweiterten Transferregeln.
 * Darf ausschließlich auf dem Server ausgeführt werden.
 */
bool ApplySchedule(
	AFGTrain* Train,
	const TArray<AFGTrainStationIdentifier*>& RouteStations,
	const TArray<FATCMStopRules>& RequestedStops);

	const TArray<FATCMTrainSchedule>& GetSchedules() const
	{
		return Schedules;
	}

	FATCMOnSchedulesChanged OnSchedulesChanged;

private:
	UFUNCTION()
	void OnRep_Schedules();

	void SanitizeSchedules();

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Schedules)
	TArray<FATCMTrainSchedule> Schedules;
};
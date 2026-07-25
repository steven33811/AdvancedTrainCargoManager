#pragma once

#include "CoreMinimal.h"
#include "FGTrainDockingRules.h"
#include "Resources/FGItemDescriptor.h"
#include "ATCMTypes.generated.h"

class AFGRailroadTimeTable;
class AFGTrainStationIdentifier;

/**
 * Richtung eines erweiterten Frachttransfers.
 */
UENUM(BlueprintType)
enum class EATCMTransferDirection : uint8
{
	Load UMETA(DisplayName = "Load"),
	Unload UMETA(DisplayName = "Unload")
};

/**
 * Eine einzelne Lade- oder Entladeregel.
 */
USTRUCT(BlueprintType)
struct ADVANCEDTRAINCARGOMANAGER_API FATCMTransferRule
{
	GENERATED_BODY()
	/**
	 * Nullbasierter Index der Frachtwagen.
	 * Lokomotiven werden nicht mitgezählt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 FreightWagonIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	EATCMTransferDirection Direction = EATCMTransferDirection::Load;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TSubclassOf<UFGItemDescriptor> ItemDescriptor;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		SaveGame,
		meta = (ClampMin = "1"))
	int32 Amount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool TransferAll = false;
};

/**
 * Erweiterte Regeln eines einzelnen Fahrplanhalts.
 */
USTRUCT(BlueprintType)
struct ADVANCEDTRAINCARGOMANAGER_API FATCMStopRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 StopIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TObjectPtr<AFGTrainStationIdentifier> Station = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FATCMTransferRule> Rules;

	/**
	 * Ursprüngliche Satisfactory-Regeln dieses Halts.
	 * Sie werden wiederhergestellt, wenn alle erweiterten Regeln entfernt werden.
	 */
	UPROPERTY(SaveGame)
	FTrainDockingRuleSet BaseDockingRules;

	UPROPERTY(SaveGame)
	bool HasBaseDockingRules = false;
};

/**
 * Alle erweiterten Regeln eines Zugfahrplans.
 */
USTRUCT(BlueprintType)
struct ADVANCEDTRAINCARGOMANAGER_API FATCMTrainSchedule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TObjectPtr<AFGRailroadTimeTable> TimeTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<FATCMStopRules> Stops;
};

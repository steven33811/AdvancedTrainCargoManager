#include "ATCMRemoteCallObject.h"

#include "AdvancedTrainCargoManager.h"
#include "ATCMSubsystem.h"
#include "FGPlayerController.h"
#include "FGTrain.h"
#include "FGTrainStationIdentifier.h"
#include "Net/UnrealNetwork.h"

namespace
{
	uint8 GetRemoteCargoCategory(
		TSubclassOf<UFGItemDescriptor> ItemDescriptor)
	{
		if (!ItemDescriptor)
		{
			return 0;
		}

		switch (UFGItemDescriptor::GetForm(ItemDescriptor))
		{
		case EResourceForm::RF_SOLID:
			return 1;

		case EResourceForm::RF_LIQUID:
		case EResourceForm::RF_GAS:
			return 2;

		default:
			return 0;
		}
	}
}

void UATCMRemoteCallObject::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(
		UATCMRemoteCallObject,
		ForceNetField);
}

bool UATCMRemoteCallObject::Server_SaveSchedule_Validate(
	AFGTrain* Train,
	const TArray<AFGTrainStationIdentifier*>& RouteStations,
	const TArray<FATCMStopRules>& Stops)
{
	if (!IsValid(Train) ||
		RouteStations.IsEmpty() ||
		RouteStations.Num() > 100 ||
		Stops.Num() > 100)
	{
		return false;
	}

	for (AFGTrainStationIdentifier* Station : RouteStations)
	{
		if (!IsValid(Station))
		{
			return false;
		}
	}

	int32 TotalRuleCount = 0;
	TSet<int32> UsedStopIndices;
	TMap<int32, uint8> WagonCargoCategories;

	for (const FATCMStopRules& Stop : Stops)
	{
		if (Stop.StopIndex < 0 ||
			Stop.StopIndex >= RouteStations.Num() ||
			Stop.Rules.Num() > 256)
		{
			return false;
		}

		if (UsedStopIndices.Contains(Stop.StopIndex))
		{
			return false;
		}

		UsedStopIndices.Add(Stop.StopIndex);

		if (Stop.Station.Get() !=
			RouteStations[Stop.StopIndex])
		{
			return false;
		}

		TotalRuleCount += Stop.Rules.Num();

		if (TotalRuleCount > 2048)
		{
			return false;
		}

		for (const FATCMTransferRule& Rule : Stop.Rules)
		{
			if (Rule.FreightWagonIndex < 0 ||
				Rule.FreightWagonIndex > 255 ||
				(!Rule.TransferAll && Rule.Amount <= 0) ||
				!Rule.ItemDescriptor)
			{
				return false;
			}

			const uint8 CargoCategory =
				GetRemoteCargoCategory(Rule.ItemDescriptor);
			const uint8* ExistingCategory =
				WagonCargoCategories.Find(
					Rule.FreightWagonIndex);

			if (CargoCategory == 0
				|| (ExistingCategory
					&& *ExistingCategory != CargoCategory))
			{
				return false;
			}

			WagonCargoCategories.FindOrAdd(
				Rule.FreightWagonIndex) =
				CargoCategory;
		}
	}

	return true;
}

void UATCMRemoteCallObject::Server_SaveSchedule_Implementation(
	AFGTrain* Train,
	const TArray<AFGTrainStationIdentifier*>& RouteStations,
	const TArray<FATCMStopRules>& Stops)
{
	AATCMSubsystem* Subsystem =
		AATCMSubsystem::Get(this);

	if (!Subsystem)
	{
		UE_LOG(
			LogAdvancedTrainCargoManager,
			Error,
			TEXT("Fahrplan konnte nicht gespeichert werden: "
			     "ATCM-Subsystem fehlt.")
		);

		return;
	}

	if (!Subsystem->ApplySchedule(
		Train,
		RouteStations,
		Stops))
	{
		UE_LOG(
			LogAdvancedTrainCargoManager,
			Warning,
			TEXT("Fahrplanänderung von %s wurde abgelehnt."),
			*GetNameSafe(GetOwnerPlayerController())
		);
	}
}

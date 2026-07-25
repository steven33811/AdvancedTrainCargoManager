#include "ATCMSubsystem.h"

#include "AdvancedTrainCargoManager.h"
#include "EngineUtils.h"
#include "FGRailroadTimeTable.h"
#include "FGTrain.h"
#include "FGTrainStationIdentifier.h"
#include "Net/UnrealNetwork.h"

namespace
{
	uint8 GetSubsystemCargoCategory(
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

	bool AreScheduleCargoFormsValid(
		const TArray<FATCMStopRules>& Stops)
	{
		TMap<int32, uint8> WagonCargoCategories;

		for (const FATCMStopRules& Stop : Stops)
		{
			for (const FATCMTransferRule& Rule : Stop.Rules)
			{
				const uint8 CargoCategory =
					GetSubsystemCargoCategory(
						Rule.ItemDescriptor);
				const uint8* ExistingCategory =
					WagonCargoCategories.Find(
						Rule.FreightWagonIndex);

				if (CargoCategory == 0
					|| (ExistingCategory
						&& *ExistingCategory !=
							CargoCategory))
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
}

AATCMSubsystem::AATCMSubsystem()
{
	ReplicationPolicy =
		ESubsystemReplicationPolicy::SpawnOnServer_Replicate;

	bAlwaysRelevant = true;
	bNetUseOwnerRelevancy = false;
}

AATCMSubsystem* AATCMSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext
		? WorldContext->GetWorld()
		: nullptr;

	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AATCMSubsystem> Iterator(World);
		Iterator;
		++Iterator)
	{
		return *Iterator;
	}

	return nullptr;
}

void AATCMSubsystem::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AATCMSubsystem, Schedules);
}

void AATCMSubsystem::PreSaveGame_Implementation(
	int32 SaveVersion,
	int32 GameVersion)
{
	SanitizeSchedules();
}

void AATCMSubsystem::PostSaveGame_Implementation(
	int32 SaveVersion,
	int32 GameVersion)
{
}

void AATCMSubsystem::PreLoadGame_Implementation(
	int32 SaveVersion,
	int32 GameVersion)
{
}

void AATCMSubsystem::PostLoadGame_Implementation(
	int32 SaveVersion,
	int32 GameVersion)
{
	SanitizeSchedules();
}

void AATCMSubsystem::GatherDependencies_Implementation(
	TArray<UObject*>& OutDependentObjects)
{
	for (const FATCMTrainSchedule& Schedule : Schedules)
	{
		if (IsValid(Schedule.TimeTable.Get()))
		{
			OutDependentObjects.Add(Schedule.TimeTable.Get());
		}

		for (const FATCMStopRules& Stop : Schedule.Stops)
		{
			if (IsValid(Stop.Station.Get()))
			{
				OutDependentObjects.Add(Stop.Station.Get());
			}
		}
	}
}

bool AATCMSubsystem::NeedTransform_Implementation()
{
	return false;
}

bool AATCMSubsystem::ShouldSave_Implementation() const
{
	return true;
}

const FATCMTrainSchedule* AATCMSubsystem::FindSchedule(
	const AFGRailroadTimeTable* TimeTable) const
{
	if (!IsValid(TimeTable))
	{
		return nullptr;
	}

	return Schedules.FindByPredicate(
		[TimeTable](const FATCMTrainSchedule& Candidate)
		{
			return Candidate.TimeTable.Get() == TimeTable;
		});
}

const FATCMStopRules* AATCMSubsystem::FindStopRules(
	const AFGRailroadTimeTable* TimeTable,
	const AFGTrainStationIdentifier* Station,
	int32 StopIndex) const
{
	const FATCMTrainSchedule* Schedule =
		FindSchedule(TimeTable);

	if (!Schedule || !IsValid(Station))
	{
		return nullptr;
	}

	return Schedule->Stops.FindByPredicate(
		[Station, StopIndex](const FATCMStopRules& Stop)
		{
			return Stop.StopIndex == StopIndex
				&& Stop.Station.Get() == Station
				&& !Stop.Rules.IsEmpty();
		});
}

void AATCMSubsystem::OnRep_Schedules()
{
	OnSchedulesChanged.Broadcast();
}

void AATCMSubsystem::SanitizeSchedules()
{
	Schedules.RemoveAll(
		[](FATCMTrainSchedule& Schedule)
		{
			if (!IsValid(Schedule.TimeTable.Get()))
			{
				return true;
			}

			TMap<int32, uint8> WagonCargoCategories;

			Schedule.Stops.RemoveAll(
				[&WagonCargoCategories](
					FATCMStopRules& Stop)
				{
					if (!IsValid(Stop.Station.Get()) ||
						Stop.StopIndex < 0)
					{
						return true;
					}

					Stop.Rules.RemoveAll(
						[&WagonCargoCategories](
							const FATCMTransferRule& Rule)
						{
							if (Rule.FreightWagonIndex < 0
								|| (!Rule.TransferAll
									&& Rule.Amount <= 0)
								|| !Rule.ItemDescriptor)
							{
								return true;
							}

							const uint8 CargoCategory =
								GetSubsystemCargoCategory(
									Rule.ItemDescriptor);
							const uint8* ExistingCategory =
								WagonCargoCategories.Find(
									Rule.FreightWagonIndex);

							if (CargoCategory == 0
								|| (ExistingCategory
									&& *ExistingCategory !=
										CargoCategory))
							{
								return true;
							}

							WagonCargoCategories.FindOrAdd(
								Rule.FreightWagonIndex) =
									CargoCategory;

							return false;
						});

					return Stop.Rules.IsEmpty();
				});

			return Schedule.Stops.IsEmpty();
		});
}

bool AATCMSubsystem::ApplySchedule(
	AFGTrain* Train,
	const TArray<AFGTrainStationIdentifier*>& RouteStations,
	const TArray<FATCMStopRules>& RequestedStops)
{
	if (!HasAuthority() ||
		!IsValid(Train) ||
		RouteStations.Num() > 100 ||
		!AreScheduleCargoFormsValid(RequestedStops))
	{
		return false;
	}

	/*
	 * Alle Stationen müssen existieren und zum selben
	 * Schienennetz wie der Zug gehören.
	 */
	for (AFGTrainStationIdentifier* Station : RouteStations)
	{
		if (!IsValid(Station) ||
			Station->GetTrackGraphID() != Train->GetTrackGraphID())
		{
			return false;
		}
	}

	AFGRailroadTimeTable* TimeTable = Train->GetTimeTable();

	if (!TimeTable)
	{
		TimeTable = Train->NewTimeTable();
	}

	if (!TimeTable)
	{
		return false;
	}

	TArray<FTimeTableStop> PreviousVanillaStops;
	TimeTable->GetStops(PreviousVanillaStops);

	const FATCMTrainSchedule* ExistingSchedule =
		FindSchedule(TimeTable);

	TArray<FATCMStopRules> NormalizedAdvancedStops;
	TArray<FTimeTableStop> NewVanillaStops;

	NewVanillaStops.Reserve(RouteStations.Num());

	for (int32 StopIndex = 0;
		StopIndex < RouteStations.Num();
		++StopIndex)
	{
		AFGTrainStationIdentifier* Station =
			RouteStations[StopIndex];

		const FATCMStopRules* Requested =
			RequestedStops.FindByPredicate(
				[StopIndex, Station](
					const FATCMStopRules& Stop)
				{
					return Stop.StopIndex == StopIndex
						&& Stop.Station.Get() == Station;
				});

		const FATCMStopRules* ExistingAdvanced = nullptr;

		if (ExistingSchedule)
		{
			ExistingAdvanced =
				ExistingSchedule->Stops.FindByPredicate(
					[StopIndex, Station](
						const FATCMStopRules& Stop)
					{
						return Stop.StopIndex == StopIndex
							&& Stop.Station.Get() == Station;
					});

			/*
			 * Falls ein eindeutiger Halt verschoben wurde,
			 * suchen wir zusätzlich nur anhand der Station.
			 */
			if (!ExistingAdvanced)
			{
				ExistingAdvanced =
					ExistingSchedule->Stops.FindByPredicate(
						[Station](
							const FATCMStopRules& Stop)
						{
							return Stop.Station.Get() == Station;
						});
			}
		}

		FTrainDockingRuleSet BaseRules;
		bool HasBaseRules = false;

		if (ExistingAdvanced &&
			ExistingAdvanced->HasBaseDockingRules)
		{
			BaseRules =
				ExistingAdvanced->BaseDockingRules;

			HasBaseRules = true;
		}
		else if (
			PreviousVanillaStops.IsValidIndex(StopIndex) &&
			PreviousVanillaStops[StopIndex].Station.Get() ==
				Station)
		{
			BaseRules =
				PreviousVanillaStops[StopIndex].DockingRuleSet;

			HasBaseRules = true;
		}
		else
		{
			const FTimeTableStop* PreviousStationStop =
				PreviousVanillaStops.FindByPredicate(
					[Station](
						const FTimeTableStop& Stop)
					{
						return Stop.Station.Get() == Station;
					});

			if (PreviousStationStop)
			{
				BaseRules =
					PreviousStationStop->DockingRuleSet;

				HasBaseRules = true;
			}
		}

		FTimeTableStop VanillaStop;
		VanillaStop.Station = Station;
		VanillaStop.DockingRuleSet = BaseRules;

		if (Requested && !Requested->Rules.IsEmpty())
		{
			FATCMStopRules Normalized;

			Normalized.StopIndex = StopIndex;
			Normalized.Station = Station;
			Normalized.BaseDockingRules = BaseRules;
			Normalized.HasBaseDockingRules = HasBaseRules;

			for (const FATCMTransferRule& Rule :
				Requested->Rules)
			{
				if (Rule.FreightWagonIndex < 0 ||
					(!Rule.TransferAll
						&& Rule.Amount <= 0) ||
					!Rule.ItemDescriptor)
				{
					continue;
				}

				Normalized.Rules.Add(Rule);
			}

			if (!Normalized.Rules.IsEmpty())
			{
				/*
				 * Ein erweiterter Halt soll nach einem
				 * Transferdurchlauf abgeschlossen sein.
				 */
				VanillaStop.DockingRuleSet.DockingDefinition =
					ETrainDockingDefinition::TDD_LoadUnloadOnce;

				VanillaStop.DockingRuleSet
					.IgnoreFullLoadUnloadIfTransferBlockedByFilters =
					true;

				VanillaStop.DockingRuleSet
					.LoadFilterDescriptors.Reset();

				VanillaStop.DockingRuleSet
					.UnloadFilterDescriptors.Reset();

				for (const FATCMTransferRule& Rule :
					Normalized.Rules)
				{
					TArray<TSubclassOf<UFGItemDescriptor>>&
						TargetFilters =
							Rule.Direction ==
								EATCMTransferDirection::Load
							? VanillaStop.DockingRuleSet
								.LoadFilterDescriptors
							: VanillaStop.DockingRuleSet
								.UnloadFilterDescriptors;

					TargetFilters.AddUnique(
						Rule.ItemDescriptor);
				}

				NormalizedAdvancedStops.Add(
					MoveTemp(Normalized));
			}
		}

		NewVanillaStops.Add(MoveTemp(VanillaStop));
	}

	if (!TimeTable->SetStops(NewVanillaStops))
	{
		UE_LOG(
			LogAdvancedTrainCargoManager,
			Warning,
			TEXT("Fahrplan von Zug %s konnte nicht aktualisiert werden."),
			*Train->GetName()
		);

		return false;
	}

	Schedules.RemoveAll(
		[TimeTable](
			const FATCMTrainSchedule& Schedule)
		{
			return Schedule.TimeTable.Get() == TimeTable;
		});

	if (!NormalizedAdvancedStops.IsEmpty())
	{
		FATCMTrainSchedule& NewSchedule =
			Schedules.AddDefaulted_GetRef();

		NewSchedule.TimeTable = TimeTable;
		NewSchedule.Stops =
			MoveTemp(NormalizedAdvancedStops);
	}

	ForceNetUpdate();
	OnSchedulesChanged.Broadcast();

	UE_LOG(
		LogAdvancedTrainCargoManager,
		Log,
		TEXT("Erweiterter Fahrplan für Zug %s wurde gespeichert."),
		*Train->GetName()
	);

	return true;
}

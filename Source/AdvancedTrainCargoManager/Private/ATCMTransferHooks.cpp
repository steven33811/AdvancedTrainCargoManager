#include "ATCMTransferHooks.h"

#include "AdvancedTrainCargoManager.h"
#include "ATCMSubsystem.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "Components/SkeletalMeshComponent.h"
#include "FGFreightWagon.h"
#include "FGInventoryComponent.h"
#include "FGRailroadSubsystem.h"
#include "FGRailroadTimeTable.h"
#include "FGTrain.h"
#include "FGTrainStationIdentifier.h"
#include "Patching/NativeHookManager.h"

FDelegateHandle FATCMTransferHooks::TransferInventoryHook;
FDelegateHandle FATCMTransferHooks::BeginLoadSequenceHook;
FDelegateHandle FATCMTransferHooks::BeginUnloadSequenceHook;
FDelegateHandle FATCMTransferHooks::NotifyTrainDockedVisualHook;
FDelegateHandle FATCMTransferHooks::TransferCompleteAnimationHook;
FDelegateHandle FATCMTransferHooks::SwapCargoContainerVisibilityHook;
FDelegateHandle FATCMTransferHooks::ShowPlatformCargoContainerHook;
FDelegateHandle FATCMTransferHooks::MatchCargoContainerVisualsHook;
FDelegateHandle FATCMTransferHooks::CargoMeshVisibilityHook;
FDelegateHandle FATCMTransferHooks::UpdateDockingStatusVisualHook;
FDelegateHandle FATCMTransferHooks::ForceUpdateAnimInstanceHook;

namespace
{
	TSet<TWeakObjectPtr<AFGFreightWagon>> SuppressedCargoVisualWagons;

	const FATCMStopRules* ResolveStopRulesForDocking(
		const AATCMSubsystem* Subsystem,
		const AFGRailroadTimeTable* TimeTable,
		const AFGBuildableRailroadStation* DockingStation,
		int32& OutResolvedStopIndex)
	{
		OutResolvedStopIndex = INDEX_NONE;

		const FATCMTrainSchedule* Schedule = Subsystem
			? Subsystem->FindSchedule(TimeTable)
			: nullptr;
		if (!Schedule || !TimeTable || !DockingStation)
		{
			return nullptr;
		}

		const AFGTrainStationIdentifier* DockingStationIdentifier =
			DockingStation->GetStationIdentifier();

		const auto MatchesDockingStation =
			[DockingStation, DockingStationIdentifier](const FATCMStopRules& Stop)
			{
				const AFGTrainStationIdentifier* Candidate = Stop.Station.Get();
				return Candidate
					&& (Candidate == DockingStationIdentifier
						|| Candidate->GetStation() == DockingStation);
			};

		const auto FindAtIndex =
			[Schedule, &MatchesDockingStation](const int32 StopIndex)
			{
				return Schedule->Stops.FindByPredicate(
					[StopIndex, &MatchesDockingStation](const FATCMStopRules& Stop)
					{
						return Stop.StopIndex == StopIndex
							&& !Stop.Rules.IsEmpty()
							&& MatchesDockingStation(Stop);
					});
			};

		const int32 CurrentStopIndex = TimeTable->GetCurrentStop();

		if (const FATCMStopRules* CurrentRules = FindAtIndex(CurrentStopIndex))
		{
			OutResolvedStopIndex = CurrentStopIndex;
			return CurrentRules;
		}

		const int32 StopCount = TimeTable->GetNumStops();
		if (StopCount > 0 && CurrentStopIndex >= 0)
		{
			const int32 PreviousStopIndex =
				(CurrentStopIndex + StopCount - 1) % StopCount;
			if (PreviousStopIndex != CurrentStopIndex)
			{
				if (const FATCMStopRules* PreviousRules =
					FindAtIndex(PreviousStopIndex))
				{
					OutResolvedStopIndex = PreviousStopIndex;
					return PreviousRules;
				}
			}
		}

		// Only use the station-only fallback if it is unambiguous. Duplicate
		// visits to the same station must continue to use their stop index.
		const FATCMStopRules* UniqueStationRules = nullptr;
		for (const FATCMStopRules& Stop : Schedule->Stops)
		{
			if (Stop.Rules.IsEmpty() || !MatchesDockingStation(Stop))
			{
				continue;
			}

			if (UniqueStationRules)
			{
				return nullptr;
			}

			UniqueStationRules = &Stop;
			OutResolvedStopIndex = Stop.StopIndex;
		}

		return UniqueStationRules;
	}
}

bool FATCMTransferHooks::ShouldSuppressCargoAnimation(
	AFGBuildableTrainPlatformCargo* Platform,
	const bool IsLoadDirection)
{
	if (!IsValid(Platform))
	{
		return false;
	}

	AFGFreightWagon* Wagon =
		Cast<AFGFreightWagon>(Platform->GetDockedActor());
	AFGBuildableRailroadStation* Station =
		Platform->mStationDockingMaster;
	AFGTrain* Train = Wagon ? Wagon->GetTrain() : nullptr;
	AFGRailroadTimeTable* TimeTable =
		Train ? Train->GetTimeTable() : nullptr;
	AATCMSubsystem* Subsystem = AATCMSubsystem::Get(Platform);

	if (!Wagon || !Station || !Train || !TimeTable || !Subsystem)
	{
		return false;
	}

	int32 ResolvedStopIndex = INDEX_NONE;
	const FATCMStopRules* StopRules =
		ResolveStopRulesForDocking(
			Subsystem,
			TimeTable,
			Station,
			ResolvedStopIndex);

	// No advanced entries at this stop means completely vanilla behavior.
	if (!StopRules)
	{
		return false;
	}

	const int32 WagonIndex =
		GetFreightWagonIndex(Train, Wagon);
	if (WagonIndex == INDEX_NONE)
	{
		return false;
	}

	const EATCMTransferDirection Direction =
		IsLoadDirection
			? EATCMTransferDirection::Load
			: EATCMTransferDirection::Unload;

	const bool HasApplicableRule =
		StopRules->Rules.ContainsByPredicate(
			[WagonIndex, Direction](const FATCMTransferRule& Rule)
			{
				return Rule.FreightWagonIndex == WagonIndex
					&& Rule.Direction == Direction
					&& Rule.ItemDescriptor
					&& (Rule.TransferAll
						|| Rule.Amount > 0);
			});

	return !HasApplicableRule;
}

bool FATCMTransferHooks::IsPlatformCargoVisualSuppressed(
	AFGBuildableTrainPlatformCargo* Platform)
{
	if (!IsValid(Platform))
	{
		return false;
	}

	AFGFreightWagon* Wagon =
		Cast<AFGFreightWagon>(Platform->GetDockedActor());

	if (IsCargoAnimationSuppressed(Wagon))
	{
		return true;
	}

	return ShouldSuppressCargoAnimation(
		Platform,
		Platform->GetIsInLoadMode());
}

void FATCMTransferHooks::SynchronizeWagonVisuals(
	AFGBuildableTrainPlatformCargo* Platform)
{
	if (!IsValid(Platform))
	{
		return;
	}

	AFGFreightWagon* Wagon =
		Cast<AFGFreightWagon>(Platform->GetDockedActor());
	if (!Wagon)
	{
		return;
	}

	Wagon->UpdateFreightCargoType();
	Wagon->UpdateFreightPayloadMass();

	UFGInventoryComponent* WagonInventory =
		Wagon->GetFreightInventory();

	Wagon->SetCargoMeshVisibility(
		WagonInventory && !WagonInventory->IsEmpty());
}

void FATCMTransferHooks::SetCargoAnimationSuppressed(
	AFGBuildableTrainPlatformCargo* Platform,
	const bool Suppressed)
{
	AFGFreightWagon* Wagon = IsValid(Platform)
		? Cast<AFGFreightWagon>(Platform->GetDockedActor())
		: nullptr;

	if (!Wagon)
	{
		return;
	}

	const TWeakObjectPtr<AFGFreightWagon> WagonKey(Wagon);
	const bool WasSuppressed =
		SuppressedCargoVisualWagons.Contains(WagonKey);

	if (Suppressed)
	{
		SuppressedCargoVisualWagons.Add(WagonKey);
	}
	else
	{
		SuppressedCargoVisualWagons.Remove(WagonKey);
	}

	// The cargo platform animation blueprint also reads the replicated docking
	// state directly. Cancelling its blueprint events alone therefore is not
	// sufficient. Pause the skeletal cargo animation while this wagon has no
	// applicable entry, then resume and refresh it on the next normal docking.
	if (Platform->mMagicBoxSkelMeshComponent)
	{
		Platform->mMagicBoxSkelMeshComponent->bPauseAnims =
			Suppressed;
	}

	if (!Suppressed && WasSuppressed)
	{
		Platform->ForceUpdateAnimInstance();
	}
}

bool FATCMTransferHooks::IsCargoAnimationSuppressed(
	AFGFreightWagon* Wagon)
{
	if (!Wagon)
	{
		return false;
	}

	const TWeakObjectPtr<AFGFreightWagon> WagonKey(Wagon);
	return SuppressedCargoVisualWagons.Contains(WagonKey);
}

void FATCMTransferHooks::Install()
{
	if (TransferInventoryHook.IsValid())
	{
		return;
	}

	TransferInventoryHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::TransferInventory,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform,
			UFGInventoryComponent* Source,
			UFGInventoryComponent* Target)
		{
			if (!IsValid(Platform)
				|| !Platform->HasAuthority()
				|| !IsValid(Source)
				|| !IsValid(Target))
			{
				return;
			}

			AFGFreightWagon* Wagon =
				Cast<AFGFreightWagon>(Platform->GetDockedActor());
			AFGBuildableRailroadStation* Station =
				Platform->mStationDockingMaster;
			AFGTrain* Train = Wagon ? Wagon->GetTrain() : nullptr;
			AFGRailroadTimeTable* TimeTable =
				Train ? Train->GetTimeTable() : nullptr;
			AATCMSubsystem* Subsystem =
				AATCMSubsystem::Get(Platform);

			if (!Wagon || !Station || !TimeTable || !Subsystem)
			{
				return;
			}

			int32 ResolvedStopIndex = INDEX_NONE;
			const FATCMStopRules* StopRules =
				ResolveStopRulesForDocking(
					Subsystem,
					TimeTable,
					Station,
					ResolvedStopIndex);

			if (!StopRules)
			{
				if (Subsystem->FindSchedule(TimeTable))
				{
					const AFGTrainStationIdentifier* StationIdentifier =
						Station->GetStationIdentifier();
					UE_LOG(
						LogAdvancedTrainCargoManager,
						Warning,
						TEXT("Stored schedule exists, but no advanced rule matched station '%s' (CurrentStop=%d)"),
						StationIdentifier
							? *StationIdentifier->GetStationName().ToString()
							: *GetNameSafe(Station),
						TimeTable->GetCurrentStop());
				}
				return;
			}

			// As soon as this stop has advanced entries, unlisted
			// wagons/directions explicitly transfer nothing.
			const int32 WagonIndex =
				GetFreightWagonIndex(Train, Wagon);
			if (WagonIndex == INDEX_NONE)
			{
				return;
			}

			const EATCMTransferDirection ActiveDirection =
				Platform->GetIsInLoadMode()
					? EATCMTransferDirection::Load
					: EATCMTransferDirection::Unload;

			bool HasApplicableRule = false;
			int64 TotalTransferred = 0;

			for (const FATCMTransferRule& Rule : StopRules->Rules)
			{
				if (Rule.FreightWagonIndex == WagonIndex
					&& Rule.Direction == ActiveDirection
					&& Rule.ItemDescriptor
					&& (Rule.TransferAll
						|| Rule.Amount > 0))
				{
					HasApplicableRule = true;
					TotalTransferred += TransferItemAmount(
						Source,
						Target,
						Rule.ItemDescriptor,
						Rule.TransferAll
							? MAX_int32
							: Rule.Amount);
				}
			}

			// Vanilla also resets its internal docking state in this function.
			// Run it exactly once with a temporarily empty source so that it
			// completes the sequence without performing an additional transfer.
			TArray<FInventoryStack> PreservedSourceStacks =
				Source->mInventoryStacks;
			for (FInventoryStack& Stack : Source->mInventoryStacks)
			{
				Stack = FInventoryStack();
			}

			Scope(Platform, Source, Target);

			Source->mInventoryStacks =
				MoveTemp(PreservedSourceStacks);
			Source->MarkInventoryContentsDirty();

			// Keep the suppression flag for an unlisted wagon. Satisfactory can
			// issue another visibility update after the docking actor was
			// detached, so clearing it here would reveal an empty container.
			SetCargoAnimationSuppressed(
				Platform,
				!HasApplicableRule);

			Wagon->UpdateFreightCargoType();
			Wagon->UpdateFreightPayloadMass();

			UFGInventoryComponent* WagonInventory =
				Wagon->GetFreightInventory();
			Wagon->SetCargoMeshVisibility(
				WagonInventory && !WagonInventory->IsEmpty());

			Platform->UpdateItemTransferRate(
				static_cast<int32>(
					FMath::Min<int64>(
						TotalTransferred,
						MAX_int32)));

			UE_LOG(
				LogAdvancedTrainCargoManager,
				Log,
				TEXT("Advanced transfer at station '%s': %lld unit(s), stop %d, CurrentStop=%d, freight wagon %d"),
				Station->GetStationIdentifier()
					? *Station->GetStationIdentifier()->GetStationName().ToString()
					: *GetNameSafe(Station),
				TotalTransferred,
				ResolvedStopIndex,
				TimeTable->GetCurrentStop(),
				WagonIndex + 1);
		});

	// This observer only initializes the visual suppression state. It never
	// changes or completes a vanilla docking state.
	NotifyTrainDockedVisualHook =
		SUBSCRIBE_UOBJECT_METHOD_AFTER(
			AFGBuildableTrainPlatformCargo,
			NotifyTrainDocked,
			[](AFGBuildableTrainPlatformCargo* Platform,
				AFGRailroadVehicle*,
				AFGBuildableRailroadStation*)
			{
				const bool IsLoadDirection =
					Platform && Platform->GetIsInLoadMode();
				const bool Suppress =
					FATCMTransferHooks::
						ShouldSuppressCargoAnimation(
							Platform,
							IsLoadDirection);

				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						Suppress);

				if (Suppress)
				{
					FATCMTransferHooks::
						SynchronizeWagonVisuals(Platform);
				}
			});

	// OnRep_UpdateDockingStatus starts client-side cargo-platform visuals.
	// Cancelling it is visual-only; unlike UpdateDockingSequence it does not
	// stop the server-side docking state machine.
	UpdateDockingStatusVisualHook =
		SUBSCRIBE_UOBJECT_METHOD(
			AFGBuildableTrainPlatformCargo,
			OnRep_UpdateDockingStatus,
			[](auto& Scope,
				AFGBuildableTrainPlatformCargo* Platform)
			{
				const bool Suppress =
					FATCMTransferHooks::
						ShouldSuppressCargoAnimation(
							Platform,
							Platform
								&& Platform->GetIsInLoadMode());

				if (Suppress)
				{
					FATCMTransferHooks::
						SetCargoAnimationSuppressed(
							Platform,
							true);
					Scope.Cancel();
					FATCMTransferHooks::
						SynchronizeWagonVisuals(Platform);
					return;
				}

				// A newly docked applicable/vanilla stop must be able to clear a
				// suppression marker left by an earlier empty no-rule docking.
				if (Platform && Platform->GetDockedActor())
				{
					FATCMTransferHooks::
						SetCargoAnimationSuppressed(
							Platform,
							false);
				}
			});

	// This method only forces the cargo animation instance to update. Blocking
	// it for a suppressed platform prevents the crane animation from being
	// restarted through a late replicated visual update.
	ForceUpdateAnimInstanceHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::ForceUpdateAnimInstance,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			if (FATCMTransferHooks::
				IsPlatformCargoVisualSuppressed(Platform))
			{
				Scope.Cancel();
			}
		});

	BeginLoadSequenceHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::OnBeginLoadSequence,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			const bool Suppress =
				FATCMTransferHooks::
					ShouldSuppressCargoAnimation(
						Platform,
						true);

			FATCMTransferHooks::
				SetCargoAnimationSuppressed(
					Platform,
					Suppress);

			if (Suppress)
			{
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
		});

	BeginUnloadSequenceHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::OnBeginUnloadSequence,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			const bool Suppress =
				FATCMTransferHooks::
					ShouldSuppressCargoAnimation(
						Platform,
						false);

			FATCMTransferHooks::
				SetCargoAnimationSuppressed(
					Platform,
					Suppress);

			if (Suppress)
			{
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
		});

	TransferCompleteAnimationHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::OnTransferComplete,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			const bool Suppress =
				FATCMTransferHooks::
					IsPlatformCargoVisualSuppressed(
						Platform);

			if (Suppress)
			{
				// Keep the marker beyond OnTransferComplete because the wagon
				// receives a final mesh-visibility update after undocking.
				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						true);
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
			else
			{
				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						false);
			}
		});

	SwapCargoContainerVisibilityHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::
			SwapCargoContainerVisibility,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			if (FATCMTransferHooks::
				IsPlatformCargoVisualSuppressed(Platform))
			{
				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						true);
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
		});

	ShowPlatformCargoContainerHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::
			ShowPlatformCargoContainer,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			if (FATCMTransferHooks::
				IsPlatformCargoVisualSuppressed(Platform))
			{
				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						true);
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
		});

	MatchCargoContainerVisualsHook = SUBSCRIBE_METHOD(
		AFGBuildableTrainPlatformCargo::
			MatchVisualsOfCargoContainerToTrain,
		[](auto& Scope,
			AFGBuildableTrainPlatformCargo* Platform)
		{
			if (FATCMTransferHooks::
				IsPlatformCargoVisualSuppressed(Platform))
			{
				FATCMTransferHooks::
					SetCargoAnimationSuppressed(
						Platform,
						true);
				Scope.Cancel();
				FATCMTransferHooks::
					SynchronizeWagonVisuals(Platform);
			}
		});

	CargoMeshVisibilityHook = SUBSCRIBE_METHOD(
		AFGFreightWagon::SetCargoMeshVisibility,
		[](auto& Scope,
			AFGFreightWagon* Wagon,
			const bool IsVisible)
		{
			if (!IsVisible
				|| !FATCMTransferHooks::
					IsCargoAnimationSuppressed(Wagon))
			{
				return;
			}

			UFGInventoryComponent* WagonInventory =
				Wagon ? Wagon->GetFreightInventory() : nullptr;

			// Never hide real cargo. Only reject a request to show the
			// temporary container on a genuinely empty suppressed wagon.
			if (!WagonInventory || WagonInventory->IsEmpty())
			{
				Scope.Cancel();
			}
		});
}

void FATCMTransferHooks::Uninstall()
{
	if (CargoMeshVisibilityHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGFreightWagon::SetCargoMeshVisibility,
			CargoMeshVisibilityHook);
		CargoMeshVisibilityHook.Reset();
	}

	if (MatchCargoContainerVisualsHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				MatchVisualsOfCargoContainerToTrain,
			MatchCargoContainerVisualsHook);
		MatchCargoContainerVisualsHook.Reset();
	}

	if (ShowPlatformCargoContainerHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				ShowPlatformCargoContainer,
			ShowPlatformCargoContainerHook);
		ShowPlatformCargoContainerHook.Reset();
	}

	if (SwapCargoContainerVisibilityHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				SwapCargoContainerVisibility,
			SwapCargoContainerVisibilityHook);
		SwapCargoContainerVisibilityHook.Reset();
	}

	if (TransferCompleteAnimationHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::OnTransferComplete,
			TransferCompleteAnimationHook);
		TransferCompleteAnimationHook.Reset();
	}

	if (BeginUnloadSequenceHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				OnBeginUnloadSequence,
			BeginUnloadSequenceHook);
		BeginUnloadSequenceHook.Reset();
	}

	if (BeginLoadSequenceHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				OnBeginLoadSequence,
			BeginLoadSequenceHook);
		BeginLoadSequenceHook.Reset();
	}

	if (ForceUpdateAnimInstanceHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::
				ForceUpdateAnimInstance,
			ForceUpdateAnimInstanceHook);
		ForceUpdateAnimInstanceHook.Reset();
	}

	if (UpdateDockingStatusVisualHook.IsValid())
	{
		UNSUBSCRIBE_UOBJECT_METHOD(
			AFGBuildableTrainPlatformCargo,
			OnRep_UpdateDockingStatus,
			UpdateDockingStatusVisualHook);
		UpdateDockingStatusVisualHook.Reset();
	}

	if (NotifyTrainDockedVisualHook.IsValid())
	{
		UNSUBSCRIBE_UOBJECT_METHOD(
			AFGBuildableTrainPlatformCargo,
			NotifyTrainDocked,
			NotifyTrainDockedVisualHook);
		NotifyTrainDockedVisualHook.Reset();
	}

	if (TransferInventoryHook.IsValid())
	{
		UNSUBSCRIBE_METHOD(
			AFGBuildableTrainPlatformCargo::TransferInventory,
			TransferInventoryHook);
		TransferInventoryHook.Reset();
	}

	SuppressedCargoVisualWagons.Reset();
}

int32 FATCMTransferHooks::GetFreightWagonIndex(
	const AFGTrain* Train,
	const AFGFreightWagon* Wagon)
{
	if (!Train || !Wagon)
	{
		return INDEX_NONE;
	}

	int32 FreightIndex = 0;
	for (AFGRailroadSubsystem::TTrainIterator It(
		Train->GetFirstVehicle()); It; ++It)
	{
		if (const AFGFreightWagon* Candidate =
			Cast<AFGFreightWagon>(*It))
		{
			if (Candidate == Wagon)
			{
				return FreightIndex;
			}
			++FreightIndex;
		}
	}

	return INDEX_NONE;
}

int32 FATCMTransferHooks::TransferItemAmount(
	UFGInventoryComponent* Source,
	UFGInventoryComponent* Target,
	const TSubclassOf<UFGItemDescriptor> ItemDescriptor,
	int32 RequestedAmount)
{
	int32 Remaining = FMath::Max(0, RequestedAmount);
	int32 Transferred = 0;

	for (int32 SlotIndex = 0;
		SlotIndex < Source->GetSizeLinear() && Remaining > 0;
		++SlotIndex)
	{
		FInventoryStack SourceStack;
		if (!Source->GetStackFromIndex(
				SlotIndex,
				SourceStack)
			|| !SourceStack.HasItems()
			|| SourceStack.Item.GetItemClass() != ItemDescriptor)
		{
			continue;
		}

		FInventoryStack TransferStack = SourceStack;
		TransferStack.NumItems =
			FMath::Min(
				SourceStack.NumItems,
				Remaining);

		const int32 Added =
			Target->AddStack(
				TransferStack,
				true);
		if (Added > 0)
		{
			Source->RemoveFromIndex(
				SlotIndex,
				Added,
				Target);
			Transferred += Added;
			Remaining -= Added;
		}
	}

	return Transferred;
}

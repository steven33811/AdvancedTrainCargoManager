#pragma once

#include "CoreMinimal.h"

class AFGFreightWagon;
class AFGTrain;
class AFGBuildableTrainPlatformCargo;
class UFGInventoryComponent;
class UFGItemDescriptor;

/**
 * Applies the configured cargo quantities and keeps the cargo visuals in sync.
 */
class FATCMTransferHooks final
{
public:
	static void Install();
	static void Uninstall();

private:
	static bool ShouldSuppressCargoAnimation(
		AFGBuildableTrainPlatformCargo* Platform,
		bool IsLoadDirection);

	static bool IsPlatformCargoVisualSuppressed(
		AFGBuildableTrainPlatformCargo* Platform);

	static void SynchronizeWagonVisuals(
		AFGBuildableTrainPlatformCargo* Platform);

	static void SetCargoAnimationSuppressed(
		AFGBuildableTrainPlatformCargo* Platform,
		bool Suppressed);

	static bool IsCargoAnimationSuppressed(
		AFGFreightWagon* Wagon);

	static int32 GetFreightWagonIndex(
		const AFGTrain* Train,
		const AFGFreightWagon* Wagon);

	static int32 TransferItemAmount(
		UFGInventoryComponent* Source,
		UFGInventoryComponent* Target,
		TSubclassOf<UFGItemDescriptor> ItemDescriptor,
		int32 RequestedAmount);

	static FDelegateHandle TransferInventoryHook;
	static FDelegateHandle BeginLoadSequenceHook;
	static FDelegateHandle BeginUnloadSequenceHook;
	static FDelegateHandle NotifyTrainDockedVisualHook;
	static FDelegateHandle TransferCompleteAnimationHook;
	static FDelegateHandle SwapCargoContainerVisibilityHook;
	static FDelegateHandle ShowPlatformCargoContainerHook;
	static FDelegateHandle MatchCargoContainerVisualsHook;
	static FDelegateHandle CargoMeshVisibilityHook;
	static FDelegateHandle UpdateDockingStatusVisualHook;
	static FDelegateHandle ForceUpdateAnimInstanceHook;
};

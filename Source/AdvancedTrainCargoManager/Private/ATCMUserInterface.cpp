#include "ATCMUserInterface.h"
#include "ATCMManagerWidget.h"
#include "ATCMModConfiguration.h"

#include "AdvancedTrainCargoManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	APlayerController* GetLocalPlayerController()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}

		UWorld* World =
			GEngine->GameViewport->GetWorld();

		return World
			? World->GetFirstPlayerController()
			: nullptr;
	}

	FInputChord GetConfiguredHotkey()
	{
		APlayerController* PlayerController =
			GetLocalPlayerController();

		return UATCMModConfiguration::
			GetOpenManagerHotkey(PlayerController);
	}

	template<typename TInputEvent>
	bool MatchesConfiguredHotkey(
		const FKey& EventKey,
		const TInputEvent& InputEvent)
	{
		if (UATCMModConfiguration::IsCapturingHotkey())
		{
			return false;
		}

		const FInputChord Hotkey =
			GetConfiguredHotkey();

		if (!Hotkey.IsValidChord() ||
			EventKey != Hotkey.Key)
		{
			return false;
		}

		return
			InputEvent.IsControlDown() ==
				static_cast<bool>(Hotkey.bCtrl) &&
			InputEvent.IsAltDown() ==
				static_cast<bool>(Hotkey.bAlt) &&
			InputEvent.IsShiftDown() ==
				static_cast<bool>(Hotkey.bShift) &&
			InputEvent.IsCommandDown() ==
				static_cast<bool>(Hotkey.bCmd);
	}

	class FATCMInputProcessor final : public IInputProcessor
	{
	public:
		virtual void Tick(
			const float DeltaTime,
			FSlateApplication& SlateApp,
			TSharedRef<ICursor> Cursor) override
		{
			// Keine Tick-Logik erforderlich.
		}

		virtual bool HandleKeyDownEvent(
			FSlateApplication& SlateApp,
			const FKeyEvent& InKeyEvent) override
		{
			if (MatchesConfiguredHotkey(
					InKeyEvent.GetKey(),
					InKeyEvent))
			{
				FATCMUserInterface::Toggle();
				return true;
			}

			if (InKeyEvent.GetKey() == EKeys::Escape &&
				FATCMUserInterface::IsOpen())
			{
				FATCMUserInterface::Close();
				return true;
			}

			return false;
		}

		virtual bool HandleMouseButtonDownEvent(
			FSlateApplication& SlateApp,
			const FPointerEvent& MouseEvent) override
		{
			if (MatchesConfiguredHotkey(
					MouseEvent.GetEffectingButton(),
					MouseEvent))
			{
				FATCMUserInterface::Toggle();
				return true;
			}

			return false;
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("AdvancedTrainCargoManagerInput");
		}
	};

	TSharedPtr<FATCMInputProcessor> InputProcessor;
	TSharedPtr<SWidget> ViewportContainer;
	TWeakObjectPtr<APlayerController> OwningPlayerController;

	bool PreviousShowMouseCursor = false;

	FAutoConsoleCommand ToggleConsoleCommand(
		TEXT("ATCM.Open"),
		TEXT("Öffnet oder schließt den Advanced Train Cargo Manager."),
		FConsoleCommandDelegate::CreateStatic(
			&FATCMUserInterface::Toggle));
}

void FATCMUserInterface::Initialize()
{
	if (IsRunningDedicatedServer() ||
		InputProcessor.IsValid() ||
		!FSlateApplication::IsInitialized())
	{
		return;
	}

	InputProcessor = MakeShared<FATCMInputProcessor>();

	FSlateApplication::Get().RegisterInputPreProcessor(
		InputProcessor,
		0);

	UE_LOG(
		LogAdvancedTrainCargoManager,
		Log,
		TEXT("ATCM-Benutzerschnittstelle wurde initialisiert."));
}

void FATCMUserInterface::Shutdown()
{
	Close();

	if (InputProcessor.IsValid() &&
		FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get()
			.UnregisterInputPreProcessor(InputProcessor);

		InputProcessor.Reset();
	}
}

void FATCMUserInterface::Toggle()
{
	if (IsOpen())
	{
		Close();
	}
	else
	{
		Open();
	}
}

void FATCMUserInterface::Open()
{
	if (IsOpen() ||
		!GEngine ||
		!GEngine->GameViewport ||
		!FSlateApplication::IsInitialized())
	{
		return;
	}

	UWorld* World =
		GEngine->GameViewport->GetWorld();

	APlayerController* PlayerController =
		World ? World->GetFirstPlayerController() : nullptr;

	if (!PlayerController)
	{
		UE_LOG(
			LogAdvancedTrainCargoManager,
			Warning,
			TEXT("ATCM-UI kann ohne lokalen Spieler "
			     "nicht geöffnet werden."));

		return;
	}

	OwningPlayerController = PlayerController;
	PreviousShowMouseCursor =
		PlayerController->ShouldShowMouseCursor();

	ViewportContainer =
		SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderBackgroundColor(
				FLinearColor(0.0f, 0.0f, 0.0f, 0.70f))
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(1100.0f)
			.HeightOverride(720.0f)
			[
				CreateATCMManagerWidget(
					FSimpleDelegate::CreateStatic(
						&FATCMUserInterface::Close))
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(
		ViewportContainer.ToSharedRef(),
		10000);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(ViewportContainer);
	InputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);

	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(true);

	UE_LOG(
		LogAdvancedTrainCargoManager,
		Log,
		TEXT("ATCM-UI wurde geöffnet."));
}

void FATCMUserInterface::Close()
{
	if (ViewportContainer.IsValid() &&
		GEngine &&
		GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(
			ViewportContainer.ToSharedRef());
	}

	if (APlayerController* PlayerController =
		OwningPlayerController.Get())
	{
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);

		PlayerController->SetShowMouseCursor(
			PreviousShowMouseCursor);
	}

	ViewportContainer.Reset();
	OwningPlayerController.Reset();
}

bool FATCMUserInterface::IsOpen()
{
	return ViewportContainer.IsValid();
}

FText FATCMUserInterface::GetHotkeyDisplayText()
{
	return GetConfiguredHotkey().GetInputText(true);
}

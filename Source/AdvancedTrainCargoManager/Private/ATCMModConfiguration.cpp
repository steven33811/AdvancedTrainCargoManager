#include "ATCMModConfiguration.h"

#include "Configuration/ConfigManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ATCMConfig"

namespace
{
	const TCHAR* HotkeyPropertyKey =
		TEXT("OpenManagerHotkey");

	const FString DefaultHotkeyValue =
		TEXT("F6|0|0|0|0");

	bool bCapturingHotkey = false;

	FInputChord GetDefaultHotkey()
	{
		return FInputChord(EKeys::F6);
	}
}

UATCMHotkeyConfigProperty::UATCMHotkeyConfigProperty()
{
	DisplayName = LOCTEXT(
		"OpenManagerHotkeyLabel",
		"Open/close menu");

	Tooltip = LOCTEXT(
		"OpenManagerHotkeyTooltip",
		"Key used to open or close the Advanced Train "
		"Cargo Manager.");

	DefaultValue = DefaultHotkeyValue;
	Value = DefaultHotkeyValue;
}

FInputChord UATCMHotkeyConfigProperty::GetInputChord() const
{
	return DeserializeInputChord(Value);
}

void UATCMHotkeyConfigProperty::SetInputChord(
	const FInputChord& InputChord)
{
	if (!InputChord.IsValidChord())
	{
		return;
	}

	const FString NewValue =
		SerializeInputChord(InputChord);

	if (Value == NewValue)
	{
		return;
	}

	Value = NewValue;
	MarkDirty();
}

FString UATCMHotkeyConfigProperty::SerializeInputChord(
	const FInputChord& InputChord)
{
	return FString::Printf(
		TEXT("%s|%d|%d|%d|%d"),
		*InputChord.Key.GetFName().ToString(),
		InputChord.bShift ? 1 : 0,
		InputChord.bCtrl ? 1 : 0,
		InputChord.bAlt ? 1 : 0,
		InputChord.bCmd ? 1 : 0);
}

FInputChord UATCMHotkeyConfigProperty::DeserializeInputChord(
	const FString& SerializedInputChord)
{
	TArray<FString> Parts;
	SerializedInputChord.ParseIntoArray(
		Parts,
		TEXT("|"),
		false);

	if (Parts.IsEmpty())
	{
		return GetDefaultHotkey();
	}

	const FKey Key{FName(*Parts[0])};

	if (!Key.IsValid() || Key.IsModifierKey())
	{
		return GetDefaultHotkey();
	}

	if (Parts.Num() == 1)
	{
		return FInputChord(Key);
	}

	if (Parts.Num() != 5)
	{
		return GetDefaultHotkey();
	}

	return FInputChord(
		Key,
		FCString::Atoi(*Parts[1]) != 0,
		FCString::Atoi(*Parts[2]) != 0,
		FCString::Atoi(*Parts[3]) != 0,
		FCString::Atoi(*Parts[4]) != 0);
}

UATCMConfigRootSection::UATCMConfigRootSection()
{
	bAllowUserReset = true;

	UATCMHotkeyConfigProperty* HotkeyProperty =
		CreateDefaultSubobject<UATCMHotkeyConfigProperty>(
			TEXT("OpenManagerHotkey"));

	SectionProperties.Add(
		HotkeyPropertyKey,
		HotkeyProperty);
}

UUserWidget*
	UATCMConfigRootSection::CreateEditorWidget_Implementation(
		UUserWidget* ParentWidget) const
{
	if (!ParentWidget)
	{
		return nullptr;
	}

	UATCMHotkeyConfigWidget* Widget =
		CreateWidget<UATCMHotkeyConfigWidget>(
			ParentWidget,
			UATCMHotkeyConfigWidget::StaticClass());

	if (Widget)
	{
		Widget->SetConfigProperty(
			GetHotkeyProperty());
	}

	return Widget;
}

UATCMHotkeyConfigProperty*
	UATCMConfigRootSection::GetHotkeyProperty() const
{
	const TObjectPtr<UConfigProperty>* Property =
		SectionProperties.Find(HotkeyPropertyKey);

	return Property
		? Cast<UATCMHotkeyConfigProperty>(Property->Get())
		: nullptr;
}

void UATCMHotkeyConfigWidget::SetConfigProperty(
	UATCMHotkeyConfigProperty* InConfigProperty)
{
	ConfigProperty = InConfigProperty;
}

TSharedRef<SWidget>
	UATCMHotkeyConfigWidget::RebuildWidget()
{
	return
		SNew(SBorder)
		.BorderBackgroundColor(
			FLinearColor(0.035f, 0.045f, 0.055f, 0.95f))
		.Padding(14.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT(
						"OpenManagerHotkeyLabel",
						"Open/close menu"))
					.Font(
						FCoreStyle::GetDefaultFontStyle(
							"Bold",
							12))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 12.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT(
						"OpenManagerHotkeyTooltip",
						"Key used to open or close the "
						"Advanced Train Cargo Manager."))
					.ColorAndOpacity(
						FLinearColor(
							0.65f,
							0.68f,
							0.72f,
							1.0f))
					.AutoWrapText(true)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(150.0f)
				[
					SAssignNew(
						InputKeySelector,
						SInputKeySelector)
						.SelectedKey_Lambda(
							[this]
							{
								return GetSelectedInputChord();
							})
						.KeySelectionText(LOCTEXT(
							"SelectHotkeyPrompt",
							"Press a key..."))
						.NoKeySpecifiedText(LOCTEXT(
							"NoHotkey",
							"Unbound"))
						.AllowModifierKeys(true)
						.AllowGamepadKeys(false)
						.EscapeCancelsSelection(true)
						.OnKeySelected(
							SInputKeySelector::
								FOnKeySelected::
								CreateUObject(
									this,
									&UATCMHotkeyConfigWidget::
										HandleInputChordSelected))
						.OnIsSelectingKeyChanged(
							SInputKeySelector::
								FOnIsSelectingKeyChanged::
								CreateUObject(
									this,
									&UATCMHotkeyConfigWidget::
										HandleSelectingKeyChanged))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
					.Text(LOCTEXT(
						"ResetHotkeyButton",
						"Reset to F6"))
					.OnClicked_UObject(
						this,
						&UATCMHotkeyConfigWidget::
							HandleResetClicked)
			]
		];
}

void UATCMHotkeyConfigWidget::NativeDestruct()
{
	UATCMModConfiguration::SetCapturingHotkey(false);
	InputKeySelector.Reset();

	Super::NativeDestruct();
}

FInputChord
	UATCMHotkeyConfigWidget::GetSelectedInputChord() const
{
	return ConfigProperty
		? ConfigProperty->GetInputChord()
		: GetDefaultHotkey();
}

void UATCMHotkeyConfigWidget::HandleInputChordSelected(
	const FInputChord& InputChord)
{
	if (ConfigProperty)
	{
		ConfigProperty->SetInputChord(InputChord);
	}
}

void UATCMHotkeyConfigWidget::HandleSelectingKeyChanged()
{
	UATCMModConfiguration::SetCapturingHotkey(
		InputKeySelector.IsValid() &&
		InputKeySelector->GetIsSelectingKey());
}

FReply UATCMHotkeyConfigWidget::HandleResetClicked()
{
	if (ConfigProperty)
	{
		ConfigProperty->ResetToDefault();
	}

	return FReply::Handled();
}

UATCMModConfiguration::UATCMModConfiguration()
{
	ConfigId = GetATCMConfigId();
	DisplayName = LOCTEXT(
		"ConfigurationDisplayName",
		"Controls");
	Description = LOCTEXT(
		"ConfigurationDescription",
		"Configure Advanced Train Cargo Manager controls.");

	RootSection =
		CreateDefaultSubobject<UATCMConfigRootSection>(
			TEXT("RootSection"));
}

FConfigId UATCMModConfiguration::GetATCMConfigId()
{
	FConfigId Result;
	Result.ModReference =
		TEXT("AdvancedTrainCargoManager");
	Result.ConfigCategory = TEXT("");
	return Result;
}

FInputChord UATCMModConfiguration::GetOpenManagerHotkey(
	const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return GetDefaultHotkey();
	}

	UGameInstance* GameInstance =
		Cast<UGameInstance>(
			const_cast<UObject*>(WorldContext));

	if (!GameInstance)
	{
		if (const UWorld* World =
			WorldContext->GetWorld())
		{
			GameInstance = World->GetGameInstance();
		}
	}

	if (!GameInstance)
	{
		return GetDefaultHotkey();
	}

	UConfigManager* ConfigManager =
		GameInstance->GetSubsystem<UConfigManager>();

	if (!ConfigManager)
	{
		return GetDefaultHotkey();
	}

	UATCMConfigRootSection* RootSectionValue =
		Cast<UATCMConfigRootSection>(
			ConfigManager->GetConfigurationRootSection(
				GetATCMConfigId()));

	UATCMHotkeyConfigProperty* HotkeyProperty =
		RootSectionValue
			? RootSectionValue->GetHotkeyProperty()
			: nullptr;

	return HotkeyProperty
		? HotkeyProperty->GetInputChord()
		: GetDefaultHotkey();
}

bool UATCMModConfiguration::IsCapturingHotkey()
{
	return bCapturingHotkey;
}

void UATCMModConfiguration::SetCapturingHotkey(
	const bool bIsCapturing)
{
	bCapturingHotkey = bIsCapturing;
}

#undef LOCTEXT_NAMESPACE

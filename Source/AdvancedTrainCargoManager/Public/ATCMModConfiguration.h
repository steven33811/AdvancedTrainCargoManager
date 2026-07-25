#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Configuration/ModConfiguration.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/ConfigPropertyString.h"
#include "Framework/Commands/InputChord.h"
#include "ATCMModConfiguration.generated.h"

class SInputKeySelector;

UCLASS(EditInlineNew)
class ADVANCEDTRAINCARGOMANAGER_API UATCMHotkeyConfigProperty final
	: public UConfigPropertyString
{
	GENERATED_BODY()

public:
	UATCMHotkeyConfigProperty();

	FInputChord GetInputChord() const;
	void SetInputChord(const FInputChord& InputChord);

private:
	static FString SerializeInputChord(
		const FInputChord& InputChord);
	static FInputChord DeserializeInputChord(
		const FString& SerializedInputChord);
};

UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMConfigRootSection final
	: public UConfigPropertySection
{
	GENERATED_BODY()

public:
	UATCMConfigRootSection();

	virtual UUserWidget*
		CreateEditorWidget_Implementation(
			UUserWidget* ParentWidget) const override;

	UATCMHotkeyConfigProperty*
		GetHotkeyProperty() const;
};

UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMHotkeyConfigWidget final
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void SetConfigProperty(
		UATCMHotkeyConfigProperty* InConfigProperty);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	FInputChord GetSelectedInputChord() const;
	void HandleInputChordSelected(
		const FInputChord& InputChord);
	void HandleSelectingKeyChanged();
	FReply HandleResetClicked();

	UPROPERTY(Transient)
	TObjectPtr<UATCMHotkeyConfigProperty> ConfigProperty;

	TSharedPtr<SInputKeySelector> InputKeySelector;
};

UCLASS()
class ADVANCEDTRAINCARGOMANAGER_API UATCMModConfiguration final
	: public UModConfiguration
{
	GENERATED_BODY()

public:
	UATCMModConfiguration();

	static FConfigId GetATCMConfigId();
	static FInputChord GetOpenManagerHotkey(
		const UObject* WorldContext);

	static bool IsCapturingHotkey();
	static void SetCapturingHotkey(bool bIsCapturing);
};

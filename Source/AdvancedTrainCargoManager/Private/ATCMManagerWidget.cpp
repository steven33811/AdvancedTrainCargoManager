#include "ATCMManagerWidget.h"

#include "ATCMRemoteCallObject.h"
#include "ATCMSubsystem.h"
#include "ATCMUserInterface.h"
#include "Buildables/FGBuildableRailroadStation.h"
#include "Buildables/FGBuildableTrainPlatformCargo.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "FGBlueprintFunctionLibrary.h"
#include "FGFreightWagon.h"
#include "FGPlayerController.h"
#include "FGRailroadSubsystem.h"
#include "FGRailroadTimeTable.h"
#include "FGTrain.h"
#include "FGTrainPlatformConnection.h"
#include "FGTrainStationIdentifier.h"
#include "InputCoreTypes.h"
#include "Resources/FGItemDescriptor.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "AdvancedTrainCargoManager"

namespace
{
	const FLinearColor BackgroundColor(
		0.025f, 0.035f, 0.045f, 0.98f);

	const FLinearColor PanelColor(
		0.055f, 0.070f, 0.080f, 1.0f);

	const FLinearColor AccentColor(
		0.95f, 0.55f, 0.08f, 1.0f);

	const FLinearColor MutedColor(
		0.62f, 0.67f, 0.70f, 1.0f);

	enum class EATCMCargoCategory : uint8
	{
		Any,
		Solid,
		Fluid
	};

	EATCMCargoCategory GetEditableCargoCategory(
		TSubclassOf<UFGItemDescriptor> Item)
	{
		if (!Item)
		{
			return EATCMCargoCategory::Any;
		}

		switch (UFGItemDescriptor::GetForm(Item))
		{
		case EResourceForm::RF_SOLID:
			return EATCMCargoCategory::Solid;

		case EResourceForm::RF_LIQUID:
		case EResourceForm::RF_GAS:
			return EATCMCargoCategory::Fluid;

		default:
			return EATCMCargoCategory::Any;
		}
	}

	int32 CountFreightWagons(AFGTrain* Train)
	{
		int32 Count = 0;

		if (Train)
		{
			for (AFGRailroadSubsystem::TTrainIterator Iterator(
					Train->GetFirstVehicle());
				Iterator;
				++Iterator)
			{
				if (Cast<AFGFreightWagon>(*Iterator))
				{
					++Count;
				}
			}
		}

		return Count;
	}

	int32 CountConnectedCargoTerminals(
		const AFGTrainStationIdentifier* Identifier)
	{
		AFGBuildableRailroadStation* Station =
			Identifier ? Identifier->GetStation() : nullptr;
		UFGTrainPlatformConnection* Connection =
			Station
				? Station->GetStationOutputConnection()
				: nullptr;

		int32 Count = 0;
		TSet<AFGBuildableTrainPlatform*> VisitedPlatforms;

		while (Connection && Connection->GetConnectedTo())
		{
			UFGTrainPlatformConnection* ConnectedConnection =
				Connection->GetConnectedTo();
			AFGBuildableTrainPlatform* Platform =
				ConnectedConnection
					? ConnectedConnection->GetPlatformOwner()
					: nullptr;

			if (!Platform || VisitedPlatforms.Contains(Platform))
			{
				break;
			}

			VisitedPlatforms.Add(Platform);

			if (Cast<AFGBuildableTrainPlatformCargo>(Platform))
			{
				++Count;
			}

			Connection =
				Platform->GetConnectionInOppositeDirection(
					ConnectedConnection);
		}

		return Count;
	}

	FText GetFreightWagonCountText(const int32 Count)
	{
		return Count == 1
			? LOCTEXT("FreightWagonCountOne", "1 freight car")
			: FText::Format(
				LOCTEXT(
					"FreightWagonCountMany",
					"{0} freight cars"),
				FText::AsNumber(Count));
	}

	FText GetTerminalCountText(const int32 Count)
	{
		return Count == 1
			? LOCTEXT("TerminalCountOne", "1 terminal")
			: FText::Format(
				LOCTEXT(
					"TerminalCountMany",
					"{0} terminals"),
				FText::AsNumber(Count));
	}

	FText GetDirectionText(
		const EATCMTransferDirection Direction)
	{
		return Direction == EATCMTransferDirection::Unload
			? LOCTEXT("DirectionUnload", "Unload")
			: LOCTEXT("DirectionLoad", "Load");
	}

	FText GetWagonText(const int32 ZeroBasedIndex)
	{
		return FText::Format(
			LOCTEXT(
				"WagonNumberFormat",
				"Freight car {0}"),
			FText::AsNumber(ZeroBasedIndex + 1));
	}

	struct FATCMTrainChoice
	{
		TWeakObjectPtr<AFGTrain> Train;
		FText Label;
		int32 WagonCount = 0;

		FText GetDisplayText() const
		{
			return FText::Format(
				LOCTEXT(
					"TrainChoiceFormat",
					"{0} ({1})"),
				Label,
				GetFreightWagonCountText(WagonCount));
		}
	};

	struct FATCMStationChoice
	{
		TWeakObjectPtr<AFGTrainStationIdentifier> Station;
		FText Label;
		int32 TerminalCount = 0;

		FText GetDisplayText() const
		{
			return FText::Format(
				LOCTEXT(
					"StationChoiceFormat",
					"{0} ({1})"),
				Label,
				GetTerminalCountText(TerminalCount));
		}
	};

	struct FATCMItemChoice
	{
		TSubclassOf<UFGItemDescriptor> Item;
		FText Label;
	};

	struct FATCMEditableRule
	{
		int32 WagonIndex = 0;

		EATCMTransferDirection Direction =
			EATCMTransferDirection::Load;

		TSubclassOf<UFGItemDescriptor> Item;

		int32 Amount = 1;

		bool TransferAll = false;
	};

	struct FATCMEditableStop
	{
		int32 RouteIndex = INDEX_NONE;

		TWeakObjectPtr<AFGTrainStationIdentifier> Station;

		int32 TerminalCount = 0;

		TArray<TSharedPtr<FATCMEditableRule>> Rules;

		FText GetLabel() const
		{
			const FText StationName = Station.IsValid()
				? Station->GetStationName()
				: LOCTEXT(
					"MissingStation",
					"<missing station>");

			return FText::Format(
				LOCTEXT(
					"RouteStopFormat",
					"{0}. {1} ({2})"),
				FText::AsNumber(RouteIndex + 1),
				StationName,
				GetTerminalCountText(TerminalCount));
		}
	};
	
	class SATCMSearchableItemPicker final
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SATCMSearchableItemPicker)
		{
		}

			SLATE_ARGUMENT(
				TSharedPtr<FATCMEditableRule>,
				Rule)

			SLATE_ARGUMENT(
				const TArray<TSharedPtr<FATCMItemChoice>>*,
				ItemChoices)

			SLATE_ARGUMENT(
				TFunction<bool(
					TSubclassOf<UFGItemDescriptor>)>,
				ItemFilter)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Rule = InArgs._Rule;
			AllItemChoices = InArgs._ItemChoices;
			ItemFilter = InArgs._ItemFilter;

			ChildSlot
			[
				SAssignNew(ComboButton, SComboButton)
					.OnGetMenuContent(
						this,
						&SATCMSearchableItemPicker::
							BuildMenuContent)
					.ButtonContent()
					[
						SNew(STextBlock)
							.Text(
								this,
								&SATCMSearchableItemPicker::
									GetSelectedItemText)
					]
			];
		}

	private:
		TSharedRef<SWidget> BuildMenuContent()
		{
			SearchText = FText::GetEmpty();
			RefreshFilteredItems();

			return SNew(SBox)
				.WidthOverride(420.0f)
				.HeightOverride(500.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5.0f)
					[
						SAssignNew(SearchBox, SSearchBox)
							.HintText(
								LOCTEXT(
									"SearchItemHint",
									"Search item..."))
							.OnTextChanged(
								this,
								&SATCMSearchableItemPicker::
									OnSearchTextChanged)
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(5.0f, 0.0f, 5.0f, 5.0f)
					[
						SAssignNew(
							ItemList,
							SListView<
								TSharedPtr<FATCMItemChoice>>)
							.ListItemsSource(&FilteredItems)
							.SelectionMode(
								ESelectionMode::Single)
							.OnGenerateRow(
								this,
								&SATCMSearchableItemPicker::
									GenerateItemRow)
							.OnSelectionChanged(
								this,
								&SATCMSearchableItemPicker::
									OnItemSelected)
					]
				];
		}

		void OnSearchTextChanged(const FText& NewSearchText)
		{
			SearchText = NewSearchText;
			RefreshFilteredItems();
		}

		void RefreshFilteredItems()
		{
			FilteredItems.Reset();

			if (!AllItemChoices)
			{
				return;
			}

			FString SearchString =
				SearchText.ToString();

			SearchString.TrimStartAndEndInline();

			for (const TSharedPtr<FATCMItemChoice>& Choice :
				*AllItemChoices)
			{
				if (!Choice.IsValid())
				{
					continue;
				}

				if (ItemFilter
					&& !ItemFilter(Choice->Item))
				{
					continue;
				}

				const FString ItemLabel =
					Choice->Label.ToString();

				const FString InternalName =
					Choice->Item
						? Choice->Item->GetName()
						: FString();

				if (SearchString.IsEmpty()
					|| ItemLabel.Contains(
						SearchString,
						ESearchCase::IgnoreCase)
					|| InternalName.Contains(
						SearchString,
						ESearchCase::IgnoreCase))
				{
					FilteredItems.Add(Choice);
				}
			}

			if (ItemList.IsValid())
			{
				ItemList->RequestListRefresh();
			}
		}

		TSharedRef<ITableRow> GenerateItemRow(
			TSharedPtr<FATCMItemChoice> Choice,
			const TSharedRef<STableViewBase>& OwnerTable)
		{
			return SNew(
				STableRow<TSharedPtr<FATCMItemChoice>>,
				OwnerTable)
				.Padding(5.0f)
				[
					SNew(STextBlock)
						.Text(
							Choice.IsValid()
								? Choice->Label
								: FText::GetEmpty())
				];
		}

		void OnItemSelected(
			TSharedPtr<FATCMItemChoice> Choice,
			ESelectInfo::Type)
		{
			if (!Choice.IsValid()
				|| !Rule.IsValid()
				|| (ItemFilter
					&& !ItemFilter(Choice->Item)))
			{
				return;
			}

			Rule->Item = Choice->Item;

			if (ComboButton.IsValid())
			{
				ComboButton->SetIsOpen(false);
			}
		}

		FText GetSelectedItemText() const
		{
			if (!Rule.IsValid() || !Rule->Item)
			{
				return LOCTEXT(
					"SelectItem",
					"Select item");
			}

			return UFGItemDescriptor::GetItemName(
				Rule->Item);
		}

		TSharedPtr<FATCMEditableRule> Rule;

		const TArray<TSharedPtr<FATCMItemChoice>>*
			AllItemChoices = nullptr;

		TFunction<bool(TSubclassOf<UFGItemDescriptor>)>
			ItemFilter;

		FText SearchText;

		TArray<TSharedPtr<FATCMItemChoice>>
			FilteredItems;

		TSharedPtr<SComboButton> ComboButton;
		TSharedPtr<SSearchBox> SearchBox;

		TSharedPtr<
			SListView<TSharedPtr<FATCMItemChoice>>>
			ItemList;
	};

	class SATCMManagerWidget final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SATCMManagerWidget)
		{
		}

			SLATE_EVENT(
				FSimpleDelegate,
				OnRequestClose)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OnRequestClose = InArgs._OnRequestClose;

			DirectionOptions =
			{
				MakeShared<FString>(TEXT("Load")),
				MakeShared<FString>(TEXT("Unload"))
			};

			LoadTrainChoices();

			ChildSlot
			[
				SNew(SBorder)
				.BorderBackgroundColor(BackgroundColor)
				.Padding(24.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"WindowTitle",
									"ADVANCED TRAIN "
									"CARGO MANAGER"))
								.ColorAndOpacity(AccentColor)
								.Font(
									FCoreStyle::
									GetDefaultFontStyle(
										"Bold",
										26))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
								.Text_Lambda(
									[]
									{
										return FText::Format(
											LOCTEXT(
												"CloseButton",
												"Close [{0}]"),
											FATCMUserInterface::
												GetHotkeyDisplayText());
									})
								.OnClicked_Lambda(
									[this]
									{
										OnRequestClose
											.ExecuteIfBound();

										return
											FReply::Handled();
									})
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 14.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT(
								"WindowDescription",
								"Select a route and optionally "
								"configure exact transfers "
								"for each stop."))
							.ColorAndOpacity(MutedColor)
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(ContentBox, SBox)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(
								this,
								&SATCMManagerWidget::
								GetStatusText)
							.ColorAndOpacity(MutedColor)
					]
				]
			];

			ShowRoutePage();
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnKeyDown(
			const FGeometry& MyGeometry,
			const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				OnRequestClose.ExecuteIfBound();
				return FReply::Handled();
			}

			return SCompoundWidget::OnKeyDown(
				MyGeometry,
				InKeyEvent);
		}

	private:
		void LoadTrainChoices()
		{
			TrainChoices.Reset();
			SelectedTrain.Reset();

			UWorld* World =
				GEngine && GEngine->GameViewport
				? GEngine->GameViewport->GetWorld()
				: nullptr;

			AFGRailroadSubsystem* Railroad =
				World
					? AFGRailroadSubsystem::Get(World)
					: nullptr;

			if (!Railroad)
			{
				StatusMessage = LOCTEXT(
					"NoGameWorld",
					"No game world is available yet.");
				return;
			}

			TArray<AFGTrain*> Trains;

			Railroad->GetAllTrains(Trains);
			Railroad->SortTrains(Trains);

			for (AFGTrain* Train : Trains)
			{
				if (!IsValid(Train))
				{
					continue;
				}

				TSharedPtr<FATCMTrainChoice> Choice =
					MakeShared<FATCMTrainChoice>();

				Choice->Train = Train;
				Choice->Label = Train->GetTrainName();
				Choice->WagonCount =
					CountFreightWagons(Train);

				TrainChoices.Add(Choice);
			}

			if (TrainChoices.IsEmpty())
			{
				StatusMessage = LOCTEXT(
					"NoTrainsFound",
					"No trains were found.");
				return;
			}

			SelectedTrain = TrainChoices[0];
			LoadSelectedTrain();
		}

		void LoadSelectedTrain()
		{
			StationChoices.Reset();
			SelectedStation.Reset();
			RouteStops.Reset();
			SelectedStop.Reset();
			VisibleRules.Reset();
			ItemChoices.Reset();
			WagonOptions.Reset();

			AFGTrain* Train = SelectedTrain.IsValid()
				? SelectedTrain->Train.Get()
				: nullptr;

			if (!Train)
			{
				StatusMessage = LOCTEXT(
					"SelectedTrainMissing",
					"The selected train no longer exists.");
				return;
			}

			BuildWagonOptions(Train);
			LoadItemChoices(Train);

			AFGRailroadSubsystem* Railroad =
				AFGRailroadSubsystem::Get(Train);

			if (Railroad)
			{
				TArray<AFGTrainStationIdentifier*> Stations;

				Railroad->GetTrainStations(
					Train->GetTrackGraphID(),
					Stations);

				Railroad->SortTrainStations(Stations);

				for (AFGTrainStationIdentifier* Station :
					Stations)
				{
					if (!IsValid(Station))
					{
						continue;
					}

					TSharedPtr<FATCMStationChoice> Choice =
						MakeShared<FATCMStationChoice>();

					Choice->Station = Station;
					Choice->Label =
						Station->GetStationName();
					Choice->TerminalCount =
						CountConnectedCargoTerminals(
							Station);

					StationChoices.Add(Choice);
				}
			}

			if (!StationChoices.IsEmpty())
			{
				SelectedStation = StationChoices[0];
			}

			AFGRailroadTimeTable* TimeTable =
				Train->GetTimeTable();

			const FATCMTrainSchedule* StoredSchedule =
				nullptr;

			if (TimeTable)
			{
				if (AATCMSubsystem* Subsystem =
					AATCMSubsystem::Get(Train))
				{
					StoredSchedule =
						Subsystem->FindSchedule(TimeTable);
				}

				TArray<FTimeTableStop> Stops;
				TimeTable->GetStops(Stops);

				for (int32 StopIndex = 0;
					StopIndex < Stops.Num();
					++StopIndex)
				{
					AFGTrainStationIdentifier* Station =
						Stops[StopIndex].Station.Get();

					if (!IsValid(Station))
					{
						continue;
					}

					TSharedPtr<FATCMEditableStop>
						EditableStop =
							MakeShared<
								FATCMEditableStop>();

					EditableStop->RouteIndex =
						RouteStops.Num();

					EditableStop->Station = Station;
					EditableStop->TerminalCount =
						CountConnectedCargoTerminals(
							Station);

					const FATCMStopRules* StoredStop =
						StoredSchedule
							? StoredSchedule->Stops
								.FindByPredicate(
									[StopIndex, Station](
										const FATCMStopRules&
											Candidate)
									{
										return
											Candidate.StopIndex ==
												StopIndex &&
											Candidate.Station.Get() ==
												Station;
									})
							: nullptr;

					if (StoredStop)
					{
						for (const FATCMTransferRule&
							StoredRule :
							StoredStop->Rules)
						{
							if (!StoredRule.ItemDescriptor)
							{
								continue;
							}

							TSharedPtr<FATCMEditableRule> Rule =
								MakeShared<
									FATCMEditableRule>();

							Rule->WagonIndex =
								WagonOptions.IsValidIndex(
									StoredRule
										.FreightWagonIndex)
									? StoredRule
										.FreightWagonIndex
									: 0;

							Rule->Direction =
								StoredRule.Direction;

							Rule->Item =
								StoredRule.ItemDescriptor;

							Rule->Amount =
								FMath::Max(
									1,
									StoredRule.Amount);
							Rule->TransferAll =
								StoredRule.TransferAll;

							EditableStop->Rules.Add(Rule);
						}
					}

					RouteStops.Add(EditableStop);
				}
			}

			StatusMessage = FText::GetEmpty();
		}

		void LoadItemChoices(UObject* WorldContext)
		{
			TArray<TSubclassOf<UFGItemDescriptor>>
				Descriptors;

			UFGBlueprintFunctionLibrary::
				GetAllDescriptorsSorted(
					WorldContext,
					Descriptors,
					true,
					false,
					false);

			TSet<UClass*> Seen;

			for (TSubclassOf<UFGItemDescriptor> Descriptor :
				Descriptors)
			{
				if (!Descriptor ||
					Seen.Contains(Descriptor.Get()))
				{
					continue;
				}

				const EATCMCargoCategory Category =
					GetEditableCargoCategory(Descriptor);

				if (Category == EATCMCargoCategory::Any)
				{
					continue;
				}

				Seen.Add(Descriptor.Get());

				TSharedPtr<FATCMItemChoice> Choice =
					MakeShared<FATCMItemChoice>();

				Choice->Item = Descriptor;
				Choice->Label =
					UFGItemDescriptor::GetItemName(
						Descriptor);

				ItemChoices.Add(Choice);
			}
		}

		void BuildWagonOptions(AFGTrain* Train)
		{
			const int32 WagonCount =
				CountFreightWagons(Train);

			for (int32 Index = 0;
				Index < WagonCount;
				++Index)
			{
				WagonOptions.Add(
					MakeShared<int32>(Index));
			}
		}

		EATCMCargoCategory GetWagonCargoCategory(
			const int32 WagonIndex,
			const TSharedPtr<FATCMEditableRule>&
				IgnoredRule) const
		{
			for (const TSharedPtr<FATCMEditableStop>& Stop :
				RouteStops)
			{
				if (!Stop.IsValid())
				{
					continue;
				}

				for (const TSharedPtr<FATCMEditableRule>& Rule :
					Stop->Rules)
				{
					if (!Rule.IsValid()
						|| Rule == IgnoredRule
						|| Rule->WagonIndex != WagonIndex
						|| !Rule->Item)
					{
						continue;
					}

					const EATCMCargoCategory Category =
						GetEditableCargoCategory(Rule->Item);

					if (Category != EATCMCargoCategory::Any)
					{
						return Category;
					}
				}
			}

			return EATCMCargoCategory::Any;
		}

		bool IsItemAllowedForRule(
			const TSharedPtr<FATCMEditableRule>& Rule,
			TSubclassOf<UFGItemDescriptor> Item) const
		{
			if (!Rule.IsValid() || !Item)
			{
				return false;
			}

			const EATCMCargoCategory ItemCategory =
				GetEditableCargoCategory(Item);
			const EATCMCargoCategory RequiredCategory =
				GetWagonCargoCategory(
					Rule->WagonIndex,
					Rule);

			return ItemCategory != EATCMCargoCategory::Any
				&& (RequiredCategory ==
						EATCMCargoCategory::Any
					|| ItemCategory == RequiredCategory);
		}

		void EnsureRuleItemMatchesWagon(
			const TSharedPtr<FATCMEditableRule>& Rule)
		{
			if (!Rule.IsValid()
				|| IsItemAllowedForRule(
					Rule,
					Rule->Item))
			{
				return;
			}

			Rule->Item = nullptr;

			for (const TSharedPtr<FATCMItemChoice>& Choice :
				ItemChoices)
			{
				if (Choice.IsValid()
					&& IsItemAllowedForRule(
						Rule,
						Choice->Item))
				{
					Rule->Item = Choice->Item;
					break;
				}
			}
		}

		void ShowRoutePage()
		{
			if (ContentBox.IsValid())
			{
				ContentBox->SetContent(
					BuildRoutePage());
			}
		}

		void ShowRulesPage()
		{
			if (!SelectedStop.IsValid() ||
				!RouteStops.Contains(SelectedStop))
			{
				SelectedStop = RouteStops.IsEmpty()
					? nullptr
					: RouteStops[0];
			}

			if (ContentBox.IsValid())
			{
				ContentBox->SetContent(
					BuildRulesPage());
			}
		}

		TSharedRef<SWidget> BuildRoutePage()
		{
			return SNew(SBorder)
				.BorderBackgroundColor(PanelColor)
				.Padding(18.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
							.Text(LOCTEXT(
								"RoutePageTitle",
								"1  ROUTE AND STOPS"))
							.ColorAndOpacity(AccentColor)
							.Font(
								FCoreStyle::
								GetDefaultFontStyle(
									"Bold",
									17))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 14.0f, 0.0f, 10.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"TrainLabel",
									"Train"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SComboBox<
								TSharedPtr<FATCMTrainChoice>>)
								.OptionsSource(&TrainChoices)
								.InitiallySelectedItem(
									SelectedTrain)
								.OnGenerateWidget_Lambda(
									[](
										TSharedPtr<
											FATCMTrainChoice>
											Choice)
									{
										return
											SNew(STextBlock)
											.Text(
												Choice.IsValid()
												? Choice
													->GetDisplayText()
												: FText::
													GetEmpty());
									})
								.OnSelectionChanged_Lambda(
									[this](
										TSharedPtr<
											FATCMTrainChoice>
											Choice,
										ESelectInfo::Type)
									{
										if (!Choice.IsValid() ||
											Choice ==
												SelectedTrain)
										{
											return;
										}

										SelectedTrain = Choice;
										LoadSelectedTrain();
										ShowRoutePage();
									})
								[
									SNew(STextBlock)
										.Text(
											this,
											&SATCMManagerWidget::
											GetSelectedTrainText)
								]
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 2.0f, 0.0f, 12.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SComboBox<
								TSharedPtr<
									FATCMStationChoice>>)
								.OptionsSource(
									&StationChoices)
								.InitiallySelectedItem(
									SelectedStation)
								.OnGenerateWidget_Lambda(
									[](
										TSharedPtr<
											FATCMStationChoice>
											Choice)
									{
										return
											SNew(STextBlock)
											.Text(
												Choice.IsValid()
												? Choice
													->GetDisplayText()
												: FText::
													GetEmpty());
									})
								.OnSelectionChanged_Lambda(
									[this](
										TSharedPtr<
											FATCMStationChoice>
											Choice,
										ESelectInfo::Type)
									{
										SelectedStation =
											Choice;
									})
								[
									SNew(STextBlock)
										.Text(
											this,
											&SATCMManagerWidget::
											GetSelectedStationText)
								]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"AddStopButton",
									"+ Add stop"))
								.IsEnabled_Lambda(
									[this]
									{
										return
											SelectedStation
												.IsValid() &&
											SelectedStation
												->Station
												.IsValid() &&
											RouteStops.Num() <
												100;
									})
								.OnClicked(
									this,
									&SATCMManagerWidget::
									AddRouteStop)
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(
							RouteList,
							SListView<
								TSharedPtr<
									FATCMEditableStop>>)
							.ListItemsSource(&RouteStops)
							.SelectionMode(
								ESelectionMode::None)
							.OnGenerateRow(
								this,
								&SATCMManagerWidget::
								GenerateRouteRow)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 14.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SSpacer)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"SaveRouteButton",
									"Save schedule"))
								.IsEnabled_Lambda(
									[this]
									{
										return
											SelectedTrain.IsValid() &&
											!RouteStops.IsEmpty();
									})
								.OnClicked(
									this,
									&SATCMManagerWidget::
									SaveSchedule)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"ContinueToRulesButton",
									"Next: Transfer rules"))
								.IsEnabled_Lambda(
									[this]
									{
										return
											SelectedTrain.IsValid() &&
											!RouteStops.IsEmpty();
									})
								.OnClicked_Lambda(
									[this]
									{
										ShowRulesPage();
										return FReply::Handled();
									})
						]
					]
				];
		}

		TSharedRef<ITableRow> GenerateRouteRow(
			TSharedPtr<FATCMEditableStop> Stop,
			const TSharedRef<STableViewBase>& OwnerTable)
		{
			return SNew(
				STableRow<
					TSharedPtr<FATCMEditableStop>>,
				OwnerTable)
				.Padding(3.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text_Lambda(
								[Stop]
								{
									return Stop.IsValid()
										? Stop->GetLabel()
										: FText::GetEmpty();
								})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT(
								"MoveUpButton",
								"Up"))
							.IsEnabled_Lambda(
								[Stop]
								{
									return Stop.IsValid() &&
										Stop->RouteIndex > 0;
								})
							.OnClicked_Lambda(
								[this, Stop]
								{
									return MoveRouteStop(
										Stop,
										-1);
								})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT(
								"MoveDownButton",
								"Down"))
							.IsEnabled_Lambda(
								[this, Stop]
								{
									return Stop.IsValid() &&
										Stop->RouteIndex + 1 <
											RouteStops.Num();
								})
							.OnClicked_Lambda(
								[this, Stop]
								{
									return MoveRouteStop(
										Stop,
										1);
								})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT(
								"RemoveStopButton",
								"Remove"))
							.OnClicked_Lambda(
								[this, Stop]
								{
									if (SelectedStop == Stop)
									{
										SelectedStop.Reset();
									}

									RouteStops.Remove(Stop);
									ReindexRoute();

									if (RouteList.IsValid())
									{
										RouteList
											->RequestListRefresh();
									}

									return
										FReply::Handled();
								})
					]
				];
		}

		TSharedRef<SWidget> BuildRulesPage()
		{
			VisibleRules = SelectedStop.IsValid()
				? SelectedStop->Rules
				: TArray<
					TSharedPtr<FATCMEditableRule>>();

			return SNew(SBorder)
				.BorderBackgroundColor(PanelColor)
				.Padding(18.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
							.Text(LOCTEXT(
								"RulesPageTitle",
								"2  TRANSFER RULES "
								"PER STOP"))
							.ColorAndOpacity(AccentColor)
							.Font(
								FCoreStyle::
								GetDefaultFontStyle(
									"Bold",
									17))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 14.0f, 0.0f, 12.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SComboBox<
								TSharedPtr<
									FATCMEditableStop>>)
								.OptionsSource(&RouteStops)
								.InitiallySelectedItem(
									SelectedStop)
								.OnGenerateWidget_Lambda(
									[](
										TSharedPtr<
											FATCMEditableStop>
											Stop)
									{
										return
											SNew(STextBlock)
											.Text(
												Stop.IsValid()
												? Stop->GetLabel()
												: FText::
													GetEmpty());
									})
								.OnSelectionChanged_Lambda(
									[this](
										TSharedPtr<
											FATCMEditableStop>
											Stop,
										ESelectInfo::Type)
									{
										SelectedStop = Stop;

										VisibleRules =
											Stop.IsValid()
											? Stop->Rules
											: TArray<
												TSharedPtr<
													FATCMEditableRule>>();

										if (RuleList.IsValid())
										{
											RuleList
												->RequestListRefresh();
										}
									})
								[
									SNew(STextBlock)
										.Text(
											this,
											&SATCMManagerWidget::
											GetSelectedStopText)
								]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"AddRuleButton",
									"+ Add entry"))
								.IsEnabled_Lambda(
									[this]
									{
										return
											SelectedStop.IsValid() &&
											!WagonOptions.IsEmpty() &&
											!ItemChoices.IsEmpty();
									})
								.OnClicked(
									this,
									&SATCMManagerWidget::
									AddRule)
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(3.0f, 0.0f, 3.0f, 5.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(0.15f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"FreightWagonColumn",
									"Freight car"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.13f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"ActionColumn",
									"Action"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.34f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"ItemColumn",
									"Item"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.25f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT(
									"AmountColumn",
									"Amount / All"))
						]

						+ SHorizontalBox::Slot()
						.FillWidth(0.13f)
						[
							SNew(STextBlock)
								.Text(FText::GetEmpty())
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(
							RuleList,
							SListView<
								TSharedPtr<
									FATCMEditableRule>>)
							.ListItemsSource(&VisibleRules)
							.SelectionMode(
								ESelectionMode::None)
							.OnGenerateRow(
								this,
								&SATCMManagerWidget::
								GenerateRuleRow)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f)
					[
						SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT(
								"RulesExplanation",
								"If a stop has no entries, "
								"the normal Satisfactory logic "
								"is used. If entries exist, "
								"unlisted freight cars and the "
								"wrong terminal direction "
								"transfer nothing."))
							.ColorAndOpacity(MutedColor)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"BackToRouteButton",
									"Back to route"))
								.OnClicked_Lambda(
									[this]
									{
										ShowRoutePage();
										return
											FReply::Handled();
									})
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SSpacer)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
								.Text(LOCTEXT(
									"SaveScheduleButton",
									"Save schedule"))
								.OnClicked(
									this,
									&SATCMManagerWidget::
									SaveSchedule)
						]
					]
				];
		}

		TSharedRef<ITableRow> GenerateRuleRow(
			TSharedPtr<FATCMEditableRule> Rule,
			const TSharedRef<STableViewBase>& OwnerTable)
		{
			return SNew(
				STableRow<
					TSharedPtr<FATCMEditableRule>>,
				OwnerTable)
				.Padding(3.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(0.15f)
					.Padding(2.0f)
					[
						SNew(SComboBox<
							TSharedPtr<int32>>)
							.OptionsSource(&WagonOptions)
							.InitiallySelectedItem(
								FindWagonOption(
									Rule->WagonIndex))
							.OnGenerateWidget_Lambda(
								[](
									TSharedPtr<int32>
										Index)
								{
									return
										SNew(STextBlock)
										.Text(
											Index.IsValid()
											? GetWagonText(*Index)
											: FText::
												GetEmpty());
								})
							.OnSelectionChanged_Lambda(
								[this, Rule](
									TSharedPtr<int32> Index,
									ESelectInfo::Type)
								{
									if (Index.IsValid())
									{
										Rule->WagonIndex =
											*Index;
										EnsureRuleItemMatchesWagon(
											Rule);
									}
								})
							[
								SNew(STextBlock)
									.Text_Lambda(
										[Rule]
										{
											return GetWagonText(
												Rule->WagonIndex);
										})
							]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.13f)
					.Padding(2.0f)
					[
						SNew(SComboBox<
							TSharedPtr<FString>>)
							.OptionsSource(
								&DirectionOptions)
							.InitiallySelectedItem(
								FindDirectionOption(
									Rule->Direction))
							.OnGenerateWidget_Lambda(
								[](
									TSharedPtr<FString>
										Direction)
								{
									return
										SNew(STextBlock)
										.Text(
											Direction.IsValid()
											? GetDirectionText(
												*Direction ==
													TEXT("Unload")
												? EATCMTransferDirection::
													Unload
												: EATCMTransferDirection::
													Load)
											: FText::
												GetEmpty());
								})
							.OnSelectionChanged_Lambda(
								[Rule](
									TSharedPtr<FString>
										Direction,
									ESelectInfo::Type)
								{
									if (Direction.IsValid())
									{
										Rule->Direction =
											*Direction ==
												TEXT("Unload")
											? EATCMTransferDirection::
												Unload
											: EATCMTransferDirection::
												Load;
									}
								})
							[
								SNew(STextBlock)
									.Text_Lambda(
										[Rule]
										{
											return GetDirectionText(
												Rule->Direction);
										})
							]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.34f)
					.Padding(2.0f)
					[
						SNew(SATCMSearchableItemPicker)
							.Rule(Rule)
							.ItemChoices(&ItemChoices)
							.ItemFilter(
								[this, Rule](
									TSubclassOf<
										UFGItemDescriptor> Item)
								{
									return IsItemAllowedForRule(
										Rule,
										Item);
								})
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.25f)
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SNumericEntryBox<int32>)
								.MinValue(1)
								.MaxValue(MAX_int32)
								.MinSliderValue(1)
								.MaxSliderValue(10000)
								.IsEnabled_Lambda(
									[Rule]
									{
										return !Rule->TransferAll;
									})
								.Value_Lambda(
									[Rule]
									{
										return TOptional<int32>(
											Rule->Amount);
									})
								.OnValueChanged_Lambda(
									[Rule](int32 Amount)
									{
										Rule->Amount =
											FMath::Max(
												1,
												Amount);
									})
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(5.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SCheckBox)
								.Style(
									FCoreStyle::Get(),
									"ToggleButtonCheckbox")
								.IsChecked_Lambda(
									[Rule]
									{
										return Rule->TransferAll
											? ECheckBoxState::Checked
											: ECheckBoxState::Unchecked;
									})
								.OnCheckStateChanged_Lambda(
									[Rule](
										ECheckBoxState State)
									{
										Rule->TransferAll =
											State ==
												ECheckBoxState::
													Checked;
									})
								[
									SNew(STextBlock)
										.Text(LOCTEXT(
											"TransferAllButton",
											"All"))
								]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.13f)
					.Padding(2.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT(
								"DeleteRuleButton",
								"Delete"))
							.OnClicked_Lambda(
								[this, Rule]
								{
									if (SelectedStop.IsValid())
									{
										SelectedStop->Rules
											.Remove(Rule);
									}

									VisibleRules.Remove(Rule);

									if (RuleList.IsValid())
									{
										RuleList
											->RequestListRefresh();
									}

									return
										FReply::Handled();
								})
					]
				];
		}

		FReply AddRouteStop()
		{
			if (!SelectedStation.IsValid() ||
				!SelectedStation->Station.IsValid())
			{
				return FReply::Handled();
			}

			TSharedPtr<FATCMEditableStop> Stop =
				MakeShared<FATCMEditableStop>();

			Stop->RouteIndex = RouteStops.Num();
			Stop->Station = SelectedStation->Station;
			Stop->TerminalCount =
				SelectedStation->TerminalCount;

			RouteStops.Add(Stop);

			if (RouteList.IsValid())
			{
				RouteList->RequestListRefresh();
				RouteList->RequestScrollIntoView(Stop);
			}

			return FReply::Handled();
		}

		FReply MoveRouteStop(
			const TSharedPtr<FATCMEditableStop>& Stop,
			int32 Delta)
		{
			const int32 CurrentIndex =
				RouteStops.IndexOfByKey(Stop);

			const int32 NewIndex =
				CurrentIndex + Delta;

			if (RouteStops.IsValidIndex(CurrentIndex) &&
				RouteStops.IsValidIndex(NewIndex))
			{
				RouteStops.Swap(CurrentIndex, NewIndex);
				ReindexRoute();

				if (RouteList.IsValid())
				{
					RouteList->RequestListRefresh();
				}
			}

			return FReply::Handled();
		}

		void ReindexRoute()
		{
			for (int32 Index = 0;
				Index < RouteStops.Num();
				++Index)
			{
				RouteStops[Index]->RouteIndex = Index;
			}
		}

		FReply AddRule()
		{
			if (!SelectedStop.IsValid() ||
				WagonOptions.IsEmpty() ||
				ItemChoices.IsEmpty())
			{
				return FReply::Handled();
			}

			TSharedPtr<FATCMEditableRule> Rule =
				MakeShared<FATCMEditableRule>();

			Rule->WagonIndex = *WagonOptions[0];
			Rule->Amount = 1;
			Rule->TransferAll = false;

			EnsureRuleItemMatchesWagon(Rule);

			SelectedStop->Rules.Add(Rule);
			VisibleRules.Add(Rule);

			if (RuleList.IsValid())
			{
				RuleList->RequestListRefresh();
				RuleList->RequestScrollIntoView(Rule);
			}

			return FReply::Handled();
		}

		FReply SaveSchedule()
		{
			AFGTrain* Train = SelectedTrain.IsValid()
				? SelectedTrain->Train.Get()
				: nullptr;

			UWorld* World =
				Train ? Train->GetWorld() : nullptr;

			AFGPlayerController* PlayerController =
				World
					? Cast<AFGPlayerController>(
						World->GetFirstPlayerController())
					: nullptr;

			UATCMRemoteCallObject* RemoteCallObject =
				PlayerController
					? PlayerController
						->GetRemoteCallObjectOfClass<
							UATCMRemoteCallObject>()
					: nullptr;

			if (!Train || !RemoteCallObject)
			{
				StatusMessage = LOCTEXT(
					"SaveNetworkUnavailable",
					"Cannot save: The network object "
					"is unavailable.");

				return FReply::Handled();
			}

			TArray<AFGTrainStationIdentifier*>
				RouteStations;

			TArray<FATCMStopRules> AdvancedStops;

			for (int32 StopIndex = 0;
				StopIndex < RouteStops.Num();
				++StopIndex)
			{
				const TSharedPtr<FATCMEditableStop>&
					EditableStop =
						RouteStops[StopIndex];

				AFGTrainStationIdentifier* Station =
					EditableStop.IsValid()
						? EditableStop->Station.Get()
						: nullptr;

				if (!Station)
				{
					StatusMessage = LOCTEXT(
						"SaveStationMissing",
						"Save cancelled: A station no "
						"longer exists.");

					return FReply::Handled();
				}

				RouteStations.Add(Station);

				FATCMStopRules Stop;
				Stop.StopIndex = StopIndex;
				Stop.Station = Station;

				for (const TSharedPtr<FATCMEditableRule>&
					EditableRule :
					EditableStop->Rules)
				{
					if (!EditableRule.IsValid() ||
						!EditableRule->Item ||
						(!EditableRule->TransferAll
							&& EditableRule->Amount <= 0))
					{
						continue;
					}

					FATCMTransferRule Rule;

					Rule.FreightWagonIndex =
						EditableRule->WagonIndex;

					Rule.Direction =
						EditableRule->Direction;

					Rule.ItemDescriptor =
						EditableRule->Item;

					Rule.Amount =
						EditableRule->Amount;

					Rule.TransferAll =
						EditableRule->TransferAll;

					Stop.Rules.Add(Rule);
				}

				if (!Stop.Rules.IsEmpty())
				{
					AdvancedStops.Add(MoveTemp(Stop));
				}
			}

			RemoteCallObject->Server_SaveSchedule(
				Train,
				RouteStations,
				AdvancedStops);

			StatusMessage = LOCTEXT(
				"ScheduleSent",
				"The schedule was sent to the server.");

			return FReply::Handled();
		}

		TSharedPtr<int32> FindWagonOption(
			int32 Index) const
		{
			const TSharedPtr<int32>* Found =
				WagonOptions.FindByPredicate(
					[Index](
						const TSharedPtr<int32>& Option)
					{
						return Option.IsValid() &&
							*Option == Index;
					});

			return Found
				? *Found
				: WagonOptions.IsEmpty()
					? nullptr
					: WagonOptions[0];
		}

		TSharedPtr<FString> FindDirectionOption(
			EATCMTransferDirection Direction) const
		{
			const int32 Index =
				Direction ==
					EATCMTransferDirection::Unload
				? 1
				: 0;

			return DirectionOptions.IsValidIndex(Index)
				? DirectionOptions[Index]
				: nullptr;
		}

		TSharedPtr<FATCMItemChoice> FindItemOption(
			TSubclassOf<UFGItemDescriptor> Item) const
		{
			const TSharedPtr<FATCMItemChoice>* Found =
				ItemChoices.FindByPredicate(
					[Item](
						const TSharedPtr<
							FATCMItemChoice>& Option)
					{
						return Option.IsValid() &&
							Option->Item == Item;
					});

			return Found
				? *Found
				: ItemChoices.IsEmpty()
					? nullptr
					: ItemChoices[0];
		}

		FText GetSelectedTrainText() const
		{
			return SelectedTrain.IsValid()
				? SelectedTrain->GetDisplayText()
				: LOCTEXT(
					"SelectTrain",
					"Select train");
		}

		FText GetSelectedStationText() const
		{
			return SelectedStation.IsValid()
				? SelectedStation->GetDisplayText()
				: LOCTEXT(
					"SelectStation",
					"Select station");
		}

		FText GetSelectedStopText() const
		{
			return SelectedStop.IsValid()
				? SelectedStop->GetLabel()
				: LOCTEXT(
					"SelectStop",
					"Select stop");
		}

		FText GetStatusText() const
		{
			return StatusMessage;
		}

		FSimpleDelegate OnRequestClose;
		FText StatusMessage;

		TArray<TSharedPtr<FATCMTrainChoice>>
			TrainChoices;

		TSharedPtr<FATCMTrainChoice>
			SelectedTrain;

		TArray<TSharedPtr<FATCMStationChoice>>
			StationChoices;

		TSharedPtr<FATCMStationChoice>
			SelectedStation;

		TArray<TSharedPtr<FATCMEditableStop>>
			RouteStops;

		TSharedPtr<FATCMEditableStop>
			SelectedStop;

		TArray<TSharedPtr<FATCMEditableRule>>
			VisibleRules;

		TArray<TSharedPtr<FATCMItemChoice>>
			ItemChoices;

		TArray<TSharedPtr<int32>>
			WagonOptions;

		TArray<TSharedPtr<FString>>
			DirectionOptions;

		TSharedPtr<
			SListView<
				TSharedPtr<FATCMEditableStop>>>
			RouteList;

		TSharedPtr<
			SListView<
				TSharedPtr<FATCMEditableRule>>>
			RuleList;

		TSharedPtr<SBox> ContentBox;
	};
}

TSharedRef<SWidget> CreateATCMManagerWidget(
	FSimpleDelegate OnRequestClose)
{
	return SNew(SATCMManagerWidget)
		.OnRequestClose(OnRequestClose);
}

#undef LOCTEXT_NAMESPACE

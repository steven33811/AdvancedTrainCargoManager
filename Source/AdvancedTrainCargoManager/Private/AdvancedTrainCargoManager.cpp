#include "AdvancedTrainCargoManager.h"

#include "ATCMTransferHooks.h"
#include "Internationalization/PolyglotTextData.h"
#include "Internationalization/TextLocalizationManager.h"

DEFINE_LOG_CATEGORY(LogAdvancedTrainCargoManager);

namespace
{
	struct FATCMTranslationEntry
	{
		const TCHAR* Key;
		const TCHAR* English;
		const TCHAR* German;
	};
}

void RegisterATCMTranslations()
{
		static const FATCMTranslationEntry Entries[] =
		{
			{
				TEXT("FreightWagonCountOne"),
				TEXT("1 freight car"),
				TEXT("1 Waggon")
			},
			{
				TEXT("FreightWagonCountMany"),
				TEXT("{0} freight cars"),
				TEXT("{0} Waggons")
			},
			{
				TEXT("TerminalCountOne"),
				TEXT("1 terminal"),
				TEXT("1 Terminal")
			},
			{
				TEXT("TerminalCountMany"),
				TEXT("{0} terminals"),
				TEXT("{0} Terminals")
			},
			{
				TEXT("DirectionUnload"),
				TEXT("Unload"),
				TEXT("Entladen")
			},
			{
				TEXT("DirectionLoad"),
				TEXT("Load"),
				TEXT("Laden")
			},
			{
				TEXT("WagonNumberFormat"),
				TEXT("Freight car {0}"),
				TEXT("Waggon {0}")
			},
			{
				TEXT("TrainChoiceFormat"),
				TEXT("{0} ({1})"),
				TEXT("{0} ({1})")
			},
			{
				TEXT("StationChoiceFormat"),
				TEXT("{0} ({1})"),
				TEXT("{0} ({1})")
			},
			{
				TEXT("MissingStation"),
				TEXT("<missing station>"),
				TEXT("<fehlende Station>")
			},
			{
				TEXT("RouteStopFormat"),
				TEXT("{0}. {1} ({2})"),
				TEXT("{0}. {1} ({2})")
			},
			{
				TEXT("SearchItemHint"),
				TEXT("Search item..."),
				TEXT("Item suchen...")
			},
			{
				TEXT("SelectItem"),
				TEXT("Select item"),
				TEXT("Item auswählen")
			},
			{
				TEXT("WindowTitle"),
				TEXT("ADVANCED TRAIN CARGO MANAGER"),
				TEXT("ADVANCED TRAIN CARGO MANAGER")
			},
			{
				TEXT("CloseButton"),
				TEXT("Close [{0}]"),
				TEXT("Schließen [{0}]")
			},
			{
				TEXT("WindowDescription"),
				TEXT("Select a route and optionally configure exact transfers for each stop."),
				TEXT("Route auswählen und anschließend optional exakte Transfers pro Halt festlegen.")
			},
			{
				TEXT("NoGameWorld"),
				TEXT("No game world is available yet."),
				TEXT("Es ist noch keine Spielwelt verfügbar.")
			},
			{
				TEXT("NoTrainsFound"),
				TEXT("No trains were found."),
				TEXT("Es wurden keine Züge gefunden.")
			},
			{
				TEXT("SelectedTrainMissing"),
				TEXT("The selected train no longer exists."),
				TEXT("Der gewählte Zug existiert nicht mehr.")
			},
			{
				TEXT("RoutePageTitle"),
				TEXT("1  ROUTE AND STOPS"),
				TEXT("1  ROUTE UND HALTE")
			},
			{
				TEXT("TrainLabel"),
				TEXT("Train"),
				TEXT("Zug")
			},
			{
				TEXT("AddStopButton"),
				TEXT("+ Add stop"),
				TEXT("+ Halt hinzufügen")
			},
			{
				TEXT("SaveRouteButton"),
				TEXT("Save schedule"),
				TEXT("Fahrplan speichern")
			},
			{
				TEXT("ContinueToRulesButton"),
				TEXT("Next: Transfer rules"),
				TEXT("Weiter: Transferregeln")
			},
			{
				TEXT("MoveUpButton"),
				TEXT("Up"),
				TEXT("Hoch")
			},
			{
				TEXT("MoveDownButton"),
				TEXT("Down"),
				TEXT("Runter")
			},
			{
				TEXT("RemoveStopButton"),
				TEXT("Remove"),
				TEXT("Entfernen")
			},
			{
				TEXT("RulesPageTitle"),
				TEXT("2  TRANSFER RULES PER STOP"),
				TEXT("2  TRANSFERREGELN PRO HALT")
			},
			{
				TEXT("AddRuleButton"),
				TEXT("+ Add entry"),
				TEXT("+ Eintrag")
			},
			{
				TEXT("FreightWagonColumn"),
				TEXT("Freight car"),
				TEXT("Frachtwagen")
			},
			{
				TEXT("ActionColumn"),
				TEXT("Action"),
				TEXT("Aktion")
			},
			{
				TEXT("ItemColumn"),
				TEXT("Item"),
				TEXT("Item")
			},
			{
				TEXT("AmountColumn"),
				TEXT("Amount / All"),
				TEXT("Menge / Alles")
			},
			{
				TEXT("RulesExplanation"),
				TEXT("If a stop has no entries, the normal Satisfactory logic is used. If entries exist, unlisted freight cars and the wrong terminal direction transfer nothing."),
				TEXT("Ohne Einträge an einem Halt gilt die normale Satisfactory-Logik. Bei vorhandenen Einträgen bewegen nicht aufgeführte Wagen oder die falsche Terminalrichtung nichts.")
			},
			{
				TEXT("BackToRouteButton"),
				TEXT("Back to route"),
				TEXT("Zurück zur Route")
			},
			{
				TEXT("SaveScheduleButton"),
				TEXT("Save schedule"),
				TEXT("Fahrplan speichern")
			},
			{
				TEXT("TransferAllButton"),
				TEXT("All"),
				TEXT("Alles")
			},
			{
				TEXT("DeleteRuleButton"),
				TEXT("Delete"),
				TEXT("Löschen")
			},
			{
				TEXT("SaveNetworkUnavailable"),
				TEXT("Cannot save: The network object is unavailable."),
				TEXT("Speichern nicht möglich: Das Netzwerkobjekt ist nicht verfügbar.")
			},
			{
				TEXT("SaveStationMissing"),
				TEXT("Save cancelled: A station no longer exists."),
				TEXT("Speichern abgebrochen: Eine Station existiert nicht mehr.")
			},
			{
				TEXT("ScheduleSent"),
				TEXT("The schedule was sent to the server."),
				TEXT("Der Fahrplan wurde an den Server gesendet.")
			},
			{
				TEXT("SelectTrain"),
				TEXT("Select train"),
				TEXT("Zug auswählen")
			},
			{
				TEXT("SelectStation"),
				TEXT("Select station"),
				TEXT("Station auswählen")
			},
			{
				TEXT("SelectStop"),
				TEXT("Select stop"),
				TEXT("Halt auswählen")
			}
		};

		for (const FATCMTranslationEntry& Entry : Entries)
		{
			FPolyglotTextData Translation(
				ELocalizedTextSourceCategory::Game,
				TEXT("AdvancedTrainCargoManager"),
				Entry.Key,
				Entry.English,
				TEXT("en"));

			Translation.AddLocalizedString(
				TEXT("de"),
				Entry.German);

			FTextLocalizationManager::Get()
				.RegisterPolyglotTextData(Translation);
		}

		static const FATCMTranslationEntry ConfigEntries[] =
		{
			{
				TEXT("OpenManagerHotkeyLabel"),
				TEXT("Open/close menu"),
				TEXT("Menü öffnen/schließen")
			},
			{
				TEXT("OpenManagerHotkeyTooltip"),
				TEXT(
					"Key used to open or close the Advanced "
					"Train Cargo Manager."),
				TEXT(
					"Taste zum Öffnen oder Schließen des "
					"Advanced Train Cargo Managers.")
			},
			{
				TEXT("SelectHotkeyPrompt"),
				TEXT("Press a key..."),
				TEXT("Taste drücken...")
			},
			{
				TEXT("NoHotkey"),
				TEXT("Unbound"),
				TEXT("Nicht belegt")
			},
			{
				TEXT("ResetHotkeyButton"),
				TEXT("Reset to F6"),
				TEXT("Auf F6 zurücksetzen")
			},
			{
				TEXT("ConfigurationDisplayName"),
				TEXT("Controls"),
				TEXT("Steuerung")
			},
			{
				TEXT("ConfigurationDescription"),
				TEXT(
					"Configure Advanced Train Cargo Manager "
					"controls."),
				TEXT(
					"Steuerung des Advanced Train Cargo "
					"Managers konfigurieren.")
			}
		};

		for (const FATCMTranslationEntry& Entry :
			ConfigEntries)
		{
			FPolyglotTextData Translation(
				ELocalizedTextSourceCategory::Game,
				TEXT("ATCMConfig"),
				Entry.Key,
				Entry.English,
				TEXT("en"));

			Translation.AddLocalizedString(
				TEXT("de"),
				Entry.German);

			FTextLocalizationManager::Get()
				.RegisterPolyglotTextData(Translation);
		}
}

void FAdvancedTrainCargoManagerModule::StartupModule()
{
	RegisterATCMTranslations();

#if !WITH_EDITOR
	FATCMTransferHooks::Install();
#endif

	UE_LOG(
		LogAdvancedTrainCargoManager,
		Log,
		TEXT(
			"Advanced Train Cargo Manager wurde "
			"erfolgreich geladen."));
}

void FAdvancedTrainCargoManagerModule::ShutdownModule()
{
#if !WITH_EDITOR
	FATCMTransferHooks::Uninstall();
#endif

	UE_LOG(
		LogAdvancedTrainCargoManager,
		Log,
		TEXT("Advanced Train Cargo Manager wurde entladen."));
}

IMPLEMENT_MODULE(
	FAdvancedTrainCargoManagerModule,
	AdvancedTrainCargoManager)

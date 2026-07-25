#pragma once

#include "CoreMinimal.h"

/**
 * Verwaltet das native Slate-Fenster der Mod.
 */
class FATCMUserInterface final
{
public:
	static void Initialize();
	static void Shutdown();

	static void Toggle();
	static void Open();
	static void Close();

	static bool IsOpen();
	static FText GetHotkeyDisplayText();
};

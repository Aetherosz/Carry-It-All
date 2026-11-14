#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "InventoryBonusConfig.generated.h"

/**
 * Shows up in the game's Mods -> Mod Configuration menu (main menu and pause menu).
 * SML saves it to FactoryGame/Configs/InventoryBonus.cfg.
 *
 * The bonus itself is stored per save (AInventoryBonusSubsystem). The visible "Bonus" value
 * mirrors the loaded save while playing, and the "new saves" default while in the main menu.
 */
UCLASS()
class UInventoryBonusConfig : public UModConfiguration
{
	GENERATED_BODY()

public:
	UInventoryBonusConfig();

	/** Read-only line showing where your slots come from (an empty section used as a label). */
	static const FString BreakdownKey;
	/** The bonus: this save's while playing, the "new saves" default in the main menu. */
	static const FString BonusKey;
	/** Dedicated servers: whether players (not only the server console) may change the bonus. */
	static const FString AnyPlayerCanChangeKey;
	/** Hidden: bonus for saves that don't have one yet. -1 = not set (first run after v1.0). */
	static const FString NewSaveBonusKey;

	static FConfigId GetId();
};

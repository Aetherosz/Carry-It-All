#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "InventoryBonusConfig.generated.h"

/**
 * Shows up in the game's Mods -> Mod Configuration menu (main menu and pause menu).
 * SML saves it to FactoryGame/Configs/InventoryBonus.cfg.
 */
UCLASS()
class UInventoryBonusConfig : public UModConfiguration
{
	GENERATED_BODY()

public:
	UInventoryBonusConfig();

	/** Key of the bonus slider inside RootSection. */
	static const FString BonusKey;

	static FConfigId GetId();
};

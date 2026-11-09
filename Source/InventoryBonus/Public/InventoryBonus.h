#pragma once

#include "CoreMinimal.h"

class AFGCharacterPlayer;
class UFGInventoryComponent;
class UGameInstance;
class UWorld;

/** Where a player's inventory slots come from. */
struct FInventorySlotBreakdown
{
	/** Starting slots every new player gets. */
	int32 Default = 0;
	/** Slots from vanilla schematics: HUB milestones, MAM research, hard drive alternates. */
	int32 VanillaUpgrades = 0;
	/** Slots from other mods' schematics (e.g. InventorySpace2 / InventorySpace3). */
	int32 ModUpgrades = 0;
	/** Slots added by this mod. */
	int32 Bonus = 0;
	/** Default + VanillaUpgrades + ModUpgrades + Bonus. */
	int32 Wanted = 0;
	/** Actual inventory size right now. */
	int32 Current = 0;
	/** Slots past Wanted that are kept because they still hold items. */
	int32 HeldByItems = 0;
};

/**
 * Keeps every player's main inventory at:
 *     default + vanilla upgrades + other mods' upgrades + Bonus
 *
 * Everything except Bonus is recomputed from the game's own data each time, so
 * buying more upgrades or adding/removing other inventory mods keeps working.
 * Bonus lives in SML's mod config (Mods -> Mod Configuration -> Inventory Bonus),
 * saved by SML to FactoryGame/Configs/InventoryBonus.cfg. The /invbonus command edits the same value.
 */
class FInventoryBonus
{
public:
	/** Bonus used the first time the mod runs (before you ever change it). */
	static constexpr int32 DefaultBonus = 1000;
	/** Safety cap: very large inventories make the inventory UI slow. */
	static constexpr int32 MaxBonus = 20000;

	static void RegisterHooks();

	static int32 GetBonus();
	/** Writes the new bonus to the mod config (SML saves it) and resizes every player. Server only. */
	static void SetBonus(UWorld* World, int32 NewBonus);

	static void ApplyToAllPlayers(UWorld* World);
	static void ApplyToPlayer(AFGCharacterPlayer* Player);

	static FInventorySlotBreakdown GetBreakdown(AFGCharacterPlayer* Player);

private:
	/** Fills OutBreakdown (except Current/HeldByItems). Returns false if the game data isn't ready yet. */
	static bool ComputeBreakdown(AFGCharacterPlayer* Player, FInventorySlotBreakdown& OutBreakdown);
	/** Never shrink past a slot that still holds items. */
	static int32 KeepOccupiedSlots(const UFGInventoryComponent* Inventory, int32 DesiredSize);
	/** The player whose *main* inventory this is, or null (equipment, storage, etc.). */
	static AFGCharacterPlayer* GetOwningPlayer(const UFGInventoryComponent* Inventory);

	/** Registers UInventoryBonusConfig with SML so it appears in Mods -> Mod Configuration. */
	static void RegisterConfig(UGameInstance* GameInstance);
	/** The live "Bonus" value from SML's config, or null before the config is registered. */
	static class UConfigPropertyInteger* GetBonusProperty();

	static TWeakObjectPtr<class UConfigManager> ConfigManager;
	/** World that already has the once-per-second "did the bonus change?" timer. */
	static TWeakObjectPtr<UWorld> PolledWorld;
	/** Bonus value used by the last ApplyToAllPlayers. */
	static int32 LastAppliedBonus;
};

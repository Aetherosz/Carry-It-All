#pragma once

#include "CoreMinimal.h"

class AFGCharacterPlayer;
class UCommandSender;
class UConfigProperty;
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

/** What happened when the bonus was changed. */
struct FBonusChangeResult
{
	/** Players who got a crate with items that no longer fit. */
	int32 PlayersWithCrate = 0;
	/** Items put into those crates. */
	int32 ItemsInCrates = 0;
};

/**
 * Keeps every player's main inventory at:
 *     default + vanilla upgrades + other mods' upgrades + Bonus
 *
 * Everything except Bonus is recomputed from the game's own data each time, so
 * buying more upgrades or adding/removing other inventory mods keeps working.
 * Bonus is stored per save (AInventoryBonusSubsystem). The mod config menu and /invbonus
 * edit the loaded save's bonus; in the main menu the config holds the bonus for new saves.
 */
class FInventoryBonus
{
public:
	/** Bonus a new save starts with until you change the "new saves" value. */
	static constexpr int32 DefaultBonus = 1000;
	/**
	 * Highest bonus the menu and /invbonus allow. More than this isn't useful, and very large
	 * inventories make the inventory UI slow. A save that already has more keeps it.
	 */
	static constexpr int32 MaxBonus = 1000;

	static void RegisterHooks();

	/** The bonus for this world's save (server), or the "new saves" value otherwise. */
	static int32 GetBonus(UWorld* World);
	/** Bonus for saves that don't have one stored yet. */
	static int32 GetNewSaveBonus();
	/**
	 * Server only. Stores the bonus in the save and resizes every player. When shrinking, items
	 * in removed slots are moved into free slots, and whatever doesn't fit goes into a crate.
	 */
	static FBonusChangeResult SetBonus(UWorld* World, int32 NewBonus);

	/** Whether this sender may change the bonus. OutReason explains a refusal. */
	static bool CanChangeBonus(UCommandSender* Sender, FString& OutReason);

	static void ApplyToAllPlayers(UWorld* World);
	static FInventorySlotBreakdown GetBreakdown(AFGCharacterPlayer* Player);

private:
	static void ApplyToPlayer(AFGCharacterPlayer* Player);
	/** User-initiated shrink: crate first, then shrink, then pull back what fits. Returns items left in the crate. */
	static int32 ShrinkWithCrate(AFGCharacterPlayer* Player, UFGInventoryComponent* Inventory, int32 Wanted);

	/** Fills OutBreakdown (except Current/HeldByItems). Returns false if the game data isn't ready yet. */
	static bool ComputeBreakdown(AFGCharacterPlayer* Player, FInventorySlotBreakdown& OutBreakdown);
	/** Never shrink past a slot that still holds items. */
	static int32 KeepOccupiedSlots(const UFGInventoryComponent* Inventory, int32 DesiredSize);
	/** The player whose *main* inventory this is, or null (equipment, storage, etc.). */
	static AFGCharacterPlayer* GetOwningPlayer(const UFGInventoryComponent* Inventory);

	/** Registers UInventoryBonusConfig with SML so it appears in Mods -> Mod Configuration. */
	static void RegisterConfig(UGameInstance* GameInstance);
	/** A property of the live mod config, or null before the config is registered. */
	static UConfigProperty* GetConfigProperty(const FString& Key);
	static int32 GetConfigInt(const FString& Key, int32 Fallback);
	/** Sets an integer config value. bSaveToFile also writes the .cfg. */
	static void SetConfigInt(const FString& Key, int32 Value, bool bSaveToFile);

	/**
	 * Mod config changed (menu or file). In the main menu it becomes the "new saves" bonus right away.
	 * In a save it only becomes pending: the menu shows a warning, and the once-per-second poll applies
	 * it once the value has stopped changing for MenuApplyDelay seconds. Dragging the value therefore
	 * shrinks (and fills a crate) once, not at every step of the drag.
	 */
	static void OnConfigChanged(UWorld* World);
	/** Poll (server, once per second while the game runs): applies a settled pending value and refreshes the menu text. */
	static void PollMenu(UWorld* World);
	/** Server, after a save is loaded: show that save's bonus in the mod config. */
	static void ShowSaveInConfig(UWorld* World);
	/** A game world is closing: the config goes back to showing the "new saves" bonus. */
	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** Tells every player in chat what a bonus change did. */
	static void AnnounceChange(UWorld* World, const FBonusChangeResult& Result);

	enum class EMenuMode { MainMenu, InSave, Client };
	/** Rewrites the config menu's labels. LocalPlayer is used for InSave (breakdown, items carried, warning). */
	static void UpdateMenuText(EMenuMode Mode, AFGCharacterPlayer* LocalPlayer = nullptr);
	/** The player whose screen shows the menu (single player / host), or null (dedicated server). */
	static AFGCharacterPlayer* GetLocalPlayer(UWorld* World);

	/** How long a menu value must stay unchanged before it is applied to the save. */
	static constexpr double MenuApplyDelay = 2.0;

	static TWeakObjectPtr<class UConfigManager> ConfigManager;
	/** World that already has the once-per-second "did the bonus change?" timer. */
	static TWeakObjectPtr<UWorld> PolledWorld;
	/** True while we change the config ourselves, so our own change handler ignores it. */
	static bool bWritingConfig;
	/** Menu value waiting to be applied to the save, or -1. */
	static int32 PendingMenuValue;
	/** FPlatformTime::Seconds() of the last change to PendingMenuValue. */
	static double PendingSince;
};

#include "InventoryBonus.h"

#include "InventoryBonusCommand.h"
#include "InventoryBonusConfig.h"
#include "InventoryBonusSubsystem.h"

#include "Command/ChatCommandLibrary.h"
#include "Command/CommandSender.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGChatManager.h"
#include "FGCrate.h"
#include "FGInventoryComponent.h"
#include "FGItemPickup_Spawnable.h"
#include "FGPlayerController.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Patching/NativeHookManager.h"
#include "TimerManager.h"
#include "Unlocks/FGUnlockInventorySlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryBonus, Log, All);

TWeakObjectPtr<UConfigManager> FInventoryBonus::ConfigManager;
TWeakObjectPtr<UWorld> FInventoryBonus::PolledWorld;
bool FInventoryBonus::bWritingConfig = false;
int32 FInventoryBonus::PendingMenuValue = -1;
double FInventoryBonus::PendingSince = 0.0;

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

void FInventoryBonus::RegisterHooks()
{
	// Register the mod config as soon as the game instance exists, so it is
	// listed in the main menu's Mods -> Mod Configuration before any save loads.
	FWorldDelegates::OnStartGameInstance.AddStatic(&FInventoryBonus::RegisterConfig);
	FWorldDelegates::OnWorldCleanup.AddStatic(&FInventoryBonus::OnWorldCleanup);

#if !WITH_EDITOR
	// Bonus changed in the Mod Configuration menu -> store it in the save (or as "new saves").
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty,
		[](UConfigManager* Self, const FConfigId& Id)
		{
			if (Id != UInventoryBonusConfig::GetId())
			{
				return;
			}
			if (!bWritingConfig)
			{
				if (UGameInstance* GameInstance = Self->GetGameInstance())
				{
					OnConfigChanged(GameInstance->GetWorld());
				}
			}
			// Write it now. SML otherwise saves on a timer that doesn't run while the game is paused,
			// and its flush at shutdown crashes (its mod info subsystem is already gone by then), so
			// "change a value in the pause menu, then Exit to Desktop" would crash the game.
			Self->FlushPendingSaves();
		});

	// Every resize of a player's main inventory (spawn, load, unlocks, other code)
	// is replaced by our own total, so the bonus always sits on top.
	// This path never moves items: it only refuses to drop slots that still hold something.
	SUBSCRIBE_METHOD(UFGInventoryComponent::Resize,
		[](auto& Scope, UFGInventoryComponent* Self, int32 NewSize)
		{
			AFGCharacterPlayer* Player = GetOwningPlayer(Self);
			if (Player && Player->HasAuthority())
			{
				FInventorySlotBreakdown Breakdown;
				if (ComputeBreakdown(Player, Breakdown))
				{
					NewSize = KeepOccupiedSlots(Self, Breakdown.Wanted);
				}
			}
			Scope(Self, NewSize);
		});

	// On spawn: set up the save's bonus, register the chat command and apply. Applied again a
	// bit later, because on save load the schematic data can arrive after the player spawns.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGCharacterPlayer::BeginPlay, GetMutableDefault<AFGCharacterPlayer>(),
		[](AFGCharacterPlayer* Self)
		{
			UWorld* World = Self->GetWorld();
			if (!World)
			{
				return;
			}
			if (World->GetNetMode() == NM_Client)
			{
				UpdateMenuText(EMenuMode::Client);
				return;
			}
			if (!ConfigManager.IsValid())
			{
				RegisterConfig(Self->GetGameInstance()); // fallback if OnStartGameInstance fired before we loaded
			}

			// Once per world: adopt the save's bonus, register the command, start the menu poll.
			if (PolledWorld.Get() != World)
			{
				PolledWorld = World;
				AInventoryBonusSubsystem::GetOrRegister(World);
				ShowSaveInConfig(World);

				if (AChatCommandSubsystem* Commands = AChatCommandSubsystem::Get(Self))
				{
					Commands->RegisterCommand(TEXT("InventoryBonus"), AInventoryBonusCommand::StaticClass());
				}

				// Applies menu edits once they settle, and keeps the menu's item count current.
				// World timers stop while single player is paused, so there a menu edit is
				// applied after the pause menu closes.
				PendingMenuValue = -1;
				FTimerHandle PollHandle;
				World->GetTimerManager().SetTimer(PollHandle,
					FTimerDelegate::CreateWeakLambda(World, [World]() { PollMenu(World); }),
					1.0f, true);
			}

			ApplyToPlayer(Self);

			for (const float Delay : { 2.0f, 10.0f })
			{
				FTimerHandle Handle;
				Self->GetWorldTimerManager().SetTimer(Handle,
					FTimerDelegate::CreateWeakLambda(Self, [Self]() { ApplyToPlayer(Self); }),
					Delay, false);
			}
		});

	// New schematic (milestone, MAM, hard drive, modded) -> recompute everyone.
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GiveAccessToSchematic,
		[](AFGSchematicManager* Self, TSubclassOf<UFGSchematic>, AFGCharacterPlayer*, ESchematicUnlockFlags)
		{
			ApplyToAllPlayers(Self->GetWorld());
		});
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GiveAccessToSchematics,
		[](AFGSchematicManager* Self, const TArray<TSubclassOf<UFGSchematic>>&, AFGCharacterPlayer*, ESchematicUnlockFlags)
		{
			ApplyToAllPlayers(Self->GetWorld());
		});
#endif
}

// ---------------------------------------------------------------------------
// Slot math
// ---------------------------------------------------------------------------

bool FInventoryBonus::ComputeBreakdown(AFGCharacterPlayer* Player, FInventorySlotBreakdown& Out)
{
	AFGSchematicManager* SchematicManager = AFGSchematicManager::Get(Player->GetWorld());
	if (!SchematicManager)
	{
		return false;
	}

	Out.Default = Player->mDefaultPlayerInventorySlots; // friend access, see Config/AccessTransformers.ini
	Out.VanillaUpgrades = 0;
	Out.ModUpgrades = 0;

	TArray<TSubclassOf<UFGSchematic>> Purchased;
	SchematicManager->GetAllPurchasedSchematics(Purchased);
	for (const TSubclassOf<UFGSchematic>& Schematic : Purchased)
	{
		if (!Schematic)
		{
			continue;
		}
		// Vanilla content lives under /Game/, mods under /<ModReference>/.
		const bool bVanilla = Schematic->GetPathName().StartsWith(TEXT("/Game/"));
		for (const UFGUnlock* Unlock : UFGSchematic::GetUnlocks(Schematic))
		{
			if (const UFGUnlockInventorySlot* SlotUnlock = Cast<UFGUnlockInventorySlot>(Unlock))
			{
				(bVanilla ? Out.VanillaUpgrades : Out.ModUpgrades) += SlotUnlock->GetNumInventorySlotsToUnlock();
			}
		}
	}

	Out.Bonus = GetBonus(Player->GetWorld());
	Out.Wanted = FMath::Max(1, Out.Default + Out.VanillaUpgrades + Out.ModUpgrades + Out.Bonus);
	return true;
}

int32 FInventoryBonus::KeepOccupiedSlots(const UFGInventoryComponent* Inventory, int32 DesiredSize)
{
	for (int32 Index = Inventory->GetSizeLinear() - 1; Index >= DesiredSize; --Index)
	{
		if (!Inventory->IsIndexEmpty(Index))
		{
			return Index + 1;
		}
	}
	return DesiredSize;
}

AFGCharacterPlayer* FInventoryBonus::GetOwningPlayer(const UFGInventoryComponent* Inventory)
{
	AFGCharacterPlayer* Player = Inventory ? Cast<AFGCharacterPlayer>(Inventory->GetOwner()) : nullptr;
	return (Player && Player->GetInventory() == Inventory) ? Player : nullptr;
}

FInventorySlotBreakdown FInventoryBonus::GetBreakdown(AFGCharacterPlayer* Player)
{
	FInventorySlotBreakdown Breakdown;
	if (Player && ComputeBreakdown(Player, Breakdown))
	{
		if (const UFGInventoryComponent* Inventory = Player->GetInventory())
		{
			Breakdown.Current = Inventory->GetSizeLinear();
			Breakdown.HeldByItems = FMath::Max(0, Breakdown.Current - Breakdown.Wanted);
		}
	}
	return Breakdown;
}

// ---------------------------------------------------------------------------
// Applying
// ---------------------------------------------------------------------------

void FInventoryBonus::ApplyToPlayer(AFGCharacterPlayer* Player)
{
	if (!Player || !Player->HasAuthority())
	{
		return;
	}
	if (UFGInventoryComponent* Inventory = Player->GetInventory())
	{
		// The Resize hook swaps in the correct total.
		Inventory->Resize(Inventory->GetSizeLinear());
	}
	if (Player->IsLocallyControlled())
	{
		UpdateMenuText(EMenuMode::InSave, Player);
	}
}

void FInventoryBonus::ApplyToAllPlayers(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		ApplyToPlayer(*It);
	}
}

int32 FInventoryBonus::ShrinkWithCrate(AFGCharacterPlayer* Player, UFGInventoryComponent* Inventory, int32 Wanted)
{
	const int32 Size = Inventory->GetSizeLinear();
	TArray<FInventoryStack> Removed;
	for (int32 Index = Wanted; Index < Size; ++Index)
	{
		FInventoryStack Stack;
		if (Inventory->GetStackFromIndex(Index, Stack) && Stack.HasItems())
		{
			Removed.Add(Stack);
		}
	}
	if (Removed.Num() == 0)
	{
		return 0; // nothing in the removed slots, the normal resize handles it
	}

	// 1. Spawn the crate with copies of the items first. If that fails, nothing is touched.
	FSpawnInventoryCrateParameters Params;
	Params.ActorsToIgnore.Add(Player);
	Params.bAllowCrateMerging = false;
	AFGCrate* Crate = AFGItemPickup_Spawnable::SpawnInventoryCrate(Player->GetWorld(), Removed, Player->GetActorLocation(), Params);
	if (!Crate || !Crate->GetInventory())
	{
		UE_LOG(LogInventoryBonus, Warning, TEXT("Couldn't spawn a crate for %s; keeping the occupied slots instead"), *Player->GetName());
		return 0;
	}

	// 2. The items are safe in the crate: clear the removed slots and shrink.
	for (int32 Index = Wanted; Index < Size; ++Index)
	{
		Inventory->RemoveAllFromIndex(Index);
	}
	Inventory->Resize(Inventory->GetSizeLinear()); // the Resize hook now shrinks to Wanted

	// 3. Pull back whatever fits into the smaller inventory. An emptied crate removes itself.
	UFGInventoryComponent* CrateInventory = Crate->GetInventory();
	for (int32 Index = 0; IsValid(Crate) && Index < CrateInventory->GetSizeLinear(); ++Index)
	{
		FInventoryStack Stack;
		if (CrateInventory->GetStackFromIndex(Index, Stack) && Stack.HasItems())
		{
			const int32 Added = Inventory->AddStack(Stack, true);
			if (Added > 0)
			{
				CrateInventory->RemoveFromIndex(Index, Added);
			}
		}
	}

	int32 ItemsLeft = 0;
	if (IsValid(Crate))
	{
		for (int32 Index = 0; Index < CrateInventory->GetSizeLinear(); ++Index)
		{
			FInventoryStack Stack;
			if (CrateInventory->GetStackFromIndex(Index, Stack))
			{
				ItemsLeft += Stack.NumItems;
			}
		}
		if (ItemsLeft == 0)
		{
			Crate->Destroy();
		}
	}
	return ItemsLeft;
}

// ---------------------------------------------------------------------------
// Bonus value (stored per save, mirrored in SML's mod config)
// ---------------------------------------------------------------------------

int32 FInventoryBonus::GetBonus(UWorld* World)
{
	if (AInventoryBonusSubsystem* Subsystem = AInventoryBonusSubsystem::Get(World))
	{
		return Subsystem->GetBonus();
	}
	return GetNewSaveBonus();
}

int32 FInventoryBonus::GetNewSaveBonus()
{
	int32 Value = GetConfigInt(UInventoryBonusConfig::NewSaveBonusKey, -1);
	if (Value < 0)
	{
		Value = GetConfigInt(UInventoryBonusConfig::BonusKey, DefaultBonus); // not migrated yet
	}
	return FMath::Clamp(Value, 0, MaxBonus);
}

FBonusChangeResult FInventoryBonus::SetBonus(UWorld* World, int32 NewBonus)
{
	FBonusChangeResult Result;
	AInventoryBonusSubsystem* Subsystem = AInventoryBonusSubsystem::GetOrRegister(World);
	if (!Subsystem)
	{
		UE_LOG(LogInventoryBonus, Warning, TEXT("No save loaded (or not the server); bonus change ignored"));
		return Result;
	}
	Subsystem->SetBonus(NewBonus);
	SetConfigInt(UInventoryBonusConfig::BonusKey, Subsystem->GetBonus(), false);

	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		AFGCharacterPlayer* Player = *It;
		UFGInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
		FInventorySlotBreakdown Breakdown;
		if (Inventory && ComputeBreakdown(Player, Breakdown) && Inventory->GetSizeLinear() > Breakdown.Wanted)
		{
			const int32 ItemsInCrate = ShrinkWithCrate(Player, Inventory, Breakdown.Wanted);
			if (ItemsInCrate > 0)
			{
				++Result.PlayersWithCrate;
				Result.ItemsInCrates += ItemsInCrate;
			}
		}
		ApplyToPlayer(Player);
	}
	return Result;
}

bool FInventoryBonus::CanChangeBonus(UCommandSender* Sender, FString& OutReason)
{
	if (!Sender->IsPlayerSender())
	{
		return true; // server console
	}
	const AFGPlayerController* Controller = Sender->GetPlayer();
	if (Controller && Controller->IsLocalController())
	{
		return true; // single player, or the host of a hosted game
	}
	UWorld* World = Sender->GetWorld();
	if (World && World->GetNetMode() == NM_DedicatedServer)
	{
		const UConfigPropertyBool* AnyPlayer = Cast<UConfigPropertyBool>(GetConfigProperty(UInventoryBonusConfig::AnyPlayerCanChangeKey));
		if (AnyPlayer && AnyPlayer->Value)
		{
			return true;
		}
		OutReason = TEXT("On this server only the server console can change the inventory bonus. "
			"The server owner can allow all players in the mod's config.");
		return false;
	}
	OutReason = TEXT("Only the host can change the inventory bonus.");
	return false;
}

// ---------------------------------------------------------------------------
// Mod config
// ---------------------------------------------------------------------------

void FInventoryBonus::RegisterConfig(UGameInstance* GameInstance)
{
	if (!GameInstance)
	{
		return;
	}
	UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>();
	if (!Manager)
	{
		UE_LOG(LogInventoryBonus, Error, TEXT("SML config manager not found; using default bonus +%d"), DefaultBonus);
		return;
	}
	UInventoryBonusConfig::UseMenuWidgets();
	Manager->RegisterModConfiguration(UInventoryBonusConfig::StaticClass());
	ConfigManager = Manager;

	// First run after v1.0: the old global bonus becomes the "new saves" bonus, so saves
	// from v1.0 keep the bonus they had.
	if (GetConfigInt(UInventoryBonusConfig::NewSaveBonusKey, -1) < 0)
	{
		SetConfigInt(UInventoryBonusConfig::NewSaveBonusKey,
			FMath::Clamp(GetConfigInt(UInventoryBonusConfig::BonusKey, DefaultBonus), 0, MaxBonus), true);
	}
	// In the main menu the visible value is the "new saves" bonus (the game may have been
	// closed while a save's value was shown).
	SetConfigInt(UInventoryBonusConfig::BonusKey, GetNewSaveBonus(), false);
	UpdateMenuText(EMenuMode::MainMenu);
}

UConfigProperty* FInventoryBonus::GetConfigProperty(const FString& Key)
{
	UConfigManager* Manager = ConfigManager.Get();
	UConfigPropertySection* Root = Manager ? Manager->GetConfigurationRootSection(UInventoryBonusConfig::GetId()) : nullptr;
	const TObjectPtr<UConfigProperty>* Property = Root ? Root->SectionProperties.Find(Key) : nullptr;
	return Property ? Property->Get() : nullptr;
}

int32 FInventoryBonus::GetConfigInt(const FString& Key, int32 Fallback)
{
	const UConfigPropertyInteger* Property = Cast<UConfigPropertyInteger>(GetConfigProperty(Key));
	return Property ? Property->Value : Fallback;
}

void FInventoryBonus::SetConfigInt(const FString& Key, int32 Value, bool bSaveToFile)
{
	if (UConfigPropertyInteger* Property = Cast<UConfigPropertyInteger>(GetConfigProperty(Key)))
	{
		Property->Value = Value;
		if (bSaveToFile)
		{
			TGuardValue<bool> Guard(bWritingConfig, true);
			Property->MarkDirty();
		}
	}
}

void FInventoryBonus::OnConfigChanged(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return; // in multiplayer the host's value counts
	}
	const int32 RawMenuValue = GetConfigInt(UInventoryBonusConfig::BonusKey, DefaultBonus);
	const int32 MenuValue = FMath::Clamp(RawMenuValue, 0, MaxBonus);

	if (AInventoryBonusSubsystem* Subsystem = AInventoryBonusSubsystem::Get(World))
	{
		// A save from before the cap may hold more than MaxBonus; showing it must not shrink it.
		if (RawMenuValue == Subsystem->GetBonus())
		{
			PendingMenuValue = -1;
			UpdateMenuText(EMenuMode::InSave, GetLocalPlayer(World));
			return;
		}
		// In a save: never apply straight away. The menu's number box reports every value it
		// passes while being dragged; applying each one would shrink (and crate) many times.
		const int32 NewPending = MenuValue == Subsystem->GetBonus() ? -1 : MenuValue;
		if (NewPending != PendingMenuValue)
		{
			PendingMenuValue = NewPending;
			PendingSince = FPlatformTime::Seconds();
		}
		UpdateMenuText(EMenuMode::InSave, GetLocalPlayer(World));
	}
	else if (PolledWorld.Get() != World && MenuValue != GetConfigInt(UInventoryBonusConfig::NewSaveBonusKey, -1))
	{
		// Main menu: the value is the bonus for new saves. Nothing to resize, so it's stored right away.
		SetConfigInt(UInventoryBonusConfig::NewSaveBonusKey, MenuValue, true);
	}
}

void FInventoryBonus::PollMenu(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	OnConfigChanged(World); // safety net: catches menu changes the hook didn't see

	if (PendingMenuValue >= 0 && FPlatformTime::Seconds() - PendingSince >= MenuApplyDelay)
	{
		const int32 Value = PendingMenuValue;
		PendingMenuValue = -1;
		AnnounceChange(World, SetBonus(World, Value)); // a lowered bonus moves leftovers into one crate
	}
	else
	{
		// Keeps "items carried" current, so the menu is right whenever it's opened.
		UpdateMenuText(EMenuMode::InSave, GetLocalPlayer(World));
	}
}

void FInventoryBonus::AnnounceChange(UWorld* World, const FBonusChangeResult& Result)
{
	AFGChatManager* Chat = AFGChatManager::Get(World);
	if (!Chat)
	{
		return;
	}
	FChatMessageStruct Message;
	Message.MessageType = EFGChatMessageType::CMT_SystemMessage;
	Message.MessageSender = FText::FromString(TEXT("Inventory Bonus"));
	Message.MessageText = FText::FromString(FString::Printf(TEXT("Inventory bonus for this save set to +%d."), GetBonus(World)));
	Chat->BroadcastChatMessage(Message);

	if (Result.PlayersWithCrate > 0)
	{
		Message.MessageSenderColor = FLinearColor::Yellow;
		Message.MessageText = FText::FromString(FString::Printf(
			TEXT("%d item(s) didn't fit and were put in a crate at the feet of %d player(s)."),
			Result.ItemsInCrates, Result.PlayersWithCrate));
		Chat->BroadcastChatMessage(Message);
	}
}

void FInventoryBonus::ShowSaveInConfig(UWorld* World)
{
	if (AInventoryBonusSubsystem* Subsystem = AInventoryBonusSubsystem::Get(World))
	{
		SetConfigInt(UInventoryBonusConfig::BonusKey, Subsystem->GetBonus(), false);
		UpdateMenuText(EMenuMode::InSave, GetLocalPlayer(World));
	}
}

void FInventoryBonus::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	if (PolledWorld.Get() == World)
	{
		PolledWorld.Reset();
		PendingMenuValue = -1; // an edit that never settled is dropped with the world
	}
	if (ConfigManager.IsValid())
	{
		SetConfigInt(UInventoryBonusConfig::BonusKey, GetNewSaveBonus(), false);
		UpdateMenuText(EMenuMode::MainMenu);
	}
}

AFGCharacterPlayer* FInventoryBonus::GetLocalPlayer(UWorld* World)
{
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return (Controller && Controller->IsLocalController()) ? Cast<AFGCharacterPlayer>(Controller->GetPawn()) : nullptr;
}

void FInventoryBonus::UpdateMenuText(EMenuMode Mode, AFGCharacterPlayer* LocalPlayer)
{
	UConfigProperty* Line1 = GetConfigProperty(UInventoryBonusConfig::BreakdownKey);
	UConfigProperty* Line2 = GetConfigProperty(UInventoryBonusConfig::StatusKey);
	UConfigProperty* BonusProperty = GetConfigProperty(UInventoryBonusConfig::BonusKey);
	if (!Line1 || !Line2 || !BonusProperty)
	{
		return;
	}
	// The menu shows each label as a one-line section header, so keep each line short.
	const auto SetLines = [Line1, Line2](const FString& First, const FString& Second)
	{
		const auto SetLine = [](UConfigProperty* Line, const FString& Text)
		{
			Line->DisplayName = FText::FromString(Text);
			if (UCP_Section* Header = Cast<UCP_Section>(Line))
			{
				Header->HeaderText = FText::GetEmpty(); // the header shows "{HeaderText} ({DisplayName})", or just DisplayName when HeaderText is empty
			}
		};
		SetLine(Line1, First);
		SetLine(Line2, Second);
	};

	switch (Mode)
	{
	case EMenuMode::MainMenu:
		SetLines(TEXT("Load a save to see your inventory slots."),
			TEXT("In the main menu, this is the bonus new saves start with."));
		BonusProperty->DisplayName = FText::FromString(TEXT("Bonus inventory slots (new saves)"));
		return;
	case EMenuMode::Client:
		SetLines(TEXT("Multiplayer: the host's bonus is used."),
			TEXT("Type /invbonus to see your slots."));
		BonusProperty->DisplayName = FText::FromString(TEXT("Bonus inventory slots (set by the host)"));
		return;
	case EMenuMode::InSave:
		break;
	}

	BonusProperty->DisplayName = FText::FromString(TEXT("Bonus inventory slots (this save)"));
	const UFGInventoryComponent* Inventory = LocalPlayer ? LocalPlayer->GetInventory() : nullptr;
	FInventorySlotBreakdown B;
	if (!Inventory || !ComputeBreakdown(LocalPlayer, B))
	{
		return;
	}

	// What you're carrying right now.
	int32 UsedSlots = 0;
	int32 Items = 0;
	for (int32 Index = 0; Index < Inventory->GetSizeLinear(); ++Index)
	{
		FInventoryStack Stack;
		if (Inventory->GetStackFromIndex(Index, Stack) && Stack.HasItems())
		{
			++UsedSlots;
			Items += Stack.NumItems;
		}
	}
	const int32 Base = B.Default + B.VanillaUpgrades + B.ModUpgrades;
	const int32 LowestSafeBonus = FMath::Max(0, UsedSlots - Base);
	const int32 ShownBonus = PendingMenuValue >= 0 ? PendingMenuValue : B.Bonus;
	const int32 ShownSlots = FMath::Max(1, Base + ShownBonus);

	// Line 1: where the slots come from (with the value being picked, while it's pending).
	const FString Slots = FString::Printf(TEXT("Slots: %d default + %d vanilla + %d mods + %d bonus = %d%s"),
		B.Default, B.VanillaUpgrades, B.ModUpgrades, ShownBonus, ShownSlots,
		PendingMenuValue >= 0 ? *FString::Printf(TEXT("  (now %d)"), B.Wanted) : TEXT(""));

	// Line 2: what you carry, and what the new value would do to it.
	FString Status;
	if (PendingMenuValue >= 0 && PendingMenuValue < LowestSafeBonus)
	{
		// Lowering below what you carry: make it impossible to miss.
		const int32 StacksToCrate = UsedSlots - ShownSlots;
		Status = FString::Printf(TEXT("!!! WARNING: %d of your %d stacks go into a crate. Keep +%d or more !!!"),
			StacksToCrate, UsedSlots, LowestSafeBonus);
		BonusProperty->DisplayName = FText::FromString(FString::Printf(
			TEXT("Bonus inventory slots  !!! %d stacks to a crate !!!"), StacksToCrate));
	}
	else if (PendingMenuValue >= 0)
	{
		Status = FString::Printf(TEXT("Everything fits (%d stacks). Applies 2 s after you stop, or when you close the menu."),
			UsedSlots);
	}
	else
	{
		Status = FString::Printf(TEXT("Carrying %d items in %d stacks. %s"), Items, UsedSlots,
			LowestSafeBonus > 0
				? *FString::Printf(TEXT("Below +%d, extra stacks go into a crate."), LowestSafeBonus)
				: TEXT("Any bonus fits them all."));
	}
	SetLines(Slots, Status);
}

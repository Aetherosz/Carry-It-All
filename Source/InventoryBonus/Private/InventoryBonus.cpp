#include "InventoryBonus.h"

#include "InventoryBonusCommand.h"
#include "InventoryBonusConfig.h"

#include "Command/ChatCommandLibrary.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Patching/NativeHookManager.h"
#include "TimerManager.h"
#include "Unlocks/FGUnlockInventorySlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventoryBonus, Log, All);

TWeakObjectPtr<UConfigManager> FInventoryBonus::ConfigManager;
TWeakObjectPtr<UWorld> FInventoryBonus::PolledWorld;
int32 FInventoryBonus::LastAppliedBonus = -1;

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------

void FInventoryBonus::RegisterHooks()
{
	// Register the mod config as soon as the game instance exists, so it is
	// listed in the main menu's Mods -> Mod Configuration before any save loads.
	FWorldDelegates::OnStartGameInstance.AddStatic(&FInventoryBonus::RegisterConfig);

#if !WITH_EDITOR
	// Bonus changed in the Mod Configuration menu (or by /invbonus) -> resize everyone now.
	SUBSCRIBE_METHOD_AFTER(UConfigManager::MarkConfigurationDirty,
		[](UConfigManager* Self, const FConfigId& Id)
		{
			if (Id == UInventoryBonusConfig::GetId())
			{
				if (UGameInstance* GameInstance = Self->GetGameInstance())
				{
					ApplyToAllPlayers(GameInstance->GetWorld());
				}
			}
		});

	// Every resize of a player's main inventory (spawn, load, unlocks, other code)
	// is replaced by our own total, so the bonus always sits on top.
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

	// On spawn: register the chat command and apply the bonus. Applied again a bit
	// later, because on save load the schematic data can arrive after the player spawns.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGCharacterPlayer::BeginPlay, GetMutableDefault<AFGCharacterPlayer>(),
		[](AFGCharacterPlayer* Self)
		{
			if (!Self->HasAuthority())
			{
				return;
			}
			if (!ConfigManager.IsValid())
			{
				RegisterConfig(Self->GetGameInstance()); // fallback if OnStartGameInstance fired before we loaded
			}
			if (AChatCommandSubsystem* Commands = AChatCommandSubsystem::Get(Self))
			{
				Commands->RegisterCommand(TEXT("InventoryBonus"), AInventoryBonusCommand::StaticClass());
			}

			// Safety net for menu edits: once per world, check every second whether the
			// configured bonus differs from what was last applied, and re-apply if so.
			UWorld* World = Self->GetWorld();
			if (World && PolledWorld.Get() != World)
			{
				PolledWorld = World;
				FTimerHandle PollHandle;
				World->GetTimerManager().SetTimer(PollHandle,
					FTimerDelegate::CreateWeakLambda(World, [World]()
					{
						if (GetBonus() != LastAppliedBonus)
						{
							ApplyToAllPlayers(World);
						}
					}),
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

	Out.Bonus = GetBonus();
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
}

void FInventoryBonus::ApplyToAllPlayers(UWorld* World)
{
	if (!World)
	{
		return;
	}
	LastAppliedBonus = GetBonus();
	for (TActorIterator<AFGCharacterPlayer> It(World); It; ++It)
	{
		ApplyToPlayer(*It);
	}
}

// ---------------------------------------------------------------------------
// Bonus value (stored in SML's mod config)
// ---------------------------------------------------------------------------

void FInventoryBonus::RegisterConfig(UGameInstance* GameInstance)
{
	if (!GameInstance)
	{
		return;
	}
	if (UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>())
	{
		Manager->RegisterModConfiguration(UInventoryBonusConfig::StaticClass());
		ConfigManager = Manager;
	}
	else
	{
		UE_LOG(LogInventoryBonus, Error, TEXT("SML config manager not found; using default bonus +%d"), DefaultBonus);
	}
}

UConfigPropertyInteger* FInventoryBonus::GetBonusProperty()
{
	UConfigManager* Manager = ConfigManager.Get();
	if (!Manager)
	{
		return nullptr;
	}
	UConfigPropertySection* Root = Manager->GetConfigurationRootSection(UInventoryBonusConfig::GetId());
	if (!Root)
	{
		return nullptr;
	}
	const TObjectPtr<UConfigProperty>* Property = Root->SectionProperties.Find(UInventoryBonusConfig::BonusKey);
	return Property ? Cast<UConfigPropertyInteger>(Property->Get()) : nullptr;
}

int32 FInventoryBonus::GetBonus()
{
	const UConfigPropertyInteger* Property = GetBonusProperty();
	return Property ? FMath::Clamp(Property->Value, 0, MaxBonus) : DefaultBonus;
}

void FInventoryBonus::SetBonus(UWorld* World, int32 NewBonus)
{
	if (UConfigPropertyInteger* Property = GetBonusProperty())
	{
		Property->Value = FMath::Clamp(NewBonus, 0, MaxBonus);
		Property->MarkDirty(); // SML saves the .cfg; the MarkConfigurationDirty hook also resizes players
	}
	else
	{
		UE_LOG(LogInventoryBonus, Warning, TEXT("Config not registered yet; bonus change ignored"));
	}
	ApplyToAllPlayers(World);
}

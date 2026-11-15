#include "InventoryBonusSubsystem.h"

#include "InventoryBonus.h"

#include "Engine/World.h"
#include "Subsystem/SubsystemActorManager.h"

AInventoryBonusSubsystem::AInventoryBonusSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

AInventoryBonusSubsystem* AInventoryBonusSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<AInventoryBonusSubsystem>() : nullptr;
}

AInventoryBonusSubsystem* AInventoryBonusSubsystem::GetOrRegister(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	if (!Manager)
	{
		return nullptr;
	}
	if (AInventoryBonusSubsystem* Existing = Manager->GetSubsystemActor<AInventoryBonusSubsystem>())
	{
		return Existing;
	}
	// Finds the actor loaded from the save by name, or spawns a new one.
	Manager->RegisterSubsystemActor(StaticClass());
	return Manager->GetSubsystemActor<AInventoryBonusSubsystem>();
}

int32 AInventoryBonusSubsystem::GetBonus()
{
	if (Bonus < 0)
	{
		Bonus = FInventoryBonus::GetNewSaveBonus();
	}
	return Bonus;
}

void AInventoryBonusSubsystem::SetBonus(int32 NewBonus)
{
	Bonus = FMath::Clamp(NewBonus, 0, FInventoryBonus::MaxBonus);
}

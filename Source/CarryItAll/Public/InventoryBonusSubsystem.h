#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "InventoryBonusSubsystem.generated.h"

/**
 * Holds this save's bonus and is written into the save file. Server only.
 *
 * Saves without a stored bonus (new saves, and saves from v1.0, which kept the bonus
 * in the global mod config) start with the "new saves" value from the mod config.
 */
UCLASS()
class AInventoryBonusSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AInventoryBonusSubsystem();

	/** The subsystem for this world, or null (main menu, clients, not registered yet). */
	static AInventoryBonusSubsystem* Get(const UObject* WorldContext);
	/** Registers the subsystem, adopting the instance loaded from the save if there is one. Server only. */
	static AInventoryBonusSubsystem* GetOrRegister(UWorld* World);

	/** This save's bonus. The first call on a save without one stores the "new saves" value. */
	int32 GetBonus();
	void SetBonus(int32 NewBonus);

	//~ Begin IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }
	//~ End IFGSaveInterface

private:
	/** -1 until this save gets its first value. */
	UPROPERTY(SaveGame)
	int32 Bonus = -1;
};

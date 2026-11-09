#include "InventoryBonusModule.h"

#include "InventoryBonus.h"

void FInventoryBonusModule::StartupModule()
{
	FInventoryBonus::RegisterHooks();
}

IMPLEMENT_MODULE(FInventoryBonusModule, InventoryBonus)

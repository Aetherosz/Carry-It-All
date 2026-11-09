#include "InventoryBonusConfig.h"

#include "InventoryBonus.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"

#define LOCTEXT_NAMESPACE "InventoryBonus"

const FString UInventoryBonusConfig::BonusKey = TEXT("Bonus");

FConfigId UInventoryBonusConfig::GetId()
{
	FConfigId Id;
	Id.ModReference = TEXT("InventoryBonus");
	Id.ConfigCategory = TEXT("");
	return Id;
}

UInventoryBonusConfig::UInventoryBonusConfig()
{
	ConfigId = GetId();
	DisplayName = LOCTEXT("ConfigName", "Inventory Bonus");
	Description = LOCTEXT("ConfigDescription",
		"Extra inventory slots added on top of your default slots, vanilla upgrades and other mods' upgrades.");

	RootSection = CreateDefaultSubobject<UConfigPropertySection>(TEXT("RootSection"));

	UConfigPropertyInteger* Bonus = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("Bonus"));
	Bonus->DisplayName = LOCTEXT("BonusName", "Bonus inventory slots");
	Bonus->Tooltip = FText::Format(
		LOCTEXT("BonusTooltip",
			"Slots added on top of everything else (0 - {0}).\n"
			"Can be changed from the pause menu while playing.\n"
			"Lowering it never deletes items: slots that still hold items are kept until you empty them."),
		FText::AsNumber(FInventoryBonus::MaxBonus));
	Bonus->DefaultValue = FInventoryBonus::DefaultBonus;
	Bonus->Value = FInventoryBonus::DefaultBonus;
	Bonus->bRequiresWorldReload = false;

	RootSection->SectionProperties.Add(BonusKey, Bonus);
}

#undef LOCTEXT_NAMESPACE

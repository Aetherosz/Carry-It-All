#include "InventoryBonusConfig.h"

#include "InventoryBonus.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"

#define LOCTEXT_NAMESPACE "InventoryBonus"

const FString UInventoryBonusConfig::BreakdownKey = TEXT("Breakdown");
const FString UInventoryBonusConfig::BonusKey = TEXT("Bonus");
const FString UInventoryBonusConfig::AnyPlayerCanChangeKey = TEXT("AnyPlayerCanChange");
const FString UInventoryBonusConfig::NewSaveBonusKey = TEXT("NewSaveBonus");

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
		"Extra inventory slots added on top of your default slots, vanilla upgrades and other mods' upgrades. Each save keeps its own bonus.");

	RootSection = CreateDefaultSubobject<UConfigPropertySection>(TEXT("RootSection"));

	// Label only: its title is rewritten at runtime with the live breakdown.
	UConfigPropertySection* Breakdown = CreateDefaultSubobject<UConfigPropertySection>(TEXT("Breakdown"));
	Breakdown->DisplayName = LOCTEXT("BreakdownMenu", "Load a save to see your inventory breakdown.");
	Breakdown->bAllowUserReset = false;
	RootSection->SectionProperties.Add(BreakdownKey, Breakdown);

	UConfigPropertyInteger* Bonus = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("Bonus"));
	Bonus->DisplayName = LOCTEXT("BonusNameMenu", "Bonus inventory slots (new saves)");
	Bonus->Tooltip = FText::Format(
		LOCTEXT("BonusTooltip",
			"Slots added on top of everything else (0 - {0}).\n"
			"In a save, this changes that save's bonus, and you can change it from the pause menu.\n"
			"In the main menu, it's the bonus new saves start with.\n"
			"Lowering it never deletes items: anything that doesn't fit goes into a crate at your feet."),
		FText::AsNumber(FInventoryBonus::MaxBonus));
	Bonus->DefaultValue = FInventoryBonus::DefaultBonus;
	Bonus->Value = FInventoryBonus::DefaultBonus;
	Bonus->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(BonusKey, Bonus);

	UConfigPropertyBool* AnyPlayer = CreateDefaultSubobject<UConfigPropertyBool>(TEXT("AnyPlayerCanChange"));
	AnyPlayer->DisplayName = LOCTEXT("AnyPlayerName", "Dedicated servers: any player can change the bonus");
	AnyPlayer->Tooltip = LOCTEXT("AnyPlayerTooltip",
		"Only matters on a dedicated server, and only the server's own setting counts.\n"
		"Off: only the server console can use /invbonus to change the bonus.\n"
		"On: every player can.\n"
		"In single player and when hosting, the host can always change it.");
	AnyPlayer->DefaultValue = false;
	AnyPlayer->Value = false;
	AnyPlayer->bRequiresWorldReload = false;
	RootSection->SectionProperties.Add(AnyPlayerCanChangeKey, AnyPlayer);

	UConfigPropertyInteger* NewSaveBonus = CreateDefaultSubobject<UConfigPropertyInteger>(TEXT("NewSaveBonus"));
	NewSaveBonus->DisplayName = LOCTEXT("NewSaveBonusName", "Bonus for new saves");
	NewSaveBonus->DefaultValue = -1;
	NewSaveBonus->Value = -1;
	NewSaveBonus->bHidden = true;
	NewSaveBonus->bAllowUserReset = false;
	RootSection->SectionProperties.Add(NewSaveBonusKey, NewSaveBonus);
}

#undef LOCTEXT_NAMESPACE

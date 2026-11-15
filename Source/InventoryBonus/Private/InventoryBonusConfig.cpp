#include "InventoryBonusConfig.h"

#include "InventoryBonus.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/WidgetExtension/CP_Integer.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"

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

namespace
{
	/** Copy of Native as SML's Blueprint version of its class, or Native itself if there is none. */
	UConfigProperty* ToMenuProperty(UConfigProperty* Native, UObject* Outer)
	{
		const UClass* NativeClass = Native->GetClass();
		const TCHAR* BlueprintName =
			NativeClass == UConfigPropertySection::StaticClass() ? TEXT("BP_ConfigPropertySection") :
			NativeClass == UConfigPropertyInteger::StaticClass() ? TEXT("BP_ConfigPropertyInteger") :
			NativeClass == UConfigPropertyBool::StaticClass() ? TEXT("BP_ConfigPropertyBool") : nullptr;
		if (!BlueprintName)
		{
			return Native; // already a Blueprint class
		}
		UClass* BlueprintClass = LoadClass<UConfigProperty>(nullptr,
			*FString::Printf(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/%s.%s_C"), BlueprintName, BlueprintName));
		if (!BlueprintClass)
		{
			UE_LOG(LogTemp, Error, TEXT("InventoryBonus: %s not found, the config menu will be empty"), BlueprintName);
			return Native;
		}

		// Unique name: the native property may live in the same outer under its name (e.g. RootSection),
		// and UE refuses to replace an object with one of a different class. The name isn't used anyway;
		// sections look properties up by their map key.
		UConfigProperty* Menu = NewObject<UConfigProperty>(Outer, BlueprintClass,
			MakeUniqueObjectName(Outer, BlueprintClass, FName(*(Native->GetName() + TEXT("_Menu")))));
		Menu->DisplayName = Native->DisplayName;
		Menu->Tooltip = Native->Tooltip;
		Menu->bRequiresWorldReload = Native->bRequiresWorldReload;
		Menu->bHidden = Native->bHidden;
		Menu->bAllowUserReset = Native->bAllowUserReset;

		if (const UConfigPropertyInteger* Int = Cast<UConfigPropertyInteger>(Native))
		{
			UConfigPropertyInteger* MenuInt = CastChecked<UConfigPropertyInteger>(Menu);
			MenuInt->DefaultValue = Int->DefaultValue;
			MenuInt->Value = Int->Value;
			if (UCP_Integer* Widget = Cast<UCP_Integer>(MenuInt))
			{
				// Drag or type. SML's default is a text box with min = max = 0.
				Widget->WidgetType = ECP_IntegerWidgetType::CPI_Spinbox;
				Widget->MinValue = 0;
				Widget->MaxValue = FInventoryBonus::MaxBonus;
			}
		}
		else if (const UConfigPropertyBool* Bool = Cast<UConfigPropertyBool>(Native))
		{
			UConfigPropertyBool* MenuBool = CastChecked<UConfigPropertyBool>(Menu);
			MenuBool->DefaultValue = Bool->DefaultValue;
			MenuBool->Value = Bool->Value;
		}
		else if (const UConfigPropertySection* Section = Cast<UConfigPropertySection>(Native))
		{
			UConfigPropertySection* MenuSection = CastChecked<UConfigPropertySection>(Menu);
			if (UCP_Section* Widget = Cast<UCP_Section>(MenuSection))
			{
				// SML's default is one horizontal row with a scroll bar. An empty section (the
				// breakdown label) shows its text as a header.
				Widget->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
				Widget->HasHeader = Section->SectionProperties.IsEmpty();
				Widget->HeaderText = Native->DisplayName;
			}
			for (const TPair<FString, TObjectPtr<UConfigProperty>>& Child : Section->SectionProperties)
			{
				MenuSection->SectionProperties.Add(Child.Key, Child.Value ? ToMenuProperty(Child.Value, MenuSection) : nullptr);
			}
		}
		return Menu;
	}
}

void UInventoryBonusConfig::UseMenuWidgets()
{
	UInventoryBonusConfig* Default = GetMutableDefault<UInventoryBonusConfig>();
	if (Default->RootSection)
	{
		Default->RootSection = CastChecked<UConfigPropertySection>(ToMenuProperty(Default->RootSection, Default));
	}
}

#undef LOCTEXT_NAMESPACE

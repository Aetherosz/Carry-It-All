#include "InventoryBonusCommand.h"

#include "InventoryBonus.h"

#include "Command/CommandSender.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"

namespace
{
	AFGCharacterPlayer* FindPlayerFor(UCommandSender* Sender)
	{
		if (Sender->IsPlayerSender())
		{
			if (AFGPlayerController* Controller = Sender->GetPlayer())
			{
				if (AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Controller->GetPawn()))
				{
					return Player;
				}
			}
		}
		// Console / server sender: show the first player we can find.
		TActorIterator<AFGCharacterPlayer> It(Sender->GetWorld());
		return It ? *It : nullptr;
	}

	void PrintBreakdown(UCommandSender* Sender, AFGCharacterPlayer* Player)
	{
		const FInventorySlotBreakdown B = FInventoryBonus::GetBreakdown(Player);
		Sender->SendChatMessage(FString::Printf(
			TEXT("Inventory: %d default + %d vanilla upgrades + %d mod upgrades + %d bonus = %d slots"),
			B.Default, B.VanillaUpgrades, B.ModUpgrades, B.Bonus, B.Wanted));
		if (B.HeldByItems > 0)
		{
			Sender->SendChatMessage(FString::Printf(
				TEXT("Kept %d extra slot(s) because they still hold items. Empty your last slots, then run /invbonus again."),
				B.HeldByItems), FLinearColor::Yellow);
		}
	}
}

AInventoryBonusCommand::AInventoryBonusCommand()
{
	CommandName = TEXT("invbonus");
	Aliases.Add(TEXT("inventorybonus"));
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = false;
	Usage = NSLOCTEXT("InventoryBonus", "Usage",
		"/invbonus [amount | +amount | -amount] - show or change your bonus inventory slots");
}

EExecutionStatus AInventoryBonusCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	if (Arguments.Num() == 0)
	{
		PrintBreakdown(Sender, FindPlayerFor(Sender));
		return EExecutionStatus::COMPLETED;
	}

	// In multiplayer only the host may change it (it affects everyone).
	if (Sender->IsPlayerSender())
	{
		const AFGPlayerController* Controller = Sender->GetPlayer();
		if (!Controller || !Controller->IsLocalController())
		{
			Sender->SendChatMessage(TEXT("Only the host can change the inventory bonus."), FLinearColor::Red);
			return EExecutionStatus::INSUFFICIENT_PERMISSIONS;
		}
	}

	FString Arg = Arguments[0];
	Arg.TrimStartAndEndInline();
	const bool bRelative = Arg.StartsWith(TEXT("+")) || Arg.StartsWith(TEXT("-"));
	if (!Arg.IsNumeric())
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	const int32 Value = FCString::Atoi(*Arg);
	const int32 NewBonus = bRelative ? FInventoryBonus::GetBonus() + Value : Value;
	FInventoryBonus::SetBonus(Sender->GetWorld(), NewBonus);

	Sender->SendChatMessage(FString::Printf(TEXT("Inventory bonus set to +%d."), FInventoryBonus::GetBonus()));
	PrintBreakdown(Sender, FindPlayerFor(Sender));
	return EExecutionStatus::COMPLETED;
}

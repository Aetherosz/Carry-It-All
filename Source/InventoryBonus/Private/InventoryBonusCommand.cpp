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
		Sender->SendChatMessage(FInventoryBonus::DescribeSlots(B, B.Bonus, B.Wanted));
		if (B.HeldByItems > 0)
		{
			Sender->SendChatMessage(FString::Printf(
				TEXT("Kept %d extra slot(s) because they still hold items. Run /invbonus with the bonus you want to move them into a crate."),
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

	// It affects everyone: the host, the server console, or (dedicated servers, if allowed) any player.
	FString Reason;
	if (!FInventoryBonus::CanChangeBonus(Sender, Reason))
	{
		Sender->SendChatMessage(Reason, FLinearColor::Red);
		return EExecutionStatus::INSUFFICIENT_PERMISSIONS;
	}

	FString Arg = Arguments[0];
	Arg.TrimStartAndEndInline();
	const bool bRelative = Arg.StartsWith(TEXT("+")) || Arg.StartsWith(TEXT("-"));
	if (!Arg.IsNumeric())
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}

	UWorld* World = Sender->GetWorld();
	const int32 Value = FCString::Atoi(*Arg);
	const int32 NewBonus = bRelative ? FInventoryBonus::GetBonus(World) + Value : Value;
	const FBonusChangeResult Result = FInventoryBonus::SetBonus(World, NewBonus);

	Sender->SendChatMessage(FString::Printf(TEXT("Inventory bonus for this save set to +%d."), FInventoryBonus::GetBonus(World)));
	if (Result.PlayersWithCrate > 0)
	{
		Sender->SendChatMessage(FString::Printf(
			TEXT("%d item(s) didn't fit and were put in a crate at the feet of %d player(s)."),
			Result.ItemsInCrates, Result.PlayersWithCrate), FLinearColor::Yellow);
	}
	PrintBreakdown(Sender, FindPlayerFor(Sender));
	return EExecutionStatus::COMPLETED;
}

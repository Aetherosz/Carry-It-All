#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "InventoryBonusCommand.generated.h"

/**
 * /invbonus           show where your slots come from
 * /invbonus 500       set the bonus to +500
 * /invbonus +200      add 200 to the bonus
 * /invbonus -200      remove 200 from the bonus
 */
UCLASS()
class AInventoryBonusCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	AInventoryBonusCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

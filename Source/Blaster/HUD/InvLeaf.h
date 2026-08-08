#pragma once

#include "CoreMinimal.h"
#include "InvCompositeBase.h"
#include "InvLeaf.generated.h"

/**
 *
 */
	UCLASS()
	class BLASTER_API UInvLeaf : public UInvCompositeBase
{
	GENERATED_BODY()
public:
	virtual void ApplyFunction(FuncType Function) override;
};
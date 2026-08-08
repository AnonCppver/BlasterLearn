// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InvCompositeBase.h"
#include "InvComposite.generated.h"

/**
 *
 */
	UCLASS()
	class BLASTER_API UInvComposite : public UInvCompositeBase
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual void ApplyFunction(FuncType Function) override;
	virtual void Collapse() override;
	TArray<UInvCompositeBase*> GetChildren() { return Children; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UInvCompositeBase>> Children;
};
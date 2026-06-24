// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InvComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UInvComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInvComponent();

protected:
	virtual void BeginPlay() override;

private:
	void ConstructInventory();
};

// Fill out your copyright notice in the Description page of Project Settings.


#include "InvItemComponent.h"
#include "Net/UnrealNetwork.h"

UInvItemComponent::UInvItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PickupMessage = FString("E - 拾起");
	SetIsReplicatedByDefault(true);
}

void UInvItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
}

void UInvItemComponent::InitItemManifest(FInvItemManifest CopyOfManifest)
{
	ItemManifest = CopyOfManifest;
}

void UInvItemComponent::PickedUp()
{
	OnPickedUp();
	UE_LOG(LogTemp, Warning, TEXT("invitemcomponent::pickup"));
	GetOwner()->Destroy();
}
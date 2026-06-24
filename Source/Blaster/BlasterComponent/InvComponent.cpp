#include "InvComponent.h"

UInvComponent::UInvComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UInvComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInvComponent::ConstructInventory()
{
	// Implementation for constructing the inventory
}


// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterOverlay.h"
#include "Blaster/BlasterComponent/InvComponent.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"

void UCharacterOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ABlasterPlayerController* PlayerController = Cast< ABlasterPlayerController>(GetOwningPlayer());
	if (!PlayerController)return;

	UInvComponent* InventoryComponent = PlayerController->FindComponentByClass<UInvComponent>();

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->NoRoomInInventory.AddDynamic(this, &UCharacterOverlay::OnNoRoom);
	}

	UE_LOG(LogTemp, Warning, TEXT("Add InfoMessage Delegate"));
}

void UCharacterOverlay::OnNoRoom()
{
	if (!IsValid(InfoMessage)) return;
	InfoMessage->SetMessage(TEXT("背包空间不足"));
}

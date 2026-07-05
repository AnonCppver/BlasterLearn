// Fill out your copyright notice in the Description page of Project Settings.


#include "InvUtils.h"

int32 UInvUtils::GetIndexFromPosition(const FIntPoint& Position, int32 Col)
{
	return Position.Y * Col + Position.X;
}

FIntPoint UInvUtils::GetPositionFromIndex(int32 Index, int32 Col)
{
	return FIntPoint{ Index % Col,Index / Col };
}

UInvComponent* UInvUtils::GetInventoryComponent(const APlayerController* PlayerController)
{
	if (!IsValid(PlayerController)) return nullptr;
	UInvComponent* InventoryComponent = PlayerController->FindComponentByClass<UInvComponent>();
	return InventoryComponent;
}

EInvItemCategory UInvUtils::GetItemCategoryFromItemComp(UInvItemComponent* ItemComp)
{
	if (!IsValid(ItemComp)) return EInvItemCategory::None;
	return ItemComp->GetItemManifest().GetItemCategory();
}


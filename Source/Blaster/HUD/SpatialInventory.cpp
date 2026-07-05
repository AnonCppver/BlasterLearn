// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialInventory.h"

#include "Blaster/Item/InvItemManifest.h"
#include "Blaster/HUD/InvGrid.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"

FInvSlotAvailabilityResult USpatialInventory::HasRoomForItem(const UInvItemComponent* ItemComponent) const
{
	if (!IsValid(ItemComponent))return {};
	ItemComponent->GetItemManifest().GetItemCategory();

	return InvGrid->HasRoomForItem(ItemComponent);
}
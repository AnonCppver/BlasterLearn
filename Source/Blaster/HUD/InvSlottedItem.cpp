// Fill out your copyright notice in the Description page of Project Settings.


#include "InvSlottedItem.h"
#include "Blaster/HUD/InvItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

FReply UInvSlottedItem::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	OnSlottedItemClicked.Broadcast(GridIndex, MouseEvent);
	return FReply::Handled();
}

void UInvSlottedItem::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//UInv_InventoryStatics::ItemHovered(GetOwningPlayer(), InventoryItem.Get());
}

void UInvSlottedItem::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	//UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
}

void UInvSlottedItem::SetInventoryItem(UInvItem* Item)
{
	InventoryItem = Item;
}

void UInvSlottedItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);
}

void UInvSlottedItem::UpdateStackCount(int32 StackCount)
{
	if (StackCount > 0)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
		Text_StackCount->SetText(FText::AsNumber(StackCount));
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}
// Fill out your copyright notice in the Description page of Project Settings.


#include "HoverItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blaster/HUD/InvItem.h"

void UHoverItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);
}

void UHoverItem::UpdateStackCount(const int32 Count)
{
	StackCount = Count;
	if (Count > 0)
	{
		Text_StackCount->SetText(FText::AsNumber(Count));
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FGameplayTag UHoverItem::GetItemType() const
{
	if (InventoryItem.IsValid())
	{
		return InventoryItem->GetItemManifest().GetItemType();
	}
	return FGameplayTag();
}

void UHoverItem::SetIsStackable(bool bStacks)
{
	bIsStackable = bStacks;
	if (!bStacks)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UInvItem* UHoverItem::GetInventoryItem() const
{
	return InventoryItem.Get();
}

void UHoverItem::SetInventoryItem(UInvItem* Item)
{
	InventoryItem = Item;
}
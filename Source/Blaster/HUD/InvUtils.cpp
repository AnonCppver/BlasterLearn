// Fill out your copyright notice in the Description page of Project Settings.


#include "InvUtils.h"
#include "Blueprint/SlateBlueprintLibrary.h"

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

void UInvUtils::ItemHovered(APlayerController* PC, UInvItem* Item)
{
	UInvComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return;

	UInventoryBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return;

	if (InventoryBase->HasHoverItem()) return;

	InventoryBase->OnItemHovered(Item);
}

void UInvUtils::ItemUnhovered(APlayerController* PC)
{
	UInvComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return;

	UInventoryBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return;

	InventoryBase->OnItemUnHovered();
}

UHoverItem* UInvUtils::GetHoverItem(APlayerController* PC)
{
	UInvComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return nullptr;

	UInventoryBase* InventoryBase = IC->GetInventoryMenu();
	if (!IsValid(InventoryBase)) return nullptr;

	return InventoryBase->GetHoverItem();
}

UInventoryBase* UInvUtils::GetInventoryWidget(APlayerController* PC)
{
	UInvComponent* IC = GetInventoryComponent(PC);
	if (!IsValid(IC)) return nullptr;

	return IC->GetInventoryMenu();
}

FVector2D UInvUtils::GetWidgetPosition(UWidget* Widget)
{
	const FGeometry Geometry = Widget->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewportPosition;
	USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, USlateBlueprintLibrary::GetLocalTopLeft(Geometry), PixelPosition, ViewportPosition);
	return ViewportPosition;
}

FVector2D UInvUtils::GetWidgetSize(UWidget* Widget)
{
	const FGeometry Geometry = Widget->GetCachedGeometry();
	return Geometry.GetLocalSize();
}

bool UInvUtils::IsWithinBounds(const FVector2D& BoundaryPos, const FVector2D& WidgetSize, const FVector2D& MousePos)
{
	return MousePos.X >= BoundaryPos.X && MousePos.X <= (BoundaryPos.X + WidgetSize.X) &&
		MousePos.Y >= BoundaryPos.Y && MousePos.Y <= (BoundaryPos.Y + WidgetSize.Y);
}

FVector2D UInvUtils::GetClampedWidgetPosition(const FVector2D& Boundary, const FVector2D& WidgetSize, const FVector2D& MousePos)
{
	FVector2D ClampedPosition = MousePos;

	// Adjust horizontal position to ensure that the widget stays within the boundary
	if (MousePos.X + WidgetSize.X > Boundary.X) // Widget exceeds the right edge
	{
		ClampedPosition.X = Boundary.X - WidgetSize.X;
	}
	if (MousePos.X < 0.f) // Widget exceeds the left edge
	{
		ClampedPosition.X = 0.f;
	}

	// Adjust vertical position to ensure that the widget stays within the boundary
	if (MousePos.Y + WidgetSize.Y > Boundary.Y) // Widget exceeds the bottom edge
	{
		ClampedPosition.Y = Boundary.Y - WidgetSize.Y;
	}
	if (MousePos.Y < 0.f) // Widget exceeds the top edge
	{
		ClampedPosition.Y = 0.f;
	}

	return ClampedPosition;
}

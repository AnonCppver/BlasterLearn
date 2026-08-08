// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialInventory.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include "InvItemDescription.h"
#include "InvUtils.h"
#include "Blaster/Item/InvItemManifest.h"
#include "Blaster/HUD/InvGrid.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"

void USpatialInventory::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(InvGrid))
	{
		InvGrid->HideCursor();
		InvGrid->ShowCursor();
		InvGrid->SetOwningCanvas(CanvasPanel);
	}
}

void USpatialInventory::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(ItemDescription)) return;
	SetItemDescriptionSizeAndPosition(ItemDescription, CanvasPanel);
}

void USpatialInventory::SetItemDescriptionSizeAndPosition(UInvItemDescription* Description, UCanvasPanel* Canvas) const
{
	UCanvasPanelSlot* ItemDescriptionCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(Description);
	if (!IsValid(ItemDescriptionCPS)) return;

	const FVector2D ItemDescriptionSize = Description->GetBoxSize();
	ItemDescriptionCPS->SetSize(ItemDescriptionSize);

	FVector2D ClampedPosition = UInvUtils::GetClampedWidgetPosition(
		UInvUtils::GetWidgetSize(Canvas),
		ItemDescriptionSize,
		UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer()));

	ItemDescriptionCPS->SetPosition(ClampedPosition);
}

FReply USpatialInventory::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	InvGrid->DropItem();
	return FReply::Handled();
}

FInvSlotAvailabilityResult USpatialInventory::HasRoomForItem(const UInvItemComponent* ItemComponent) const
{
	if (!IsValid(ItemComponent))return {};
	ItemComponent->GetItemManifest().GetItemCategory();

	return InvGrid->HasRoomForItem(ItemComponent);
}

void USpatialInventory::OnItemHovered(UInvItem* Item)
{
	const auto& Manifest = Item->GetItemManifest();
	UInvItemDescription* DescriptionWidget = GetItemDescription();
	DescriptionWidget->SetVisibility(ESlateVisibility::Collapsed);

	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);

	FTimerDelegate DescriptionTimerDelegate;
	DescriptionTimerDelegate.BindLambda([this, Item, &Manifest, DescriptionWidget]()
		{
			GetItemDescription()->SetVisibility(ESlateVisibility::HitTestInvisible);
			Manifest.AssimilateInventoryFragments(DescriptionWidget);
		});

	GetOwningPlayer()->GetWorldTimerManager().SetTimer(DescriptionTimer, DescriptionTimerDelegate, DescriptionTimerDelay, false);
}

void USpatialInventory::OnItemUnHovered()
{
	GetItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
}

bool USpatialInventory::HasHoverItem() const
{
	if (InvGrid->HasHoverItem()) return true;

	return false;
}

UHoverItem* USpatialInventory::GetHoverItem() const
{
	if (!IsValid(InvGrid)) return nullptr;
	return InvGrid->GetHoverItem();
}

float USpatialInventory::GetTileSize() const
{
	return 0.f;
}

UInvItemDescription* USpatialInventory::GetItemDescription()
{
	if (!IsValid(ItemDescription))
	{
		ItemDescription = CreateWidget<UInvItemDescription>(GetOwningPlayer(), ItemDescriptionClass);
		CanvasPanel->AddChild(ItemDescription);
	}
	return ItemDescription;
}
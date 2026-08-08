// Fill out your copyright notice in the Description page of Project Settings.


#include "InvGridSlot.h"
#include "Components/Image.h"
#include "Blaster/HUD/InvItemPopUp.h"

void UInvGridSlot::SetOccupiedTexture()
{
	GridSlotState = EInvGridSlotState::Occupied;
	SlotImage->SetBrush(Brush_Occupied);
}

void UInvGridSlot::SetUnoccupiedTexture()
{
	GridSlotState = EInvGridSlotState::Unoccupied;
	SlotImage->SetBrush(Brush_Unoccupied);
}

void UInvGridSlot::SetSelectedTexture()
{
	GridSlotState = EInvGridSlotState::Selected;
	SlotImage->SetBrush(Brush_Selected);
}

void UInvGridSlot::SetGrayedOutTexture()
{
	GridSlotState = EInvGridSlotState::GrayedOut;
	SlotImage->SetBrush(Brush_GrayedOut);
}

void UInvGridSlot::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	GridSlotHovered.Broadcast(TileIndex, MouseEvent);
}

void UInvGridSlot::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	GridSlotUnhovered.Broadcast(TileIndex, MouseEvent);
}

FReply UInvGridSlot::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	GridSlotClicked.Broadcast(TileIndex, MouseEvent);
	return FReply::Handled();
}

void UInvGridSlot::SetItemPopUp(UInvItemPopUp* PopUp)
{
	ItemPopUp = PopUp;
	ItemPopUp->SetGridIndex(TileIndex);
	ItemPopUp->OnNativeDestruct.AddUObject(this, &UInvGridSlot::OnItemPopUpDestruct);
}

UInvItemPopUp* UInvGridSlot::GetItemPopUp() const
{
	return ItemPopUp.Get();
}

void UInvGridSlot::OnItemPopUpDestruct(UUserWidget* Menu)
{
	ItemPopUp.Reset();
}


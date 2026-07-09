// Fill out your copyright notice in the Description page of Project Settings.


#include "InvGridSlot.h"
#include "Components/Image.h"

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


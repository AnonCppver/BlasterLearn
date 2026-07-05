// Fill out your copyright notice in the Description page of Project Settings.


#include "InvGridSlot.h"
#include "Components/Image.h"

void UInvGridSlot::SetImageOpacityUnoccupied()
{
	GridSlotState = EInvGridSlotState::Unoccupied;
	SlotImage->SetRenderOpacity(OpacityUnoccupied);
}

void UInvGridSlot::SetImageOpacityOccupied()
{
	GridSlotState = EInvGridSlotState::Occupied;
	SlotImage->SetRenderOpacity(OpacityOccupied);
}

void UInvGridSlot::SetImageOpacitySelected()
{
	GridSlotState = EInvGridSlotState::Selected;
	SlotImage->SetRenderOpacity(OpacitySelected);
}

void UInvGridSlot::SetImageOpacityGrayedOut()
{
	GridSlotState = EInvGridSlotState::GrayedOut;
	SlotImage->SetRenderOpacity(OpacityGrayedOut);
}


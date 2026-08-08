// Fill out your copyright notice in the Description page of Project Settings.


#include "InvItem.h"
#include "Net/UnrealNetWork.h"

void UInvItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
}

void UInvItem::SetItemManifest(const FInvItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FInvItemManifest>(Manifest);
}

bool UInvItem::IsStackable() const
{
	const FInvStackableFragment* StackableFragment = GetItemManifest().GetFragmentOfType<FInvStackableFragment>();
	return StackableFragment != nullptr;
}	

bool UInvItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInvItemCategory::Consumable;
}


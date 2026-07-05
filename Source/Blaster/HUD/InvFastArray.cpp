#include "InvFastArray.h"

#include "Blaster/BlasterComponent/InvComponent.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"
#include "InvItem.h"


TArray<UInvItem*> FInvFastArray::GetAllItems() const
{
	TArray<UInvItem*> Results;
	Results.Reserve(Entries.Num());
	for (const auto& Entry : Entries)
	{
		if (!IsValid(Entry.Item)) continue;
		Results.Add(Entry.Item);
	}
	return Results;
}

void FInvFastArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	UInvComponent* IC = Cast<UInvComponent>(OwnerComponent);
	if (!IsValid(IC)) return;

	for (int32 Index : RemovedIndices)
	{
		IC->OnItemRemoved.Broadcast(Entries[Index].Item);
	}
}

void FInvFastArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	UInvComponent* IC = Cast<UInvComponent>(OwnerComponent);
	if (!IsValid(IC)) return;

	for (int32 Index : AddedIndices)
	{
		IC->OnItemAdded.Broadcast(Entries[Index].Item);
	}
}

UInvItem* FInvFastArray::AddEntry(UInvItemComponent* ItemComponent)
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());
	UInvComponent* IC = Cast<UInvComponent>(OwnerComponent);
	if (!IsValid(IC)) return nullptr;

	FInvEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = ItemComponent->GetItemManifest().Manifest(OwningActor);

	IC->AddRepSubobj(NewEntry.Item);
	MarkItemDirty(NewEntry);

	return NewEntry.Item;
}

UInvItem* FInvFastArray::AddEntry(UInvItem* Item)
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	FInvEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;

	MarkItemDirty(NewEntry);
	return Item;
}

void FInvFastArray::RemoveEntry(UInvItem* Item)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FInvEntry& Entry = *EntryIt;
		if (Entry.Item == Item)
		{
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
		}
	}
}

UInvItem* FInvFastArray::FindFirstItemByType(const FGameplayTag& ItemType)
{
	auto* FoundItem = Entries.FindByPredicate([&ItemType](const FInvEntry& Entry)
		{
			return IsValid(Entry.Item) && Entry.Item->GetItemManifest().GetItemType().MatchesTagExact(ItemType);
		});
	return FoundItem ? FoundItem->Item : nullptr;
}
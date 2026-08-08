
#include "InvItemManifest.h"

#include "Blaster/HUD/InvItem.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"
#include "InvFragment.h"
#include "Blaster/HUD/InvCompositeBase.h"

UInvItem* FInvItemManifest::Manifest(UObject* NewOuter)
{
	UInvItem* Item = NewObject<UInvItem>(NewOuter, UInvItem::StaticClass());
	Item->SetItemManifest(*this);
	
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();

	return Item;
}

void FInvItemManifest::AssimilateInventoryFragments(UInvCompositeBase* Composite) const
{
	const auto& InventoryItemFragments = GetAllFragmentsOfType<FInvAssimilateFragment>();
	for (const auto* Fragment : InventoryItemFragments)
	{
		Composite->ApplyFunction([Fragment](UInvCompositeBase* Widget)
			{
				Fragment->Assimilate(Widget);
			});
	}
}

void FInvItemManifest::SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject)) return;

	AActor* SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
	if (!IsValid(SpawnedActor)) return;

	// 复制ItemManifest
	UInvItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UInvItemComponent>();
	check(ItemComp);

	ItemComp->InitItemManifest(*this);
}

void FInvItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{
		Fragment.Reset();
	}
	Fragments.Empty();
}
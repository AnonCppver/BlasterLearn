
#include "InvItemManifest.h"

#include "Blaster/HUD/InvItem.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"
//#include "InvItemFragment.h"
//#include "Widgets/Composite/InvCompositeBase.h"

UInvItem* FInvItemManifest::Manifest(UObject* NewOuter)
{
	UInvItem* Item = NewObject<UInvItem>(NewOuter, UInvItem::StaticClass());
	Item->SetItemManifest(*this);
	/*
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();*/

	return Item;
}

//void FInvItemManifest::AssimilateInventoryFragments(UInvCompositeBase* Composite) const
//{
//	const auto& InventoryItemFragments = GetAllFragmentsOfType<FInvItemFragment>();
//	for (const auto* Fragment : InventoryItemFragments)
//	{
//		Composite->ApplyFunction([Fragment](UInvCompositeBase* Widget)
//			{
//				Fragment->Assimilate(Widget);
//			});
//	}
//}
//
//void FInvItemManifest::SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation)
//{
//	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject)) return;
//
//	AActor* SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
//	if (!IsValid(SpawnedActor)) return;
//
//	// Set the item manifest, item category, item type, etc.
//	UInvComponent* ItemComp = SpawnedActor->FindComponentByClass<UInvItemComponent>();
//	check(ItemComp);
//
//	ItemComp->InitItemManifest(*this);
//}
//
//void FInvItemManifest::ClearFragments()
//{
//	for (auto& Fragment : Fragments)
//	{
//		Fragment.Reset();
//	}
//	Fragments.Empty();
//}
#pragma once

#include "CoreMinimal.h"
#include "Blaster/BlasterTypes/InvTypes.h"
#include "Blaster/Item/InvFragment.h"
#include "InvFragment.h"
#include "InstancedStruct.h"
#include "GameplayTagContainer.h"

#include "InvItemManifest.generated.h"

/**
 * The Item Manifest contains all of the necessary data
 * for creating a new Inventory Item
 */

class UInvItem;
class UInvCompositeBase;

USTRUCT(BlueprintType)
struct BLASTER_API FInvItemManifest
{
	GENERATED_BODY()

	TArray<TInstancedStruct<FInvFragment>>& GetFragmentsMutable() { return Fragments; }
	UInvItem* Manifest(UObject* NewOuter);
	EInvItemCategory GetItemCategory() const { return ItemCategory; }
	FGameplayTag GetItemType() const { return ItemType; }
//	void AssimilateInventoryFragments(UInvCompositeBase* Composite) const;
//
	template<typename T> requires std::derived_from<T, FInvFragment>
	const T* GetFragmentOfTypeWithTag(const FGameplayTag& FragmentTag) const;

	template<typename T> requires std::derived_from<T, FInvFragment>
	const T* GetFragmentOfType() const;

	template<typename T> requires std::derived_from<T, FInvFragment>
	T* GetFragmentOfTypeMutable();

	template<typename t> requires std::derived_from<t, FInvFragment>
	TArray<const t*> GetAllFragmentsOfType() const;

//	void SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation);

private:

	// 仅允许添加派生类
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FInvFragment>> Fragments;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	EInvItemCategory ItemCategory{ EInvItemCategory::None };

	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "GameItems"))
	FGameplayTag ItemType;

//	UPROPERTY(EditAnywhere, Category = "Inventory")
//	TSubclassOf<AActor> PickupActorClass;
//
	void ClearFragments();
};


template<typename T>
	requires std::derived_from<T, FInvFragment>
const T* FInvItemManifest::GetFragmentOfTypeWithTag(const FGameplayTag& FragmentTag) const
{
	for (const TInstancedStruct<FInvFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			if (!FragmentPtr->GetFragmentTag().MatchesTagExact(FragmentTag)) continue;
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <typename T> requires std::derived_from<T, FInvFragment>
const T* FInvItemManifest::GetFragmentOfType() const
{
	for (const TInstancedStruct<FInvFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <typename T> requires std::derived_from<T, FInvFragment>
T* FInvItemManifest::GetFragmentOfTypeMutable()
{
	for (TInstancedStruct<FInvFragment>& Fragment : Fragments)
	{
		if (T* FragmentPtr = Fragment.GetMutablePtr<T>())
		{
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <typename T> requires std::derived_from<T, FInvFragment>
TArray<const T*> FInvItemManifest::GetAllFragmentsOfType() const
{
	TArray<const T*> Result;
	for (const TInstancedStruct<FInvFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			Result.Add(FragmentPtr);
		}
	}
	return Result;
}
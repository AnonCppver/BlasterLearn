// 用于存储背包物品的信息，不是物品本身

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Blaster/Item/InvItemManifest.h"
#include "Blaster/Item/InvFragment.h"
#include "InvItem.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API UInvItem : public UObject
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override { return true; }

	void SetItemManifest(const FInvItemManifest& Manifest);
	const FInvItemManifest& GetItemManifest() const { return ItemManifest.Get<FInvItemManifest>(); }
	FInvItemManifest& GetItemManifestMutable() { return ItemManifest.GetMutable<FInvItemManifest>(); }
	bool IsStackable() const;
	bool IsConsumable() const;
	int32 GetTotalStackCount() const { return TotalStackCount; }
	void SetTotalStackCount(int32 Count) { TotalStackCount = Count; }
private:

	UPROPERTY(VisibleAnywhere, meta = (BaseStruct = "/Script/Blaster.InvItemManifest"), Replicated)
	FInstancedStruct ItemManifest;

	UPROPERTY(Replicated)
	int32 TotalStackCount{ 0 };
	
};

template <typename FragmentType>
const FragmentType* GetFragment(const UInvItem* Item, const FGameplayTag& Tag)
{
	if (!IsValid(Item)) return nullptr;

	const FInvItemManifest& Manifest = Item->GetItemManifest();
	return Manifest.GetFragmentOfTypeWithTag<FragmentType>(Tag);
}

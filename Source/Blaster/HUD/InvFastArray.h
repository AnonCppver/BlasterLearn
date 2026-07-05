// 高效的增量网络同步容器

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "InvFastArray.generated.h"

struct FGameplayTag;
class UInvItem;
class UInvComponent;
class UInvItemComponent;
struct FInvFastArray;

USTRUCT(BlueprintType)
struct FInvEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	friend UInvComponent;
	friend FInvFastArray;
	FInvEntry() {};

private:
	UPROPERTY()
	TObjectPtr<UInvItem>Item = nullptr;
};

USTRUCT(BlueprintType)
struct FInvFastArray : public FFastArraySerializer
{
	GENERATED_BODY()
	friend UInvComponent;

	FInvFastArray():OwnerComponent(nullptr){};
	FInvFastArray(UActorComponent* InOwnerComponent) :OwnerComponent(InOwnerComponent) {};

	TArray<UInvItem*> GetAllItems() const;

	// FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	// End of FFastArraySerializer contract

	UInvItem* AddEntry(UInvItemComponent* ItemComponent);
	UInvItem* AddEntry(UInvItem* Item);
	void RemoveEntry(UInvItem* Item);
	UInvItem* FindFirstItemByType(const FGameplayTag& ItemType);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<FInvEntry, FInvFastArray>(Entries, DeltaParams, *this);
	}

private:
	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent>OwnerComponent;

	UPROPERTY()
	TArray<FInvEntry>Entries;
};

template<>
struct TStructOpsTypeTraits<FInvFastArray> : public TStructOpsTypeTraitsBase2<FInvFastArray>
{
	enum { WithNetDeltaSerializer = true };
};
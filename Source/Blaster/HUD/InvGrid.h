// 背包的仓库部分

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/HUD/InvItem.h"
#include "InvGrid.generated.h"

class UInvGridSlot;
class UCanvasPanel;
class UInvComponent;
class UInvItemComponent;
class UInvItem;
class UInvSlottedItem;
struct FInvGridFragment;
struct FInvImageFragment;
struct FInvItemManifest;
/**
 * 
 */
UCLASS()
class BLASTER_API UInvGrid : public UUserWidget
{
	GENERATED_BODY()
	
private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "Inventory")
	EInvItemCategory ItemCategory = EInvItemCategory::Consumable;

	UPROPERTY()
	TArray<TObjectPtr<UInvGridSlot>> GridSlots;

	UPROPERTY()
	TMap<int32, TObjectPtr<UInvSlottedItem>> SlottedItems;

	TWeakObjectPtr<UInvComponent> InvComponent;

	// classes
	UPROPERTY(EditAnywhere, Category = "Grids")
	TSubclassOf<UInvGridSlot> GridSlotClass;

	UPROPERTY(EditAnywhere, Category = "Grids")
	TSubclassOf<UInvSlottedItem> SlottedItemClass;

	// widgets
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	// grid size
	UPROPERTY(EditAnywhere, Category="Grids")
	int32 Row;
	UPROPERTY(EditAnywhere, Category="Grids")
	int32 Col;
	UPROPERTY(EditAnywhere, Category = "Grids")
	float Size;

	void AddItemToIndices(const FInvSlotAvailabilityResult& Result, UInvItem* NewItem);
	FVector2D GetDrawSize(const FInvGridFragment* GridFragment);
	void SetSlottedItemImage(const UInvSlottedItem* SlottedItem, const FInvGridFragment* GridFragment, const FInvImageFragment* ImageFragment);
	void AddItemAtIndex(UInvItem* NewItem, int32 Index, bool bStackable, int32 StackAmount = 0);
	void AddSlottedItemToCanvas(int32 Index, const FInvGridFragment* GridFragment,UInvSlottedItem* SlottedItem);
	void UpdateGridSlots(UInvItem* NewItem, int32 Index, bool bStackable, int32 StackAmount = 0);
protected:
	virtual void NativeOnInitialized() override;
public:
	EInvItemCategory GetItemCategory() const { return ItemCategory; }
	FORCEINLINE bool MatchesCategory(UInvItem* Item) { return Item->GetItemManifest().GetItemCategory() == ItemCategory; }
	void ConstructGrid();
	UFUNCTION()
	void AddItem(UInvItem* Item);


	FInvSlotAvailabilityResult HasRoomForItem(const UInvItemComponent* ItemComponent);
	FInvSlotAvailabilityResult HasRoomForItem(const UInvItem* Item);
	FInvSlotAvailabilityResult HasRoomForItem(const FInvItemManifest& Manifest);
	bool HasRoomAtIndex(const UInvGridSlot* GridSlot,
		const FIntPoint& Dimensions,
		const TSet<int32>& CheckedIndices,
		TSet<int32>& OutTentativelyClaimed,
		const FGameplayTag& ItemType,
		const int32 MaxStackSize);
	bool CheckSlotConstraints(const UInvGridSlot* GridSlot,
		const UInvGridSlot* SubGridSlot,
		const TSet<int32>& CheckedIndices,
		TSet<int32>& OutTentativelyClaimed,
		const FGameplayTag& ItemType,
		const int32 MaxStackSize) const;
	FIntPoint GetItemDimensions(const FInvItemManifest& Manifest) const;
	bool IsUpperLeftSlot(const UInvGridSlot* GridSlot, const UInvGridSlot* SubGridSlot) const;
	bool DoesItemTypeMatch(const UInvItem* SubItem, const FGameplayTag& ItemType) const;
	bool IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const;
	int32 GetStackAmount(const UInvGridSlot* GridSlot) const;
	int32 DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill, const UInvGridSlot* GridSlot) const;

};

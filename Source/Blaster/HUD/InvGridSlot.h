// 背包的仓库的一个单元格

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/HUD/InvItem.h"
#include "InvGridSlot.generated.h"

class UImage;

UENUM(BlueprintType)
enum class EInvGridSlotState : uint8
{
	Unoccupied,
	Occupied,
	Selected,
	GrayedOut
};

/**
 * 
 */
UCLASS()
class BLASTER_API UInvGridSlot : public UUserWidget
{
	GENERATED_BODY()
	
private:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> SlotImage;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float OpacityUnoccupied;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float OpacityOccupied;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float OpacitySelected;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float OpacityGrayedOut;

	EInvGridSlotState GridSlotState;

	int32 TileIndex;
	int32 StackCount;
	int32 FirstGridIndex{ INDEX_NONE };
	bool bAvailable{ true };

	TWeakObjectPtr<UInvItem> InventoryItem;


public:
	FORCEINLINE void SetTileIndex(int32 Index) { TileIndex = Index; }
	FORCEINLINE int32 GetTileIndex() const { return TileIndex; }
	FORCEINLINE EInvGridSlotState GetGridSlotState() const{ return GridSlotState; }
	FORCEINLINE TWeakObjectPtr<UInvItem> GetInventoryItem() const { return InventoryItem; }
	FORCEINLINE void SetInventoryItem(UInvItem* Item) { InventoryItem = Item; }
	FORCEINLINE int32 GetStackCount() const { return StackCount; }
	FORCEINLINE void SetStackCount(int32 Count) { StackCount = Count; }
	FORCEINLINE int32 GetFirstGridIndex() const { return FirstGridIndex; }
	FORCEINLINE void SetFirstGridIndex(int32 Index) { FirstGridIndex = Index; }
	FORCEINLINE bool IsAvailable() const { return bAvailable; }
	FORCEINLINE void SetAvailable(bool bIsAvailable) { bAvailable = bIsAvailable; }

	void SetImageOpacityUnoccupied();
	void SetImageOpacityOccupied();
	void SetImageOpacitySelected();
	void SetImageOpacityGrayedOut();
};

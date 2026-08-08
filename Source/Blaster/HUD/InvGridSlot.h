// 背包的仓库的一个单元格

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/HUD/InvItem.h"
#include "InvGridSlot.generated.h"

class UImage;
class UInvItemPopUp;

UENUM(BlueprintType)
enum class EInvGridSlotState : uint8
{
	Unoccupied,
	Occupied,
	Selected,
	GrayedOut
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGridSlotEvent, int32, GridIndex, const FPointerEvent&, MouseEvent);

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
	FSlateBrush Brush_Unoccupied;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_Occupied;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_Selected;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_GrayedOut;

	EInvGridSlotState GridSlotState;

	int32 TileIndex;
	int32 StackCount;
	int32 FirstGridIndex{ INDEX_NONE };
	bool bAvailable{ true };

	TWeakObjectPtr<UInvItem> InventoryItem;
	TWeakObjectPtr<UInvItemPopUp> ItemPopUp;

	UFUNCTION()
	void OnItemPopUpDestruct(UUserWidget* Menu);
public:
	virtual void NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

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
	void SetItemPopUp(UInvItemPopUp* PopUp);
	UInvItemPopUp* GetItemPopUp() const;

	void SetOccupiedTexture();
	void SetUnoccupiedTexture();
	void SetSelectedTexture();
	void SetGrayedOutTexture();

	FGridSlotEvent GridSlotClicked;
	FGridSlotEvent GridSlotHovered;
	FGridSlotEvent GridSlotUnhovered;
};

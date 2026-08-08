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
class UHoverItem;
class UInvItemPopUp;
struct FInvGridFragment;
struct FInvImageFragment;
struct FInvItemManifest;
enum class EInvGridSlotState : uint8;
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
	TWeakObjectPtr<UCanvasPanel> OwningCanvasPanel;

	// classes
	UPROPERTY(EditAnywhere, Category = "Grids")
	TSubclassOf<UInvGridSlot> GridSlotClass;

	UPROPERTY(EditAnywhere, Category = "Grids")
	TSubclassOf<UInvSlottedItem> SlottedItemClass;

	UPROPERTY(EditAnywhere, Category = "Grids")
	TSubclassOf<UHoverItem> HoverItemClass;

	UPROPERTY()
	TObjectPtr<UHoverItem> HoverItem;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInvItemPopUp> ItemPopUpClass;

	UPROPERTY()
	TObjectPtr<UInvItemPopUp> ItemPopUp;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FVector2D ItemPopUpOffset{ 20.f, 20.f };

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UUserWidget> VisibleCursorWidgetClass;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UUserWidget> HiddenCursorWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> VisibleCursorWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> HiddenCursorWidget;

	// 光标位置
	FInvTileParameters TileParameters;
	FInvTileParameters LastTileParameters;

	// 当拥有物品的鼠标点击在有效位置时，物品将被放置的索引
	int32 ItemDropIndex{ INDEX_NONE };
	// 以起始位置为基准的查询结果，用于范围高亮和占用判定
	FInvSpaceQueryResult CurrentQueryResult;
	bool bMouseWithinCanvas;
	bool bLastMouseWithinCanvas;
	int32 LastHighlightedIndex;
	FIntPoint LastHighlightedDimensions;

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
	// 拿起的物品hover相关操作
	bool IsRightClick(const FPointerEvent& MouseEvent) const;
	bool IsLeftClick(const FPointerEvent& MouseEvent) const;
	void PickUp(UInvItem* ClickedInventoryItem, const int32 GridIndex);
	void AssignHoverItem(UInvItem* InventoryItem, const int32 GridIndex, const int32 PreviousGridIndex);
	void AssignHoverItem(UInvItem* InventoryItem);
	void RemoveItemFromGrid(UInvItem* InventoryItem, const int32 GridIndex);
	void UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition);
	FIntPoint CalculateHoveredCoordinates(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	EInvTileQuadrant CalculateTileQuadrant(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	void OnTileParametersUpdated(const FInvTileParameters& Parameters);
	FIntPoint CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EInvTileQuadrant Quadrant) const;
	FInvSpaceQueryResult CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions);
	bool CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location);
	void HighlightSlots(const int32 Index, const FIntPoint& Dimensions);
	void UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions);
	void ChangeHoverType(const int32 Index, const FIntPoint& Dimensions, EInvGridSlotState GridSlotState);
	// 拿起的物品放下相关操作
	void PutDownOnIndex(const int32 Index);
	void ClearHoverItem();
	UUserWidget* GetVisibleCursorWidget();
	UUserWidget* GetHiddenCursorWidget();
	bool IsSameStackable(const UInvItem* ClickedInventoryItem) const;
	void SwapWithHoverItem(UInvItem* ClickedInventoryItem, const int32 GridIndex);
	bool ShouldSwapStackCounts(const int32 RoomInClickedSlot, const int32 HoveredStackCount, const int32 MaxStackSize) const;
	void SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldConsumeHoverItemStacks(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const;
	void ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldFillInStack(const int32 RoomInClickedSlot, const int32 HoveredStackCount) const;
	void FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index);

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

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
public:
	EInvItemCategory GetItemCategory() const { return ItemCategory; }
	FORCEINLINE bool MatchesCategory(UInvItem* Item) { return Item->GetItemManifest().GetItemCategory() == ItemCategory; }
	void ConstructGrid();
	void ShowCursor();
	void HideCursor();
	void SetOwningCanvas(UCanvasPanel* OwningCanvas);
	void DropItem();
	bool HasHoverItem() const;
	UHoverItem* GetHoverItem() const;

	UFUNCTION()
	void AddItem(UInvItem* Item);

	UFUNCTION()
	void AddStacks(const FInvSlotAvailabilityResult& Result);

	UFUNCTION()
	void OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent);

	UFUNCTION()
	void CreateItemPopUp(const int32 GridIndex);

	UFUNCTION()
	void OnPopUpMenuSplit(int32 SplitAmount, int32 Index);

	UFUNCTION()
	void OnPopUpMenuDrop(int32 Index);

	UFUNCTION()
	void OnPopUpMenuConsume(int32 Index);

	UFUNCTION()
	void OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent);

	// 鼠标悬停在格子上时的回调函数
	UFUNCTION()
	void OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent);

	UFUNCTION()
	void OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent);


	FInvSlotAvailabilityResult HasRoomForItem(const UInvItemComponent* ItemComponent);
	FInvSlotAvailabilityResult HasRoomForItem(const UInvItem* Item);
	FInvSlotAvailabilityResult HasRoomForItem(const FInvItemManifest& Manifest);
};

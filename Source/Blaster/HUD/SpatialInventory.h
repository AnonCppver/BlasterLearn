// 背包总体

#pragma once

#include "CoreMinimal.h"
#include "InventoryBase.h"
#include "SpatialInventory.generated.h"

struct FGameplayTag;
class UInvItemDescription;
class UInvGrid;
class UWidgetSwitcher;
class UButton;
class UCanvasPanel;
class UHoverItem;

/**
 * 
 */
UCLASS()
class BLASTER_API USpatialInventory : public UInventoryBase
{
	GENERATED_BODY()

public:
	virtual FInvSlotAvailabilityResult HasRoomForItem(const UInvItemComponent* ItemComponent) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnItemHovered(UInvItem* Item) override;
	virtual void OnItemUnHovered() override;
	virtual bool HasHoverItem() const override;
	virtual UHoverItem* GetHoverItem() const override;
	virtual float GetTileSize() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInvGrid> InvGrid;

	/*ItemDescription*/
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInvItemDescription> ItemDescriptionClass;

	UPROPERTY()
	TObjectPtr<UInvItemDescription> ItemDescription;

	FTimerHandle DescriptionTimer;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DescriptionTimerDelay = 0.5f;

	UInvItemDescription* GetItemDescription();

	void SetItemDescriptionSizeAndPosition(UInvItemDescription* Description, UCanvasPanel* Canvas) const;
};

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
class UInvHoverItem;

/**
 * 
 */
UCLASS()
class BLASTER_API USpatialInventory : public UInventoryBase
{
	GENERATED_BODY()

public:
	virtual FInvSlotAvailabilityResult HasRoomForItem(const UInvItemComponent* ItemComponent) const override;
	
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInvGrid> InvGrid;

};

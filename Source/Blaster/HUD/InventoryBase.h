#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/BlasterTypes/InvTypes.h"
#include "InventoryBase.generated.h"

class UInvItemComponent;
class UInvItem;
//class UInvHoverItem;

UCLASS()
class BLASTER_API UInventoryBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual FInvSlotAvailabilityResult HasRoomForItem(const UInvItemComponent* ItemComponent) const { return FInvSlotAvailabilityResult(); }
	virtual void OnItemHovered(UInvItem* Item) {}
	virtual void OnItemUnHovered() {}
	virtual bool HasHoverItem() const { return false; }
	//virtual UInvHoverItem* GetHoverItem() const { return nullptr; }
	virtual float GetTileSize() const { return 0.f; }
};
// controller进行背包交互的入口

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/HUD/InventoryBase.h"
#include "Blaster/HUD/InvFastArray.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "InvComponent.generated.h"

class UInvItemComponent;
class UInvItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryItemChange, UInvItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStackChange, const FInvSlotAvailabilityResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemEquipStatusChanged, UInvItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryMenuToggled, bool, bOpen);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class BLASTER_API UInvComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInvComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void TryAddItem(UInvItemComponent* ItemComponent);

	UFUNCTION(Server, Reliable)
	void Server_AddNewItem(UInvItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItem(UInvItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(UInvItem* Item, int32 StackCount);

	void SpawnDroppedItem(UInvItem* Item, int32 StackCount);

	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UInvItem* Item);

	void ToggleMenu();

	void AddRepSubobj(UObject* Subobj);

	UInventoryBase* GetInventoryMenu() const { return InventoryMenu; }

	FInventoryItemChange OnItemAdded;
	FInventoryItemChange OnItemRemoved;
	FNoRoomInInventory NoRoomInInventory;
	FStackChange OnStackChanged;
	FItemEquipStatusChanged OnItemEquipped;
	FItemEquipStatusChanged OnItemUnequipped;
	FInventoryMenuToggled OnInventoryMenuToggled;

protected:
	virtual void BeginPlay() override;

private:
	void ConstructInventory();

	TWeakObjectPtr<ABlasterPlayerController> OwningController;

	// Menu
	UPROPERTY()
	TObjectPtr<UInventoryBase> InventoryMenu;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInventoryBase> InventoryMenuClass;

	bool bIsMenuOpen = false;
	void OpenMenu();
	void CloseMenu();

	// Items
	UPROPERTY(Replicated)
	FInvFastArray InventoryList;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMin = -85.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMax = 85.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMin = 10.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMax = 50.f;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float RelativeSpawnElevation = 70.f;

};

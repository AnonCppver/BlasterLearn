#include "InvComponent.h"

#include "Blaster/HUD/InventoryBase.h"
#include "InvItemComponent.h"
#include "Blaster/HUD/InvItem.h"
#include "Net/UnrealNetwork.h"

UInvComponent::UInvComponent():InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UInvComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

void UInvComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
	CloseMenu();
}

void UInvComponent::ConstructInventory()
{
	OwningController = Cast<ABlasterPlayerController>(GetOwner());

	if (!OwningController.IsValid() || !OwningController->IsLocalController()|| !InventoryMenuClass|| InventoryMenu)
	{
		return;
	}

	InventoryMenu = CreateWidget<UInventoryBase>(
		OwningController.Get(),
		InventoryMenuClass
	);

	if (!InventoryMenu)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Failed to create InventoryMenu from class: %s"),
			*GetNameSafe(InventoryMenuClass)
		);
		return;
	}

	InventoryMenu->AddToViewport();
}

void UInvComponent::OpenMenu()
{
	if (!IsValid(InventoryMenu))return;

	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bIsMenuOpen = true;

	if (!OwningController.IsValid())return;

	FInputModeGameAndUI InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->bShowMouseCursor = true;
}

void UInvComponent::CloseMenu()
{
	if (!IsValid(InventoryMenu))return;

	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	bIsMenuOpen = false;

	if (!OwningController.IsValid())return;

	FInputModeGameOnly InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->bShowMouseCursor = false;
}

void UInvComponent::ToggleMenu()
{
	if (bIsMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UInvComponent::AddRepSubobj(UObject* Subobj)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(Subobj))
	{
		AddReplicatedSubObject(Subobj);
	}
}

void UInvComponent::TryAddItem(UInvItemComponent* ItemComponent)
{
	FInvSlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(ItemComponent);

	UInvItem* FoundItem = InventoryList.FindFirstItemByType(ItemComponent->GetItemManifest().GetItemType());
	Result.Item = FoundItem;

	if (Result.TotalRoomToFill == 0)
	{
		NoRoomInInventory.Broadcast();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("remainder %d"), Result.Remainder);
	if (Result.Item.IsValid() && Result.bStackable)
	{
		OnStackChanged.Broadcast(Result);
		Server_AddStacksToItem(ItemComponent, Result.TotalRoomToFill, Result.Remainder);
	}
	else if (Result.TotalRoomToFill > 0)
	{
		Server_AddNewItem(ItemComponent, Result.bStackable ? Result.TotalRoomToFill : 0, Result.Remainder);
	}
}

void UInvComponent::Server_AddNewItem_Implementation(UInvItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	UInvItem* NewItem = InventoryList.AddEntry(ItemComponent);
	NewItem->SetTotalStackCount(StackCount);

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}
	
	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInvStackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInvStackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInvComponent::Server_AddStacksToItem_Implementation(UInvItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	const FGameplayTag& ItemType = IsValid(ItemComponent) ? ItemComponent->GetItemManifest().GetItemType() : FGameplayTag::EmptyTag;
	UInvItem* Item = InventoryList.FindFirstItemByType(ItemType);
	if (!IsValid(Item)) return;

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInvStackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInvStackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInvComponent::Server_DropItem_Implementation(UInvItem* Item, int32 StackCount)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - StackCount;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	SpawnDroppedItem(Item, StackCount);
}

void UInvComponent::SpawnDroppedItem(UInvItem* Item, int32 StackCount)
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotatedForward = OwningPawn->GetActorForwardVector();
	RotatedForward = RotatedForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotatedForward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
	SpawnLocation.Z -= RelativeSpawnElevation;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FInvItemManifest& ItemManifest = Item->GetItemManifestMutable();
	if (FInvStackableFragment* StackableFragment = ItemManifest.GetFragmentOfTypeMutable<FInvStackableFragment>())
	{
		StackableFragment->SetStackCount(StackCount);
	}
	ItemManifest.SpawnPickupActor(this, SpawnLocation, SpawnRotation);
}

void UInvComponent::Server_ConsumeItem_Implementation(UInvItem* Item)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - 1;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	if (FInvConsumableFragment* ConsumableFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInvConsumableFragment>())
	{
		ConsumableFragment->OnConsume(OwningController.Get());
	}
}

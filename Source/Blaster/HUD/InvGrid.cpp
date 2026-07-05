#include "InvGrid.h"
#include "InvGridSlot.h"
#include "InvUtils.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h" 
#include "Blueprint/WidgetLayoutLibrary.h"

#include "Blaster/HUD/InvItem.h"
#include "Blaster/HUD/InvSlottedItem.h"
#include "Blaster/Item/InvItemManifest.h"
#include "Blaster/Item/InvFragment.h"
#include "Blaster/BlasterTypes/InvFragmentTag.h"
#include "Blaster/BlasterComponent/InvComponent.h"
#include "Blaster/BlasterComponent/InvItemComponent.h"


void UInvGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	ConstructGrid();

	APlayerController* PC = GetOwningPlayer();
	if (IsValid(PC))
	{
		InvComponent=PC->FindComponentByClass< UInvComponent>();
	}
	if (InvComponent.IsValid())
	{
		InvComponent->OnItemAdded.AddDynamic(this, &UInvGrid::AddItem);
	}
}

void UInvGrid::ConstructGrid()
{
	GridSlots.Reserve(Row * Col);
	
	for(int32 i=0; i < Row; ++i)
	{
		for(int32 j=0; j < Col; ++j)
		{
			UInvGridSlot* GridSlot = CreateWidget<UInvGridSlot>(this, GridSlotClass);

			GridSlot->SetTileIndex(UInvUtils::GetIndexFromPosition(FIntPoint(j, i), Col));
			CanvasPanel->AddChild(GridSlot);

			UCanvasPanelSlot* GridCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(GridSlot);
			if(GridCPS)
			{
				FVector2D Position(j * Size, i * Size);
				FVector2D SlotSize(Size, Size);
				GridCPS->SetPosition(Position);
				GridCPS->SetSize(SlotSize);
			}

			GridSlots.Add(GridSlot);
		}
	}
}

void UInvGrid::AddItem(UInvItem* Item)
{
	if (!MatchesCategory(Item)) return;

	FInvSlotAvailabilityResult Result = HasRoomForItem(Item);
	AddItemToIndices(Result, Item);
	UE_LOG(LogTemp, Warning, TEXT("InvGrid::AddItem "));
}

void UInvGrid::AddItemToIndices(const FInvSlotAvailabilityResult& Result,UInvItem* NewItem)
{
	for (const auto& Availability : Result.SlotAvailabilities)
	{
		AddItemAtIndex(NewItem, Availability.Index, Result.bStackable, Availability.AmountToFill);
		UpdateGridSlots(NewItem, Availability.Index, Result.bStackable, Availability.AmountToFill);
	}
}
// 界面上添加物品
void UInvGrid::AddItemAtIndex(UInvItem* NewItem, int32 Index, bool bStackable, int32 StackAmount)
{
	const FInvGridFragment* GridFragment = GetFragment<FInvGridFragment>(NewItem, FragmentTags::GridFragment);
	const FInvImageFragment* ImageFragment = GetFragment<FInvImageFragment>(NewItem, FragmentTags::IconFragment);

	if (!GridFragment || !ImageFragment)return;

	UInvSlottedItem* SlottedItem = CreateWidget<UInvSlottedItem>(GetOwningPlayer(), SlottedItemClass);
	SlottedItem->SetInventoryItem(NewItem);
	SetSlottedItemImage(SlottedItem, GridFragment, ImageFragment);
	SlottedItem->SetGridIndex(Index);
	SlottedItem->SetIsStackable(bStackable);
	int32 ST = bStackable ? StackAmount : 0;
	SlottedItem->UpdateStackCount(ST);
	AddSlottedItemToCanvas(Index, GridFragment, SlottedItem);

	SlottedItems.Add(Index, SlottedItem);
}
// 更改相关格子的状态
void UInvGrid::UpdateGridSlots(UInvItem* NewItem, int32 Index, bool bStackable, int32 StackAmount)
{
	check(GridSlots.IsValidIndex(Index));

	if (bStackable)
	{
		GridSlots[Index]->SetStackCount(StackAmount);

	}
	const FInvGridFragment* GridFragment = GetFragment<FInvGridFragment>(NewItem, FragmentTags::GridFragment);
	FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);

	UInvUtils::ForEach2D(GridSlots,Index,Dimensions,Col,[&](UInvGridSlot* GridSlot)
	{
			GridSlot->SetInventoryItem(NewItem);
			GridSlot->SetFirstGridIndex(Index);
			GridSlot->SetAvailable(false);
			GridSlot->SetImageOpacityUnoccupied();
		});
}

FVector2D UInvGrid::GetDrawSize(const FInvGridFragment* GridFragment)
{
	const float IconTileWidth = Size - GridFragment->GetGridPadding() * 2;
	return GridFragment->GetGridSize() * IconTileWidth;
}

void UInvGrid::SetSlottedItemImage(const UInvSlottedItem* SlottedItem, const FInvGridFragment* GridFragment, const FInvImageFragment* ImageFragment)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = GetDrawSize(GridFragment);

	SlottedItem->SetImageBrush(Brush);
}

void UInvGrid::AddSlottedItemToCanvas(int32 Index, const FInvGridFragment* GridFragment, UInvSlottedItem* SlottedItem)
{
	CanvasPanel->AddChild(SlottedItem);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(SlottedItem);
	CanvasSlot->SetSize(GetDrawSize(GridFragment));
	FVector2D DrawPos=UInvUtils::GetPositionFromIndex(Index, Col)* Size;
	FVector2D DrawPosWithPadding = DrawPos + FVector2D(GridFragment->GetGridPadding());
	CanvasSlot->SetPosition(DrawPosWithPadding);
}

FInvSlotAvailabilityResult UInvGrid::HasRoomForItem(const UInvItemComponent* ItemComponent)
{
	return HasRoomForItem(ItemComponent->GetItemManifest());
}

FInvSlotAvailabilityResult UInvGrid::HasRoomForItem(const UInvItem* Item)
{
	return HasRoomForItem(Item->GetItemManifest());
} 

FInvSlotAvailabilityResult UInvGrid::HasRoomForItem(const FInvItemManifest& Manifest)
{
	FInvSlotAvailabilityResult Result{};
	
	const FInvStackableFragment* StackableFragment = Manifest.GetFragmentOfType<FInvStackableFragment>();
	Result.bStackable = StackableFragment != nullptr;

	int32 AmountToFill = Result.bStackable ? StackableFragment->GetStackCount() : 1;
	const int32 MaxStackSize = StackableFragment ? StackableFragment->GetMaxStackSize() : 1;

	TSet<int32> CheckedIndices;
	for(const UInvGridSlot* GridSlot : GridSlots)
	{
		if (AmountToFill == 0) break;

		if(CheckedIndices.Contains(GridSlot->GetTileIndex())) continue;

		FIntPoint Dimensions=GetItemDimensions(Manifest);

		if (!IsInGridBounds(GridSlot->GetTileIndex(), Dimensions)) continue;

		TSet<int32> TentativelyClaimed;
		if (!HasRoomAtIndex(GridSlot, Dimensions, CheckedIndices, TentativelyClaimed, Manifest.GetItemType(), MaxStackSize))
		{
			continue;
		}

		int32 AmountToFillForThisSlot = DetermineFillAmountForSlot(Result.bStackable, MaxStackSize, AmountToFill, GridSlot);
		if (AmountToFillForThisSlot == 0)continue;

		CheckedIndices.Append(TentativelyClaimed);

		// Update the amount left to fill
		Result.TotalRoomToFill += AmountToFillForThisSlot;
		bool HasValidItem = GridSlot->GetInventoryItem().IsValid();
		Result.SlotAvailabilities.Emplace(
			FInvSlotAvailability{
				HasValidItem ? GridSlot->GetFirstGridIndex() : GridSlot->GetTileIndex(),
				Result.bStackable ? AmountToFillForThisSlot : 0,
				HasValidItem
			}
		);

		AmountToFill -= AmountToFillForThisSlot;

		// How much is the Remainder?
		Result.Remainder = AmountToFill;

		if (AmountToFill == 0) return Result;
	}

	return Result;
}

bool UInvGrid::HasRoomAtIndex(const UInvGridSlot* GridSlot,
	const FIntPoint& Dimensions,
	const TSet<int32>& CheckedIndices,
	TSet<int32>& OutTentativelyClaimed,
	const FGameplayTag& ItemType,
	const int32 MaxStackSize)
{
	// Is there room at this index? (i.e. are there other items in the way?)
	bool bHasRoomAtIndex = true;
	UInvUtils::ForEach2D(GridSlots, GridSlot->GetTileIndex(), Dimensions, Col, [&](const UInvGridSlot* SubGridSlot)
		{
			if (CheckSlotConstraints(GridSlot, SubGridSlot, CheckedIndices, OutTentativelyClaimed, ItemType, MaxStackSize))
			{
				OutTentativelyClaimed.Add(SubGridSlot->GetTileIndex());
			}
			else
			{
				bHasRoomAtIndex = false;
			}
		});

	return bHasRoomAtIndex;
}

bool UInvGrid::CheckSlotConstraints(const UInvGridSlot* GridSlot,
	const UInvGridSlot* SubGridSlot,
	const TSet<int32>& CheckedIndices,
	TSet<int32>& OutTentativelyClaimed,
	const FGameplayTag& ItemType,
	const int32 MaxStackSize) const
{
	if (CheckedIndices.Contains(GridSlot->GetTileIndex())) return false;

	// Has valid item?
	if (!SubGridSlot->GetInventoryItem().IsValid())
	{
		OutTentativelyClaimed.Add(SubGridSlot->GetTileIndex());
		return true;
	}

	// Is this Grid Slot an upper left slot?
	if (!IsUpperLeftSlot(GridSlot, SubGridSlot)) return false;

	// If so, is this a stackable item?
	const UInvItem* SubItem = SubGridSlot->GetInventoryItem().Get();
	if (!SubItem->IsStackable()) return false;

	// Is this item the same type as the item we're trying to add?
	if (!DoesItemTypeMatch(SubItem, ItemType)) return false;

	// If stackable, is this slot at the max stack size already?
	if (GridSlot->GetStackCount() >= MaxStackSize) return false;

	return true;
}

FIntPoint UInvGrid::GetItemDimensions(const FInvItemManifest& Manifest) const
{
	const FInvGridFragment* GridFragment = Manifest.GetFragmentOfType<FInvGridFragment>();
	return GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
}

bool UInvGrid::IsUpperLeftSlot(const UInvGridSlot* GridSlot, const UInvGridSlot* SubGridSlot) const
{
	return SubGridSlot->GetFirstGridIndex() == GridSlot->GetTileIndex();
}

bool UInvGrid::DoesItemTypeMatch(const UInvItem* SubItem, const FGameplayTag& ItemType) const
{
	return SubItem->GetItemManifest().GetItemType().MatchesTagExact(ItemType);
}

bool UInvGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num()) return false;
	const int32 EndColumn = (StartIndex % Col) + ItemDimensions.X;
	const int32 EndRow = (StartIndex / Col) + ItemDimensions.Y;
	return EndColumn <= Col && EndRow <= Row;
}

int32 UInvGrid::GetStackAmount(const UInvGridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	// If we are at a slot that doesn't hold the stack count. we must get the actual stack count.
	if (const int32 UpperLeftIndex = GridSlot->GetFirstGridIndex(); UpperLeftIndex != INDEX_NONE)
	{
		UInvGridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftGridSlot->GetStackCount();
	}
	return CurrentSlotStackCount;
}

int32 UInvGrid::DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill, const UInvGridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackSize - GetStackAmount(GridSlot);
	return bStackable ? FMath::Min(AmountToFill, RoomInSlot) : 1;
}
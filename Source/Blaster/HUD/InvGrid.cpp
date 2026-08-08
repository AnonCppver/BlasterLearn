#include "InvGrid.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h" 
#include "Blueprint/WidgetLayoutLibrary.h"

#include "InvUtils.h"
#include "InvGridSlot.h"

#include "Blaster/HUD/InvSlottedItem.h"
#include "Blaster/HUD/HoverItem.h"
#include "Blaster/HUD/InvItemPopUp.h"
#include "Blaster/Item/InvItemManifest.h"
#include "Blaster/Item/InvFragment.h"
#include "Blaster/BlasterTypes/InvTypes.h"
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
		InvComponent->OnStackChanged.AddDynamic(this, &ThisClass::AddStacks);
		
	}
}

void UInvGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2D CanvasPosition = UInvUtils::GetWidgetPosition(CanvasPanel);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	if (CursorExitedCanvas(CanvasPosition, UInvUtils::GetWidgetSize(CanvasPanel), MousePosition))
	{
		return;
	}

	UpdateTileParameters(CanvasPosition, MousePosition);
}

void UInvGrid::UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition)
{
	if (!bMouseWithinCanvas) return;

	const FIntPoint HoveredTileCoordinates = CalculateHoveredCoordinates(CanvasPosition, MousePosition);

	LastTileParameters = TileParameters;
	TileParameters.TileCoordinats = HoveredTileCoordinates;
	TileParameters.TileIndex = UInvUtils::GetIndexFromPosition(HoveredTileCoordinates, Col);
	TileParameters.TileQuadrant = CalculateTileQuadrant(CanvasPosition, MousePosition);

	OnTileParametersUpdated(TileParameters);
}

void UInvGrid::OnTileParametersUpdated(const FInvTileParameters& Parameters)
{
	if (!IsValid(HoverItem)) return;

	const FIntPoint Dimensions = HoverItem->GetGridDimensions();

	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinats, Dimensions, Parameters.TileQuadrant);
	ItemDropIndex = UInvUtils::GetIndexFromPosition(StartingCoordinate, Col);

	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);

	if (CurrentQueryResult.bHasSpace)
	{
		HighlightSlots(ItemDropIndex, Dimensions);
		return;
	}
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	
	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		const FInvGridFragment* GridFragment = GetFragment<FInvGridFragment>(CurrentQueryResult.ValidItem.Get(), FragmentTags::GridFragment);
		if (!GridFragment) return;

		ChangeHoverType(CurrentQueryResult.UpperLeftIndex, GridFragment->GetGridSize(), EInvGridSlotState::GrayedOut);
	}
}

FIntPoint UInvGrid::CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EInvTileQuadrant Quadrant) const
{
	// 光标为Item的中心点
	// 当边长格子数为偶数时，光标所处格子不能被放置在中心位置，而是需要根据象限偏移一格
	// 奇数边时，光标所处格子可以被放置在中心位置，无需偏移
	const int32 HasEvenWidth = Dimensions.X % 2 == 0 ? 1 : 0;
	const int32 HasEvenHeight = Dimensions.Y % 2 == 0 ? 1 : 0;

	FIntPoint StartingCoord;
	switch (Quadrant)
	{
	case EInvTileQuadrant::TopLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EInvTileQuadrant::TopRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EInvTileQuadrant::BottomLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	case EInvTileQuadrant::BottomRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("Invalid Quadrant."));
		return FIntPoint(-1, -1);
	}
	return StartingCoord;
}
// 光标所在的格子坐标
FIntPoint UInvGrid::CalculateHoveredCoordinates(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const
{
	return FIntPoint{
		static_cast<int32>(FMath::FloorToInt((MousePosition.X - CanvasPosition.X) / Size)),
		static_cast<int32>(FMath::FloorToInt((MousePosition.Y - CanvasPosition.Y) / Size))
	};
}

EInvTileQuadrant UInvGrid::CalculateTileQuadrant(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const
{
	// 光标在一个格子的相对坐标
	const float TileLocalX = FMath::Fmod(MousePosition.X - CanvasPosition.X, Size);
	const float TileLocalY = FMath::Fmod(MousePosition.Y - CanvasPosition.Y, Size);

	const bool bIsTop = TileLocalY < Size / 2.f;
	const bool bIsLeft = TileLocalX < Size / 2.f;

	EInvTileQuadrant HoveredTileQuadrant{ EInvTileQuadrant::None };
	if (bIsTop && bIsLeft) HoveredTileQuadrant = EInvTileQuadrant::TopLeft;
	else if (bIsTop && !bIsLeft) HoveredTileQuadrant = EInvTileQuadrant::TopRight;
	else if (!bIsTop && bIsLeft) HoveredTileQuadrant = EInvTileQuadrant::BottomLeft;
	else if (!bIsTop && !bIsLeft) HoveredTileQuadrant = EInvTileQuadrant::BottomRight;

	return HoveredTileQuadrant;
}

FInvSpaceQueryResult UInvGrid::CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions)
{
	FInvSpaceQueryResult Result;

	// 超出边界
	if (!IsInGridBounds(UInvUtils::GetIndexFromPosition(Position, Col), Dimensions)) return Result;

	Result.bHasSpace = true;

	// 从起始位置统计范围内占用的物品堆数
	TSet<int32> OccupiedUpperLeftIndices;
	UInvUtils::ForEach2D(GridSlots, UInvUtils::GetIndexFromPosition(Position, Col), Dimensions, Col, [&](const UInvGridSlot* GridSlot)
		{
			if (GridSlot->GetInventoryItem().IsValid())
			{
				OccupiedUpperLeftIndices.Add(GridSlot->GetFirstGridIndex());
				Result.bHasSpace = false;
			}
		});

	// 如果只有一堆，则可以交换/合并
	if (OccupiedUpperLeftIndices.Num() == 1)
	{
		const int32 Index = *OccupiedUpperLeftIndices.CreateConstIterator();
		Result.ValidItem = GridSlots[Index]->GetInventoryItem();
		Result.UpperLeftIndex = GridSlots[Index]->GetFirstGridIndex();
	}

	return Result;
}

bool UInvGrid::CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location)
{
	bLastMouseWithinCanvas = bMouseWithinCanvas;
	bMouseWithinCanvas = UInvUtils::IsWithinBounds(BoundaryPos, BoundarySize, Location);
	if (!bMouseWithinCanvas && bLastMouseWithinCanvas)// 光标从canvas内移出
	{
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
		return true;
	}
	return false;
}

void UInvGrid::HighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	if (!bMouseWithinCanvas) return;
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UInvUtils::ForEach2D(GridSlots, Index, Dimensions, Col, [&](UInvGridSlot* GridSlot)
		{
			GridSlot->SetOccupiedTexture();
		});
	LastHighlightedDimensions = Dimensions;
	LastHighlightedIndex = Index;
}

void UInvGrid::UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	UInvUtils::ForEach2D(GridSlots, Index, Dimensions, Col, [&](UInvGridSlot* GridSlot)
		{
			if (GridSlot->IsAvailable())
			{
				GridSlot->SetUnoccupiedTexture();
			}
			else
			{
				GridSlot->SetOccupiedTexture();
			}
		});
}

void UInvGrid::ChangeHoverType(const int32 Index, const FIntPoint& Dimensions, EInvGridSlotState GridSlotState)
{
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UInvUtils::ForEach2D(GridSlots, Index, Dimensions, Col, [State = GridSlotState](UInvGridSlot* GridSlot)
		{
			switch (State)
			{
			case EInvGridSlotState::Occupied:
				GridSlot->SetOccupiedTexture();
				break;
			case EInvGridSlotState::Unoccupied:
				GridSlot->SetUnoccupiedTexture();
				break;
			case EInvGridSlotState::GrayedOut:
				GridSlot->SetGrayedOutTexture();
				break;
			case EInvGridSlotState::Selected:
				GridSlot->SetSelectedTexture();
				break;
			}
		});

	LastHighlightedIndex = Index;
	LastHighlightedDimensions = Dimensions;
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
			GridSlot->GridSlotClicked.AddDynamic(this, &ThisClass::OnGridSlotClicked);
			GridSlot->GridSlotHovered.AddDynamic(this, &ThisClass::OnGridSlotHovered);
			GridSlot->GridSlotUnhovered.AddDynamic(this, &ThisClass::OnGridSlotUnhovered);
		}
	}
}

void UInvGrid::OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent) 
{
	if (!IsValid(HoverItem)) return;
	if (!GridSlots.IsValidIndex(ItemDropIndex)) return;
	// 如果点击位置有物品，那么捡起来再放下
	// 想要放置的起始目标可能与占用这个区域的物品的起始位置不同，所以需要检查点击位置的物品的起始位置
	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		OnSlottedItemClicked(CurrentQueryResult.UpperLeftIndex, MouseEvent);
		return;
	}

	if (!IsInGridBounds(ItemDropIndex, HoverItem->GetGridDimensions())) return;
	auto GridSlot = GridSlots[ItemDropIndex];
	if (CurrentQueryResult.bHasSpace)
	{
		PutDownOnIndex(ItemDropIndex);
	}
}

void UInvGrid::PutDownOnIndex(const int32 Index)
{
	AddItemAtIndex(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	UpdateGridSlots(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	ClearHoverItem();
}

void UInvGrid::ClearHoverItem()
{
	if (!IsValid(HoverItem)) return;

	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetIsStackable(false);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->UpdateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());

	HoverItem->RemoveFromParent();
	HoverItem = nullptr;

	ShowCursor();
}

UUserWidget* UInvGrid::GetVisibleCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(VisibleCursorWidget))
	{
		VisibleCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), VisibleCursorWidgetClass);
	}
	return VisibleCursorWidget;
}

UUserWidget* UInvGrid::GetHiddenCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(HiddenCursorWidget))
	{
		HiddenCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), HiddenCursorWidgetClass);
	}
	return HiddenCursorWidget;
}

bool UInvGrid::IsSameStackable(const UInvItem* ClickedInventoryItem) const
{
	const bool bIsSameItem = ClickedInventoryItem == HoverItem->GetInventoryItem();
	const bool bIsStackable = ClickedInventoryItem->IsStackable();
	return bIsSameItem && bIsStackable && HoverItem->GetItemType().MatchesTagExact(ClickedInventoryItem->GetItemManifest().GetItemType());
}

void UInvGrid::SwapWithHoverItem(UInvItem* ClickedInventoryItem, const int32 GridIndex)
{
	if (!IsValid(HoverItem)) return;

	UInvItem* TempInventoryItem = HoverItem->GetInventoryItem();
	const int32 TempStackCount = HoverItem->GetStackCount();
	const bool bTempIsStackable = HoverItem->IsStackable();

	AssignHoverItem(ClickedInventoryItem, GridIndex, HoverItem->GetPreviousGridIndex());
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
	AddItemAtIndex(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
	UpdateGridSlots(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
}

bool UInvGrid::ShouldSwapStackCounts(const int32 RoomInClickedSlot, const int32 HoveredStackCount, const int32 MaxStackSize) const
{
	return RoomInClickedSlot == 0 && HoveredStackCount < MaxStackSize;
}

void UInvGrid::SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index)
{
	UInvGridSlot* GridSlot = GridSlots[Index];
	GridSlot->SetStackCount(HoveredStackCount);

	UInvSlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpdateStackCount(HoveredStackCount);

	HoverItem->UpdateStackCount(ClickedStackCount);
}

bool UInvGrid::ShouldConsumeHoverItemStacks(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const
{
	return RoomInClickedSlot >= HoveredStackCount;
}

void UInvGrid::ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index)
{
	const int32 AmountToTransfer = HoveredStackCount;
	const int32 NewClickedStackCount = ClickedStackCount + AmountToTransfer;

	GridSlots[Index]->SetStackCount(NewClickedStackCount);
	SlottedItems.FindChecked(Index)->UpdateStackCount(NewClickedStackCount);
	ClearHoverItem();
	ShowCursor();

	const FInvGridFragment* GridFragment = GridSlots[Index]->GetInventoryItem()->GetItemManifest().GetFragmentOfType<FInvGridFragment>();
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
	HighlightSlots(Index, Dimensions);
}

bool UInvGrid::ShouldFillInStack(const int32 RoomInClickedSlot, const int32 HoveredStackCount) const
{
	return RoomInClickedSlot < HoveredStackCount;
}

void UInvGrid::FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index)
{
	UInvGridSlot* GridSlot = GridSlots[Index];
	const int32 NewStackCount = GridSlot->GetStackCount() + FillAmount;

	GridSlot->SetStackCount(NewStackCount);

	UInvSlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpdateStackCount(NewStackCount);

	HoverItem->UpdateStackCount(Remainder);
}

void UInvGrid::ShowCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetVisibleCursorWidget());
}

void UInvGrid::HideCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetHiddenCursorWidget());
}

void UInvGrid::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	OwningCanvasPanel = OwningCanvas;
}

// 如果已经拿起物品，不需要更改悬停格子的状态
void UInvGrid::OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UInvGridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetOccupiedTexture();
	}
}

void UInvGrid::OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UInvGridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetUnoccupiedTexture();
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
	SlottedItem->OnSlottedItemClicked.AddDynamic(this, &UInvGrid::OnSlottedItemClicked);
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
			GridSlot->SetOccupiedTexture();
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

		// 更新剩余需要填充的数量
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

	// 格子为空
	if (!SubGridSlot->GetInventoryItem().IsValid())
	{
		OutTentativelyClaimed.Add(SubGridSlot->GetTileIndex());
		return true;
	}

	// 只有左上角格子参与比较
	if (!IsUpperLeftSlot(GridSlot, SubGridSlot)) return false;

	// 该物品需要可堆叠
	const UInvItem* SubItem = SubGridSlot->GetInventoryItem().Get();
	if (!SubItem->IsStackable()) return false;

	// 需要同类别
	if (!DoesItemTypeMatch(SubItem, ItemType)) return false;

	// 如果可堆叠，该格子是否能堆叠更多
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
// 在该坐标放置物品的左上角是否越界
bool UInvGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num()) return false;
	const int32 EndColumn = (StartIndex % Col) + ItemDimensions.X;
	const int32 EndRow = (StartIndex / Col) + ItemDimensions.Y;
	return EndColumn <= Col && EndRow <= Row;
}
// 通过该格子的左上角格子获取该格子的堆叠数量
int32 UInvGrid::GetStackAmount(const UInvGridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	const int32 UpperLeftIndex = GridSlot->GetFirstGridIndex();

	if ( UpperLeftIndex != INDEX_NONE)
	{
		UInvGridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftGridSlot->GetStackCount();
	}

	return CurrentSlotStackCount;
}
// 计算该格子可以放置的数量
int32 UInvGrid::DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize, const int32 AmountToFill, const UInvGridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackSize - GetStackAmount(GridSlot);
	return bStackable ? FMath::Min(AmountToFill, RoomInSlot) : 1;
}

void UInvGrid::AddStacks(const FInvSlotAvailabilityResult& Result)
{
	if (!MatchesCategory(Result.Item.Get())) return;

	for (const auto& Availability : Result.SlotAvailabilities)
	{
		if (Availability.bItemAtIndex)//堆叠
		{
			const auto& GridSlot = GridSlots[Availability.Index];
			const auto& SlottedItem = SlottedItems.FindChecked(Availability.Index);
			SlottedItem->UpdateStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
			GridSlot->SetStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
		}
		else//新物品
		{
			AddItemAtIndex(Result.Item.Get(), Availability.Index, Result.bStackable, Availability.AmountToFill);
			UpdateGridSlots(Result.Item.Get(), Availability.Index, Result.bStackable, Availability.AmountToFill);
		}
	}
}

void UInvGrid::OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	UInvUtils::ItemUnhovered(GetOwningPlayer());

	check(GridSlots.IsValidIndex(GridIndex));
	UInvItem* ClickedInventoryItem = GridSlots[GridIndex]->GetInventoryItem().Get();

	if (!IsValid(HoverItem) && IsLeftClick(MouseEvent))
	{
		PickUp(ClickedInventoryItem, GridIndex);
		return;
	}

	if (IsRightClick(MouseEvent))
	{
		CreateItemPopUp(GridIndex);
		return;
	}

	// 悬浮物品与光标所在格物品品类相同且可堆叠
	if (IsSameStackable(ClickedInventoryItem))
	{
		const int32 ClickedStackCount = GridSlots[GridIndex]->GetStackCount();
		const FInvStackableFragment* StackableFragment = ClickedInventoryItem->GetItemManifest().GetFragmentOfType<FInvStackableFragment>();
		const int32 MaxStackSize = StackableFragment->GetMaxStackSize();
		const int32 RoomInClickedSlot = MaxStackSize - ClickedStackCount;
		const int32 HoveredStackCount = HoverItem->GetStackCount();

		// 所在格已满，交换数量
		if (ShouldSwapStackCounts(RoomInClickedSlot, HoveredStackCount, MaxStackSize))
		{
			SwapStackCounts(ClickedStackCount, HoveredStackCount, GridIndex);
			return;
		}

		// 所在格未满，悬浮物品堆叠数量小于所在格剩余空间，全部放入
		if (ShouldConsumeHoverItemStacks(HoveredStackCount, RoomInClickedSlot))
		{
			ConsumeHoverItemStacks(ClickedStackCount, HoveredStackCount, GridIndex);
			return;
		}

		// 所在格未满，悬浮物品堆叠数量小于所在格剩余空间，部分放入
		if (ShouldFillInStack(RoomInClickedSlot, HoveredStackCount))
		{
			FillInStack(RoomInClickedSlot, HoveredStackCount - RoomInClickedSlot, GridIndex);
			return;
		}

		if (RoomInClickedSlot == 0)
		{
			return;
		}
	}

	if (CurrentQueryResult.ValidItem.IsValid())
	{
		SwapWithHoverItem(ClickedInventoryItem, GridIndex);
	}
}

void UInvGrid::CreateItemPopUp(const int32 GridIndex)
{
	UInvItem* RightClickedItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (IsValid(GridSlots[GridIndex]->GetItemPopUp())) return;

	ItemPopUp = CreateWidget<UInvItemPopUp>(this, ItemPopUpClass);
	GridSlots[GridIndex]->SetItemPopUp(ItemPopUp);

	OwningCanvasPanel->AddChild(ItemPopUp);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ItemPopUp);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	CanvasSlot->SetPosition(MousePosition - ItemPopUpOffset);
	CanvasSlot->SetSize(ItemPopUp->GetBoxSize());

	const int32 SliderMax = GridSlots[GridIndex]->GetStackCount();
	if (RightClickedItem->IsStackable() && SliderMax > 0)
	{
		ItemPopUp->OnSplit.BindDynamic(this, &ThisClass::OnPopUpMenuSplit);
		ItemPopUp->SetSliderParams(SliderMax, FMath::Max(1, GridSlots[GridIndex]->GetStackCount() / 2));
	}
	else
	{
		ItemPopUp->CollapseSplitButton();
	}

	ItemPopUp->OnDrop.BindDynamic(this, &ThisClass::OnPopUpMenuDrop);

	if (RightClickedItem->IsConsumable())
	{
		ItemPopUp->OnConsume.BindDynamic(this, &ThisClass::OnPopUpMenuConsume);
	}
	else
	{
		ItemPopUp->CollapseConsumeButton();
	}
}

void UInvGrid::OnPopUpMenuSplit(int32 SplitAmount, int32 Index)
{
	UInvItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (!RightClickedItem->IsStackable()) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetFirstGridIndex();
	UInvGridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = UpperLeftGridSlot->GetStackCount();
	const int32 NewStackCount = StackCount - SplitAmount;

	if (NewStackCount == 0)
	{
		RemoveItemFromGrid(RightClickedItem, Index);
	}
	else
	{
		UpperLeftGridSlot->SetStackCount(NewStackCount);
	}

	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	AssignHoverItem(RightClickedItem, UpperLeftIndex, UpperLeftIndex);
	HoverItem->UpdateStackCount(SplitAmount);
}

void UInvGrid::OnPopUpMenuDrop(int32 Index)
{
	UInvItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	PickUp(RightClickedItem, Index);
	DropItem();
}

void UInvGrid::OnPopUpMenuConsume(int32 Index)
{
	UInvItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetFirstGridIndex();
	UInvGridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 NewStackCount = UpperLeftGridSlot->GetStackCount() - 1;

	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	InvComponent->Server_ConsumeItem(RightClickedItem);

	if (NewStackCount <= 0)
	{
		RemoveItemFromGrid(RightClickedItem, Index);
	}
}

void UInvGrid::DropItem()
{
	if (!IsValid(HoverItem)) return;
	if (!IsValid(HoverItem->GetInventoryItem())) return;

	InvComponent->Server_DropItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());

	ClearHoverItem();
	ShowCursor();
}

bool UInvGrid::HasHoverItem() const
{
	return IsValid(HoverItem);
}

UHoverItem* UInvGrid::GetHoverItem() const
{
	return HoverItem;
}

bool UInvGrid::IsRightClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
}

bool UInvGrid::IsLeftClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
}

void UInvGrid::PickUp(UInvItem* ClickedInventoryItem, const int32 GridIndex)
{
	AssignHoverItem(ClickedInventoryItem, GridIndex, GridIndex);
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
}
// 根据GridSlot的索引获取物品的碎片信息创建HoverItem到光标
void UInvGrid::AssignHoverItem(UInvItem* InventoryItem)
{
	if (!IsValid(HoverItem))
	{
		HoverItem = CreateWidget<UHoverItem>(GetOwningPlayer(), HoverItemClass);
	}

	const FInvGridFragment* GridFragment = GetFragment<FInvGridFragment>(InventoryItem, FragmentTags::GridFragment);
	const FInvImageFragment* ImageFragment = GetFragment<FInvImageFragment>(InventoryItem, FragmentTags::IconFragment);
	if (!GridFragment || !ImageFragment) return;

	const FVector2D DrawSize = GetDrawSize(GridFragment);

	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(ImageFragment->GetIcon());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.ImageSize = DrawSize * UWidgetLayoutLibrary::GetViewportScale(this);

	HoverItem->SetImageBrush(IconBrush);
	HoverItem->SetGridDimensions(GridFragment->GetGridSize());
	HoverItem->SetInventoryItem(InventoryItem);
	HoverItem->SetIsStackable(InventoryItem->IsStackable());

	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, HoverItem);
}

void UInvGrid::AssignHoverItem(UInvItem* InventoryItem, const int32 GridIndex, const int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);

	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpdateStackCount(InventoryItem->IsStackable() ? GridSlots[GridIndex]->GetStackCount() : 0);
}

void UInvGrid::RemoveItemFromGrid(UInvItem* InventoryItem, const int32 GridIndex)
{
	const FInvGridFragment* GridFragment = GetFragment<FInvGridFragment>(InventoryItem, FragmentTags::GridFragment);
	if (!GridFragment) return;

	UInvUtils::ForEach2D(GridSlots, GridIndex, GridFragment->GetGridSize(), Col, [&](UInvGridSlot* GridSlot)
		{
			GridSlot->SetInventoryItem(nullptr);
			GridSlot->SetFirstGridIndex(INDEX_NONE);
			GridSlot->SetUnoccupiedTexture();
			GridSlot->SetAvailable(true);
			GridSlot->SetStackCount(0);
		});

	if (SlottedItems.Contains(GridIndex))
	{
		TObjectPtr<UInvSlottedItem> FoundSlottedItem;
		SlottedItems.RemoveAndCopyValue(GridIndex, FoundSlottedItem);
		FoundSlottedItem->RemoveFromParent();
	}
}
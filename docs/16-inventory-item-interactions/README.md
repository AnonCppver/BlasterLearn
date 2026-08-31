# 背包物品交互

## 功能点介绍

空间背包不仅需要显示物品，还需要处理不同尺寸物品的鼠标操作。本项目使用 `UInvSlottedItem` 接收物品点击，以 `UHoverItem` 表示鼠标当前拿起的物品，并根据 HoverItem 将要覆盖的完整矩形区域决定放下、堆叠、交换或拒绝操作。

右键菜单为可堆叠、可丢弃和可消耗物品提供不同操作入口；其中移动、交换、拆分和合并只改变本地 Grid 布局，丢弃与消耗会改变服务器上的逻辑数量，因此通过 Server RPC 执行。

本单元包含：

- 判断鼠标点击的逻辑物品。
- 多格物品与左上角索引映射。
- 创建跟随鼠标的 HoverItem。
- 根据鼠标象限计算物品目标区域。
- 空白放置、堆叠合并和连续交换。
- 右键拆分、丢弃和消耗菜单。
- 服务器减少数量、移除 FastArray 条目并重建场景物品。

---

## 核心交互对象

```text
UInvItem
→ 背包中的逻辑物品
→ 保存 Manifest 和 TotalStackCount

UInvSlottedItem
→ 放置在 CanvasPanel 中的物品 Widget
→ 显示图标和可视堆叠数量

UInvGridSlot
→ 表示一个空间格子
→ 保存占用状态、物品引用和 FirstGridIndex

UHoverItem
→ 当前由鼠标携带的物品
→ 保存物品、尺寸、数量和原位置

UInvItemPopUp
→ 右键上下文菜单
→ 提供 Split、Drop 和 Consume
```

`UInvGrid` 维护的主要状态为：

```text
GridSlots
→ 所有空间格子

SlottedItems
→ 左上角 Index 到 UInvSlottedItem 的映射

HoverItem
→ 当前鼠标拿起的物品

ItemDropIndex
→ HoverItem 当前目标左上角下标

CurrentQueryResult
→ 当前目标区域的空间查询结果
```

---

## 交互状态总览

```text
没有 HoverItem
├─ 左键物品
│  └─ PickUp → 创建 HoverItem
├─ 右键物品
│  └─ CreateItemPopUp
└─ 悬停物品
   └─ 延迟显示物品描述

存在 HoverItem
├─ 目标区域为空
│  └─ PutDownOnIndex
├─ 目标是同一逻辑可堆叠物品
│  ├─ 交换可视数量
│  ├─ 全部合并
│  └─ 部分合并
├─ 目标区域只覆盖一个其他物品
│  └─ SwapWithHoverItem
├─ 目标区域覆盖多个物品
│  └─ 不执行放置或交换
└─ 点击背包背景
   └─ DropItem → Server_DropItem
```

---

## 普通点击如何确定物品

背包中每个可视物品由 `UInvSlottedItem` 表示。创建 Widget 时，`UInvGrid::AddItemAtIndex()`为其写入物品左上角下标：

```cpp
SlottedItem->SetGridIndex(Index);
```

并绑定点击委托：

```cpp
SlottedItem->OnSlottedItemClicked.AddDynamic(
	this,
	&UInvGrid::OnSlottedItemClicked
);
```

### UInvSlottedItem::NativeOnMouseButtonDown

物品 Widget 直接接收鼠标点击：

```cpp
FReply UInvSlottedItem::NativeOnMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent
)
{
	OnSlottedItemClicked.Broadcast(
		GridIndex,
		MouseEvent
	);

	return FReply::Handled();
}
```

调用链：

```text
鼠标点击物品 Widget
→ UInvSlottedItem::NativeOnMouseButtonDown
→ OnSlottedItemClicked.Broadcast
→ UInvGrid::OnSlottedItemClicked
```

`UInvGrid`通过下标取得逻辑物品：

```cpp
UInvItem* ClickedInventoryItem =
	GridSlots[GridIndex]
		->GetInventoryItem()
		.Get();
```

因此普通点击不需要根据鼠标坐标再次搜索物品。事件由具体的物品 Widget 接收，并携带其 `GridIndex`。

---

## 多格物品与 FirstGridIndex

物品落位时，`UpdateGridSlots()`将同一个逻辑 `UInvItem` 写入物品覆盖的全部格子：

```cpp
GridSlot->SetInventoryItem(NewItem);
GridSlot->SetFirstGridIndex(Index);
```

例如一个 `2×2` 物品从 Index 6 开始：

```text
6   7
12  13
```

四个格子保存：

```text
InventoryItem = 同一个 UInvItem
FirstGridIndex = 6
```

`FirstGridIndex` 的作用是：

- 将多格物品的所有子格映射回左上角。
- 使用唯一 Index 查找 `UInvSlottedItem`。
- 从左上角读取可视堆叠数量。
- 在拖拽检测中对同一多格物品去重。
- 统一移除、交换和右键操作的目标位置。

---

## 鼠标悬停与物品描述

### UInvSlottedItem::NativeOnMouseEnter

鼠标进入物品 Widget 时调用：

```cpp
UInvUtils::ItemHovered(
	GetOwningPlayer(),
	InventoryItem.Get()
);
```

`UInvUtils::ItemHovered()`依次取得：

```text
PlayerController
→ UInvComponent
→ InventoryMenu
```

如果当前存在 HoverItem：

```cpp
InventoryBase->HasHoverItem()
```

则不显示描述，避免描述窗口干扰拖拽。

否则调用：

```cpp
InventoryBase->OnItemHovered(Item);
```

实际进入 `USpatialInventory::OnItemHovered()`。

### 延迟显示描述

`OnItemHovered()`取得 Manifest，隐藏并复用描述 Widget，然后设置延迟 Timer：

```text
清除上一个 DescriptionTimer
→ 等待 DescriptionTimerDelay
→ 显示 UInvItemDescription
→ Manifest.AssimilateInventoryFragments
```

Fragment 根据 GameplayTag 将图标、文本和数值写入对应描述控件。

### 鼠标离开

```text
UInvSlottedItem::NativeOnMouseLeave
→ UInvUtils::ItemUnhovered
→ USpatialInventory::OnItemUnHovered
→ 隐藏描述
→ 清除 Timer
```

`USpatialInventory::NativeTick()`持续把描述面板放到鼠标附近，并限制在 Canvas 边界内。

---

## 左键拿起物品

`UInvGrid::OnSlottedItemClicked()`首先关闭悬停描述：

```cpp
UInvUtils::ItemUnhovered(
	GetOwningPlayer()
);
```

如果当前没有 HoverItem，并且鼠标按键是左键：

```cpp
if (!IsValid(HoverItem) && IsLeftClick(MouseEvent))
{
	PickUp(
		ClickedInventoryItem,
		GridIndex
	);
	return;
}
```

### PickUp

```cpp
void UInvGrid::PickUp(
	UInvItem* ClickedInventoryItem,
	const int32 GridIndex
)
{
	AssignHoverItem(
		ClickedInventoryItem,
		GridIndex,
		GridIndex
	);

	RemoveItemFromGrid(
		ClickedInventoryItem,
		GridIndex
	);
}
```

拿起过程：

```text
根据目标物品创建 HoverItem
→ 保存图标、尺寸、数量和原位置
→ 从 Grid 中移除原物品 Widget
→ 清除原占用区域
```

---

## AssignHoverItem：创建鼠标物品

如果 HoverItem 不存在：

```cpp
HoverItem = CreateWidget<UHoverItem>(
	GetOwningPlayer(),
	HoverItemClass
);
```

从逻辑物品 Manifest 中取得：

```cpp
FInvGridFragment
FInvImageFragment
```

分别提供：

```text
GridFragment
→ GridDimensions

ImageFragment
→ Icon
```

根据物品绘制尺寸和 Viewport Scale 创建图标 Brush：

```cpp
IconBrush.ImageSize =
	DrawSize *
	UWidgetLayoutLibrary::GetViewportScale(this);
```

向 HoverItem 写入：

```text
图标
GridDimensions
逻辑 UInvItem
是否可堆叠
PreviousGridIndex
当前可视 StackCount
```

最后将 HoverItem 设置为鼠标光标 Widget：

```cpp
GetOwningPlayer()->SetMouseCursorWidget(
	EMouseCursor::Default,
	HoverItem
);
```

---

## RemoveItemFromGrid：清除原位置

`RemoveItemFromGrid()`从 Manifest 取得物品尺寸，然后遍历原物品覆盖的二维区域：

```cpp
GridSlot->SetInventoryItem(nullptr);
GridSlot->SetFirstGridIndex(INDEX_NONE);
GridSlot->SetUnoccupiedTexture();
GridSlot->SetAvailable(true);
GridSlot->SetStackCount(0);
```

随后从：

```cpp
TMap<int32, TObjectPtr<UInvSlottedItem>> SlottedItems;
```

删除左上角下标对应的物品 Widget：

```cpp
SlottedItems.RemoveAndCopyValue(
	GridIndex,
	FoundSlottedItem
);

FoundSlottedItem->RemoveFromParent();
```

这个过程只修改本地 Grid UI，不删除 FastArray 中的逻辑 `UInvItem`。

---

## 拖拽期间的鼠标位置计算

`UInvGrid::NativeTick()`每帧取得：

```text
CanvasPosition
MousePosition
```

如果鼠标仍在 Canvas 中：

```cpp
UpdateTileParameters(
	CanvasPosition,
	MousePosition
);
```

### CalculateHoveredCoordinates

将鼠标像素坐标转换为 Grid 坐标：

```cpp
X = FloorToInt(
	(MouseX - CanvasX) / Size
);

Y = FloorToInt(
	(MouseY - CanvasY) / Size
);
```

得到当前鼠标所在的格子坐标和一维 TileIndex。

### CalculateTileQuadrant

计算鼠标在单个格子中的局部位置：

```cpp
TileLocalX = Fmod(MouseX - CanvasX, Size);
TileLocalY = Fmod(MouseY - CanvasY, Size);
```

将格子划分为：

```text
TopLeft
TopRight
BottomLeft
BottomRight
```

象限主要用于偶数尺寸物品。`2×2` 物品没有唯一中心格子，需要根据鼠标位于当前格子的哪个象限决定左上角偏移。

### CalculateStartingCoordinate

根据：

```text
鼠标所在格子
HoverItem.GridDimensions
鼠标象限
```

计算物品目标左上角，转换为：

```cpp
ItemDropIndex
```

---

## CheckHoverPosition：确定目标物品

拖拽期间不能只判断鼠标所在的一个格子，因为 HoverItem 可能占用多个格子。`CheckHoverPosition()`检查 HoverItem 将要覆盖的完整矩形。

首先验证目标区域没有越界：

```cpp
if (!IsInGridBounds(TargetIndex, Dimensions))
{
	return Result;
}
```

然后创建：

```cpp
TSet<int32> OccupiedUpperLeftIndices;
```

遍历目标区域中的所有 GridSlot。遇到已有物品时：

```cpp
OccupiedUpperLeftIndices.Add(
	GridSlot->GetFirstGridIndex()
);

Result.bHasSpace = false;
```

使用 `TSet` 可以把同一多格物品的多个子格合并成一个唯一的左上角 Index。

### 目标区域为空

```text
OccupiedUpperLeftIndices.Num() == 0
bHasSpace = true
ValidItem = nullptr
```

允许直接放下。

### 目标区域只覆盖一个物品

```cpp
if (OccupiedUpperLeftIndices.Num() == 1)
```

记录：

```cpp
Result.ValidItem =
	GridSlots[Index]->GetInventoryItem();

Result.UpperLeftIndex =
	GridSlots[Index]->GetFirstGridIndex();
```

允许进入交换或堆叠判断。

### 目标区域覆盖多个物品

```text
bHasSpace = false
ValidItem = nullptr
```

不会执行单物品交换。

---

## 空间预览

### 空白目标区域

```cpp
if (CurrentQueryResult.bHasSpace)
{
	HighlightSlots(
		ItemDropIndex,
		Dimensions
	);
}
```

候选区域显示为占用状态，提示物品将落在这里。

### 覆盖一个物品

```cpp
ChangeHoverType(
	CurrentQueryResult.UpperLeftIndex,
	ExistingItemDimensions,
	EInvGridSlotState::GrayedOut
);
```

目标物品区域变灰，提示可以交换或合并。

### 鼠标离开 Canvas

```cpp
CursorExitedCanvas()
→ UnHighlightSlots()
```

恢复格子的真实可用或占用纹理。

---

## 点击空格放下物品

空格子通过 `UInvGridSlot::NativeOnMouseButtonDown()`广播：

```cpp
GridSlotClicked.Broadcast(
	TileIndex,
	MouseEvent
);
```

最终进入：

```cpp
UInvGrid::OnGridSlotClicked()
```

如果当前没有 HoverItem，函数直接返回。

如果目标区域实际覆盖一个已有物品，则将操作重定向到其左上角：

```cpp
OnSlottedItemClicked(
	CurrentQueryResult.UpperLeftIndex,
	MouseEvent
);
```

如果目标区域为空：

```cpp
PutDownOnIndex(ItemDropIndex);
```

### PutDownOnIndex

```text
AddItemAtIndex
→ 创建新的 UInvSlottedItem

UpdateGridSlots
→ 写入物品覆盖区域

ClearHoverItem
→ 清除鼠标物品并恢复普通光标
```

移动物品不会改变 FastArray 成员或 `TotalStackCount`，因此没有 Server RPC。

---

## ClearHoverItem：结束鼠标携带状态

```cpp
HoverItem->SetInventoryItem(nullptr);
HoverItem->SetIsStackable(false);
HoverItem->SetPreviousGridIndex(INDEX_NONE);
HoverItem->UpdateStackCount(0);
HoverItem->SetImageBrush(FSlateNoResource());
```

随后：

```cpp
HoverItem->RemoveFromParent();
HoverItem = nullptr;
ShowCursor();
```

普通鼠标光标重新显示。

---

## 判断是否属于同一逻辑堆叠

`IsSameStackable()`要求：

```cpp
ClickedInventoryItem ==
HoverItem->GetInventoryItem();
```

并同时要求：

```text
ClickedInventoryItem 可堆叠
ItemType MatchesTagExact
```

指针相同表示两个可视堆叠共同引用同一个逻辑 `UInvItem`。这些堆叠可以重新分配显示数量，而逻辑总数仍保存在同一个 `TotalStackCount` 中。

---

## 堆叠数量交互

取得：

```text
ClickedStackCount
MaxStackSize
RoomInClickedSlot
HoveredStackCount
```

其中：

```cpp
RoomInClickedSlot =
	MaxStackSize - ClickedStackCount;
```

### 交换可视数量

条件：

```cpp
RoomInClickedSlot == 0 &&
HoveredStackCount < MaxStackSize
```

调用：

```cpp
SwapStackCounts()
```

执行：

```text
目标 GridSlot 数量 = HoveredStackCount
目标 SlottedItem 文本 = HoveredStackCount
HoverItem 数量 = 原目标数量
```

### 全部合并

条件：

```cpp
RoomInClickedSlot >= HoveredStackCount
```

调用：

```cpp
ConsumeHoverItemStacks()
```

执行：

```text
目标数量 += HoverItem 数量
更新目标 SlottedItem
清除 HoverItem
```

### 部分合并

条件：

```cpp
RoomInClickedSlot < HoveredStackCount
```

调用：

```cpp
FillInStack()
```

执行：

```text
目标堆叠增加 FillAmount
目标更新为满堆或接近满堆
HoverItem 保留 Remainder
```

这些操作只重新分配同一个逻辑物品的可视数量，不改变逻辑总数。

---

## SwapWithHoverItem：交换悬浮物品

当目标区域只覆盖一个其他物品时：

```cpp
if (CurrentQueryResult.ValidItem.IsValid())
{
	SwapWithHoverItem(
		ClickedInventoryItem,
		GridIndex
	);
}
```

### 缓存原 HoverItem

```cpp
UInvItem* TempInventoryItem =
	HoverItem->GetInventoryItem();

const int32 TempStackCount =
	HoverItem->GetStackCount();

const bool bTempIsStackable =
	HoverItem->IsStackable();
```

### 将目标物品写入 HoverItem

```cpp
AssignHoverItem(
	ClickedInventoryItem,
	GridIndex,
	HoverItem->GetPreviousGridIndex()
);
```

此后 HoverItem 显示目标物品的图标、尺寸和数量。

### 移除目标物品

```cpp
RemoveItemFromGrid(
	ClickedInventoryItem,
	GridIndex
);
```

### 放入原 HoverItem

```cpp
AddItemAtIndex(
	TempInventoryItem,
	ItemDropIndex,
	bTempIsStackable,
	TempStackCount
);

UpdateGridSlots(
	TempInventoryItem,
	ItemDropIndex,
	bTempIsStackable,
	TempStackCount
);
```

最终效果：

```text
原 HoverItem
→ 放入目标区域

原目标物品
→ 成为新的 HoverItem
→ 继续跟随鼠标
```

这是一种连续交换：玩家放下手中的物品，同时拿起目标物品。交换只改变本地空间布局，不改变 FastArray 和逻辑数量。

---

## 右键上下文菜单

`OnSlottedItemClicked()`检测右键：

```cpp
if (IsRightClick(MouseEvent))
{
	CreateItemPopUp(GridIndex);
	return;
}
```

### CreateItemPopUp

取得目标物品：

```cpp
UInvItem* RightClickedItem =
	GridSlots[GridIndex]
		->GetInventoryItem()
		.Get();
```

如果该 GridSlot 已经保存有效弹窗，则不重复创建。

创建并记录：

```cpp
ItemPopUp = CreateWidget<UInvItemPopUp>(
	this,
	ItemPopUpClass
);

GridSlots[GridIndex]
	->SetItemPopUp(ItemPopUp);
```

将弹窗加入背包根 Canvas，并放在鼠标附近：

```cpp
CanvasSlot->SetPosition(
	MousePosition - ItemPopUpOffset
);
```

---

## 菜单按钮显示规则

### Split

只有物品可堆叠且当前可视数量大于 0 时绑定：

```cpp
ItemPopUp->OnSplit.BindDynamic(
	this,
	&UInvGrid::OnPopUpMenuSplit
);
```

Slider 参数：

```text
Min = 1
Max = 当前 GridSlot.StackCount
Default = max(1, StackCount / 2)
```

不可堆叠物品隐藏拆分按钮、Slider 和数量文本。

### Drop

所有有效物品都绑定：

```cpp
ItemPopUp->OnDrop.BindDynamic(
	this,
	&UInvGrid::OnPopUpMenuDrop
);
```

### Consume

`UInvItem::IsConsumable()`判断：

```cpp
ItemCategory == EInvItemCategory::Consumable
```

可消耗物品绑定：

```cpp
ItemPopUp->OnConsume.BindDynamic(
	this,
	&UInvGrid::OnPopUpMenuConsume
);
```

其他物品隐藏 Consume 按钮。

---

## UInvItemPopUp 内部回调

弹窗初始化时绑定：

```text
Button_Split.OnClicked
→ SplitButtonClicked

Button_Drop.OnClicked
→ DropButtonClicked

Button_Consume.OnClicked
→ ConsumeButtonClicked

Slider_Split.OnValueChanged
→ SliderValueChanged
```

Slider 变化后，使用向下取整值更新文本。

按钮通过单播 Delegate 返回 `UInvGrid`：

```text
OnSplit.ExecuteIfBound
OnDrop.ExecuteIfBound
OnConsume.ExecuteIfBound
```

执行成功后移除弹窗。

鼠标离开弹窗范围时也会调用：

```cpp
RemoveFromParent();
```

`UInvGridSlot`通过弹窗销毁回调清除自己的 `ItemPopUp` 弱引用。

---

## 拆分堆叠

菜单 Split 进入：

```cpp
UInvGrid::OnPopUpMenuSplit(
	int32 SplitAmount,
	int32 Index
)
```

取得物品左上角：

```cpp
const int32 UpperLeftIndex =
	GridSlots[Index]->GetFirstGridIndex();
```

计算原位置剩余量：

```cpp
const int32 NewStackCount =
	StackCount - SplitAmount;
```

更新原位置后创建 HoverItem：

```cpp
AssignHoverItem(
	RightClickedItem,
	UpperLeftIndex,
	UpperLeftIndex
);

HoverItem->UpdateStackCount(
	SplitAmount
);
```

最终状态：

```text
原位置
→ 保留 NewStackCount

HoverItem
→ 携带 SplitAmount

两者
→ 引用同一个逻辑 UInvItem
```

拆分不改变 `TotalStackCount`，因此不发送 Server RPC。

---

## 消耗物品

菜单 Consume 进入：

```cpp
UInvGrid::OnPopUpMenuConsume(Index)
```

客户端先取得左上角并计算：

```cpp
const int32 NewStackCount =
	UpperLeftGridSlot->GetStackCount() - 1;
```

立即更新：

```text
UpperLeftGridSlot.StackCount
UInvSlottedItem 数量文本
```

然后调用 Server RPC：

```cpp
InvComponent->Server_ConsumeItem(
	RightClickedItem
);
```

如果本地可视数量归零，则调用：

```cpp
RemoveItemFromGrid(
	RightClickedItem,
	Index
);
```

### Server_ConsumeItem

服务器计算：

```cpp
NewStackCount =
	Item->GetTotalStackCount() - 1;
```

总数归零时：

```cpp
InventoryList.RemoveEntry(Item);
```

仍有剩余时：

```cpp
Item->SetTotalStackCount(NewStackCount);
```

最后从 Manifest 取得：

```cpp
FInvConsumableFragment
```

并调用：

```cpp
ConsumableFragment->OnConsume(
	OwningController.Get()
);
```

完整链路：

```text
右键 Consume
→ 客户端可视数量减一
→ Server_ConsumeItem
→ 服务器逻辑总数减一
→ 必要时删除 FastArray 条目
→ ConsumableFragment::OnConsume
```

---

## 丢弃物品入口

丢弃有两个入口：

```text
右键菜单 Drop
点击背包背景丢弃当前 HoverItem
```

### 右键菜单 Drop

```text
UInvItemPopUp::DropButtonClicked
→ OnDrop.ExecuteIfBound
→ UInvGrid::OnPopUpMenuDrop
```

`OnPopUpMenuDrop()`先调用：

```cpp
PickUp(
	RightClickedItem,
	Index
);
```

将目标物品转成 HoverItem，并从 Grid 移除，然后立即调用：

```cpp
DropItem();
```

### 背包背景 Drop

`USpatialInventory::NativeOnMouseButtonDown()`调用：

```cpp
InvGrid->DropItem();
```

如果当前没有 HoverItem，`DropItem()`直接返回；如果正在携带物品，则进入相同的服务器丢弃链路。

---

## UInvGrid::DropItem

首先验证：

```text
HoverItem 有效
HoverItem.InventoryItem 有效
```

然后发送可靠 Server RPC：

```cpp
InvComponent->Server_DropItem(
	HoverItem->GetInventoryItem(),
	HoverItem->GetStackCount()
);
```

参数含义：

```text
UInvItem
→ 丢弃哪个逻辑物品

StackCount
→ 丢弃当前可视堆叠中的多少数量
```

发送 RPC 后立即清除本地 HoverItem。

---

## Server_DropItem：服务器扣除数量

服务器计算权威剩余量：

```cpp
const int32 NewStackCount =
	Item->GetTotalStackCount() -
	StackCount;
```

### 全部丢弃

```cpp
if (NewStackCount <= 0)
{
	InventoryList.RemoveEntry(Item);
}
```

逻辑 `UInvItem` 从 FastArray 删除。

### 部分丢弃

```cpp
else
{
	Item->SetTotalStackCount(
		NewStackCount
	);
}
```

逻辑物品继续存在，只减少总数量。

随后统一调用：

```cpp
SpawnDroppedItem(
	Item,
	StackCount
);
```

---

## SpawnDroppedItem：计算场景位置

取得玩家 Pawn 和前方向：

```cpp
const APawn* OwningPawn =
	OwningController->GetPawn();

FVector RotatedForward =
	OwningPawn->GetActorForwardVector();
```

使用配置范围随机旋转方向：

```cpp
RotatedForward.RotateAngleAxis(
	FMath::FRandRange(
		DropSpawnAngleMin,
		DropSpawnAngleMax
	),
	FVector::UpVector
);
```

使用随机距离计算位置：

```cpp
SpawnLocation =
	PawnLocation +
	RotatedForward *
	FMath::FRandRange(
		DropSpawnDistanceMin,
		DropSpawnDistanceMax
	);
```

再按照 `RelativeSpawnElevation` 调整 Z 轴，旋转使用 `FRotator::ZeroRotator`。

---

## 将运行时 Manifest 写回场景物品

取得逻辑物品 Manifest：

```cpp
FInvItemManifest& ItemManifest =
	Item->GetItemManifestMutable();
```

如果物品可堆叠，将 Fragment 数量设置为本次丢弃量：

```cpp
StackableFragment->SetStackCount(
	StackCount
);
```

然后调用：

```cpp
ItemManifest.SpawnPickupActor(
	this,
	SpawnLocation,
	SpawnRotation
);
```

`SpawnPickupActor()`执行：

```text
读取 PickupActorClass
→ SpawnActor
→ 查找生成 Actor 的 UInvItemComponent
→ InitItemManifest
```

最终完成：

```text
背包 UInvItem
└─ 运行时 FInvItemManifest
   ↓
场景 AItem
└─ UInvItemComponent
   └─ 保存丢弃物品数据和数量
```

---

## 本地布局与服务器权威数据

当前背包将操作分成两类。

### 只修改本地布局

```text
拿起物品
放下物品
改变格子位置
交换物品
拆分可视堆叠
合并可视堆叠
交换可视堆叠数量
```

这些操作不会改变：

```text
FastArray 中有哪些 UInvItem
UInvItem::TotalStackCount
```

因此不发送 Server RPC。

### 修改服务器逻辑数据

```text
丢弃物品
消耗物品
```

它们会改变：

```text
UInvItem::TotalStackCount
或
FastArray 成员关系
```

因此由 Server RPC 执行。

---

## 函数与回调速查

| 函数或回调 | 作用 |
|---|---|
| `UInvSlottedItem::NativeOnMouseButtonDown` | 把物品 GridIndex 和鼠标事件发送给 Grid |
| `OnSlottedItemClicked` | 物品 Widget 点击委托 |
| `UInvGrid::OnSlottedItemClicked` | 处理左键、右键、堆叠和交换分支 |
| `PickUp` | 创建 HoverItem 并移除原 Grid 表现 |
| `AssignHoverItem` | 设置鼠标物品的图标、尺寸、数量和原位置 |
| `RemoveItemFromGrid` | 清除物品 Widget 和覆盖格子 |
| `CalculateHoveredCoordinates` | 将鼠标像素位置转换为 Grid 坐标 |
| `CalculateTileQuadrant` | 判断鼠标处于格子的哪个象限 |
| `CalculateStartingCoordinate` | 计算 HoverItem 的目标左上角 |
| `CheckHoverPosition` | 判断目标区域为空、覆盖一个物品或多个物品 |
| `HighlightSlots` | 预览空白放置区域 |
| `ChangeHoverType` | 将交换目标显示为灰色 |
| `PutDownOnIndex` | 把 HoverItem 放到空白位置 |
| `IsSameStackable` | 判断两个可视堆叠是否属于同一逻辑物品 |
| `SwapStackCounts` | 交换两个可视堆叠的数量 |
| `ConsumeHoverItemStacks` | 将 HoverItem 全部并入目标堆叠 |
| `FillInStack` | 部分填充目标并保留 HoverItem 剩余量 |
| `SwapWithHoverItem` | 放下原 HoverItem 并拿起目标物品 |
| `CreateItemPopUp` | 创建右键菜单并绑定可用操作 |
| `OnPopUpMenuSplit` | 将一部分可视数量放入 HoverItem |
| `OnPopUpMenuDrop` | 拿起目标物品并进入丢弃链路 |
| `OnPopUpMenuConsume` | 本地数量减一并请求服务器消耗 |
| `DropItem` | 发送丢弃物品 Server RPC |
| `Server_DropItem` | 扣除权威数量并生成场景物品 |
| `SpawnDroppedItem` | 计算丢弃位置并调用 Manifest 生成 Actor |
| `Server_ConsumeItem` | 扣除权威数量并执行 Consumable Fragment |

---

## 技术亮点

### 点选与区域检测分离

普通点击由 `UInvSlottedItem` 自身 GridIndex 确定物品；拖拽期间则检查 HoverItem 将覆盖的完整二维区域，支持不同尺寸物品。

### FirstGridIndex 统一多格物品

多格物品的所有子格都映射到同一个左上角，使点击、查询、移除、堆叠和交换共享统一的物品身份。

### 鼠标象限改善偶数尺寸拖拽

系统把格子划分成四个象限，根据鼠标位置决定偶数尺寸物品的左上角偏移，使物品视觉中心跟随光标。

### HoverItem 支持连续交换

交换时把原 HoverItem 放入目标位置，并把目标物品变成新的 HoverItem，玩家可以连续整理多个物品而不必中断拖拽。

### 逻辑总数与可视堆叠分离

同一个 `UInvItem` 可以对应多个可视堆叠位置。拆分和合并只重新分配 UI 数量，不改变服务器逻辑总数。

### 上下文菜单按物品能力裁剪

Split 只对可堆叠物品显示，Consume 只对消耗品显示，Drop 对所有有效物品提供，菜单内容由物品能力决定。

### Manifest 完成背包到场景的反向转换

丢弃时复用运行时 Manifest，通过 `PickupActorClass` 和 `InitItemManifest()`重建场景物品，保留物品类型、Fragment 数据和丢弃数量。

---

## 单元总结

背包中的普通点击由 `UInvSlottedItem` 携带的 GridIndex 直接确定物品；拖拽期间则根据鼠标格子、象限和 HoverItem 尺寸计算目标左上角，再通过目标矩形内唯一的 `FirstGridIndex` 集合判断空白放置、堆叠、交换或拒绝操作。

左键通过 `PickUp()`把物品转换为 HoverItem，并清除原 Grid 表现；目标为空时通过 `PutDownOnIndex()`重新创建 Widget 和格子占用；目标是同一逻辑可堆叠物品时重新分配可视数量；目标是其他单一物品时通过 `SwapWithHoverItem()`完成连续交换。

右键菜单根据物品能力提供拆分、丢弃和消耗操作。拆分、合并和移动只改变本地布局；丢弃和消耗会修改 `TotalStackCount` 或 FastArray 成员，因此通过 Server RPC 执行。丢弃时，服务器复用 `FInvItemManifest` 生成新的场景 Actor，并把本次丢弃数量写入其 `UInvItemComponent`，完成从背包逻辑物品到场景可拾取物品的反向转换。

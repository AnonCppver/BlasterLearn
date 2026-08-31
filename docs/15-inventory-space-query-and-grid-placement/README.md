# 背包空间查询与物品落位

## 功能点介绍

当交互层取得场景物品的 `UInvItemComponent` 后，背包组件从 `UInvComponent::TryAddItem()`开始处理拾取请求。

本单元包含：

- 计算背包能够接收的物品数量。
- 检测多格物品是否越界或与已有物品冲突。
- 优先填充同类未满堆叠，再使用空白区域。
- 区分空间不足、加入已有逻辑物品和创建新逻辑物品。
- 通过 Server RPC 修改服务器权威背包。
- 处理场景物品的全部拾取和部分拾取。
- 通过本地委托或 FastArray 回调刷新 Grid UI。
- 创建物品 Widget，并标记其占用的全部格子。

本单元入口为：

```cpp
void UInvComponent::TryAddItem(
	UInvItemComponent* ItemComponent
)
```

玩家如何锁定场景物品以及如何触发交互不在本单元展开。

---

## 完整链路

```text
UInvComponent::TryAddItem
→ InventoryMenu 查询可用空间
→ 得到 FInvSlotAvailabilityResult
→ 从 FastArray 查找同类型 UInvItem
→ 判断空间不足 / 追加堆叠 / 创建新物品

空间不足
→ NoRoomInInventory
→ HUD 显示提示

已有同类可堆叠物品
→ OnStackChanged
→ Grid 立即更新
→ Server_AddStacksToItem
→ 服务器增加 TotalStackCount

新的逻辑物品
→ Server_AddNewItem
→ FastArray::AddEntry
→ Manifest 创建 UInvItem
→ MarkItemDirty
→ 客户端 PostReplicatedAdd
→ OnItemAdded
→ UInvGrid::AddItem

服务器处理场景物品
├─ Remainder == 0：PickedUp → Destroy
└─ Remainder > 0：更新场景 StackCount

Grid 最终落位
→ AddItemAtIndex
→ 创建 UInvSlottedItem
→ AddSlottedItemToCanvas
→ UpdateGridSlots
```

---

## UInvComponent::TryAddItem：拾取决策入口

```cpp
void UInvComponent::TryAddItem(
	UInvItemComponent* ItemComponent
)
{
	FInvSlotAvailabilityResult Result =
		InventoryMenu->HasRoomForItem(ItemComponent);

	UInvItem* FoundItem = InventoryList.FindFirstItemByType(
		ItemComponent->GetItemManifest().GetItemType()
	);
	Result.Item = FoundItem;

	if (Result.TotalRoomToFill == 0)
	{
		NoRoomInInventory.Broadcast();
		return;
	}

	if (Result.Item.IsValid() && Result.bStackable)
	{
		OnStackChanged.Broadcast(Result);
		Server_AddStacksToItem(
			ItemComponent,
			Result.TotalRoomToFill,
			Result.Remainder
		);
	}
	else if (Result.TotalRoomToFill > 0)
	{
		Server_AddNewItem(
			ItemComponent,
			Result.bStackable
				? Result.TotalRoomToFill
				: 0,
			Result.Remainder
		);
	}
}
```

这个函数不直接操作具体 GridSlot，而是负责组织以下数据流：

```text
空间查询结果
+
FastArray 中的同类型逻辑物品
+
场景物品组件
→ 选择后续处理分支
```

---

## 空间查询结果结构

空间查询使用两个结构保存结果。

### FInvSlotAvailability

表示一个可以接收物品的具体位置：

```cpp
USTRUCT()
struct FInvSlotAvailability
{
	GENERATED_BODY()

	int32 Index{ INDEX_NONE };
	int32 AmountToFill{ 0 };
	bool bItemAtIndex{ false };
};
```

字段含义：

| 字段 | 含义 |
|---|---|
| `Index` | 目标物品或目标区域左上角格子的下标 |
| `AmountToFill` | 这个位置本次可以接收的数量 |
| `bItemAtIndex` | 目标位置原本是否已有可继续堆叠的物品 |

### FInvSlotAvailabilityResult

保存一次完整查询的汇总结果：

```cpp
USTRUCT()
struct FInvSlotAvailabilityResult
{
	GENERATED_BODY()

	TWeakObjectPtr<UInvItem> Item;
	int32 TotalRoomToFill{ 0 };
	int32 Remainder{ 0 };
	bool bStackable{ false };
	TArray<FInvSlotAvailability> SlotAvailabilities;
};
```

字段含义：

| 字段 | 含义 |
|---|---|
| `Item` | FastArray 中已有的同类型逻辑 `UInvItem` |
| `TotalRoomToFill` | 背包本次总共能够接收的数量 |
| `Remainder` | 背包无法接收、需要继续留在场景中的数量 |
| `bStackable` | Manifest 是否包含 `FInvStackableFragment` |
| `SlotAvailabilities` | 每个目标位置的下标和填充数量 |

例如场景中存在 12 个材料，已有堆叠还能接收 3 个，另一个空位置还能接收 5 个：

```text
SlotAvailabilities
├─ Index 4：AmountToFill = 3，bItemAtIndex = true
└─ Index 8：AmountToFill = 5，bItemAtIndex = false

TotalRoomToFill = 8
Remainder = 4
bStackable = true
```

---

## 空间查询所使用的物品数据

### FInvGridFragment

提供物品在空间背包中占用的二维尺寸：

```cpp
UPROPERTY(EditAnywhere, Category = "Inventory")
FIntPoint GridSize{ 1, 1 };
```

例如：

```text
1 × 1：占用一个格子
2 × 1：横向占用两个格子
1 × 3：纵向占用三个格子
2 × 2：占用四个格子
```

没有 Grid Fragment 时使用默认尺寸：

```cpp
FIntPoint(1, 1)
```

### FInvStackableFragment

Fragment 存在即表示物品可堆叠：

```cpp
UPROPERTY(EditAnywhere, Category = "Inventory")
int32 MaxStackSize{ 1 };

UPROPERTY(EditAnywhere, Category = "Inventory")
int32 StackCount{ 1 };
```

其中：

```text
StackCount
→ 场景物品当前携带的数量

MaxStackSize
→ 一个可视堆叠位置能够容纳的最大数量
```

没有 Stackable Fragment 时：

```text
bStackable = false
AmountToFill = 1
MaxStackSize = 1
```

### ItemType GameplayTag

同类堆叠使用 Manifest 的 ItemType，并通过：

```cpp
MatchesTagExact(ItemType)
```

要求两个物品的类型标签完全一致。

---

## UInvGridSlot 保存的空间状态

每个 `UInvGridSlot` 保存：

```text
TileIndex
StackCount
FirstGridIndex
bAvailable
InventoryItem
```

### TileIndex

格子在 `GridSlots` 一维数组中的下标。

### InventoryItem

指向当前占用该格子的逻辑 `UInvItem`。

一个多格物品占用的所有格子都会保存同一个 `UInvItem`。

### FirstGridIndex

记录物品左上角格子的下标。

例如一个 `2×2` 物品从 Index 6 开始：

```text
6   7
12  13
```

四个格子的 `FirstGridIndex` 都是 6。

### StackCount

可堆叠物品的可视数量主要记录在左上角格子。

### bAvailable

表示该格子是否为空闲。

物品落位后，其覆盖范围内的所有格子都会设置为不可用。

---

## 初始化空间查询

核心查询开始时创建空结果：

```cpp
FInvSlotAvailabilityResult Result{};
```

然后取得 Stackable Fragment：

```cpp
const FInvStackableFragment* StackableFragment =
	Manifest.GetFragmentOfType<FInvStackableFragment>();
```

设置是否可堆叠：

```cpp
Result.bStackable = StackableFragment != nullptr;
```

计算需要安排的数量：

```cpp
int32 AmountToFill = Result.bStackable
	? StackableFragment->GetStackCount()
	: 1;
```

计算单格堆叠上限：

```cpp
const int32 MaxStackSize = StackableFragment
	? StackableFragment->GetMaxStackSize()
	: 1;
```

---

## 按顺序寻找候选位置

空间查询按照 `GridSlots` 的顺序执行 first-fit 搜索：

```cpp
TSet<int32> CheckedIndices;

for (const UInvGridSlot* GridSlot : GridSlots)
{
	if (AmountToFill == 0) break;

	if (CheckedIndices.Contains(GridSlot->GetTileIndex()))
	{
		continue;
	}

	// 检查当前格子能否作为目标区域左上角
}
```

`CheckedIndices` 记录本次查询已经正式使用的格子，防止同一个位置被多个 `FInvSlotAvailability` 重复占用。

查询会优先使用数组中较靠前的有效位置。当物品数量大于单个堆叠上限时，会继续向后寻找更多位置，直到：

```text
AmountToFill == 0
```

或者遍历完全部格子。

---

## GetItemDimensions：取得二维尺寸

```cpp
FIntPoint UInvGrid::GetItemDimensions(
	const FInvItemManifest& Manifest
) const
{
	const FInvGridFragment* GridFragment =
		Manifest.GetFragmentOfType<FInvGridFragment>();

	return GridFragment
		? GridFragment->GetGridSize()
		: FIntPoint(1, 1);
}
```

返回值用于：

- 检查右侧和底部边界。
- 检查候选矩形中的全部格子。
- 最终标记物品占用区域。
- 计算物品 Widget 的绘制尺寸。

---

## IsInGridBounds：二维边界检测

```cpp
bool UInvGrid::IsInGridBounds(
	const int32 StartIndex,
	const FIntPoint& ItemDimensions
) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num())
	{
		return false;
	}

	const int32 EndColumn =
		(StartIndex % Col) + ItemDimensions.X;

	const int32 EndRow =
		(StartIndex / Col) + ItemDimensions.Y;

	return EndColumn <= Col && EndRow <= Row;
}
```

这里同时验证：

```text
起始下标有效
物品右边缘没有超过列数
物品下边缘没有超过行数
```

它可以阻止数组下标仍然有效，但物品从一行末尾错误跨入下一行的情况。

---

## TentativelyClaimed：候选区域的试探式提交

每次尝试一个起始位置时创建：

```cpp
TSet<int32> TentativelyClaimed;
```

然后调用：

```cpp
HasRoomAtIndex(
	GridSlot,
	Dimensions,
	CheckedIndices,
	TentativelyClaimed,
	Manifest.GetItemType(),
	MaxStackSize
);
```

`TentativelyClaimed` 表示当前候选矩形暂时通过的格子。

只有整个矩形全部通过后，才提交到正式集合：

```cpp
CheckedIndices.Append(TentativelyClaimed);
```

执行过程为：

```text
开始检查候选矩形
→ 把通过的格子放入 TentativelyClaimed
→ 所有格子都通过
   → 合并到 CheckedIndices
→ 任意格子失败
   → 放弃整个临时集合
```

这样可以避免失败候选位置污染后续搜索结果。

---

## HasRoomAtIndex：检查整个候选矩形

```cpp
bool UInvGrid::HasRoomAtIndex(
	const UInvGridSlot* GridSlot,
	const FIntPoint& Dimensions,
	const TSet<int32>& CheckedIndices,
	TSet<int32>& OutTentativelyClaimed,
	const FGameplayTag& ItemType,
	const int32 MaxStackSize
)
```

函数通过：

```cpp
UInvUtils::ForEach2D(
	GridSlots,
	GridSlot->GetTileIndex(),
	Dimensions,
	Col,
	Callback
);
```

遍历物品候选区域内的全部格子。

只要任意格子不满足条件：

```cpp
bHasRoomAtIndex = false;
```

最终只有整个矩形均通过时才返回 `true`。

---

## UInvUtils::ForEach2D：二维区域遍历

```cpp
for (int32 j = 0; j < Range2D.Y; ++j)
{
	for (int32 i = 0; i < Range2D.X; ++i)
	{
		const FIntPoint Coordinates =
			GetPositionFromIndex(Index, GridColumns) +
			FIntPoint(i, j);

		const int32 TileIndex =
			GetIndexFromPosition(
				Coordinates,
				GridColumns
			);

		if (Array.IsValidIndex(TileIndex))
		{
			Function(Array[TileIndex]);
		}
	}
}
```

一维下标与二维坐标之间的转换为：

```cpp
Index = Position.Y * Col + Position.X;
```

```cpp
Position = FIntPoint{
	Index % Col,
	Index / Col
};
```

这个工具函数让空间检测和最终格子占用可以复用同一套矩形遍历方式。

---

## CheckSlotConstraints：单格通过规则

每个候选子格进入：

```cpp
CheckSlotConstraints(
	GridSlot,
	SubGridSlot,
	CheckedIndices,
	OutTentativelyClaimed,
	ItemType,
	MaxStackSize
);
```

### 空格子

```cpp
if (!SubGridSlot->GetInventoryItem().IsValid())
{
	OutTentativelyClaimed.Add(
		SubGridSlot->GetTileIndex()
	);
	return true;
}
```

空格可以直接用于新物品。

### 已有物品

已有物品必须满足以下全部条件。

#### 候选起点对应已有物品左上角

```cpp
if (!IsUpperLeftSlot(GridSlot, SubGridSlot))
{
	return false;
}
```

内部比较：

```cpp
SubGridSlot->GetFirstGridIndex()
==
GridSlot->GetTileIndex()
```

这使多格物品以左上角格子作为唯一代表位置。

#### 已有物品可堆叠

```cpp
const UInvItem* SubItem =
	SubGridSlot->GetInventoryItem().Get();

if (!SubItem->IsStackable())
{
	return false;
}
```

#### ItemType 完全一致

```cpp
if (!DoesItemTypeMatch(SubItem, ItemType))
{
	return false;
}
```

内部调用：

```cpp
SubItem->GetItemManifest()
	.GetItemType()
	.MatchesTagExact(ItemType);
```

#### 现有堆叠没有达到上限

```cpp
if (GridSlot->GetStackCount() >= MaxStackSize)
{
	return false;
}
```

满足全部条件后，候选区域可以用于向已有物品追加数量。

---

## GetStackAmount：从左上角读取堆叠数

```cpp
int32 UInvGrid::GetStackAmount(
	const UInvGridSlot* GridSlot
) const
{
	int32 CurrentSlotStackCount =
		GridSlot->GetStackCount();

	const int32 UpperLeftIndex =
		GridSlot->GetFirstGridIndex();

	if (UpperLeftIndex != INDEX_NONE)
	{
		CurrentSlotStackCount =
			GridSlots[UpperLeftIndex]
				->GetStackCount();
	}

	return CurrentSlotStackCount;
}
```

多格物品的所有子格都指向同一个 `FirstGridIndex`，因此堆叠数统一从左上角取得。

---

## DetermineFillAmountForSlot：计算当前位置填充量

```cpp
const int32 RoomInSlot =
	MaxStackSize - GetStackAmount(GridSlot);
```

可堆叠物品返回：

```cpp
FMath::Min(
	AmountToFill,
	RoomInSlot
);
```

不可堆叠物品返回：

```cpp
1
```

例如：

```text
场景剩余数量 AmountToFill = 8
单格最大数量 MaxStackSize = 5
当前格子已有 3

RoomInSlot = 2
AmountToFillForThisSlot = min(8, 2) = 2
```

当前位置填入 2 个，剩余 6 个继续寻找后续位置。

---

## 生成 SlotAvailabilities

候选区域通过且填充量大于 0 后：

```cpp
Result.TotalRoomToFill +=
	AmountToFillForThisSlot;
```

判断目标是否已有物品：

```cpp
const bool HasValidItem =
	GridSlot->GetInventoryItem().IsValid();
```

保存目标位置：

```cpp
Result.SlotAvailabilities.Emplace(
	FInvSlotAvailability{
		HasValidItem
			? GridSlot->GetFirstGridIndex()
			: GridSlot->GetTileIndex(),
		Result.bStackable
			? AmountToFillForThisSlot
			: 0,
		HasValidItem
	}
);
```

更新剩余数量：

```cpp
AmountToFill -= AmountToFillForThisSlot;
Result.Remainder = AmountToFill;
```

如果全部数量均已安排：

```cpp
if (AmountToFill == 0)
{
	return Result;
}
```

否则继续遍历后面的格子。

---

## FindFirstItemByType：取得同类型逻辑物品

空间查询结束后，`TryAddItem()`从 FastArray 查询：

```cpp
UInvItem* FoundItem =
	InventoryList.FindFirstItemByType(
		ItemComponent
			->GetItemManifest()
			.GetItemType()
	);
```

FastArray 内部使用：

```cpp
Entries.FindByPredicate(
	[&ItemType](const FInvEntry& Entry)
	{
		return IsValid(Entry.Item) &&
			Entry.Item->GetItemManifest()
				.GetItemType()
				.MatchesTagExact(ItemType);
	}
);
```

这个查询不决定物品放在哪个格子，而是决定是否已有同类型逻辑 `UInvItem` 可以继续承载总数量。

职责划分为：

```text
SlotAvailabilities
→ 具体格子布局

Result.Item
→ 对应的逻辑 UInvItem
```

可堆叠物品可以由一个逻辑 `UInvItem` 对应多个可视堆叠位置；不可堆叠物品每次都进入新物品分支。

---

## 空间不足分支

```cpp
if (Result.TotalRoomToFill == 0)
{
	NoRoomInInventory.Broadcast();
	return;
}
```

此时：

```text
不发送 Server RPC
不修改 FastArray
不修改场景物品
不创建 Grid UI
```

`UCharacterOverlay::NativeOnInitialized()`绑定：

```cpp
InventoryComponent->NoRoomInInventory.AddDynamic(
	this,
	&UCharacterOverlay::OnNoRoom
);
```

广播后的调用链为：

```text
NoRoomInInventory.Broadcast
→ UCharacterOverlay::OnNoRoom
→ InfoMessage::SetMessage
→ 显示“背包空间不足”
```

---

## 已有同类可堆叠物品分支

进入条件：

```cpp
if (Result.Item.IsValid() && Result.bStackable)
```

先执行本地 UI 通知：

```cpp
OnStackChanged.Broadcast(Result);
```

再发送服务器 RPC：

```cpp
Server_AddStacksToItem(
	ItemComponent,
	Result.TotalRoomToFill,
	Result.Remainder
);
```

因此顺序为：

```text
客户端根据本地空间查询立即刷新 Grid
→ 向服务器请求增加逻辑总数量
```

### OnStackChanged 回调链

`UInvGrid::NativeOnInitialized()`已经绑定：

```cpp
InvComponent->OnStackChanged.AddDynamic(
	this,
	&UInvGrid::AddStacks
);
```

调用链：

```text
TryAddItem
→ OnStackChanged.Broadcast(Result)
→ UInvGrid::AddStacks(Result)
```

### UInvGrid::AddStacks

首先过滤 Grid Category：

```cpp
if (!MatchesCategory(Result.Item.Get()))
{
	return;
}
```

然后遍历每个目标位置。

#### 目标位置已经有物品

```cpp
if (Availability.bItemAtIndex)
```

同时更新物品 Widget 和左上角 GridSlot：

```cpp
SlottedItem->UpdateStackCount(
	GridSlot->GetStackCount() +
	Availability.AmountToFill
);

GridSlot->SetStackCount(
	GridSlot->GetStackCount() +
	Availability.AmountToFill
);
```

#### 目标位置为空

```cpp
else
{
	AddItemAtIndex(...);
	UpdateGridSlots(...);
}
```

已有逻辑 `UInvItem` 可以继续在新的空位置创建一个可视堆叠。

---

## Server_AddStacksToItem：服务器增加总数量

这是一个可靠 Server RPC：

```cpp
UFUNCTION(Server, Reliable)
void Server_AddStacksToItem(
	UInvItemComponent* ItemComponent,
	int32 StackCount,
	int32 Remainder
);
```

服务器重新从场景物品读取 ItemType：

```cpp
const FGameplayTag& ItemType = IsValid(ItemComponent)
	? ItemComponent->GetItemManifest().GetItemType()
	: FGameplayTag::EmptyTag;
```

在服务器权威 FastArray 中查找同类型逻辑物品：

```cpp
UInvItem* Item =
	InventoryList.FindFirstItemByType(ItemType);
```

增加总数量：

```cpp
Item->SetTotalStackCount(
	Item->GetTotalStackCount() + StackCount
);
```

该分支没有新增 FastArray 条目。变化发生在现有 `UInvItem` 的复制属性 `TotalStackCount` 上。

---

## 新逻辑物品分支

当背包不存在可复用的同类型可堆叠 `UInvItem` 时：

```cpp
Server_AddNewItem(
	ItemComponent,
	Result.bStackable
		? Result.TotalRoomToFill
		: 0,
	Result.Remainder
);
```

不可堆叠物品即使存在相同 ItemType，也会进入这个分支，为每件物品创建独立 FastArray 条目。

---

## Server_AddNewItem：服务器创建逻辑物品

服务器首先调用：

```cpp
UInvItem* NewItem =
	InventoryList.AddEntry(ItemComponent);
```

然后设置总堆叠数量：

```cpp
NewItem->SetTotalStackCount(StackCount);
```

监听服务器和单机不会接收自己的 FastArray 回调，因此主动广播：

```cpp
if (
	GetOwner()->GetNetMode() == NM_ListenServer ||
	GetOwner()->GetNetMode() == NM_Standalone
)
{
	OnItemAdded.Broadcast(NewItem);
}
```

调用链为：

```text
监听服务器 / 单机
→ Server_AddNewItem
→ OnItemAdded.Broadcast
→ UInvGrid::AddItem
```

普通客户端则由 FastArray 的 `PostReplicatedAdd()`触发同一个委托。

---

## FInvFastArray::AddEntry：创建 FastArray 条目

服务器创建默认条目：

```cpp
FInvEntry& NewEntry =
	Entries.AddDefaulted_GetRef();
```

从场景 Manifest 创建运行时物品：

```cpp
NewEntry.Item =
	ItemComponent
		->GetItemManifest()
		.Manifest(OwningActor);
```

注册复制子对象：

```cpp
IC->AddRepSubobj(NewEntry.Item);
```

标记 FastArray 新条目：

```cpp
MarkItemDirty(NewEntry);
```

最终返回：

```cpp
return NewEntry.Item;
```

完整过程：

```text
Entries 创建 FInvEntry
→ Manifest 创建 UInvItem
→ UInvItem 进入 Registered Subobject List
→ MarkItemDirty 分配复制 ID 并标记增量
```

---

## FInvItemManifest::Manifest：生成运行时 UInvItem

```cpp
UInvItem* Item = NewObject<UInvItem>(
	NewOuter,
	UInvItem::StaticClass()
);
```

复制 Manifest：

```cpp
Item->SetItemManifest(*this);
```

遍历运行时 Manifest 中的 Fragment：

```cpp
for (
	auto& Fragment :
	Item->GetItemManifestMutable()
		.GetFragmentsMutable()
)
{
	Fragment.GetMutable().Manifest();
}
```

每个 Fragment 可以在这一阶段完成首次实例化，例如生成随机数值。

然后清理 Manifest 工作副本中的 Fragment：

```cpp
ClearFragments();
```

最后返回运行时 `UInvItem`。

---

## 场景物品全部拾取与部分拾取

两个 Server RPC 最后都根据 `Remainder` 处理场景物品。

### 全部拾取

```cpp
if (Remainder == 0)
{
	ItemComponent->PickedUp();
}
```

`UInvItemComponent::PickedUp()`执行：

```cpp
OnPickedUp();
GetOwner()->Destroy();
```

调用顺序：

```text
服务器确认场景数量全部进入背包
→ UInvItemComponent::PickedUp
→ BlueprintImplementableEvent OnPickedUp
→ 销毁场景 Actor
→ Actor 销毁复制到客户端
```

### 部分拾取

当 `Remainder > 0` 时取得场景物品的可堆叠 Fragment：

```cpp
FInvStackableFragment* StackableFragment =
	ItemComponent->GetItemManifestMutable()
		.GetFragmentOfTypeMutable<FInvStackableFragment>();
```

更新场景剩余量：

```cpp
StackableFragment->SetStackCount(Remainder);
```

场景 Actor 不会销毁，玩家可以在背包腾出空间后再次拾取剩余部分。

---

## FastArray PostReplicatedAdd：普通客户端收到新物品

服务器 `MarkItemDirty()`后，FastArray 增量数据复制到客户端。

客户端反序列化新增条目后调用：

```cpp
void FInvFastArray::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices,
	int32 FinalSize
)
```

函数取得 `UInvComponent`：

```cpp
UInvComponent* IC =
	Cast<UInvComponent>(OwnerComponent);
```

遍历新增下标并广播：

```cpp
for (int32 Index : AddedIndices)
{
	IC->OnItemAdded.Broadcast(
		Entries[Index].Item
	);
}
```

普通客户端通知链为：

```text
服务器 MarkItemDirty
→ FastArray 增量复制
→ 客户端创建 FInvEntry
→ PostReplicatedAdd
→ OnItemAdded.Broadcast
→ UInvGrid::AddItem
```

---

## UInvGrid::AddItem：接收新增逻辑物品

首先按 Grid Category 过滤：

```cpp
if (!MatchesCategory(Item))
{
	return;
}
```

然后根据客户端当前 Grid 状态取得最终落位结果：

```cpp
FInvSlotAvailabilityResult Result =
	HasRoomForItem(Item);
```

最后调用：

```cpp
AddItemToIndices(Result, Item);
```

---

## AddItemToIndices：遍历目标位置

```cpp
for (
	const auto& Availability :
	Result.SlotAvailabilities
)
{
	AddItemAtIndex(
		NewItem,
		Availability.Index,
		Result.bStackable,
		Availability.AmountToFill
	);

	UpdateGridSlots(
		NewItem,
		Availability.Index,
		Result.bStackable,
		Availability.AmountToFill
	);
}
```

每个 `SlotAvailability` 都会完成两件事：

```text
AddItemAtIndex
→ 创建物品可视 Widget

UpdateGridSlots
→ 写入实际格子占用状态
```

---

## AddItemAtIndex：创建物品 Widget

从 Manifest 获取：

```cpp
const FInvGridFragment* GridFragment =
	GetFragment<FInvGridFragment>(
		NewItem,
		FragmentTags::GridFragment
	);

const FInvImageFragment* ImageFragment =
	GetFragment<FInvImageFragment>(
		NewItem,
		FragmentTags::IconFragment
	);
```

创建 `UInvSlottedItem`：

```cpp
UInvSlottedItem* SlottedItem =
	CreateWidget<UInvSlottedItem>(
		GetOwningPlayer(),
		SlottedItemClass
	);
```

设置：

```text
逻辑 UInvItem
图标
左上角 GridIndex
是否可堆叠
当前可视堆叠数
点击回调
```

然后加入 CanvasPanel，并保存到：

```cpp
SlottedItems.Add(Index, SlottedItem);
```

---

## SetSlottedItemImage：设置图标和绘制尺寸

```cpp
FSlateBrush Brush;
Brush.SetResourceObject(ImageFragment->GetIcon());
Brush.DrawAs = ESlateBrushDrawType::Image;
Brush.ImageSize = GetDrawSize(GridFragment);
```

单格图标有效尺寸：

```cpp
IconTileWidth =
	Size - GridPadding * 2;
```

整个物品绘制尺寸：

```cpp
GridSize * IconTileWidth
```

---

## AddSlottedItemToCanvas：计算可视位置

将 Widget 加入 Canvas：

```cpp
CanvasPanel->AddChild(SlottedItem);
```

根据 Grid Index 取得二维坐标：

```cpp
GetPositionFromIndex(Index, Col)
```

计算像素位置：

```cpp
DrawPos = GridCoordinate * Size;
```

加入 Padding：

```cpp
DrawPosWithPadding =
	DrawPos + FVector2D(GridPadding);
```

最后设置 CanvasSlot 的尺寸与位置。

---

## UpdateGridSlots：写入占用状态

如果物品可堆叠，先在左上角格子保存数量：

```cpp
GridSlots[Index]->SetStackCount(
	StackAmount
);
```

随后遍历物品覆盖的二维区域：

```cpp
UInvUtils::ForEach2D(
	GridSlots,
	Index,
	Dimensions,
	Col,
	Callback
);
```

每个占用格子执行：

```cpp
GridSlot->SetInventoryItem(NewItem);
GridSlot->SetFirstGridIndex(Index);
GridSlot->SetAvailable(false);
GridSlot->SetOccupiedTexture();
```

最终形成：

```text
UInvSlottedItem
→ 负责显示图标和数量

SlottedItems[Index]
→ 通过左上角 Index 管理物品 Widget

GridSlots 覆盖区域
→ 保存 UInvItem、FirstGridIndex 和占用状态
```

---

## 网络端完整时序

### 新逻辑物品

```text
本地客户端
│
├─ UInvComponent::TryAddItem
├─ 计算 FInvSlotAvailabilityResult
├─ 未找到可复用的同类型可堆叠 UInvItem
└─ Server_AddNewItem RPC
      │
      ▼
服务器
│
├─ FInvFastArray::AddEntry
├─ FInvItemManifest::Manifest
├─ 创建并注册 UInvItem
├─ MarkItemDirty
├─ SetTotalStackCount
└─ 处理 Remainder
      │
      ▼
普通客户端
│
├─ FastArray::PostReplicatedAdd
├─ OnItemAdded.Broadcast
├─ UInvGrid::AddItem
├─ AddItemToIndices
├─ AddItemAtIndex
└─ UpdateGridSlots
```

### 加入已有可堆叠物品

```text
本地客户端
│
├─ UInvComponent::TryAddItem
├─ 找到同类型 UInvItem
├─ OnStackChanged.Broadcast
├─ UInvGrid::AddStacks
└─ Server_AddStacksToItem RPC
      │
      ▼
服务器
│
├─ FindFirstItemByType
├─ 增加 UInvItem::TotalStackCount
└─ 处理 Remainder
```

该分支没有新增 FastArray 条目，因此不会通过 `PostReplicatedAdd()`创建新的逻辑物品。

---

## 函数与通知速查

| 函数或通知 | 执行端 | 作用 |
|---|---|---|
| `UInvComponent::TryAddItem` | 本地玩家 | 组织空间结果并选择处理分支 |
| `HasRoomForItem` | 本地玩家 | 计算可放位置、数量和剩余量 |
| `GetItemDimensions` | 本地玩家 | 从 Grid Fragment 取得物品尺寸 |
| `IsInGridBounds` | 本地玩家 | 检查候选区域是否越界 |
| `HasRoomAtIndex` | 本地玩家 | 检查整个候选矩形 |
| `CheckSlotConstraints` | 本地玩家 | 检查空格或同类未满堆叠 |
| `GetStackAmount` | 本地玩家 | 从左上角读取当前堆叠数 |
| `DetermineFillAmountForSlot` | 本地玩家 | 计算单个位置可接收数量 |
| `FindFirstItemByType` | 调用方所在端 | 查找同类型逻辑 `UInvItem` |
| `NoRoomInInventory.Broadcast` | 本地玩家 | 通知 HUD 显示空间不足 |
| `OnStackChanged.Broadcast` | 本地玩家 | 通知 Grid 立即更新已有堆叠 |
| `UInvGrid::AddStacks` | 本地玩家 | 更新已有位置或创建新的可视堆叠 |
| `Server_AddStacksToItem` | 服务器 RPC | 增加服务器权威总数量 |
| `Server_AddNewItem` | 服务器 RPC | 创建新的逻辑背包物品 |
| `FInvFastArray::AddEntry` | 服务器 | 创建 FastArray 条目与 `UInvItem` |
| `FInvItemManifest::Manifest` | 服务器 | 将配置态 Manifest 实例化为运行时物品 |
| `MarkItemDirty` | 服务器 | 标记新 FastArray 条目需要同步 |
| `UInvItemComponent::PickedUp` | 服务器 | 触发蓝图事件并销毁场景 Actor |
| `PostReplicatedAdd` | 普通客户端 | 接收新增 FastArray 条目 |
| `OnItemAdded.Broadcast` | 客户端或监听服务器 | 通知 Grid 创建新物品 UI |
| `UInvGrid::AddItem` | 本地玩家 | 为新增逻辑物品重新计算最终落位 |
| `AddItemToIndices` | 本地玩家 | 遍历所有目标位置 |
| `AddItemAtIndex` | 本地玩家 | 创建 `UInvSlottedItem` |
| `AddSlottedItemToCanvas` | 本地玩家 | 设置 Widget 尺寸和位置 |
| `UpdateGridSlots` | 本地玩家 | 标记物品覆盖区域 |

---

## 技术亮点

### Fragment 驱动空间规则

物品尺寸和堆叠规则由 `FInvGridFragment` 与 `FInvStackableFragment` 组合提供，不需要为每种物品编写独立的空间检测代码。

### 二维不规则物品放置

一维 `GridSlots` 通过下标和坐标转换支持二维矩形检测，可以处理 `1×1`、`2×1`、`2×2` 等不同尺寸物品。

### 试探式区域提交

`TentativelyClaimed` 先保存候选矩形，整个区域通过后才合并到 `CheckedIndices`，保证失败查询不会污染后续结果。

### 多格物品统一映射

所有占用格子通过 `FirstGridIndex` 指向左上角，使堆叠数量、Widget 映射和后续交互可以使用统一代表位置。

### 一次结果表达完整拾取状态

`FInvSlotAvailabilityResult` 同时表达已有堆叠、空白位置、总可接收量与场景剩余量，使 UI 更新和 Server RPC 可以共享同一次查询结果。

### 逻辑物品与可视位置分离

FastArray 和 `UInvItem` 维护逻辑背包数据；`UInvGrid`、`UInvGridSlot` 和 `UInvSlottedItem` 维护具体空间布局和显示。

### 分端通知 UI

普通客户端通过 FastArray 的 `PostReplicatedAdd()`刷新新物品，监听服务器和单机通过服务器主动广播 `OnItemAdded`，最终统一进入 `UInvGrid::AddItem()`。

---

## 单元总结

背包拾取从 `UInvComponent::TryAddItem()`开始。客户端首先计算物品尺寸、可堆叠状态、已有堆叠容量和空白矩形区域，生成包含目标位置、可接收数量与场景剩余量的 `FInvSlotAvailabilityResult`。

如果没有空间，`NoRoomInInventory` 通知 HUD；如果背包已经存在同类型可堆叠 `UInvItem`，`OnStackChanged` 立即刷新 Grid，随后服务器增加逻辑总数量；否则通过 `Server_AddNewItem` 创建新的 `UInvItem` 和 FastArray 条目。

服务器根据 `Remainder` 决定销毁场景物品，或者保留 Actor 并更新剩余数量。新 FastArray 条目复制到普通客户端后触发 `PostReplicatedAdd → OnItemAdded → UInvGrid::AddItem`；监听服务器则直接广播同一事件。最终 `AddItemAtIndex()`创建物品 Widget，`UpdateGridSlots()`把逻辑物品、左上角索引和占用状态写入整个二维区域，完成从拾取决策到背包 UI 落位的完整链路。

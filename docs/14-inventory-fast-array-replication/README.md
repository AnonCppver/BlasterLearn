# FastArray 背包条目增量复制

## 功能点介绍

背包中的物品集合需要在服务器与客户端之间保持一致。物品被拾取、丢弃或消耗时，背包数组会发生新增和删除；在多人游戏中，如果每次变化都重新发送整个数组，会产生不必要的网络数据。

本项目使用 Unreal Engine 的 `FFastArraySerializer` 管理背包条目：

```text
UInvComponent
└─ Replicated FInvFastArray InventoryList
   └─ TArray<FInvEntry> Entries
      └─ TObjectPtr<UInvItem> Item
```

FastArray 负责同步“背包中有哪些物品”，每个 `UInvItem` 则作为可复制子对象，同步自己的 Manifest 和堆叠数量。

本单元只介绍现有 FastArray 的结构、操作含义与复制流程，不涉及物品如何寻找背包空位，也不涉及拖拽、拆分、丢弃和消耗等背包交互。

---

## 普通复制数组与 FastArray 的区别

### 普通复制 TArray

普通复制数组可以直接声明为：

```cpp
UPROPERTY(Replicated)
TArray<FInvEntry> Entries;
```

它适合以下情况：

- 数组规模较小。
- 数组很少发生改变。
- 不需要分别处理某个元素的新增、删除和修改通知。
- 数组结构变化不会频繁发生在中间位置。

普通属性复制主要从数组位置和属性变化角度比较状态。当数组中间的元素被删除时，后续元素的下标会向前移动；从数组状态角度看，多个位置都可能发生变化。

因此，普通复制数组可以完成同步，但不擅长表达：

```text
“ID 为 12 的条目被删除，其他条目没有变化。”
```

它更接近表达：

```text
“这个数组的长度和若干数组位置发生了变化。”
```

### FastArray

FastArray 为每个条目分配稳定的 `ReplicationID`，并为条目和数组维护变化版本。服务器不依赖元素下标识别物品，而是比较：

```text
ReplicationID → ReplicationKey
```

从而将变化划分为：

```text
旧状态没有该 ID，当前状态存在
→ 新增

旧状态存在该 ID，ReplicationKey 发生变化
→ 修改

旧状态存在该 ID，当前状态不再存在
→ 删除
```

FastArray 更适合：

- 背包物品列表。
- 技能列表。
- 状态效果列表。
- 任务列表。
- 频繁新增或删除元素的多人游戏集合。

### 对比

| 对比项 | 普通复制 TArray | FastArray |
|---|---|---|
| 条目身份 | 更依赖数组位置 | 使用稳定的 `ReplicationID` |
| 增量依据 | 数组和属性状态变化 | 条目 ID 与版本号变化 |
| 中间删除 | 后续下标变化可能产生更多差异 | 直接发送被删除条目的 ID |
| 新增通知 | 通常通过 `OnRep` 后自行比较 | `PostReplicatedAdd` |
| 删除通知 | 通常通过 `OnRep` 后自行比较 | `PreReplicatedRemove` |
| 修改通知 | 通常通过 `OnRep` 后自行比较 | `PostReplicatedChange` |
| 标记要求 | 由普通属性复制系统追踪 | 游戏代码必须调用 Dirty 接口 |
| 元素顺序 | 通常直接按照数组位置理解 | 不保证客户端与服务器顺序始终完全一致 |
| 使用复杂度 | 较低 | 需要遵循 FastArray 合约 |
| 适用场景 | 小型、低频变化集合 | 频繁增删、需要条目级事件的集合 |

FastArray 的主要收益是减少结构变化时的网络数据，并直接提供新增、删除和修改回调。它仍然需要服务器计算数组差异，因此不能简单理解为所有 CPU 工作都只与变化条目数量有关。

---

## FInvEntry：可单独追踪的背包条目

项目中的条目定义为：

```cpp
USTRUCT(BlueprintType)
struct FInvEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	friend UInvComponent;
	friend FInvFastArray;

private:
	UPROPERTY()
	TObjectPtr<UInvItem> Item = nullptr;
};
```

继承 `FFastArraySerializerItem` 后，条目获得 FastArray 内部使用的三个状态：

```text
ReplicationID
ReplicationKey
MostRecentArrayReplicationKey
```

### ReplicationID

`ReplicationID` 是条目的稳定复制身份。

首次对新条目调用 `MarkItemDirty()` 时，FastArray 会为它分配一个 ID。之后即使该条目在本地数组中的下标发生改变，网络系统仍然通过这个 ID 识别它。

```text
服务器 Entries[2]：ReplicationID = 7
客户端 Entries[5]：ReplicationID = 7
```

两端下标可以不同，但它们表示的是同一个网络条目。

### ReplicationKey

`ReplicationKey` 表示单个条目的变化版本。

每次调用：

```cpp
MarkItemDirty(Entry);
```

该条目的 `ReplicationKey` 都会增加。服务器把当前 Key 与该连接上次确认的 Key 比较，从而判断这个条目是否需要重新发送。

### MostRecentArrayReplicationKey

`MostRecentArrayReplicationKey` 用于记录条目最近一次关联的数组复制版本，辅助 FastArray 在接收端判断条目的新增、删除和历史状态。

这些字段属于 FastArray 的内部协议状态。业务代码不使用它们表达格子编号、物品数量或物品类型。

---

## FInvFastArray：背包数组序列化器

项目中的数组结构为：

```cpp
USTRUCT(BlueprintType)
struct FInvFastArray : public FFastArraySerializer
{
	GENERATED_BODY()

	FInvFastArray()
		: OwnerComponent(nullptr)
	{
	}

	FInvFastArray(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

private:
	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;

	UPROPERTY()
	TArray<FInvEntry> Entries;
};
```

### Entries

`Entries` 是 FastArray 实际管理和同步的条目集合。

其中每个 `FInvEntry` 保存一个 `UInvItem` 引用。FastArray 对条目引用的新增和删除进行增量同步。

### OwnerComponent

`OwnerComponent` 指向持有该数组的 `UInvComponent`：

```cpp
UInvComponent::UInvComponent()
	: InventoryList(this)
{
}
```

它的作用不是参与网络复制，而是在客户端复制回调中找到背包组件，并广播：

```text
OnItemAdded
OnItemRemoved
```

因此它被标记为：

```cpp
UPROPERTY(NotReplicated)
```

---

## 启用 FastArray 增量序列化

### NetDeltaSerialize

`FInvFastArray` 将 `Entries` 交给 UE 的 FastArray 模板函数：

```cpp
bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FastArrayDeltaSerialize<
		FInvEntry,
		FInvFastArray
	>(Entries, DeltaParams, *this);
}
```

这个函数负责：

1. 读取当前数组条目的 ID 和 Key。
2. 取得该网络连接上一次保存的数组状态。
3. 计算新增、修改和删除条目。
4. 写入或读取增量数据。
5. 在客户端调用对应的复制回调。

### WithNetDeltaSerializer

还需要通过 Struct Traits 告诉 UE，该结构使用自定义增量序列化：

```cpp
template<>
struct TStructOpsTypeTraits<FInvFastArray>
	: public TStructOpsTypeTraitsBase2<FInvFastArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};
```

如果没有这个 Traits 标记，UE 不会将 `NetDeltaSerialize()` 作为该结构的增量网络序列化入口。

### 将 FastArray 放入复制属性链

FastArray 自身实现增量序列化后，仍然必须作为复制属性存在：

```cpp
UPROPERTY(Replicated)
FInvFastArray InventoryList;
```

并在组件中注册：

```cpp
void UInvComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, InventoryList);
}
```

完整关系为：

```text
UInvComponent 参与网络复制
└─ InventoryList 是 Replicated 属性
   └─ FInvFastArray 启用 NetDeltaSerialize
      └─ Entries 使用 FastArrayDeltaSerialize
```

---

## MarkItemDirty：标记新增或修改条目

FastArray 不会因为执行了 `Entries.Add()` 就自动知道游戏代码希望同步这个条目。新增或修改条目后，必须调用：

```cpp
MarkItemDirty(Entry);
```

它的主要含义是：

```text
如果条目没有 ReplicationID
→ 为它分配新的 ReplicationID

增加条目的 ReplicationKey
→ 表示条目版本发生变化

增加 ArrayReplicationKey
→ 表示整个数组存在需要检查的变化
```

因此同一个操作可以表示两种情况。

### 新增条目

```cpp
FInvEntry& NewEntry = Entries.AddDefaulted_GetRef();
NewEntry.Item = NewItem;
MarkItemDirty(NewEntry);
```

因为 `NewEntry` 还没有复制 ID，`MarkItemDirty()` 会将其识别为新条目。

### 修改已有条目

如果将来直接修改 `FInvEntry` 内部的复制字段，同样调用：

```cpp
MarkItemDirty(ExistingEntry);
```

该条目已经拥有 ID，因此只增加 Key，接收端会把它识别为已有条目的修改。

当前项目中 `FInvEntry` 只保存稳定的 `UInvItem` 引用，`MarkItemDirty()` 主要用于新增条目。

---

## MarkArrayDirty：标记数组结构变化

删除条目时，原条目已经不再存在，无法再把它传给 `MarkItemDirty()`，因此使用：

```cpp
MarkArrayDirty();
```

它的主要含义是：

```text
增加 ArrayReplicationKey
→ 通知网络系统重新比较数组状态

重置内部条目映射缓存
→ 下一次序列化时重新建立 ID 到下标的对应关系
```

当前删除代码为：

```cpp
void FInvFastArray::RemoveEntry(UInvItem* Item)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FInvEntry& Entry = *EntryIt;
		if (Entry.Item == Item)
		{
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
		}
	}
}
```

下一次增量序列化时，服务器将旧的 ID 集合与当前 ID 集合比较：

```text
旧状态：ID 1、ID 2、ID 3
当前状态：ID 1、ID 3

结果：ID 2 被删除
```

网络中传递的是被删除条目的 ID，而不是要求客户端删除一个固定数组下标。

---

## PreReplicatedRemove：客户端删除前通知

项目实现了 FastArray 的删除前回调：

```cpp
void FInvFastArray::PreReplicatedRemove(
	const TArrayView<int32> RemovedIndices,
	int32 FinalSize
)
{
	UInvComponent* IC = Cast<UInvComponent>(OwnerComponent);
	if (!IsValid(IC)) return;

	for (int32 Index : RemovedIndices)
	{
		IC->OnItemRemoved.Broadcast(Entries[Index].Item);
	}
}
```

它在客户端真正删除数组条目之前执行。

此时：

- 被删除的条目仍然存在于 `Entries` 中。
- `RemovedIndices` 可以用于访问对应的 `UInvItem`。
- 可以在条目消失前通知 UI 或其他本地系统。

调用顺序为：

```text
客户端收到删除 ID
→ 将 ID 映射为当前本地下标
→ PreReplicatedRemove
→ OnItemRemoved.Broadcast(Item)
→ 从客户端 Entries 删除条目
```

`RemovedIndices` 只在本次回调期间有效。FastArray 不保证删除后其他元素仍然保持相同下标。

`FinalSize` 表示本次复制应用完成后的数组大小，当前实现没有使用它。

---

## PostReplicatedAdd：客户端新增后通知

项目实现了新增完成回调：

```cpp
void FInvFastArray::PostReplicatedAdd(
	const TArrayView<int32> AddedIndices,
	int32 FinalSize
)
{
	UInvComponent* IC = Cast<UInvComponent>(OwnerComponent);
	if (!IsValid(IC)) return;

	for (int32 Index : AddedIndices)
	{
		IC->OnItemAdded.Broadcast(Entries[Index].Item);
	}
}
```

它在客户端已经创建并反序列化新增条目后执行。

调用链为：

```text
服务器新增 FInvEntry
→ MarkItemDirty
→ FastArray 发送新增条目
→ 客户端创建 FInvEntry
→ PostReplicatedAdd
→ OnItemAdded.Broadcast(Item)
→ UInvGrid::AddItem
→ 在背包 UI 中显示物品
```

`AddedIndices` 表示本次新增条目在客户端当前数组中的下标。它用于读取新增条目，不应被当作物品的长期稳定身份。

`FinalSize` 表示本次复制应用完成后的数组大小，当前实现没有使用它。

---

## PostReplicatedChange：客户端修改后通知

FastArray 还支持：

```cpp
void PostReplicatedChange(
	const TArrayView<int32> ChangedIndices,
	int32 FinalSize
);
```

它的含义是：

```text
客户端原本已经存在某个 ReplicationID
且服务器发送了该条目的新版本
→ 条目反序列化完成
→ PostReplicatedChange
```

当前 `FInvFastArray` 没有实现这个回调，因为条目本身只保存 `UInvItem` 引用。物品 Manifest 和堆叠数量属于 `UInvItem` 子对象的复制属性，不属于 `FInvEntry` 自身的字段变化。

因此现有职责是：

```text
FastArray 回调
├─ 新增 UInvItem 引用
└─ 删除 UInvItem 引用

UInvItem 属性复制
├─ ItemManifest
└─ TotalStackCount
```

---

## AddEntry(UInvItemComponent*)：从场景物品创建背包条目

服务器拾取一个新的场景物品时调用：

```cpp
UInvItem* FInvFastArray::AddEntry(
	UInvItemComponent* ItemComponent
)
```

### 1. 检查拥有者和服务器权限

```cpp
check(OwnerComponent);
AActor* OwningActor = OwnerComponent->GetOwner();
check(OwningActor->HasAuthority());
```

FastArray 中的权威背包条目由服务器创建。

### 2. 创建默认条目

```cpp
FInvEntry& NewEntry = Entries.AddDefaulted_GetRef();
```

此时条目只存在于服务器数组，还没有 FastArray 复制 ID。

### 3. 从 Manifest 创建运行时物品

```cpp
NewEntry.Item = ItemComponent
	->GetItemManifest()
	.Manifest(OwningActor);
```

这一步调用上一单元介绍的 Manifest 实例化流程，生成一个 `UInvItem`。

### 4. 注册可复制子对象

```cpp
IC->AddRepSubobj(NewEntry.Item);
```

它使 `UInvItem` 的复制属性进入网络复制链。

### 5. 标记新条目

```cpp
MarkItemDirty(NewEntry);
```

FastArray 为新条目分配复制 ID，并在下一次网络更新中将其作为新增条目发送。

完整链路为：

```text
服务器 AddEntry(ItemComponent)
→ Entries.AddDefaulted_GetRef
→ Manifest(OwningActor)
→ 创建 UInvItem
→ AddRepSubobj(UInvItem)
→ MarkItemDirty(NewEntry)
→ FastArray 增量复制
```

---

## AddEntry(UInvItem*)：加入已有运行时物品

项目还提供直接加入现有 `UInvItem` 的重载：

```cpp
UInvItem* FInvFastArray::AddEntry(UInvItem* Item)
{
	check(OwnerComponent);
	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	FInvEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;

	MarkItemDirty(NewEntry);
	return Item;
}
```

它不会重新执行 Manifest 的实例化，而是直接把传入的运行时物品保存到新条目中，然后调用 `MarkItemDirty()`。

当前项目实际拾取流程使用的是 `AddEntry(UInvItemComponent*)` 重载。

---

## RemoveEntry：从背包集合移除物品

`RemoveEntry()` 根据 `UInvItem` 指针查找目标条目：

```cpp
if (Entry.Item == Item)
{
	EntryIt.RemoveCurrent();
	MarkArrayDirty();
}
```

操作含义为：

```text
从服务器 Entries 移除目标条目
→ 标记数组结构已变化
→ FastArray 比较旧 ID 集合和新 ID 集合
→ 向客户端发送删除 ID
→ 客户端 PreReplicatedRemove
→ 客户端删除对应条目
```

这里的移除表示该 `UInvItem` 不再属于背包列表。丢弃与消耗系统会在各自流程中决定是否生成场景物品或应用可消耗效果。

---

## GetAllItems 与 FindFirstItemByType

这两个函数是本地数组查询，不会直接产生网络复制。

### GetAllItems

```cpp
TArray<UInvItem*> FInvFastArray::GetAllItems() const
{
	TArray<UInvItem*> Results;
	Results.Reserve(Entries.Num());

	for (const auto& Entry : Entries)
	{
		if (!IsValid(Entry.Item)) continue;
		Results.Add(Entry.Item);
	}
	return Results;
}
```

它将内部 `FInvEntry` 转换成便于业务系统使用的 `UInvItem*` 数组，并过滤无效对象。

`Reserve()` 提前为结果数组分配容量，避免逐个添加时重复扩容。

### FindFirstItemByType

```cpp
UInvItem* FInvFastArray::FindFirstItemByType(
	const FGameplayTag& ItemType
)
{
	auto* FoundItem = Entries.FindByPredicate(
		[&ItemType](const FInvEntry& Entry)
		{
			return IsValid(Entry.Item) &&
				Entry.Item->GetItemManifest()
				.GetItemType()
				.MatchesTagExact(ItemType);
		}
	);

	return FoundItem ? FoundItem->Item : nullptr;
}
```

它通过 Manifest 中的 ItemType GameplayTag 查找同类型物品，主要用于判断背包中是否已经存在可以继续堆叠的物品。

`MatchesTagExact()` 表示要求物品类型标签完全相等，而不是只匹配父级标签。

---

## FastArray 与 UInvItem 子对象复制

### 为什么 FInvEntry 保存 UInvItem

`FInvEntry` 不直接保存完整 Manifest，而是保存：

```cpp
TObjectPtr<UInvItem> Item;
```

这样可以将两个职责分开：

```text
FInvFastArray
负责背包集合的成员变化

UInvItem
负责单个物品的运行时数据
```

### UInvItem 支持网络复制

`UInvItem` 明确声明支持网络：

```cpp
virtual bool IsSupportedForNetworking() const override
{
	return true;
}
```

并复制：

```cpp
UPROPERTY(Replicated)
FInstancedStruct ItemManifest;

UPROPERTY(Replicated)
int32 TotalStackCount;
```

### 使用 Registered Subobject List

`UInvComponent` 启用注册子对象列表：

```cpp
bReplicateUsingRegisteredSubObjectList = true;
```

新增运行时物品时调用：

```cpp
void UInvComponent::AddRepSubobj(UObject* Subobj)
{
	if (
		IsUsingRegisteredSubObjectList() &&
		IsReadyForReplication() &&
		IsValid(Subobj)
	)
	{
		AddReplicatedSubObject(Subobj);
	}
}
```

完整复制结构为：

```text
UInvComponent
├─ FInvFastArray InventoryList
│  └─ 增量同步 UInvItem 引用的新增和删除
│
└─ Registered UInvItem Subobjects
   ├─ 同步 ItemManifest
   └─ 同步 TotalStackCount
```

因此，当服务器只修改：

```cpp
Item->SetTotalStackCount(NewCount);
```

发生变化的是 `UInvItem` 的复制属性，不是 `FInvEntry`，不需要将 FastArray 条目再次作为新增条目处理。

---

## 客户端与监听服务器的 UI 通知

### 普通客户端

普通客户端通过 FastArray 接收服务器新增条目：

```text
PostReplicatedAdd
→ OnItemAdded.Broadcast
→ UInvGrid::AddItem
```

这是由复制接收过程驱动的 UI 更新。

### 监听服务器本地玩家

监听服务器是权威端，不会通过网络接收自己发送的 FastArray 数据，因此不会为自己的新增操作触发 `PostReplicatedAdd()`。

当前代码在服务器完成新增后主动广播：

```cpp
if (
	GetOwner()->GetNetMode() == NM_ListenServer ||
	GetOwner()->GetNetMode() == NM_Standalone
)
{
	OnItemAdded.Broadcast(NewItem);
}
```

因此两种模式的 UI 入口不同：

```text
普通客户端
→ FastArray 复制回调刷新 UI

监听服务器本地玩家 / 单机
→ 服务器直接广播刷新 UI
```

专用服务器没有本地背包 UI，不需要执行界面刷新。

---

## 一次新增物品的网络时序

```text
服务器
│
├─ InventoryList.AddEntry(ItemComponent)
├─ 创建 FInvEntry
├─ Manifest 创建 UInvItem
├─ 注册 UInvItem 子对象
├─ MarkItemDirty(NewEntry)
│
├──────── 下一次网络复制 ────────► 客户端
│                                  │
│                                  ├─ 读取 FastArray Header
│                                  ├─ 创建新增 FInvEntry
│                                  ├─ 关联 UInvItem
│                                  ├─ PostReplicatedAdd
│                                  ├─ OnItemAdded.Broadcast
│                                  └─ UInvGrid::AddItem
│
└─ 监听服务器本地 UI 直接广播
```

第一次同步整个背包时，客户端没有对应的历史状态，因此服务器已有的所有条目都会作为新增数据接收。之后的同步才根据 ID 和 Key 只传递增量。

---

## 一次删除物品的网络时序

```text
服务器
│
├─ InventoryList.RemoveEntry(Item)
├─ 从 Entries 删除条目
├─ MarkArrayDirty
│
├──────── 下一次网络复制 ────────► 客户端
│                                  │
│                                  ├─ 读取被删除的 ReplicationID
│                                  ├─ 映射到客户端本地下标
│                                  ├─ PreReplicatedRemove
│                                  ├─ OnItemRemoved.Broadcast
│                                  └─ 从 Entries 删除条目
```

删除同步依赖稳定 ID，因此不要求客户端和服务器拥有相同的数组下标。

---

## FastArray 操作速查

| 操作 | 含义 | 当前项目用途 |
|---|---|---|
| `FFastArraySerializerItem` | 为条目提供 ID 和版本信息 | `FInvEntry` 的父结构 |
| `FFastArraySerializer` | 提供数组增量复制状态和操作 | `FInvFastArray` 的父结构 |
| `NetDeltaSerialize` | FastArray 的网络序列化入口 | 调用 `FastArrayDeltaSerialize` |
| `WithNetDeltaSerializer` | 通知 UE 使用增量序列化 | `FInvFastArray` Struct Traits |
| `MarkItemDirty` | 标记条目新增或内容变化 | 新增背包条目 |
| `MarkArrayDirty` | 标记数组结构变化 | 删除背包条目 |
| `PreReplicatedRemove` | 客户端删除条目前通知 | 广播 `OnItemRemoved` |
| `PostReplicatedAdd` | 客户端新增条目后通知 | 广播 `OnItemAdded` |
| `PostReplicatedChange` | 客户端更新已有条目后通知 | 当前未实现 |
| `ReplicationID` | 条目的稳定网络身份 | 区分新增、已有和删除条目 |
| `ReplicationKey` | 条目的变化版本 | 判断条目是否需要重发 |
| `ArrayReplicationKey` | 整个数组的变化版本 | 判断数组是否需要重新比较 |
| `AddReplicatedSubObject` | 注册可复制 UObject 子对象 | 复制 `UInvItem` 内部属性 |
| `DOREPLIFETIME` | 将属性加入复制系统 | 注册 `InventoryList`、Manifest 和堆叠数量 |

---

## 技术亮点

### 稳定 ID 代替数组下标

FastArray 使用 `ReplicationID` 识别条目，删除数组中间元素时不需要把后续元素全部当作新的物品。

### 条目级增量同步

服务器将每条连接上一次确认的 ID 与 Key 映射同当前状态比较，只发送新增、修改和删除部分。

### 复制回调直接驱动 UI

`PostReplicatedAdd()` 和 `PreReplicatedRemove()` 将底层网络变化转换为背包组件事件，UI 不需要保存另一份旧数组并自行计算差异。

### 集合复制与物品数据复制分离

FastArray 管理 `UInvItem` 的成员关系，Registered Subobject 负责 Manifest 和堆叠数量，实现清晰的复制职责划分。

### 服务器维护权威集合

新增接口检查 `HasAuthority()`，背包集合由服务器创建，再通过 FastArray 增量同步给客户端。

---

## 单元总结

本项目使用 `FInvFastArray` 代替普通复制数组管理背包物品集合。`FInvEntry` 继承 `FFastArraySerializerItem`，通过稳定的 `ReplicationID` 和递增的 `ReplicationKey` 表示条目身份与版本；`FInvFastArray` 继承 `FFastArraySerializer`，通过 `NetDeltaSerialize()` 计算每条连接上的新增、修改和删除数据。

新增物品时，服务器创建 `FInvEntry` 和 `UInvItem`，注册物品子对象并调用 `MarkItemDirty()`；删除物品时，从数组移除条目并调用 `MarkArrayDirty()`。客户端通过 `PostReplicatedAdd()` 和 `PreReplicatedRemove()` 将网络变化转换为 `OnItemAdded` 与 `OnItemRemoved` 事件。

FastArray 只负责回答“背包里有哪些物品”，而 `UInvItem` 子对象负责同步“每件物品包含什么 Manifest、当前堆叠数量是多少”。这种分层既保留了运行时物品对象的表达能力，也使背包集合能够进行条目级增量网络同步。

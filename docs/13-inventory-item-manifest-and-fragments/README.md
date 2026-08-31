# 场景物品、Manifest 与 Fragment 数据架构

## 功能点介绍

背包系统中的场景物品没有为每一种物品建立独立的 C++ Actor 派生类，而是使用一套数据组合架构：

```text
GameplayTag 定义物品身份和数据语义
              +
Manifest 聚合一件物品的完整清单
              +
TInstancedStruct 保存不同类型的 Fragment
              +
Fragment 按需提供尺寸、图标、堆叠、文本和行为
```

同一个 `AItem` C++ 类可以表达不同尺寸、不同图标、不同堆叠上限和不同描述的物品。新增一件已有能力范围内的物品，主要通过蓝图配置 Manifest 和组合 Fragment 完成，不需要继续扩展 Actor 继承树。

本单元包含：

- 场景 Actor、物品组件、Manifest、Fragment 与运行时物品的职责。
- GameplayTag 如何同时表达物品身份和 Fragment 语义。
- 为什么使用 `TInstancedStruct<FInvFragment>`。
- 为什么不使用 `TArray<FInvFragment>` 或 `TArray<FInvFragment*>`。
- `TInstancedStruct` 的普通序列化与网络序列化能力。
- 场景 Manifest 如何生成 `UInvItem`。
- Manifest 如何驱动物品描述 UI。
- 背包丢弃时如何根据 Manifest 重建场景 Actor。

FastArray 在下一单元单独介绍。本单元只说明它调用 Manifest 创建运行时 `UInvItem` 的接口边界，不展开 FastArray 的增量复制算法。

## 总体数据结构

```text
AItem
├─ ItemMesh
├─ PickupWidget
└─ UInvItemComponent
   ├─ PickupMessage
   └─ FInvItemManifest
      ├─ EInvItemCategory
      ├─ FGameplayTag ItemType
      ├─ TSubclassOf<AActor> PickupActorClass
      └─ TArray<TInstancedStruct<FInvFragment>>
         ├─ FInvGridFragment
         ├─ FInvImageFragment
         ├─ FInvTextFragment
         ├─ FInvLabeledNumberFragment
         ├─ FInvStackableFragment
         └─ FInvConsumableFragment
                     │
                     │ Manifest()
                     ▼
                  UInvItem
                  ├─ FInstancedStruct ItemManifest
                  └─ TotalStackCount
```

该结构把“场景表现”“数据定义”和“运行时实例”分离：

- `AItem` 是可以被射线检测的世界 Actor。
- `UInvItemComponent` 让 Actor 获得背包物品身份。
- `FInvItemManifest` 是完整的物品数据清单。
- Fragment 是可以自由组合的小型能力数据。
- `UInvItem` 是物品进入背包后的可复制运行时对象。

## 相关类的职责

### AItem：场景中的物品载体

`AItem` 只处理场景表现和聚焦提示：

```cpp
ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
InvItemComponent = CreateDefaultSubobject<UInvItemComponent>(TEXT("InvItemComponent"));
```

玩家准心射线聚焦物品时，`ShowPickupWidget()` 同时控制：

- 拾取提示 Widget。
- 物品网格的 Overlay Material。

`AItem` 不保存格子尺寸、堆叠上限或消耗数值。这些数据全部来自 `UInvItemComponent` 中的 Manifest。

相关实现：

- `Source/Blaster/Item/Item.h:10`
- `Source/Blaster/Item/Item.cpp:9`

### UInvItemComponent：场景 Actor 的背包身份

任何场景 Actor 只要带有 `UInvItemComponent`，就可以向背包系统提供物品数据：

```cpp
UPROPERTY(Replicated, EditAnywhere, Category = "Inventory")
FInvItemManifest ItemManifest;
```

它的职责是：

- 保存可在蓝图中编辑的 Manifest。
- 复制场景物品的 Manifest。
- 提供拾取提示文本。
- 在完整拾取时触发蓝图 `OnPickedUp()`。
- 最后销毁原场景 Actor。

```cpp
void UInvItemComponent::PickedUp()
{
	OnPickedUp();
	GetOwner()->Destroy();
}
```

`UInvItemComponent` 不负责计算背包空间，也不负责创建格子 UI。它只对外提供物品清单和场景 Actor 生命周期入口。

相关实现：

- `Source/Blaster/BlasterComponent/InvItemComponent.h:12`
- `Source/Blaster/BlasterComponent/InvItemComponent.cpp:7`

### FInvItemManifest：物品清单

Manifest 保存四类核心数据：

```cpp
UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ExcludeBaseStruct))
TArray<TInstancedStruct<FInvFragment>> Fragments;

UPROPERTY(EditAnywhere, Category = "Inventory")
EInvItemCategory ItemCategory{ EInvItemCategory::None };

UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "GameItems"))
FGameplayTag ItemType;

UPROPERTY(EditAnywhere, Category = "Inventory")
TSubclassOf<AActor> PickupActorClass;
```

| 字段 | 作用 |
| --- | --- |
| `ItemCategory` | 区分 `Consumable`、`Craftable`、`Equippable` 等大类 |
| `ItemType` | 标识这件物品的具体类型，参与查找和堆叠匹配 |
| `PickupActorClass` | 从背包丢弃时重新生成的场景 Actor Class |
| `Fragments` | 按需组合尺寸、图标、文本、数值、堆叠与消耗能力 |

Manifest 不需要预先声明所有可能出现的物品字段。没有堆叠能力的物品不添加 `FInvStackableFragment`；不是消耗品的物品不添加 `FInvConsumableFragment`。

相关实现：

- `Source/Blaster/Item/InvItemManifest.h:15`

### FInvFragment：可组合的数据基类

所有 Fragment 都继承：

```cpp
USTRUCT(BlueprintType)
struct FInvFragment
{
	GENERATED_BODY()

	virtual void Manifest() {}

private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "FragmentTags"))
	FGameplayTag FragmentTag;
};
```

`FragmentTag` 表达该数据在物品中的语义；`Manifest()` 则允许 Fragment 在物品首次实例化时生成自己的运行时状态。

当前主要 Fragment：

| Fragment | 数据或能力 |
| --- | --- |
| `FInvGridFragment` | 二维格子尺寸与图标 Padding |
| `FInvImageFragment` | 图标纹理和描述图标尺寸 |
| `FInvTextFragment` | 物品名称、类型、背景描述等文本 |
| `FInvLabeledNumberFragment` | 标签化数值、随机范围与显示精度 |
| `FInvStackableFragment` | 最大堆叠数和当前场景堆叠数 |
| `FInvConsumableFragment` | 消耗 Modifier 集合与消费入口 |

相关实现：

- `Source/Blaster/Item/InvFragment.h:12`
- `Source/Blaster/Item/InvFragment.cpp:8`

### UInvItem：进入背包后的运行时实例

场景 Actor 不会直接塞进背包数组。服务器根据 Manifest 创建一个支持网络复制的 `UInvItem`：

```cpp
UCLASS(BlueprintType, Blueprintable)
class UInvItem : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }

private:
	UPROPERTY(Replicated)
	FInstancedStruct ItemManifest;

	UPROPERTY(Replicated)
	int32 TotalStackCount{ 0 };
};
```

`UInvItem` 保存的是一件具体物品实例，而不是场景 Mesh：

- Manifest 中已经确定的随机属性会被保留。
- `TotalStackCount` 表示背包中该物品的总数量。
- 物品被丢弃后可以根据 Manifest 重建场景 Actor。

相关实现：

- `Source/Blaster/HUD/InvItem.h:14`
- `Source/Blaster/HUD/InvItem.cpp:6`

## GameplayTag 的两个层次

项目没有把所有标签混成一种用途，而是区分物品身份和 Fragment 语义。

### ItemType：物品身份

当前定义包含：

```text
GameItems.Consumables.Medics.Small
GameItems.Consumables.Medics.Large

GameItems.Craftables.V1
GameItems.Craftables.V2
GameItems.Craftables.V3
GameItems.Craftables.V4
GameItems.Craftables.V5
```

`ItemType` 用于：

- 在背包中查找同类型物品。
- 判断两堆物品是否允许合并。
- 区分同一 Category 下的具体物品。

匹配使用：

```cpp
MatchesTagExact(ItemType)
```

因此 `GameItems.Consumables.Medics.Small` 和 `Large` 即使共享父标签，也不会被视为同一种可合并物品。

相关实现：

- `Source/Blaster/BlasterTypes/InvTags.h:5`
- `Source/Blaster/BlasterTypes/InvTags.cpp:3`

### FragmentTag：数据语义

同一个 C++ Fragment 类型可以出现多次，由 FragmentTag 区分含义。

例如三个 `FInvTextFragment` 可以分别配置为：

```text
FragmentTags.ItemNameFragment
FragmentTags.ItemTypeFragment
FragmentTags.FlavorTextFragment
```

多个 `FInvLabeledNumberFragment` 可以表示：

```text
FragmentTags.PrimaryStatFragment
FragmentTags.SellValueFragment
FragmentTags.RequiredLevelFragment
FragmentTags.StatMod.1
FragmentTags.StatMod.2
FragmentTags.StatMod.3
```

由此形成明确分工：

```text
C++ Struct 类型
└─ 决定数据格式和可执行能力

GameplayTag
└─ 决定这份数据在当前物品中代表什么
```

相关实现：

- `Source/Blaster/BlasterTypes/InvFragmentTag.h:5`
- `Source/Blaster/BlasterTypes/InvFragmentTag.cpp:3`

## 为什么选择 TInstancedStruct

### 需求：同一个数组保存不同派生 USTRUCT

Manifest 需要同时保存：

```text
FInvGridFragment
FInvImageFragment
FInvTextFragment
FInvStackableFragment
FInvConsumableFragment
```

这些类型拥有不同字段，却都继承 `FInvFragment`。

`TInstancedStruct<FInvFragment>` 是对 `FInstancedStruct` 的类型安全包装。它确保放入的实际结构必须派生自 `FInvFragment`，同时保存：

```text
实际 UScriptStruct 类型
+
该类型的完整数据内存
```

数组中的每个元素可以拥有不同的实际派生类型：

```cpp
TArray<TInstancedStruct<FInvFragment>> Fragments;
```

```text
Fragments[0] → FInvGridFragment
Fragments[1] → FInvImageFragment
Fragments[2] → FInvTextFragment
Fragments[3] → FInvStackableFragment
```

### 为什么不用 TArray<FInvFragment>

值数组的元素大小固定为基类大小。把派生 Struct 添加进去会发生对象切片：

```cpp
FInvGridFragment GridFragment;
TArray<FInvFragment> Fragments;
Fragments.Add(GridFragment);
```

最终只剩 `FInvFragment` 基类部分，`GridSize` 和 `GridPadding` 会丢失。

### 为什么不用 TArray<FInvFragment*>

裸指针可以在 C++ 运行时指向不同派生类型，但只保存内存地址，不拥有完整值语义：

- 必须手动决定 `new` 和 `delete`。
- `USTRUCT` 指针不受 UObject GC 管理。
- Manifest 默认复制只会浅拷贝指针。
- 多个 Manifest 可能共享并修改同一 Fragment。
- 容易产生悬空指针或重复释放。
- 网络另一端无法使用服务器进程中的内存地址。
- UE 编辑器无法直接在数组元素中选择并编辑派生 Struct 实例。

```text
Original.Fragments[0] ─┐
                       ├─→ 同一块 Fragment 内存
Copy.Fragments[0] ─────┘
```

而 `TInstancedStruct` 拷贝时会保留真实派生类型并复制其完整值数据。

### 为什么不全部改成 UObject Fragment

另一种可行设计是：

```cpp
TArray<TObjectPtr<UInvFragment>> Fragments;
```

但这要求把每个 Fragment 都升级为 UObject：

- 每个 Fragment 都需要独立 UObject 分配。
- 增加 GC 扫描对象数量。
- 网络复制需要管理更多复制子对象。
- Manifest 的整体值拷贝会变成对象引用语义。

UObject Fragment 更适合需要独立生命周期、蓝图复杂行为或跨物品共享引用的模块。当前 Fragment 主要是尺寸、图标、文本和少量数值配置，使用轻量的 USTRUCT 值数据更合适。

### 三种方式对照

| 方式 | 保留派生数据 | 所有权 | 编辑器配置 | 序列化 | 适用场景 |
| --- | --- | --- | --- | --- | --- |
| `TArray<FInvFragment>` | 否，会切片 | 值管理 | 只能编辑基类 | 支持基类 | 没有派生数据 |
| `TArray<FInvFragment*>` | 是 | 需要手动管理 | 不适合内联派生实例 | 需要手写 | 纯 C++ 临时多态对象 |
| `TArray<TInstancedStruct<FInvFragment>>` | 是 | 值管理 | 可选择派生 Struct | 引擎支持 | 异构小型数据模块 |
| `TArray<TObjectPtr<UInvFragment>>` | 是 | GC 管理 | 可配置 UObject | 需要子对象管理 | 复杂独立行为对象 |

## TInstancedStruct 的序列化能力

`TInstancedStruct` 底层包装 `FInstancedStruct`。UE5.4 的 `FInstancedStruct` 实现：

```cpp
bool Serialize(FArchive& Ar);
bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
```

并通过 Struct Traits 启用：

```cpp
template<>
struct TStructOpsTypeTraits<FInstancedStruct>
	: public TStructOpsTypeTraitsBase2<FInstancedStruct>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithNetSerializer = true,
		WithAddStructReferencedObjects = true
	};
};
```

普通序列化保存：

```text
实际 UScriptStruct 类型
+
序列化数据长度
+
该派生 Struct 的完整属性数据
```

加载时先恢复真实 Struct 类型，再按该类型分配内存并反序列化字段。

网络序列化同样会发送有效标记、实际 `UScriptStruct` 类型和内部字段。派生 Struct 如果实现原生 `NetSerialize`，就调用该实现；否则由 UE 的 Struct RepLayout 序列化属性。

但“支持网络序列化”不等于局部变量会自动复制。它仍然必须处于 UE 的复制链中：

```text
Replicated Actor/Component/UObject
└─ UPROPERTY(Replicated)
   └─ FInstancedStruct/TInstancedStruct
```

当前项目中：

```text
UInvItemComponent
└─ Replicated FInvItemManifest
   └─ TArray<TInstancedStruct<FInvFragment>>

UInvItem
└─ Replicated FInstancedStruct ItemManifest
   └─ FInvItemManifest
      └─ TArray<TInstancedStruct<FInvFragment>>
```

`TInstancedStruct` 负责正确表达并序列化异构 Fragment；FastArray 是否增量发送条目，由外层背包复制机制决定。

## 类型安全的 Fragment 查询

Manifest 提供三种主要查询接口。

### 取得第一个指定类型

```cpp
const FInvGridFragment* GridFragment =
	Manifest.GetFragmentOfType<FInvGridFragment>();
```

适用于每件物品只需要一份的数据，例如 Grid 或 Stackable。

内部通过：

```cpp
Fragment.GetPtr<T>()
```

判断实际派生类型是否兼容。

### 按类型和 Tag 查询

```cpp
const FInvTextFragment* ItemName =
	Manifest.GetFragmentOfTypeWithTag<FInvTextFragment>(
		FragmentTags::ItemNameFragment
	);
```

适用于一件物品中存在多个相同 Struct 类型，但语义不同的情况。

### 取得全部指定基类 Fragment

```cpp
TArray<const FInvAssimilateFragment*> UIFragments =
	Manifest.GetAllFragmentsOfType<FInvAssimilateFragment>();
```

适用于批量处理所有能够写入物品描述 UI 的 Fragment。

模板约束：

```cpp
template<typename T>
requires std::derived_from<T, FInvFragment>
```

保证查询类型必须继承自 `FInvFragment`，错误类型会在编译期被拒绝。

## Manifest 的生成调用链

### 1. 场景物品保存配置态 Manifest

```text
AItem
└─ UInvItemComponent
   └─ FInvItemManifest
      ├─ ItemCategory
      ├─ ItemType
      ├─ PickupActorClass
      └─ Fragments
```

这些数据来自场景物品蓝图配置。当前 Content 中的示例 Actor 包括：

```text
BP_ItemBlock
BP_ItemCone
BP_ItemSphere
```

它们可以共享 `AItem` C++ 类，但配置不同的 ItemType、网格尺寸、图标和堆叠数据。

### 2. 拾取链调用 InventoryList::AddEntry

服务器决定添加一条新背包物品时调用：

```cpp
UInvItem* NewItem = InventoryList.AddEntry(ItemComponent);
```

FastArray 的 `AddEntry()` 从场景组件取得 Manifest：

```cpp
NewEntry.Item = ItemComponent
	->GetItemManifest()
	.Manifest(OwningActor);
```

`GetItemManifest()` 当前返回值拷贝，因此 `Manifest()` 操作的是一份独立工作副本。

### 3. Manifest 创建 UInvItem

```cpp
UInvItem* FInvItemManifest::Manifest(UObject* NewOuter)
{
	UInvItem* Item = NewObject<UInvItem>(
		NewOuter,
		UInvItem::StaticClass()
	);

	Item->SetItemManifest(*this);

	for (auto& Fragment :
		Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		Fragment.GetMutable().Manifest();
	}

	ClearFragments();
	return Item;
}
```

完整过程：

```text
配置态 Manifest 工作副本
        ↓
NewObject<UInvItem>
        ↓
把 Manifest 复制到 UInvItem
        ↓
遍历运行时 Manifest 的全部 Fragment
        ↓
调用每个实际派生 Fragment::Manifest()
        ↓
确定随机属性等实例状态
        ↓
清理工作副本 Fragments
        ↓
返回运行时 UInvItem
```

### 4. UInvItem 保存 Manifest

```cpp
void UInvItem::SetItemManifest(const FInvItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FInvItemManifest>(Manifest);
}
```

这里又使用 `FInstancedStruct` 包装完整 Manifest，使 `UInvItem` 的运行时数据能够通过统一的 InstancedStruct 属性保存和复制。

### 5. Fragment 首次实例化

`FInvLabeledNumberFragment` 会在第一次 Manifest 时随机确定数值：

```cpp
void FInvLabeledNumberFragment::Manifest()
{
	if (bRandomizeOnManifest)
	{
		Value = FMath::FRandRange(Min, Max);
	}
	bRandomizeOnManifest = false;
}
```

这样形成：

```text
物品模板
└─ 定义 Min/Max

具体 UInvItem 实例
└─ 保存已经确定的 Value
```

物品以后从背包丢回场景时，不会重新生成属性。

`FInvConsumableFragment::Manifest()` 则继续遍历内部 ConsumeModifier，使每个 Modifier 有机会生成自己的实例数据。

## Manifest 驱动物品描述 UI

Manifest 不只服务于背包数据，也直接驱动物品描述。

调用入口：

```cpp
Manifest.AssimilateInventoryFragments(ItemDescriptionWidget);
```

实现流程：

```cpp
const auto& InventoryItemFragments =
	GetAllFragmentsOfType<FInvAssimilateFragment>();

for (const auto* Fragment : InventoryItemFragments)
{
	Composite->ApplyFunction(
		[Fragment](UInvCompositeBase* Widget)
		{
			Fragment->Assimilate(Widget);
		}
	);
}
```

UI 使用 Composite/Leaf 结构遍历全部描述节点，Fragment 通过 Tag 找到对应节点：

```cpp
bool FInvAssimilateFragment::MatchesWidgetTag(
	const UInvCompositeBase* Composite) const
{
	return Composite->GetFragmentTag()
		.MatchesTagExact(GetFragmentTag());
}
```

例如：

```text
FInvImageFragment + IconFragment
→ UInvLeafImage

FInvTextFragment + ItemNameFragment
→ 名称文本 Leaf

FInvTextFragment + FlavorTextFragment
→ 背景描述 Leaf

FInvLabeledNumberFragment + SellValueFragment
→ 出售价值 Leaf
```

没有对应 Fragment 的描述节点保持折叠。添加已有类型的数据时，不需要修改物品描述 Widget 的控制流程。

## 示例：可堆叠制作材料

一件制作材料可以通过数据组合表示：

```text
ItemCategory
└─ Craftable

ItemType
└─ GameItems.Craftables.V1

PickupActorClass
└─ BP_ItemBlock

Fragments
├─ FInvGridFragment
│  ├─ FragmentTag = GridFragment
│  └─ GridSize = 2 × 1
├─ FInvImageFragment
│  ├─ FragmentTag = IconFragment
│  └─ Icon = 合金块图标
├─ FInvTextFragment
│  ├─ FragmentTag = ItemNameFragment
│  └─ Text = “合金块”
├─ FInvTextFragment
│  ├─ FragmentTag = FlavorTextFragment
│  └─ Text = “用于制作武器配件”
├─ FInvStackableFragment
│  ├─ MaxStackSize = 20
│  └─ StackCount = 6
└─ FInvLabeledNumberFragment
   ├─ FragmentTag = SellValueFragment
   ├─ Min = 8
   └─ Max = 12
```

这件物品能够占用 `2×1` 格、显示图标与描述、最多堆叠 20 个，并在首次 Manifest 时确定出售价值，而不需要创建新的 C++ Item 派生类。

## 示例：小型医疗品的数据组合

```text
ItemCategory
└─ Consumable

ItemType
└─ GameItems.Consumables.Medics.Small

Fragments
├─ FInvGridFragment
│  └─ GridSize = 1 × 2
├─ FInvImageFragment
│  └─ 医疗品图标
├─ FInvTextFragment
│  ├─ FragmentTag = ItemNameFragment
│  └─ Text = “小型医疗品”
├─ FInvStackableFragment
│  ├─ MaxStackSize = 5
│  └─ StackCount = 2
└─ FInvConsumableFragment
   └─ ConsumeModifiers
```

背包系统只需要查询 `FInvConsumableFragment` 并调用 `OnConsume()`，不需要在 `UInvComponent` 中为每种消耗品编写 Switch。

当前源码已经提供 Consumable 与 Modifier 扩展框架，但没有发现具体医疗 Modifier 的 C++ 派生实现。因此本例用于说明数据组合方式，不把具体生命恢复描述成当前已经完成的背包消耗效果。

## 从背包丢弃后重建场景物品

Manifest 还保存 `PickupActorClass`，使运行时 `UInvItem` 可以重新生成世界 Actor。

服务器丢弃物品时：

```text
UInvItem
        ↓
取得可变 ItemManifest
        ↓
更新 StackableFragment::StackCount
        ↓
Manifest::SpawnPickupActor
        ↓
根据 PickupActorClass 生成 AActor
        ↓
查找新 Actor 的 UInvItemComponent
        ↓
InitItemManifest
        ↓
把运行时 Manifest 复制回场景物品
```

关键代码：

```cpp
void FInvItemManifest::SpawnPickupActor(
	const UObject* WorldContextObject,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation)
{
	if (!IsValid(PickupActorClass) ||
		!IsValid(WorldContextObject)) return;

	AActor* SpawnedActor = WorldContextObject->GetWorld()
		->SpawnActor<AActor>(
			PickupActorClass,
			SpawnLocation,
			SpawnRotation
		);

	if (!IsValid(SpawnedActor)) return;

	UInvItemComponent* ItemComp =
		SpawnedActor->FindComponentByClass<UInvItemComponent>();
	check(ItemComp);

	ItemComp->InitItemManifest(*this);
}
```

由于写回的是 `UInvItem` 已经确定的 Manifest，随机属性、物品类型和其他实例数据能够在“场景 → 背包 → 场景”的转换中保留。

## 完整 Manifest 生命周期

```text
蓝图配置 AItem
└─ UInvItemComponent::ItemManifest
   ├─ ItemType GameplayTag
   ├─ ItemCategory
   ├─ PickupActorClass
   └─ TInstancedStruct Fragment 组合
                    ↓
玩家请求拾取
                    ↓
服务器 InventoryList::AddEntry(ItemComponent)
                    ↓
GetItemManifest() 取得工作副本
                    ↓
FInvItemManifest::Manifest(OwningActor)
                    ↓
NewObject<UInvItem>
                    ↓
FInstancedStruct::Make<FInvItemManifest>
                    ↓
逐个调用派生 Fragment::Manifest()
                    ↓
生成并保存实例随机数据
                    ↓
UInvItem 进入背包复制系统
          ├─ GridFragment 驱动空间尺寸
          ├─ Image/Text/Number Fragment 驱动描述 UI
          ├─ StackableFragment 驱动堆叠
          └─ ConsumableFragment 提供消费入口
                    ↓
玩家丢弃物品
                    ↓
Manifest::SpawnPickupActor
                    ↓
Manifest 复制回新 UInvItemComponent
                    ↓
重新成为可交互场景 AItem
```

## 技术亮点

### 组合代替物品子类爆炸

物品能力由 Fragment 组合，不需要为“可堆叠医疗品”“不可堆叠材料”“带随机属性装备”等每种组合创建 Actor 派生类。

### 类型和语义相互独立

`TInstancedStruct` 保存真实数据类型，GameplayTag 表达数据语义。同一个 `FInvTextFragment` 可以复用于名称、类型和背景文本，同时仍保持强类型访问。

### 值语义的异构 USTRUCT 容器

`TInstancedStruct` 在不升级为 UObject 的情况下保存不同派生 USTRUCT，避免对象切片、裸指针所有权和额外 UObject 子对象开销。

### 可序列化的多态数据

底层 `FInstancedStruct` 保存实际 `UScriptStruct` 类型与完整数据，并支持普通序列化和 `NetSerialize`，使异构 Fragment 能进入 Manifest 的保存与复制链路。

### 定义数据与实例数据的转换

`Manifest()` 将场景配置复制为独立 `UInvItem`，并在首次实例化时确定随机属性。物品被丢弃后使用同一 Manifest 重建场景 Actor，实例状态不会重新生成。

### 同一份数据驱动逻辑和 UI

Grid、Stackable 和 Consumable Fragment 为背包逻辑提供数据；Image、Text 和 LabeledNumber Fragment 又通过 Tag 写入 Composite 描述 UI。物品定义与显示信息不需要维护两份平行配置。

## 单元总结

场景物品以 `AItem` 提供 Mesh 和交互提示，以 `UInvItemComponent` 保存 `FInvItemManifest`。Manifest 通过 ItemType GameplayTag 标识具体物品，通过 `TArray<TInstancedStruct<FInvFragment>>` 组合尺寸、图标、文本、数值、堆叠和消耗能力。

选择 `TInstancedStruct` 是因为 Fragment 需要多态数据，却仍然是 Manifest 内部的轻量 USTRUCT 值。它既避免普通基类数组的对象切片，也避免裸指针的所有权、浅拷贝和网络地址问题，同时保留编辑器配置、反射、普通序列化与网络序列化能力。

服务器拾取新物品时，Manifest 创建可复制的 `UInvItem`，复制完整数据并调用各派生 Fragment 的 `Manifest()` 确定实例状态；物品描述 UI 再通过 FragmentTag 自动吸收对应图标、文本和数值。丢弃时，运行时 Manifest 根据 `PickupActorClass` 重建场景 Actor，并写回 `UInvItemComponent`，形成可扩展的“场景定义—运行时实例—场景重建”数据闭环。

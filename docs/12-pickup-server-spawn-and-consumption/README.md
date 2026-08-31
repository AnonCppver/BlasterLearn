# 拾取物、服务器刷新与一次性消费

## 功能点介绍

场景拾取物由服务器统一生成并确认重叠。客户端只负责显示服务器复制过来的拾取物及其旋转、描边、音效和粒子表现，不能自行增加弹药、生命或护盾。

本单元包含：

- `APickup` 提供的公共碰撞、表现和生命周期。
- `AAmmoPickup`、`AHealthPickup`、`AShieldPickup` 三种派生效果。
- `APickupSpawnPoint` 的随机刷新与循环生成。
- 为什么重叠事件只在服务器绑定。
- `Inactive → Active → Consumed` 的明确激活阶段。
- 多名玩家同帧重叠时的一次性消费保护。
- 服务器销毁如何同步表现并驱动下一轮刷新。

系统保持当前消费规则：只要有效角色触碰拾取物，即使对应资源已经达到上限，拾取物仍然会被消耗。

## 与背包系统的边界

本单元中的 Pickup 是进入碰撞范围后立即生效的场景 Actor，不会进入玩家背包，也不会生成可持久保存的物品实例。

本单元只涉及：

```text
APickup
├─ AAmmoPickup
├─ AHealthPickup
└─ AShieldPickup

APickupSpawnPoint
└─ 负责随机生成以上 Pickup
```

以下内容不属于本单元：

- `UInvComponent` 和空间背包数据。
- `InvItemManifest`、Fragment、FastArray 与物品复制。
- 背包格子、物品旋转、堆叠、拖拽和 UI。
- 武器 `AreaSphere` 的地面拾取与装备流程。
- 人物骨骼上的 `BackpackSocket`。

弹药 Pickup 会直接增加 `CombatComponent` 中按武器类型保存的备弹；生命和护盾 Pickup 会直接向 `BuffComponent` 提交恢复参数。它们都是“立即消费型效果”，不是“加入背包型物品”。

## 类职责

| 类 | 主要职责 | 消费结果 |
| --- | --- | --- |
| `APickup` | 公共碰撞、激活状态、旋转描边、销毁音效 | 不定义具体数值效果 |
| `AAmmoPickup` | 保存弹药类型和拾取数量 | 增加对应武器类型的备弹 |
| `AHealthPickup` | 保存恢复总量和持续时间 | 向 BuffComponent 添加生命恢复量 |
| `AShieldPickup` | 保存护盾恢复量和持续时间 | 向 BuffComponent 添加护盾恢复量 |
| `APickupSpawnPoint` | 随机等待、选择 Class、生成和监听销毁 | Pickup 销毁后开始下一轮刷新 |

`APickup` 处理“什么时候能够被拾取”和“只能被消费一次”；派生类只处理“被消费时产生什么效果”。

## APickup 的基础设置

### 复制与碰撞

`APickup` 是可复制 Actor：

```cpp
bReplicates = true;
```

服务器生成后，客户端通过 Actor 复制看到相同的拾取物。基础碰撞由 `USphereComponent` 提供，只响应 Pawn Overlap：

```cpp
OverlapSphere->SetSphereRadius(150.f);
OverlapSphere->SetCollisionResponseToAllChannels(
	ECollisionResponse::ECR_Ignore
);
OverlapSphere->SetCollisionResponseToChannel(
	ECollisionChannel::ECC_Pawn,
	ECollisionResponse::ECR_Overlap
);
```

拾取物网格不参与实体碰撞：

```cpp
PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

因此角色是否拾取由球形触发区决定，网格大小、朝向和旋转不会改变触发范围。

### 客户端视觉表现

拾取物 Mesh 使用紫色 Custom Depth 描边，并在 Tick 中持续绕 Z 轴旋转：

```cpp
PickupMesh->SetRenderCustomDepth(true);
PickupMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);
```

```cpp
void APickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PickupMesh)
	{
		PickupMesh->AddWorldRotation(
			FRotator(0.f, BaseTurnRate * DeltaTime, 0.f)
		);
	}
}
```

这些表现不参与服务器对拾取结果的裁决。

## 为什么重叠只由服务器触发

`APickup::BeginPlay()` 只在权威实例上安排激活过程：

```cpp
void APickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			BindOverlapTimer,
			this,
			&APickup::BindOverlapTimerFinished,
			BindOverlapTime
		);
	}
}
```

计时结束后才注册重叠回调：

```cpp
void APickup::BindOverlapTimerFinished()
{
	OverlapSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&APickup::OnSphereOverlap
	);
}
```

客户端的 Pickup 实例不会绑定 `OnSphereOverlap()`，因此即使客户端预测到自己进入碰撞球，也不能在本地执行：

- 增加备弹。
- 添加恢复 Buff。
- 销毁 Pickup。
- 触发下一轮刷新。

权威链路为：

```text
服务器角色进入 OverlapSphere
              ↓
服务器 Pickup 收到 OnSphereOverlap
              ↓
服务器验证角色与消费状态
              ↓
服务器调用派生效果
              ↓
服务器 Destroy Pickup
              ↓
销毁结果复制到客户端
```

客户端上传的位置移动最终仍由服务器角色参与碰撞检测，拾取系统本身不需要额外的“请求拾取”Server RPC。

## 明确的激活阶段

### 原有延迟绑定的目的

Pickup 生成时，刷新点附近可能已经站着玩家。如果生成后立即开放重叠，Pickup 可能在 `SpawnActor()` 的生成流程中马上被消费和销毁，而 `APickupSpawnPoint` 还没有来得及绑定它的 `OnDestroyed`。

结果可能是：

```text
Pickup 生成
    ↓
立即与角色重叠并销毁
    ↓
SpawnPoint 尚未绑定 OnDestroyed
    ↓
没有启动下一轮刷新
```

原实现通过延迟 `0.25` 秒绑定 Overlap，避免 Pickup 刚生成就被消耗。

### 优化后的三阶段状态

采用明确的 Pickup 生命周期：

```text
Inactive
├─ 已经生成并复制
├─ OverlapSphere 关闭碰撞
├─ SpawnPoint 绑定 OnDestroyed
└─ 不允许消费
        │
        │ 激活保护时间结束
        ▼
Active
├─ OverlapSphere = QueryOnly
├─ 只响应 Pawn Overlap
├─ 服务器处理重叠
└─ 等待第一个有效角色
        │
        │ 首次有效重叠
        ▼
Consumed
├─ bConsumed = true
├─ 立即关闭碰撞
├─ 应用一次派生效果
└─ 服务器销毁 Actor
```

简化后的激活逻辑为：

```cpp
void APickup::BeginPlay()
{
	Super::BeginPlay();

	OverlapSphere->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			ActivationTimer,
			this,
			&APickup::ActivatePickup,
			ActivationDelay
		);
	}
}

void APickup::ActivatePickup()
{
	if (!HasAuthority() || bConsumed) return;

	OverlapSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&APickup::OnSphereOverlap
	);
	OverlapSphere->SetCollisionEnabled(
		ECollisionEnabled::QueryOnly
	);
}
```

相比“碰撞已经开启、只是暂时没有回调”，显式关闭碰撞能准确表达未激活状态，也不会在保护期内产生无效 Overlap 计算。

## 一次性消费保护

### 问题：同一帧可能产生多个重叠回调

多人对战中，两名角色可能在同一服务器帧进入同一个拾取球：

```text
服务器物理帧
├─ 角色 A 触发 OnSphereOverlap
└─ 角色 B 触发 OnSphereOverlap
```

如果每个回调都先应用效果、最后才调用 `Destroy()`，Actor 在本帧结束前可能已经产生了多个待处理回调。只依赖 `Destroy()` 不能明确保证只有第一个回调能够应用效果。

可能产生的问题：

- 一份弹药被两个角色同时获得。
- 同一个生命或护盾恢复效果被附加多次。
- 对同一 Pickup 重复调用 `Destroy()`。
- 生命周期与 SpawnPoint 的刷新事件变得难以推断。

### 方案：先占用，再应用效果

服务器处理重叠时采用严格顺序：

```text
1. 检查 HasAuthority
2. 检查 bConsumed
3. 验证 OtherActor 是有效 BlasterCharacter
4. 立即设置 bConsumed = true
5. 立即关闭 OverlapSphere
6. 调用派生类效果
7. 服务器 Destroy
```

简化代码：

```cpp
void APickup::OnSphereOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bConsumed) return;

	ABlasterCharacter* BlasterCharacter =
		Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter == nullptr) return;

	bConsumed = true;
	OverlapSphere->SetCollisionEnabled(
		ECollisionEnabled::NoCollision
	);

	ApplyPickupEffect(BlasterCharacter);
	Destroy();
}
```

关键点不是简单增加一个 `bool`，而是必须在调用派生效果之前设置它，并同时关闭碰撞：

```text
先标记 Consumed
→ 后续回调全部被拒绝
→ 再修改弹药或添加 Buff
```

这相当于服务器为第一个有效重叠者完成一次原子式消费占用。

### 保持满值仍然消费的规则

`bConsumed` 判断的是“Pickup 是否已经被某名有效角色取得”，不是“角色数值是否真的增加”。

因此：

| 角色状态 | 触碰结果 | Pickup 是否销毁 |
| --- | --- | --- |
| 生命未满 | 添加生命恢复量 | 是 |
| 生命已满 | 仍提交生命恢复效果 | 是 |
| 护盾未满 | 添加护盾恢复量 | 是 |
| 护盾已满 | 仍提交护盾恢复效果 | 是 |
| 备弹未满 | 增加并限制在最大值内 | 是 |
| 备弹已满 | 备弹保持最大值 | 是 |

派生效果不需要返回 `bool` 决定是否销毁。只要消费者是有效角色，本次 Pickup 就进入 `Consumed`。

## 派生 Pickup 的效果

### AAmmoPickup

`AAmmoPickup` 保存：

- `WeaponType`：弹药属于哪种武器类型。
- `AmmoAmount`：本次提供的备弹数量。

效果链路：

```text
AAmmoPickup::ApplyPickupEffect
        ↓
Character::GetCombat
        ↓
CombatComponent::PickupAmmo
        ↓
CarriedAmmoMap[WeaponType] += AmmoAmount
        ↓
Clamp 到 MaxCarriedAmmo
        ↓
如果该类型正是当前武器，刷新备弹 HUD
```

当前武器弹匣为空且弹药类型匹配时，`PickupAmmo()` 还会进入换弹流程：

```cpp
if (EquippedWeapon &&
	EquippedWeapon->IsEmpty() &&
	EquippedWeapon->GetWeaponType() == WeaponType)
{
	Reload();
}
```

这里的弹药直接进入 `CombatComponent` 的备弹表，不会生成背包物品或占用物品格。

### AHealthPickup

生命 Pickup 保存恢复总量与恢复时间：

```cpp
Buff->Heal(HealAmount, HealingTime);
```

Pickup 只负责把参数交给角色现有的 `BuffComponent`。恢复如何按时间逐步更新生命值，已经在生命、护盾与 Buff 单元中介绍，本单元不重复展开。

`AHealthPickup::Destroyed()` 额外生成 Niagara 拾取效果，然后调用基类 `Destroyed()` 播放公共拾取声音。

### AShieldPickup

护盾 Pickup 同样只负责提交效果参数：

```cpp
Buff->ReplenishShield(
	ShieldReplenishAmount,
	ShieldReplenishTime
);
```

它不直接维护护盾插值，也不在 Pickup Actor 中持续 Tick 恢复过程。Pickup 被销毁后，恢复由角色自己的 `BuffComponent` 继续执行。

## APickupSpawnPoint 的随机刷新

### 刷新点自身不需要 Tick

`APickupSpawnPoint` 使用 Timer 驱动刷新，因此构造函数关闭逐帧 Tick：

```cpp
PrimaryActorTick.bCanEverTick = false;
```

刷新时间由以下配置决定：

```cpp
float SpawnPickupTimeMin;
float SpawnPickupTimeMax;
```

每轮从范围中随机取得等待时间：

```cpp
const float SpawnTime = FMath::FRandRange(
	SpawnPickupTimeMin,
	SpawnPickupTimeMax
);
```

### 首次刷新

关卡开始后，SpawnPoint 不会立即生成 Pickup，而是先启动一次随机计时：

```cpp
void APickupSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
	StartSpawnPickupTimer(nullptr);
}
```

Timer 到期后，只有服务器执行生成：

```cpp
void APickupSpawnPoint::SpawnPickupTimerFinished()
{
	if (HasAuthority())
	{
		SpawnPickup();
	}
}
```

即使客户端拥有对应的关卡 Actor，也不会在本地随机生成一份 Pickup。

### 随机选择 Pickup 类型

SpawnPoint 持有可配置的 Class 数组：

```cpp
TArray<TSubclassOf<APickup>> PickupClasses;
```

服务器从数组中随机选择一个类型并使用 SpawnPoint Transform 生成：

```cpp
void APickupSpawnPoint::SpawnPickup()
{
	const int32 NumPickupClasses = PickupClasses.Num();
	if (NumPickupClasses > 0)
	{
		const int32 Selection = FMath::RandRange(
			0,
			NumPickupClasses - 1
		);

		SpawnedPickup = GetWorld()->SpawnActor<APickup>(
			PickupClasses[Selection],
			GetActorTransform()
		);

		if (HasAuthority() && SpawnedPickup)
		{
			SpawnedPickup->OnDestroyed.AddDynamic(
				this,
				&APickupSpawnPoint::StartSpawnPickupTimer
			);
		}
	}
}
```

生成的是 `APickup` 的派生 Class，因此同一个 SpawnPoint 可以随机刷新弹药、生命或护盾，而不需要知道派生类如何应用效果。

### 销毁驱动下一轮刷新

服务器生成 Pickup 后，将它的 `OnDestroyed` 绑定到 SpawnPoint：

```text
Pickup 被有效角色消费
        ↓
服务器 Destroy Pickup
        ↓
Pickup::OnDestroyed
        ↓
SpawnPoint::StartSpawnPickupTimer
        ↓
随机等待
        ↓
服务器生成下一件 Pickup
```

刷新点不需要每帧轮询 `SpawnedPickup` 是否存在，也不需要客户端通知服务器“拾取物已经消失”。Actor 生命周期事件直接连接两轮刷新。

## 销毁与客户端表现

所有有效消费最终都由服务器调用 `Destroy()`。因为 Pickup Actor 开启复制，销毁会同步到各客户端。

`APickup::Destroyed()` 播放公共拾取声音：

```cpp
void APickup::Destroyed()
{
	Super::Destroyed();

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			PickupSound,
			GetActorLocation()
		);
	}
}
```

生命 Pickup 在自己的 `Destroyed()` 中先生成 Niagara 效果，再调用基类播放声音：

```text
服务器确认消费并销毁
        ↓
服务器 SpawnPoint 开始下一轮计时
        +
客户端移除 Pickup Actor
        ↓
客户端播放拾取声音和派生销毁效果
```

数值效果由服务器执行，视觉反馈随着复制销毁在各客户端出现，两者职责分离。

## 完整链路

```text
服务器 APickupSpawnPoint::BeginPlay
                ↓
StartSpawnPickupTimer
                ↓
从 Min/Max 中随机等待
                ↓
SpawnPickupTimerFinished
                ↓ HasAuthority
随机选择 PickupClasses 中的派生 Class
                ↓
服务器生成可复制 Pickup
                ↓
Pickup = Inactive，关闭碰撞
                ↓
SpawnPoint 绑定 Pickup::OnDestroyed
                ↓
激活保护时间结束
                ↓
Pickup = Active，服务器开放 Pawn Overlap
                ↓
第一个有效 BlasterCharacter 进入范围
                ↓
bConsumed 检查
                ↓
先设置 bConsumed 并关闭碰撞
                ↓
├─ Ammo：增加对应类型备弹
├─ Health：添加生命恢复 Buff
└─ Shield：添加护盾恢复 Buff
                ↓
服务器 Destroy Pickup
                ↓
├─ 销毁复制给客户端，播放表现
└─ SpawnPoint 收到 OnDestroyed
                ↓
开始下一轮随机刷新
```

## 技术亮点

### 服务器权威的拾取判定

Overlap 回调只在服务器实例注册，弹药与恢复效果也只由服务器入口触发。客户端没有“请求增加资源”的 RPC，不能通过伪造拾取消息直接修改战斗资源。

### 事件驱动的循环刷新

SpawnPoint 使用 Timer 与 `OnDestroyed` 组成循环，不通过 Tick 轮询。每件 Pickup 的销毁事件自然成为下一轮随机刷新计时的起点。

### 明确的 Pickup 生命周期

`Inactive → Active → Consumed` 将生成保护、可拾取状态和消费状态清楚分离。Pickup 只有在 SpawnPoint 完成生命周期监听后才开放碰撞，避免刚生成就销毁导致刷新链断开。

### 一次性消费保护

服务器在应用派生效果前先设置 `bConsumed` 并关闭碰撞，使同一服务器帧内的多个 Overlap 回调只有第一个能够获得消费权，避免重复加弹药或重复附加 Buff。

### 基类生命周期与派生效果分离

`APickup` 负责权限、激活、碰撞、消费与销毁；派生类只提供弹药、生命或护盾效果。刷新点只依赖 `APickup` 基类和 `OnDestroyed`，不需要了解具体效果实现。

### 数值规则与消费规则分离

有效角色触碰即消费，资源是否已经满值不会改变 Pickup 生命周期。这样消费规则稳定统一，派生数值系统只负责 Clamp 或累计恢复量。

## 截图建议

建议为个人页面准备以下画面：

1. 同一个 SpawnPoint 随机刷新弹药、生命和护盾的组合截图。
2. 角色接近带紫色描边和旋转表现的 Pickup。
3. 生命 Pickup 消失并播放 Niagara 效果的瞬间。
4. 拾取物消失后，原位置经过随机等待重新生成新 Pickup。

## 单元总结

拾取系统以 `APickup` 统一碰撞和生命周期，以三个派生类分别提供弹药、生命与护盾效果，并通过 `APickupSpawnPoint` 的随机 Timer 和 `OnDestroyed` 事件形成持续刷新循环。

Pickup 的重叠处理只在服务器注册，数值效果与销毁结果均由服务器决定；客户端只显示复制得到的 Actor 和销毁表现。优化后的 `Inactive → Active → Consumed` 阶段让 Pickup 在刷新点完成生命周期绑定后才开放碰撞，而 `bConsumed` 与立即关闭碰撞保证同一帧只有第一个有效角色能够取得效果。

整个单元保持“有效角色触碰即消耗”的规则：即使弹药、生命或护盾已经达到上限，Pickup 仍会进入 `Consumed` 并被服务器销毁。该系统不生成背包物品，也不依赖空间物品格，边界始终限定在即时生效的场景 Pickup。

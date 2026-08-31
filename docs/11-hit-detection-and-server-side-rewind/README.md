# 子弹发射、伤害确认与服务器回溯

## 功能点介绍

多人射击中，客户端看到的其他角色并不是服务器“此刻”的位置，而是经过网络传输和插值后得到的历史画面。如果服务器只在收到开火请求时检测目标当前位置，高延迟玩家即使在自己的画面中准确命中，也可能因为目标已经在服务器上移动而被判定未命中。

本项目采用本地命中预测与服务器回溯（Server-Side Rewind，SSR）结合的方式：

```text
客户端立即完成射击表现和候选命中检测
                    ↓
提交目标、射击轨迹和客户端画面对应的服务器时间
                    ↓
服务器将目标 HitBox 恢复到该历史时刻
                    ↓
服务器重新执行射线或弹道检测
                    ↓
确认命中部位和伤害类型
                    ↓
服务器调用 ApplyDamage()
```

本单元包含：

- 角色骨骼 HitBox 与独立碰撞通道的设计。
- 只由服务器记录的历史帧。
- 环形缓冲区与固定 HitBox 数组的历史数据结构。
- 客户端如何计算服务器时间、平均单程延迟与 `HitTime`。
- 射线武器的本地预测、确认请求和服务器回溯。
- Projectile 武器的本地弹道模拟、确认请求和服务器弹道重建。
- 服务器确认命中后调用 `ApplyDamage()` 的权威边界。

本单元不继续介绍 `ApplyDamage()` 之后如何扣除生命与护盾，也不展开霰弹枪、火箭、淘汰和复活逻辑。

## 为什么需要服务器回溯

假设客户端与服务器之间的 RTT 为 `100 ms`，则平均单程延迟约为 `50 ms`。客户端开火时看到的远端角色，大致对应服务器时间轴上约 `50 ms` 之前的状态。

```text
服务器历史时间              客户端开火                    服务器收到请求
      │                         │                              │
      ├────约 50 ms─────────────┤────约 50 ms─────────────────┤
      │                         │                              │
目标在客户端画面中的状态       本地判定候选命中             服务器当前状态已经变化
```

因此客户端不能只告诉服务器“我现在打中了”，而要提交：

- 被本地预测命中的角色。
- 射线起点与目标点，或 Projectile 的初始速度。
- 本地画面对应的服务器历史时刻 `HitTime`。

服务器用自己的历史记录重新检测。客户端负责及时反馈，服务器负责最终裁决。

## 骨骼 HitBox 设计

### 为主要身体部位建立独立碰撞盒

角色创建了 16 个 `UBoxComponent`，分别附加到对应骨骼，并随动画姿态移动：

| 固定索引 | HitBox 名称 | 对应部位 |
| ---: | --- | --- |
| 0 | `Head` | 头部 |
| 1 | `Hips` | 骨盆 |
| 2 | `Spine1` | 躯干下部 |
| 3 | `Spine2` | 躯干上部 |
| 4 | `LeftArm` | 左上臂 |
| 5 | `RightArm` | 右上臂 |
| 6 | `LeftForeArm` | 左前臂 |
| 7 | `RightForeArm` | 右前臂 |
| 8 | `LeftHand` | 左手 |
| 9 | `RightHand` | 右手 |
| 10 | `LeftUpLeg` | 左大腿 |
| 11 | `RightUpLeg` | 右大腿 |
| 12 | `LeftLeg` | 左小腿 |
| 13 | `RightLeg` | 右小腿 |
| 14 | `LeftFoot` | 左脚 |
| 15 | `RightFoot` | 右脚 |

这些碰撞盒比胶囊体更接近角色在某一动画帧中的真实姿态，也允许服务器区分头部与身体命中。

### 独立的 `ECC_HitBox` 通道

项目在 `Blaster.h` 中定义：

```cpp
#define ECC_HitBox ECollisionChannel::ECC_GameTraceChannel2
```

每个 HitBox：

```cpp
Box.Value->SetCollisionObjectType(ECC_HitBox);
Box.Value->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

HitBox 平时关闭碰撞，只在服务器执行回溯确认的极短时间内开启。回溯检测使用 `ECC_HitBox`，不会让角色胶囊体、普通骨骼网格体或场景可见性碰撞干扰历史命中确认。

这里的 `ECC_HitBox` 专门服务于伤害回溯，与准心识别使用的碰撞设置不是同一个功能。

相关实现：

- `Source/Blaster/Blaster.h:8`
- `Source/Blaster/Character/BlasterCharacter.cpp:76`
- `Source/Blaster/Character/BlasterCharacter.cpp:142`
- `Source/Blaster/Character/BlasterCharacter.h:51`

## 历史帧记录

### 当前源码基线

当前 `ULagCompensationComponent` 使用 `TDoubleLinkedList<FFramePackage>` 保存历史帧，每帧内部使用 `TMap<FName, FBoxInformation>` 保存 HitBox：

```cpp
USTRUCT(BlueprintType)
struct FBoxInformation
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector BoxExtent;
};

USTRUCT(BlueprintType)
struct FFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY()
	TMap<FName, FBoxInformation> HitBoxInfo;

	UPROPERTY()
	ABlasterCharacter* Character;
};

TDoubleLinkedList<FFramePackage> FrameHistory;
```

新帧插入链表头部，链表尾部是最旧帧；当首尾时间差超过 `MaxRecordTime` 时删除尾部节点。服务器查找 `HitTime` 时从新到旧遍历，在较老帧与较新帧之间插值得到目标历史姿态。

### 采用的优化结构：环形缓冲区＋固定 HitBox 数组

角色 HitBox 的数量和语义在运行期间不会变化，因此不需要每帧通过 `FName` 哈希查找。优化后的设计将 HitBox 映射为固定索引，并预分配固定容量的历史帧环形缓冲区。

概念结构如下：

```cpp
enum class ERewindHitBox : uint8
{
	Head,
	Hips,
	Spine1,
	Spine2,
	LeftArm,
	RightArm,
	LeftForeArm,
	RightForeArm,
	LeftHand,
	RightHand,
	LeftUpLeg,
	RightUpLeg,
	LeftLeg,
	RightLeg,
	LeftFoot,
	RightFoot,
	Count
};

struct FRewindBoxPose
{
	FVector Location;
	FRotator Rotation;
};

struct FRewindFrame
{
	float Time = 0.f;
	TStaticArray<FRewindBoxPose,
		static_cast<uint8>(ERewindHitBox::Count)> HitBoxes;
};

struct FRewindFrameBuffer
{
	TArray<FRewindFrame> Frames; // 初始化时一次性 SetNum(Capacity)
	int32 WriteIndex = 0;
	int32 ValidCount = 0;
};
```

写入新帧时：

```text
Frames[WriteIndex] = NewFrame
WriteIndex = (WriteIndex + 1) % Capacity
ValidCount = Min(ValidCount + 1, Capacity)
```

该结构的优势是：

- 缓冲区初始化后容量固定，不会持续创建和释放链表节点。
- 每帧覆盖最旧位置，插入复杂度稳定为 `O(1)`。
- HitBox 通过整数索引直接访问，不再执行 `FName` 哈希和 `TMap` 节点遍历。
- 每帧数据连续存储，CPU Cache 局部性更好。
- 历史时间在逻辑顺序上保持有序，可以通过环形下标进行二分查找，再对相邻两帧插值。

### 不重复保存固定 `BoxExtent`

`BoxExtent` 是角色 HitBox 的形状配置，不会随普通移动或动画帧变化。优化后的历史帧只保存每个碰撞盒会变化的：

```text
Location + Rotation
```

各 HitBox 的 `BoxExtent` 在角色或回溯组件初始化时缓存一份：

```cpp
TStaticArray<FVector,
	static_cast<uint8>(ERewindHitBox::Count)> FixedBoxExtents;
```

执行回溯时通过固定索引同时取得历史姿态和固定尺寸：

```text
HistoricalPose[HitBoxIndex]
+ FixedBoxExtents[HitBoxIndex]
= 该历史时刻的完整碰撞盒
```

以 16 个 HitBox 为例，这避免了每个角色、每个历史帧重复保存 16 个 `FVector BoxExtent`。

### 只在服务器运行历史记录 Tick

历史帧只有服务器会在命中确认时使用。客户端不需要维护一份永远不会参与权威判定的数据。

当前源码在 `SaveFramePackage()` 中通过权限检查提前返回：

```cpp
if (Character == nullptr || !Character->HasAuthority()) return;
```

采用的优化设计进一步在组件初始化后关闭非权威实例的组件 Tick：

```cpp
SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
```

这样客户端不会每帧进入组件 Tick 后再提前返回，历史内存也只在服务器分配。

## 根据 `HitTime` 取得历史姿态

服务器收到确认请求后，先从被命中角色自己的历史缓冲区中寻找 `HitTime`：

```text
HitTime < OldestTime
    → 请求超出历史窗口，拒绝回溯

HitTime >= NewestTime
    → 使用最新历史帧

HitTime == 某个帧时间
    → 直接使用该帧

Older.Time < HitTime < Younger.Time
    → 对两帧的 16 组位置和旋转进行插值
```

插值比例为：

```text
Fraction = Clamp(
    (HitTime - Older.Time) / (Younger.Time - Older.Time),
    0,
    1
)
```

最终得到的不是某个历史采样帧的近似选择，而是目标在 `HitTime` 上的插值姿态。

当前源码的插值核心为：

```cpp
FFramePackage ULagCompensationComponent::InterpBetweenFrames(
	const FFramePackage& OlderFrame,
	const FFramePackage& YoungerFrame,
	float HitTime)
{
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp(
		(HitTime - OlderFrame.Time) / Distance,
		0.f,
		1.f
	);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	for (auto& YoungerPair : YoungerFrame.HitBoxInfo)
	{
		const FName& BoxInfoName = YoungerPair.Key;
		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];

		FBoxInformation InterpBoxInfo;
		InterpBoxInfo.Location = FMath::VInterpTo(
			OlderBox.Location,
			YoungerBox.Location,
			1.f,
			InterpFraction
		);
		InterpBoxInfo.Rotation = FMath::RInterpTo(
			OlderBox.Rotation,
			YoungerBox.Rotation,
			1.f,
			InterpFraction
		);
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;
		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}

	return InterpFramePackage;
}
```

固定数组版本保持相同的时间插值含义，只将 `TMap` 遍历改为 `0～15` 的连续索引循环，并从独立配置中取得 `BoxExtent`。

## 时间同步与真实服务器时间

### 单次采样

客户端发送请求时记录本地时间 `T0`，服务器收到后记录服务器时间 `Ts`，客户端收到回复时记录本地时间 `T1`：

```text
RTTᵢ = T1ᵢ - T0ᵢ
OneWayᵢ = RTTᵢ / 2
EstimatedServerTimeᵢ = Tsᵢ + OneWayᵢ
Deltaᵢ = EstimatedServerTimeᵢ - T1ᵢ
```

`OneWayᵢ` 建立在上、下行耗时近似对称的假设上。服务器返回自己的接收时间，使客户端不仅能测量 RTT，还能估算本地世界时间与服务器世界时间的差值。

### 样本过滤与平均时延

单次 RPC 可能受到可靠消息排队、线程调度和瞬时拥塞影响，因此不会直接覆盖正式时间结果。客户端维护固定容量的滑动样本窗口：

1. 收集最近若干组 `{RTTᵢ, OneWayᵢ, Deltaᵢ}`。
2. 丢弃负值、超出允许范围等无效 RTT。
3. 按 RTT 排序，只保留 RTT 较低的一半样本。
4. 对有效样本的单程延迟和时钟偏差分别求平均。

```text
SingleTripTime = Average(ValidSamples.OneWay)
ClientServerDelta = Average(ValidSamples.Delta)
```

客户端随后通过以下方式取得服务器当前时间：

```cpp
float ABlasterPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}
```

这套算法与文档 06 的比赛倒计时共用同一服务器时间基准。比赛 HUD 和伤害回溯不会各自维护一套互相偏离的网络时钟。

### 为什么命中时间还要减去单程延迟

客户端估算出服务器“现在”后，仍然不能直接把这个时刻作为命中时间，因为客户端画面中的远端角色本身来自更早的服务器状态：

```cpp
const float HitTime = GetServerTime() - SingleTripTime;
```

含义为：

```text
客户端估算的服务器当前时间
- 过滤后平均单程延迟
= 客户端开火画面中目标所对应的服务器历史时间
```

服务器收到 RPC 的时刻不会替代 `HitTime`。网络请求晚到服务器，只会影响什么时候开始验证，不会改变服务器应该回到哪个历史时刻验证。

## 射线武器完整链路

### 1. 客户端本地预测

射线武器从枪口 `MuzzleFlash` Socket 取得 `Start`，再从枪口向准心计算出的 `HitTarget` 执行 `WeaponTraceHit()`。

```cpp
FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;

World->LineTraceSingleByChannel(
	OutHit,
	TraceStart,
	End,
	ECollisionChannel::ECC_Visibility
);
```

本地射线立即得到候选目标、骨骼名称和命中位置，并立即生成弹道、枪口火焰、命中特效和音效。这里预测的是“视觉命中结果和候选伤害类型”，客户端不会直接修改其他玩家的权威生命值。

### 2. 客户端发送服务器确认请求

当本地射线命中角色，并且当前是使用 SSR 的远程客户端时，客户端发送：

```cpp
ServerScoreRequest(
	BlasterCharacter,
	Start,
	HitTarget,
	GetServerTime() - SingleTripTime,
	this
);
```

请求携带：

- `BlasterCharacter`：本地预测命中的候选角色。
- `Start`：服务器重新检测所需的射线起点。
- `HitTarget`：射线方向与终点依据。
- `HitTime`：客户端画面对应的服务器历史时刻。
- `DamageCauser`：服务器确认后读取武器伤害数据。

### 3. 服务器恢复目标历史姿态

服务器在目标角色的历史缓冲区中找到 `HitTime` 对应姿态，然后：

1. 缓存目标 HitBox 当前姿态。
2. 把所有 HitBox 移动到历史位置和旋转。
3. 临时关闭目标骨骼网格体碰撞。
4. 只启用头部 HitBox，使用 `ECC_HitBox` 执行第一次射线。
5. 若头部未命中，再启用全部 HitBox 执行身体检测。
6. 恢复目标当前 HitBox 姿态和骨骼网格体碰撞。

“先头部、后身体”的两阶段检测让服务器直接得到 `bHeadShot`，不依赖客户端上报的骨骼名称。

关键确认代码：

```cpp
FServerSideRewindResult ULagCompensationComponent::ConfirmHit(
	const FFramePackage& Package,
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("Head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World)
	{
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECC_HitBox
		);

		if (ConfirmHitResult.bBlockingHit)
		{
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(
				HitCharacter,
				ECollisionEnabled::QueryAndPhysics
			);
			return FServerSideRewindResult{ true, true };
		}

		for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
		{
			if (HitBoxPair.Value)
			{
				HitBoxPair.Value->SetCollisionEnabled(
					ECollisionEnabled::QueryAndPhysics
				);
				HitBoxPair.Value->SetCollisionResponseToChannel(
					ECC_HitBox,
					ECollisionResponse::ECR_Block
				);
			}
		}

		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECC_HitBox
		);

		if (ConfirmHitResult.bBlockingHit)
		{
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(
				HitCharacter,
				ECollisionEnabled::QueryAndPhysics
			);
			return FServerSideRewindResult{ true, false };
		}
	}

	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(
		HitCharacter,
		ECollisionEnabled::QueryAndPhysics
	);
	return FServerSideRewindResult{ false, false };
}
```

### 4. 服务器确认并调用 `ApplyDamage()`

```cpp
void ULagCompensationComponent::ServerScoreRequest_Implementation(
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize& HitLocation,
	float HitTime,
	AWeapon* DamageCauser)
{
	FServerSideRewindResult Confirm = ServerSideRewind(
		HitCharacter,
		TraceStart,
		HitLocation,
		HitTime
	);

	if (Character && HitCharacter && DamageCauser && Confirm.bHitConfirmed)
	{
		const float Damage = Confirm.bHeadShot
			? DamageCauser->GetHeadShotDamage()
			: DamageCauser->GetDamage();

		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Damage,
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}
```

`Confirm.bHitConfirmed` 与 `Confirm.bHeadShot` 都由服务器的历史 HitBox 检测产生。客户端提交的是验证材料，不是最终伤害结论。

### `AHitScanWeapon::Fire()` 完整实现

```cpp
void AHitScanWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();

		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);

		ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(FireHit.GetActor());
		// 任意玩家开火，都会因为MulticastFire而在本地调用此函数，但是只有开火者与服务端拥有InstigatorController
		if (BlasterCharacter && InstigatorController)
		{
			const float DamageToCause = FireHit.BoneName.ToString() == FString("Head") ? HeadShotDamage : Damage;
			if (HasAuthority() && (!bUseServerSideRewind || OwnerPawn->IsLocallyControlled()))
			{
				UGameplayStatics::ApplyDamage(
					BlasterCharacter,
					DamageToCause,
					InstigatorController,
					this,
					UDamageType::StaticClass()
				);
			}
			if (!HasAuthority() && bUseServerSideRewind)
			{
				BlasterOwnerCharacter = BlasterOwnerCharacter == nullptr ? Cast<ABlasterCharacter>(OwnerPawn) : BlasterOwnerCharacter;
				BlasterOwnerController = BlasterOwnerController == nullptr ? Cast<ABlasterPlayerController>(InstigatorController) : BlasterOwnerController;
				if (BlasterOwnerController && BlasterOwnerCharacter && BlasterOwnerCharacter->GetLagCompensation())
				{
					// 客户端的画面，角色位置在RTT/2之前，发送RPC也应该使用这个时间点
					BlasterOwnerCharacter->GetLagCompensation()->ServerScoreRequest(
						BlasterCharacter,
						Start,
						HitTarget,
						BlasterOwnerController->GetServerTime() - BlasterOwnerController->SingleTripTime,
						this
					);
				}
			}
		}
		if (ImpactParticles)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				ImpactParticles,
				FireHit.ImpactPoint,
				FireHit.ImpactNormal.Rotation()
			);
		}
		if (HitSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				HitSound,
				FireHit.ImpactPoint
			);
		}
		if (MuzzleFlash)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(),
				MuzzleFlash,
				SocketTransform
			);
		}
		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				FireSound,
				GetActorLocation()
			);
		}
	}
}
```

## Projectile 武器完整链路

### 1. 从枪口指向准心目标

Projectile 不是从摄像机或屏幕中心生成，而是从武器 `MuzzleFlash` Socket 生成。枪口到准心目标的向量决定初始旋转：

```cpp
FVector ToTarget = HitTarget - SocketTransform.GetLocation();
FRotator TargetRotation = ToTarget.Rotation();
```

这样屏幕准心决定玩家意图，枪口位置决定真实弹道起点。

### 2. 根据权限和本地控制关系生成 Projectile

使用 SSR 时，各网络实例生成的 Projectile 职责不同：

| 执行位置 | 角色关系 | Projectile 用途 | 是否提交 SSR |
| --- | --- | --- | --- |
| 服务器 | 服务器本地角色 | 服务器直接模拟并可直接造成伤害 | 否 |
| 服务器 | 远程客户端角色 | 服务器侧表现和网络流程 | 是 |
| 客户端 | 本地控制角色 | 本地即时弹道预测，记录回溯参数 | 是 |
| 客户端 | 非本地角色 | 远端射击表现 | 否 |

不使用 SSR 时，由服务器生成正常的可复制 Projectile。无论采用哪条路径，生成后的碰撞盒都会忽略发射者，避免子弹刚离开枪口便与自己发生碰撞。

### 3. 本地 Projectile 保存回溯参数

本地控制客户端生成 Projectile 时记录：

```cpp
SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
SpawnedProjectile->InitialVelocity =
	SpawnedProjectile->GetActorForwardVector() *
	SpawnedProjectile->InitialSpeed;
```

客户端在本地正常模拟 Projectile。发生候选角色碰撞时，不把客户端当前 Projectile 位置作为权威结果，而是把最初的发射条件交给服务器：

```cpp
const float HitTime = OwnerController->GetServerTime()
	- OwnerController->SingleTripTime;

OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
	HitCharacter,
	TraceStart,
	InitialVelocity,
	HitTime
);
```

### 4. 服务器在历史姿态上重建弹道

服务器取得目标在 `HitTime` 的历史姿态后，先只开启头部 HitBox，再使用 `PredictProjectilePath()` 根据客户端提交的 `TraceStart` 和 `InitialVelocity` 重建弹道：

```cpp
FPredictProjectilePathParams PathParams;
PathParams.bTraceWithCollision = true;
PathParams.MaxSimTime = MaxRecordTime;
PathParams.LaunchVelocity = InitialVelocity;
PathParams.StartLocation = TraceStart;
PathParams.SimFrequency = 15.f;
PathParams.ProjectileRadius = 5.f;
PathParams.TraceChannel = ECC_HitBox;
PathParams.ActorsToIgnore.Add(GetOwner());

FPredictProjectilePathResult PathResult;
UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
```

如果头部未命中，服务器启用其余历史 HitBox，再预测一次弹道。完成后恢复目标的当前碰撞状态。

射线武器重新执行的是一条直线；Projectile 武器重新执行的是带初始速度、碰撞半径和模拟频率的运动轨迹。两者最终都只信任服务器在历史 HitBox 上得到的结果。

### 5. 服务器确认 Projectile 伤害

```cpp
void ULagCompensationComponent::ProjectileServerScoreRequest_Implementation(
	ABlasterCharacter* HitCharacter,
	const FVector_NetQuantize& TraceStart,
	const FVector_NetQuantize100& InitialVelocity,
	float HitTime)
{
	FServerSideRewindResult Confirm = ProjectileServerSideRewind(
		HitCharacter,
		TraceStart,
		InitialVelocity,
		HitTime
	);

	if (Character && HitCharacter && Confirm.bHitConfirmed &&
		Character->GetEquippedWeapon())
	{
		const float Damage = Confirm.bHeadShot
			? Character->GetEquippedWeapon()->GetHeadShotDamage()
			: Character->GetEquippedWeapon()->GetDamage();

		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Damage,
			Character->Controller,
			Character->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}
```

### `AProjectileWeapon::Fire()` 完整实现

```cpp
void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	UWorld* World = GetWorld();

	if (InstigatorPawn && MuzzleFlashSocket && World)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		// From muzzle flash socket to hit location from TraceUnderCrosshairs
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = InstigatorPawn;

		AProjectile* SpawnedProjectile = nullptr;
		if (bUseServerSideRewind)
		{
			if (InstigatorPawn->HasAuthority()) // server
			{
				if (InstigatorPawn->IsLocallyControlled()) // 服务端响应服务端角色，不需要复制，且不使用SSR
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					if (SpawnedProjectile) 
					{
						SpawnedProjectile->bUseServerSideRewind = false;
						SpawnedProjectile->Damage = Damage;
						SpawnedProjectile->HeadShotDamage = HeadShotDamage;
					}
				}
				else // 服务端响应客户端角色，不需要复制，但使用SSR
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					if (SpawnedProjectile) SpawnedProjectile->bUseServerSideRewind = true;
				}
			}
			else // client, using SSR
			{
				if (InstigatorPawn->IsLocallyControlled()) // 客户端响应本地角色，不复制，使用SSR
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					if (SpawnedProjectile)
					{
						SpawnedProjectile->bUseServerSideRewind = true;
						SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
						SpawnedProjectile->InitialVelocity = SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
					}
				}
				else // 客户端响应非本地角色，只需要动画效果
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					if (SpawnedProjectile)
					{
						SpawnedProjectile->bUseServerSideRewind = false;
						SpawnedProjectile->Damage = Damage;
						SpawnedProjectile->HeadShotDamage = HeadShotDamage;
					}
				}
			}
		}
		else // 不需要延迟补偿，直接生成复制的子弹
		{
			if (InstigatorPawn->HasAuthority())
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				if (SpawnedProjectile) SpawnedProjectile->bUseServerSideRewind = false;
			}
		}
		if (SpawnedProjectile)// 子弹忽略自己
		{
			UBoxComponent* CollisionBox = SpawnedProjectile->GetCollisionBox();
			if (CollisionBox)
			{
				CollisionBox->IgnoreActorWhenMoving(InstigatorPawn, true);
			}
		}
	}
}
```

### Projectile 本地命中后的请求代码

```cpp
void AProjectileBullet::OnHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	ABlasterCharacter* OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
	ABlasterCharacter* HitCharacter = Cast<ABlasterCharacter>(OtherActor);
	ABlasterPlayerController* OwnerController = OwnerCharacter
		? Cast<ABlasterPlayerController>(OwnerCharacter->Controller)
		: nullptr;

	if (OwnerCharacter && OwnerController)
	{
		if (OwnerCharacter->HasAuthority() && OwnerCharacter->IsLocallyControlled())
		{
			const float DamageToCause = Hit.BoneName.ToString() == FString("Head")
				? HeadShotDamage
				: Damage;
			UGameplayStatics::ApplyDamage(
				OtherActor,
				DamageToCause,
				OwnerController,
				this,
				UDamageType::StaticClass()
			);
			Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
			return;
		}

		if (bUseServerSideRewind &&
			OwnerCharacter->GetLagCompensation() &&
			OwnerCharacter->IsLocallyControlled() &&
			HitCharacter)
		{
			const float HitTime = OwnerController->GetServerTime()
				- OwnerController->SingleTripTime;
			OwnerCharacter->GetLagCompensation()->ProjectileServerScoreRequest(
				HitCharacter,
				TraceStart,
				InitialVelocity,
				HitTime
			);
		}
	}

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}
```

## 两类武器的回溯链路对照

| 阶段 | HitScan | Projectile |
| --- | --- | --- |
| 本地即时检测 | 枪口射线 | 本地 Projectile 运动与碰撞 |
| 客户端候选数据 | 目标角色、`TraceStart`、`HitTarget` | 目标角色、`TraceStart`、`InitialVelocity` |
| 共同时间戳 | `GetServerTime() - SingleTripTime` | `GetServerTime() - SingleTripTime` |
| 服务器历史查询 | 目标在 `HitTime` 的 HitBox 姿态 | 目标在 `HitTime` 的 HitBox 姿态 |
| 服务器重新检测 | `LineTraceSingleByChannel` | `PredictProjectilePath` |
| 爆头确认 | 头部 HitBox 首轮检测 | 头部 HitBox 首轮弹道预测 |
| 身体确认 | 全部 HitBox 第二轮检测 | 全部 HitBox 第二轮弹道预测 |
| 权威结果 | 服务器 `ApplyDamage()` | 服务器 `ApplyDamage()` |

## 性能与规范性亮点

### 本地预测与服务器权威分离

开火者无需等待一次网络往返才看到弹道和命中特效；同时客户端不能直接决定其他玩家的最终伤害。表现响应速度与服务器权威判定被明确分离。

### 独立 HitBox 通道降低碰撞干扰

历史确认只检测临时开启的 `ECC_HitBox`。角色胶囊体和骨骼网格体不会参与回溯射线，服务器可以围绕骨骼姿态精确区分头部与身体。

### 环形缓冲区保证固定内存与稳定写入

预分配环形缓冲区把历史写入稳定为 `O(1)`，避免链表节点的频繁分配和释放；固定 HitBox 数组移除每帧 `TMap` 的哈希与节点开销，并让历史姿态连续存储。

### 固定数据与帧数据分离

每帧只记录会变化的位置和旋转，`BoxExtent` 只保存一份。历史窗口越长、玩家越多，这项优化节省的内存越明显。

### 客户端彻底停止历史 Tick

历史数据只用于服务器裁决，因此非权威组件不只是保存时提前返回，而是直接关闭 Tick，不分配历史缓冲区，也不执行无效的逐帧函数调用。

### 时间样本过滤减少回溯抖动

客户端通过多次 RPC 样本估算服务器时间，过滤高 RTT 尖峰后再计算平均单程延迟和平均时钟偏差。倒计时与 SSR 共用这一时间基准，使 `HitTime` 不会被一次偶发排队直接拉动。

### 网络向量量化

回溯 RPC 使用 `FVector_NetQuantize` 与 `FVector_NetQuantize100` 传递射线和速度数据，在保持命中确认所需精度的同时减少网络序列化体积。

## 单元总结

这套系统以客户端本地预测保证射击反馈，以服务器历史回溯保证伤害权威。角色的 16 个骨骼 HitBox 使用独立 `ECC_HitBox` 通道，平时关闭，只在服务器验证命中的短暂阶段恢复到历史姿态并开启。

客户端通过经过过滤的多组时间同步样本计算平均 `SingleTripTime` 和 `ClientServerDelta`，再用 `GetServerTime() - SingleTripTime` 标记自己开火画面对应的服务器历史时刻。HitScan 提交射线起点和目标点，由服务器重新执行直线检测；Projectile 提交发射起点和初始速度，由服务器在历史 HitBox 上重新预测弹道。两条链路都由服务器确认普通命中或爆头，最终止于服务器调用 `ApplyDamage()`。

历史帧采用预分配环形缓冲区和固定 HitBox 数组：新帧以 `O(1)` 覆盖写入，HitBox 通过固定索引访问，`BoxExtent` 不再随每帧重复保存，非服务器实例则完全关闭历史记录 Tick。由此形成了“客户端即时表现、统一服务器时间、服务器历史复原、服务器权威伤害”的完整多人射击判定链路。

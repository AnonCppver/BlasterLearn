# 生命、护盾与持续恢复 Buff

## 功能点介绍

角色的生命与护盾由服务器统一维护。收到有效伤害后，服务器先使用护盾吸收伤害，再将溢出部分扣除生命；获得恢复 Buff 后，也由服务器上的 `UBuffComponent` 按时间逐步增加对应数值。

客户端不参与生命、护盾的权威计算，只接收服务器复制的最新结果，用于刷新 HUD 和播放受击反馈。即使客户端修改本地显示，也不能改变服务器保存的真实数值，从规则层面降低了客户端作弊对比赛结果的影响。

本单元只说明：

- 收到有效伤害后，护盾与生命如何变化。
- 服务器如何将变化后的数值同步给客户端。
- 客户端如何更新 HUD 并避免重复受击反馈。
- 获得生命或护盾恢复 Buff 后，持续恢复如何执行。
- 如何通过参数校验与按需 Tick 提高规范性和运行效率。

本单元不讨论伤害来自哪里，也不讨论恢复 Buff 如何获得。生命到达零之后的流程不在本单元展开。

## 基础生命与护盾数据

角色当前使用以下默认值：

| 数据 | 初始值 | 最大值 |
| --- | ---: | ---: |
| 生命 | 100 | 100 |
| 护盾 | 0 | 50 |

当前源码分别保存 `Health` 和 `Shield`，并通过 `ReplicatedUsing` 将服务器上的修改复制到客户端：

```cpp
UPROPERTY(ReplicatedUsing = OnRep_Health)
float Health = 100.f;

UPROPERTY(ReplicatedUsing = OnRep_Shield)
float Shield = 0.f;
```

`MaxHealth` 和 `MaxShield` 负责规定上限。所有扣减与恢复结果都被限制在合法范围内，避免出现负值或超过上限的数值。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.h:129`
- `Source/Blaster/Character/BlasterCharacter.cpp:154`

## 服务器权威更新

角色只在 `HasAuthority()` 时绑定伤害接收回调，因此实际的生命与护盾计算发生在服务器：

```text
服务器收到最终有效伤害
          ↓
服务器修改护盾与生命
          ↓
服务器保存新的权威数值
          ↓
属性复制到客户端
          ↓
客户端更新显示与反馈
```

客户端不会向服务器提交“我还剩多少生命”或“这次应扣多少护盾”。客户端只消费服务器的最终结果，因此本地篡改 HUD、Buff 速率或生命变量不会改变服务器上的比赛状态。

这种职责分离同时减少了客户端负担：

- 客户端不执行伤害分配计算。
- 客户端不执行持续恢复 Tick。
- 客户端不维护一套需要与服务器校正的预测生命值。
- 客户端只在复制值变化时刷新 HUD 和表现。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:172`
- `Source/Blaster/Character/BlasterCharacter.cpp:709`

## 护盾优先承受伤害

服务器获得最终有效伤害值后，按照“护盾优先、生命承担溢出”的规则处理。

### 护盾足够

当当前护盾大于或等于伤害时：

```text
Shield = Shield - Damage
Health 保持不变
```

### 护盾不足

当护盾无法完全吸收伤害时：

```text
RemainingDamage = Damage - Shield
Shield = 0
Health = Health - RemainingDamage
```

例如：

```text
处理前：Health = 100，Shield = 30
伤害值：Damage = 50

处理后：Shield = 0，Health = 80
```

护盾和生命分别使用最大值进行限制，最终结果不会小于零或大于上限。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:713`

## 复合生命护盾状态

### 当前实现的局限

当前源码将 `Health` 和 `Shield` 作为两个独立复制属性，并分别执行 `OnRep_Health()` 和 `OnRep_Shield()`。

当一次伤害同时击穿护盾并扣除生命时，两项属性都会改变。客户端可能分别收到两次回调，并在两个回调中重复播放受击动画。两个回调之间还存在到达顺序问题，客户端看到的瞬间状态不一定代表同一次完整结算。

### 采用的规范化方案

将生命与护盾组合为一个具有明确语义的复制状态：

```cpp
USTRUCT()
struct FCharacterVitals
{
    GENERATED_BODY()

    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    float Shield = 0.f;
};
```

角色只复制一项复合状态：

```cpp
UPROPERTY(ReplicatedUsing = OnRep_Vitals)
FCharacterVitals Vitals;

UFUNCTION()
void OnRep_Vitals(const FCharacterVitals& LastVitals);
```

RepNotify 的对象是整个 `FCharacterVitals`。复制完成后，成员变量 `Vitals` 保存服务器传来的新结构体，回调参数 `LastVitals` 保存复制前的旧结构体。服务器完成一次完整计算后再更新 `Vitals`，客户端通过一次 `OnRep_Vitals(LastVitals)` 同时比较两份完整状态：

```text
服务器完成护盾与生命分配
          ↓
一次性更新 FCharacterVitals
          ↓
客户端 OnRep_Vitals
          ├─ 更新生命 HUD
          ├─ 更新护盾 HUD
          └─ 判断是否需要播放一次受击反馈
```

复合结构的主要价值是状态一致性与回调去重。它不会仅因为“使用了结构体”就天然压缩网络字节，但能减少客户端对两个独立回调的处理，并为一次结算提供完整状态快照。

## 受击反馈去重

客户端在 `OnRep_Vitals()` 中同时比较新旧状态：

```cpp
void ABlasterCharacter::OnRep_Vitals(const FCharacterVitals& LastVitals)
{
    UpdateHUDHealth();
    UpdateHUDShield();

    const bool bHealthDecreased = Vitals.Health < LastVitals.Health;
    const bool bShieldDecreased = Vitals.Shield < LastVitals.Shield;

    if (bHealthDecreased || bShieldDecreased)
    {
        PlayHitReactMontage();
    }
}
```

这里的 `Vitals` 与 `LastVitals` 都是结构体对象。回调不会再分别接收 `LastHealth` 和 `LastShield`，因此一次结构体复制只产生一次统一的状态比较与表现入口。

由此统一三种情况：

| 状态变化 | 受击反馈 |
| --- | --- |
| 只减少护盾 | 播放一次 |
| 只减少生命 | 播放一次 |
| 同时减少护盾和生命 | 仍只播放一次 |
| 生命或护盾增加 | 不播放 |

`OnRep_Vitals()` 只根据服务器复制结果判断表现，客户端不需要知道伤害来源，也不需要重新执行护盾分配逻辑。

## HUD 更新与延迟初始化

客户端收到新的生命护盾状态后，通过 PlayerController 更新：

```text
HealthPercent = Health / MaxHealth
ShieldPercent = Shield / MaxShield
```

HUD 同时显示进度条和向上取整后的文本值。

如果角色数值先于 `CharacterOverlay` 到达，PlayerController 会暂存：

```text
HUDHealth
HUDMaxHealth
HUDShield
HUDMaxShield
```

`PollInit()` 检测到 HUD 创建完成后再写入缓存值，避免因为 UI 初始化顺序不同而丢失首次生命或护盾显示。重新控制角色时，`OnPossess()` 也会主动刷新两项状态。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:68`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:90`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:251`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:500`

## BuffComponent 的职责

角色创建独立的 `UBuffComponent`，用于处理需要经过一段时间完成的生命和护盾恢复：

```text
ABlasterCharacter
└─ UBuffComponent
      ├─ Heal：登记生命恢复
      ├─ HealRampUp：逐帧处理生命恢复
      ├─ ReplenishShield：登记护盾恢复
      └─ ShieldRampUp：逐帧处理护盾恢复
```

组件在初始化后保存所属角色引用，从而通过角色提供的访问接口读取和修改生命、护盾及其最大值。

Buff 的剩余量、速率和运行标记属于服务器运行时状态，不需要复制到客户端。服务器只复制最终的 `FCharacterVitals`，客户端不需要运行一套相同的 Buff 计时器。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:42`
- `Source/Blaster/Character/BlasterCharacter.cpp:301`
- `Source/Blaster/BlasterComponent/BuffComponent.h:8`

## 生命持续恢复

生命恢复开始后记录：

```text
bHealing = true
HealingRate = HealAmount / HealingTime
AmountToHeal += HealAmount
```

例如在 5 秒内恢复 100 点生命：

```text
HealingRate = 100 / 5 = 20 点/秒
AmountToHeal += 100
```

服务器每帧计算：

```text
RequestedThisFrame = HealingRate × DeltaTime
HealThisFrame = Min(RequestedThisFrame, AmountToHeal)

Health = Clamp(Health + HealThisFrame, 0, MaxHealth)
AmountToHeal -= HealThisFrame
```

使用剩余恢复量限制最后一帧，可以避免因为帧间隔导致实际消耗量超过 Buff 声明的总量。

相关实现：

- `Source/Blaster/BlasterComponent/BuffComponent.cpp:11`
- `Source/Blaster/BlasterComponent/BuffComponent.cpp:25`

## 护盾持续恢复

护盾恢复使用独立状态：

```text
bReplenishingShield = true
ShieldReplenishRate = ShieldAmount / ReplenishTime
ShieldReplenishAmount += ShieldAmount
```

每帧按照相同原则处理：

```text
RequestedThisFrame = ShieldReplenishRate × DeltaTime
ShieldThisFrame = Min(RequestedThisFrame, ShieldReplenishAmount)

Shield = Clamp(Shield + ShieldThisFrame, 0, MaxShield)
ShieldReplenishAmount -= ShieldThisFrame
```

生命恢复和护盾恢复使用不同的运行标记、速率和剩余量，因此两种效果可以同时存在。

相关实现：

- `Source/Blaster/BlasterComponent/BuffComponent.cpp:18`
- `Source/Blaster/BlasterComponent/BuffComponent.cpp:41`

## 满值时的持续恢复规则

生命与护盾达到最大值后都不会立即取消 Buff，而是继续消耗各自剩余恢复量：

```text
数值达到最大值
       ↓
Clamp 保持最大值
       ↓
继续消耗剩余恢复池
       ↓
如果恢复窗口内再次受到扣减，后续恢复仍可生效
       ↓
恢复池耗尽后结束
```

这使生命和护盾具有一致的持续恢复语义：Buff 表达一段仍在运行的恢复窗口，而不是“数值一满就立即移除”。

当前源码中的生命恢复已经接近这一行为；护盾恢复会在达到最大值时提前结束。规范化方案移除护盾的满值提前终止条件，使两项规则保持一致。

## Buff 参数校验

恢复入口应在登记状态前统一验证：

```text
调用发生在服务器
Character 引用有效
恢复量 > 0
持续时间 > KINDA_SMALL_NUMBER
恢复量与持续时间都是有限数值
```

不满足条件时直接拒绝启动恢复，避免：

- 持续时间为零产生除零。
- 负恢复量形成反向变化。
- `NaN` 或无限值污染服务器权威状态。
- 客户端自行启动本地恢复造成显示与服务器不一致。

同类 Buff 再次到来时保留当前玩法：剩余恢复量继续累加，恢复速率由最新一次 Buff 的“恢复量 ÷ 持续时间”覆盖。文档明确这一规则，避免把它误解为多条效果独立并行。

## 按需启停组件 Tick

持续恢复需要 Tick，但没有恢复效果时不需要每帧调用两个空函数。规范化后的生命周期为：

```text
BuffComponent 创建
└─ Tick 默认关闭

任一恢复 Buff 开始
└─ 服务器开启 Tick

只有一项恢复结束
└─ 另一项仍在运行，保持 Tick

两项恢复池都耗尽
└─ 关闭 Tick
```

客户端始终不运行恢复 Tick，只响应服务器复制的 `FCharacterVitals`。当场上角色数量增加时，大多数没有 Buff 的组件也不会产生无意义的逐帧开销。

## 技术亮点

### 服务器权威保证数值可信

伤害分配和持续恢复全部由服务器执行，客户端只接收结果。客户端没有修改权威生命或护盾的入口，从架构上降低了本地篡改对比赛规则的影响。

### 复合状态保证一次结算的一致性

`FCharacterVitals` 将生命和护盾作为同一次状态快照复制。客户端无需分别处理两个可能交错的通知，也不会因为同时扣除两项数值而重复播放受击动画。

### 客户端只承担展示职责

客户端不执行伤害分配、Buff 计时或恢复计算，只在服务器状态变化时刷新 HUD 和受击表现，减少重复计算和客户端状态校正成本。

### BuffComponent 按需消耗性能

恢复状态只在服务器保存，组件仅在至少一种恢复效果运行时开启 Tick。没有 Buff 的角色不会持续执行空的恢复函数。

### 恢复规则与输入边界明确

生命和护盾采用一致的满值持续消耗规则；恢复入口校验权限、数值和持续时间；最后一帧受剩余恢复量限制，使恢复行为更容易验证和维护。

## 单元总结

角色的生命与护盾由服务器统一计算并复制，客户端只负责显示服务器给出的结果。伤害首先由护盾吸收，溢出部分再扣除生命；复合 `FCharacterVitals` 将两项数值作为一次完整快照同步，使 HUD 更新和受击反馈建立在一致状态之上。

`UBuffComponent` 在服务器上分别维护生命和护盾的恢复速率与剩余恢复量，并通过按需 Tick 逐步更新权威状态。生命或护盾达到上限后仍继续消耗剩余恢复池，从而保留持续恢复窗口；恢复结束后关闭 Tick，客户端无需运行对应计算。

这套设计的核心是“服务器负责数值，客户端负责表现”：既降低客户端持续计算和重复回调的负担，也避免客户端本地数值成为比赛判定依据。

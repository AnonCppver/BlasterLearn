# 武器射击链路与本地预测手感优化

## 功能点介绍

武器射击系统将“玩家瞄准的位置”和“武器真正发射的位置”分开计算：本地客户端从屏幕准心取得世界射击点，再从枪口指向该点计算实际射击方向。玩家按下开火后，本机立即播放角色与武器的射击表现，同时把射击目标交给服务器；服务器通过多播同步给其他客户端，并让最初开火客户端跳过重复表现。

本单元包含：

- 从屏幕中心计算世界射击点。
- 从枪口到射击点计算实际射击角度。
- 武器散布如何改变最终射击方向。
- 开火输入、射击类型分派与网络调用链。
- 本地预测射击动画及多播去重。
- `bCanFire`、`FireDelay` 与自动武器持续开火。
- 射击动画、武器动画、弹壳、弹药与准心扩散反馈。
- 按住开火时，换弹结束后立即衔接射击。
- 弹匣耗尽后自动进入换弹。

本单元不涉及射线命中结果、抛射物碰撞、伤害计算、爆炸判定、爆头或服务器延迟补偿。

## 射击系统总体流程

```text
本地角色每帧计算 HitTarget
        ↓
玩家按下开火
        ↓
CanFire 条件检查
        ↓
根据 FireType 分派
├─ Projectile
├─ HitScan
└─ Shotgun
        ↓
本地客户端立即播放射击表现
        +
Server RPC 上传射击目标
        ↓
服务器 Multicast
        ↓
其他客户端播放相同表现
        ↓
FireTimer 控制下一次可开火时间
```

`CombatComponent` 负责输入状态、射速、射击目标和网络分发；具体武器负责自身动画、弹壳与弹匣消耗。

## 从屏幕准心计算射击点

### 只为本地角色计算

`CombatComponent::TickComponent()` 只在角色由当前机器本地控制时执行准心检测：

```cpp
if (Character && Character->IsLocallyControlled())
{
    FHitResult HitResult;
    TraceUnderCrosshairs(HitResult);
    HitTarget = HitResult.ImpactPoint;
}
```

远端角色不需要在每台客户端重复计算准心射线。射击目标由真正操作该角色的客户端生成，在开火时通过 RPC 发送给服务器。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:51`

### 屏幕坐标反投影

系统取得视口尺寸并计算屏幕中心：

```text
CrosshairX = ViewportWidth / 2
CrosshairY = ViewportHeight / 2
```

然后使用 `DeprojectScreenToWorld()` 将二维屏幕点转换成：

- `CrosshairWorldPosition`：屏幕中心射线在世界中的起点。
- `CrosshairWorldDirection`：从摄像机穿过准心的世界方向。

```text
屏幕中心
└─ DeprojectScreenToWorld
   ├─ 世界起点
   └─ 世界方向
```

### 将射线起点移动到角色前方

第三人称摄像机位于角色身后。如果直接从摄像机位置检测，射线可能先经过自己的角色模型。

系统先计算射线起点到角色的距离，再沿准心方向向前移动额外的 `100` 单位：

```cpp
DistanceToCharacter =
    (CharacterLocation - Start).Size();

Start += CrosshairWorldDirection *
    (DistanceToCharacter + 100.f);
```

这使实际射线从角色前方开始，减少自身模型对瞄准点计算的干扰。

### 得到始终有效的 HitTarget

```cpp
End = Start + CrosshairWorldDirection * TRACE_LENGTH;
```

射线存在阻挡结果时，使用 `ImpactPoint`。如果没有阻挡，则把 `ImpactPoint` 主动设置为射程终点：

```cpp
if (!TraceHitResult.bBlockingHit)
{
    TraceHitResult.ImpactPoint = End;
}
```

因此本地角色始终维护一个有效的 `HitTarget`，即使准心指向天空或远处空白区域也能正常计算射击方向。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:256`

## 从 HitTarget 计算射击角度

准心射线回答“玩家希望射向哪里”，枪口方向则回答“武器实际从哪里、以什么角度射出”。

抛射武器从武器骨骼网格体取得 `MuzzleFlash` Socket：

```text
MuzzleLocation = MuzzleFlash Socket 世界位置
ToTarget = HitTarget - MuzzleLocation
TargetRotation = ToTarget.Rotation()
```

最终使用：

- 枪口 Socket 作为生成位置。
- `TargetRotation` 作为实际射击角度。

```text
摄像机准心射线
└─ 确定世界射击点 HitTarget
        ↓
枪口到 HitTarget 的方向向量
└─ 转换为 TargetRotation
        ↓
从 MuzzleFlash Socket 发射
```

这种两阶段计算解决了第三人称摄像机与枪口不在同一位置造成的视差：准心仍然代表玩家的目标，但子弹不会从摄像机或屏幕中心生成。

相关实现：

- `Source/Blaster/Weapon/ProjectileWeapon.cpp:9`

## 散布如何改变射击角度

启用 `bUseScatter` 的武器不会直接使用原始 `HitTarget`。系统从枪口指向目标，在前方创建一个散布球：

```text
TraceStart = MuzzleLocation
ToTargetNormalized = Normalize(HitTarget - TraceStart)
SphereCenter = TraceStart + ToTargetNormalized × DistanceToSphere
```

随后在球内选取随机点：

```text
RandVec = RandomUnitVector × Random(0, SphereRadius)
EndLoc = SphereCenter + RandVec
```

再从枪口指向该随机点并延伸到正式射程，得到最终射击目标。

散布仍围绕玩家准心方向分布，但每发射击会得到不同角度。霰弹枪使用同一思路生成多个目标方向。

相关实现：

- `Source/Blaster/Weapon/Weapon.cpp:293`
- `Source/Blaster/Weapon/Shotgun.cpp:100`

## 开火输入与条件检查

角色按下开火键时调用：

```text
ABlasterCharacter::FireButtonPressed
└─ UCombatComponent::FireButtonPressed(true)
   ├─ bFireButtonPressed = true
   └─ Fire()
```

松开时只把 `bFireButtonPressed` 设置为 `false`，用于终止后续自动射击。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:545`
- `Source/Blaster/Character/BlasterCharacter.cpp:554`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:247`

### `CanFire` 门控

普通武器能够开火必须满足：

- 当前装备武器有效。
- 弹匣不为空。
- `bCanFire == true`。
- `CombatState == ECS_Unoccupied`。

霰弹枪额外允许在逐发换弹过程中，只要弹匣已经存在弹药并且射速门控允许，就可以中断换弹并射击。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:441`

## 根据 FireType 分派

成功通过 `CanFire()` 后，系统立即关闭本轮射击门控：

```cpp
bCanFire = false;
```

然后根据当前武器的 `FireType` 分派：

```text
EFT_Projectile
└─ FireProjectileWeapon

EFT_HitScan
└─ FireHitScanWeapon

EFT_Shotgun
└─ FireShotgun
```

三类武器共享输入、状态、射速和网络框架，只改变发送给具体武器的射击目标形式：

- 普通武器传递单个 `HitTarget`。
- 带散布武器传递修正后的单个目标。
- 霰弹枪传递一组散布目标。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:337`

## 本地预测开火表现

### 为什么不能等待服务器

如果客户端按下鼠标后必须等待 Server RPC 往返，再播放射击动画和枪械反馈，任何网络延迟都会直接表现成输入延迟。

因此开火客户端会立即执行 `LocalFire()`：

```text
玩家按下开火
├─ 立即 LocalFire
│  ├─ 播放角色射击 Montage
│  ├─ 播放武器射击动画
│  ├─ 生成弹壳表现
│  └─ 预测扣除弹匣弹药
│
└─ 同时发送 ServerFire
```

玩家首先看到的是本机预测表现，不需要等待服务器返回。

### Server RPC 与多播

普通武器使用：

```text
Client
└─ ServerFire(HitTarget)
        ↓
Server
└─ MulticastFire(HitTarget)
        ↓
所有相关网络实例
└─ LocalFire(HitTarget)
```

霰弹枪使用相同结构，只是 RPC 参数变为多个散布目标。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:314`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:326`

### 最初开火客户端跳过多播

最初发起射击的普通客户端已经执行过 `LocalFire()`。当服务器多播再次到达该客户端时，会执行：

```cpp
if (Character &&
    Character->IsLocallyControlled() &&
    !Character->HasAuthority())
{
    return;
}
```

这样可以避免同一次射击在开火客户端重复：

- 播放两次角色射击 Montage。
- 播放两次武器动画。
- 生成两次弹壳。
- 执行两次本地弹药变化。

服务器和其他客户端仍然通过多播正常播放对应表现。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:319`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:331`

### LocalFire 的表现链路

`LocalFire()` 只有在武器有效且状态允许时才执行：

```text
LocalFire
├─ Character::PlayFireMontage(bAiming)
└─ Weapon::Fire(HitTarget)
   ├─ 武器网格播放 FireAnimation
   ├─ AmmoEject Socket 生成弹壳
   └─ SpendRound
```

角色射击 Montage 根据瞄准状态选择：

- 未瞄准：`RifleHip`。
- 正在瞄准：`RifleAim`。

武器自身同时播放 `FireAnimation`，使角色上半身动作、枪械机械动作和弹壳抛出在本地立即出现。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:397`
- `Source/Blaster/Character/BlasterCharacter.cpp:339`
- `Source/Blaster/Weapon/Weapon.cpp:190`

### 预测弹药与服务器绝对值

`Weapon::Fire()` 最后调用 `SpendRound()`，本地预测会立即把弹匣减一并刷新 HUD。服务器执行相同射击后，也会修改权威弹药并强制网络更新。

客户端最终收到复制属性 `Weapon::Ammo` 时，通过 `OnRep_Ammo()` 使用服务器绝对值刷新 HUD。也就是说：

```text
本地预测值
└─ 用于即时显示和手感

服务器复制值
└─ 用于最终校正
```

相关实现：

- `Source/Blaster/Weapon/Weapon.cpp:117`
- `Source/Blaster/Weapon/Weapon.cpp:257`

## 射击反馈

### 准心扩散脉冲

成功射击时：

```cpp
CrosshairShootingFactor = 1.5f;
```

随后每帧通过 `FInterpTo()` 插值回零。该值被加入准心总扩散量，因此射击瞬间准心张开，然后快速回收，为射击提供额外的视觉反馈。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:125`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:344`

### 弹壳与武器动画

武器通过 `AmmoEject` Socket 取得弹壳生成位置和方向。弹壳与武器动画都属于本地射击表现，因此同样受预测与多播去重保护。

## 持续开火

### FireTimer 控制射速

每次成功射击后调用 `StartFireTimer()`：

```text
本轮成功射击
└─ bCanFire = false
   └─ 启动 FireTimer，持续时间 = Weapon::FireDelay
```

计时结束后：

```cpp
bCanFire = true;

if (bFireButtonPressed && EquippedWeapon->bAutomatic)
{
    Fire();
}
```

这使持续开火必须同时满足：

- 玩家仍然按住开火。
- 武器的 `bAutomatic` 为 `true`。
- 上一发的 `FireDelay` 已结束。
- 弹匣仍有弹药。
- 当前状态仍允许射击。

`FireDelay` 既定义武器射速，也形成一次射击一个门控窗口。输入事件不会因为帧率不同而直接决定射速。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:419`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:430`
- `Source/Blaster/Weapon/Weapon.h:81`

### 自动武器与半自动武器

自动武器在计时结束时，只要按键仍然保持，就重新调用 `Fire()`。

半自动武器的 `bAutomatic` 为 `false`，不会在计时结束时自行开火。玩家必须松开并再次按下开火键，才能产生下一次射击请求。

## 手感优化：换弹结束后立即开火

换弹过程中不会清除 `bFireButtonPressed`。如果玩家一直按住开火，系统会在换弹结束时自动重新尝试射击。

存在两个衔接入口：

```text
动画到达 ReloadFinished
└─ FinishReloading
   └─ bFireButtonPressed ? Fire()
```

以及：

```text
客户端收到 CombatState = Unoccupied
└─ OnRep_CombatState
   └─ bFireButtonPressed ? Fire()
```

两个入口用于兼容动画通知和网络状态复制的不同到达顺序：

- 如果本地状态已经是 `Unoccupied`，动画通知可以立即衔接射击。
- 如果动画通知到达时本地仍是 `Reloading`，`CanFire()` 会拒绝；随后 `OnRep_CombatState()` 再补发。
- 如果其中一个入口已经成功开火，它会立刻设置 `bCanFire = false`，另一个入口无法重复通过。

因此 `CombatState + bCanFire` 同时承担时序适配和重复触发保护。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:502`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:539`

## 手感优化：弹匣耗尽后自动换弹

每次射击计时结束后都会检查当前弹匣：

```text
射出最后一发
└─ SpendRound：Ammo = 0
        ↓
等待本轮 FireDelay
        ↓
FireTimerFinished
├─ 自动武器尝试继续 Fire
│  └─ CanFire 因弹匣为空而拒绝
└─ ReloadEmptyWeapon
   └─ Reload
      └─ ServerReload
```

这里的“立即换弹”发生在当前射击间隔结束后，而不是扣除最后一发的同一帧。最后一发的动画、后坐表现和射击节奏能够正常完成，随后自动进入换弹，不需要玩家额外按键。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:430`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:756`

## 技术亮点

### 摄像机目标与枪口方向分离

屏幕中心射线确定目标点，枪口到目标点的向量决定实际射击角度，同时兼顾第三人称准心体验与武器真实发射位置。

### 本地预测消除输入等待

开火客户端立即播放角色 Montage、武器动画、弹壳和弹药 HUD 反馈，无需等待服务器往返，显著降低网络延迟对输入手感的影响。

### 多播主动跳过预测客户端

服务器多播仍然统一同步其他观察者，但最初开火客户端会跳过已经预测过的表现，避免同一发射击重复播放和重复扣弹。

### 射速门控兼顾自动开火与去重

`bCanFire + FireTimer + FireDelay` 形成独立于帧率的射击窗口，也为换弹完成后的补发入口提供天然去重。

### 输入意图跨越换弹状态

`bFireButtonPressed` 保存玩家持续开火意图。系统在动画结束和状态复制两个时机重新尝试开火，使高延迟环境下仍能平滑衔接，同时通过状态和射速门控避免重复触发。

### 自动换弹保持战斗节奏

最后一发完成自己的射击间隔后，系统自动检查空弹匣并进入服务器换弹流程，减少额外输入，让自动武器连续战斗更加顺畅。

## 单元总结

射击系统首先从本地屏幕中心反投影得到世界方向，通过准心射线维护 `HitTarget`，再从枪口指向该目标计算真实射击角度。不同 `FireType` 共享输入、状态、射速和网络框架，并使用单个或多个射击目标表达各自的方向需求。

开火客户端立即执行本地预测，播放角色和武器动画、生成弹壳、更新准心与弹药 HUD；服务器随后多播给其他客户端，并跳过已经预测的本机实例。`bCanFire` 与 `FireTimer` 控制持续开火和重复触发，`bFireButtonPressed` 则让玩家的持续开火意图跨越换弹状态。最后一发射击周期结束后自动换弹、换弹结束后自动衔接射击，共同构成射击系统的主要手感优化。

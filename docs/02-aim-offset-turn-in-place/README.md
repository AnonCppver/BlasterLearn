# 瞄准偏移与原地转身

## 功能点介绍

角色持枪站立时，镜头可以先在一定角度内独立转动，上半身通过 Aim Offset 朝向准心，脚部暂时保持原来的视觉方向；当水平瞄准偏移超过阈值后，角色进入左转或右转状态并播放对应的原地转身动画，同时通过插值逐渐回收根骨骼偏移，使身体和脚步平滑追上当前朝向。

按住瞄准键时，系统主动停止累计水平 `AO_Yaw`，避免大幅度上半身扭转和转身动画影响 ADS 状态下的枪械与镜头稳定性；垂直 `AO_Pitch` 仍然保留，因此角色仍能正确表现向上和向下瞄准。

## 玩家可见效果

```text
静止腰射，小幅左右转动镜头
        ↓
上半身跟随准心，脚部保持原方向
        ↓
水平偏移超过 ±90°
        ↓
播放左转或右转动画
        ↓
Root Bone 偏移逐渐回到 0°
        ↓
角色身体、脚步与当前朝向重新对齐
```

这套表现解决了两个常见问题：角色静止时不会因为镜头的细小变化而整个人立即旋转；需要大幅转向时，又不会让上半身无限扭曲，而是通过脚步动画重新建立身体朝向。

## 系统结构

```text
ABlasterCharacter
    计算 AO_Yaw、AO_Pitch 和 TurningInPlace
                    ↓
UBlasterAnimInstance
    读取角色状态，计算 YawOffset 和 Lean
                    ↓
Animation Blueprint
    Aim Offset + Rotate Root Bone + 转身状态机
                    ↓
站立/蹲伏左转与右转动画
```

角色类负责计算与游戏朝向有关的数据，动画实例把数据暴露给动画蓝图，动画蓝图负责混合姿势和选择具体动画。

## 技术细节

### 1. 每帧更新旋转表现

`ABlasterCharacter::Tick()` 每帧调用 `RotateInPlace()`。角色被淘汰或禁止操作时，系统停止原地转身；其他情况下继续调用 `AimOffset()`。

```text
Character Tick
    ↓
RotateInPlace
    ├─ DisableGameplay：停止转身
    └─ 正常状态：AimOffset
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:187`
- `Source/Blaster/Character/BlasterCharacter.cpp:196`

### 2. 水平瞄准偏移 AO_Yaw

系统首先移除速度的垂直分量，用水平速度判断角色是否静止，并通过 `CharacterMovement` 判断角色是否腾空。

当前水平瞄准偏移只在以下条件成立时累计：

```text
已经装备武器
水平 Speed == 0
角色没有腾空
角色没有按住瞄准键
```

系统保存开始静止时的水平瞄准方向 `StartingAimRotation`，再取得当前的 `GetBaseAimRotation().Yaw`，两者之差就是角色上半身相对于起始朝向的水平瞄准偏移。

```text
AO_Yaw = CurrentAimYaw - StartingAimYaw
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:471`
- `Source/Blaster/Character/BlasterCharacter.h:107`

### 3. 将 Yaw 归一化到 -180°～180°

角度是循环值。`180°` 和 `-180°` 表示同一个方向，如果直接用普通减法计算两个 Yaw，跨越边界时可能得到接近 `360°` 的错误差值。

项目通过 `UKismetMathLibrary::NormalizedDeltaRotator()` 计算：

```text
NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation)
```

它会把旋转差规范到约 `[-180°, 180°]`，从而得到最短、带方向的角度差。

例如：

```text
起始 Yaw： 170°
当前 Yaw：-170°

普通减法：-170° - 170° = -340°
归一化后：20°
```

角色实际上只向右跨过边界转动了 `20°`，而不是向左转动 `340°`。经过归一化后：

```text
正 AO_Yaw → 朝右侧偏移
负 AO_Yaw → 朝左侧偏移
```

这一步保证了转身方向判断不会在 `±180°` 边界发生突变。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:483`

### 4. Rotate Root Bone 的作用

装备武器后，角色使用：

```text
bOrientRotationToMovement = false
bUseControllerRotationYaw = true
```

因此 Character Actor 和碰撞胶囊会跟随控制器的水平朝向。若没有额外处理，Mesh 也会立即跟随 Actor 旋转，角色双脚会在地面上直接滑向新的方向。

Animation Blueprint 中的 `Rotate Root Bone` 用于在动画姿势层暂时抵消 Actor 已经发生的水平旋转。

假设角色从 `0°` 向右瞄准到 `60°`：

```text
Actor / Capsule 世界朝向：60°
记录的 AO_Yaw：          60°
Root Bone 动画补偿：     约 -60°
```

具体正负号取决于动画蓝图的参数连接，但作用始终是抵消 Actor 旋转造成的 Mesh 转动。

最终效果是：

```text
Actor 与碰撞已经朝向准心
        +
Root Bone 在动画中反向补偿
        =
脚部视觉上仍保持起始方向
```

`Rotate Root Bone` 只修改当前动画 Pose 中的根骨骼，不会修改：

- Character Actor 的世界旋转
- Capsule 的方向与碰撞
- 控制器或摄像机旋转
- 服务器判定的角色朝向
- 武器射线和伤害逻辑

它是一层纯动画表现补偿，为上半身 Aim Offset 和脚步转身动画提供缓冲空间。

动画蓝图资产中包含 `AnimGraphNode_RotateRootBone`：

- `Content/BluePrint/Character/Animation/ABP_Unequipped.uasset`

相关角色旋转设置：

- `Source/Blaster/Character/BlasterCharacter.cpp:36`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:204`

### 5. ±90°触发左右转身

只要角色尚未进入转身状态，`InterpAO_Yaw` 就会跟随当前 `AO_Yaw`，保存开始转身前的完整水平偏移。

系统使用 `±90°` 作为进入阈值：

```text
AO_Yaw >  90° → ETIP_Right
AO_Yaw < -90° → ETIP_Left
其他情况       → ETIP_NotTurning
```

`ETurningInPlace` 枚举把方向判断从具体动画资源中分离出来。动画蓝图根据枚举选择站立或蹲伏状态下的左转、右转动画。

相关实现：

- `Source/Blaster/BlasterTypes/TurningInPlace.h:4`
- `Source/Blaster/Character/BlasterCharacter.cpp:572`

相关动画：

- `Content/MyCharacter/Animation/FromMixamo/IdleTurnLeft.uasset`
- `Content/MyCharacter/Animation/FromMixamo/IdleTurnRight.uasset`
- `Content/MyCharacter/Animation/FromMixamo/CrouchTurnLeft.uasset`
- `Content/MyCharacter/Animation/FromMixamo/CrouchTurnRight.uasset`

### 6. FInterpTo 平滑回收偏移

进入左转或右转状态后，系统不会瞬间清零偏移，而是执行：

```text
InterpAO_Yaw = FInterpTo(
    Current = InterpAO_Yaw,
    Target = 0°,
    DeltaTime,
    InterpSpeed = 4
)
```

随后将插值结果重新赋给 `AO_Yaw`。

这段插值表达的是“角色脚步正在追上当前身体朝向”：

```text
播放转身动画
        +
AO_Yaw 逐渐接近 0°
        +
Rotate Root Bone 的抵消量逐渐解除
        =
Mesh 平滑回到 Actor 的真实朝向
```

如果直接把 `AO_Yaw` 设置为零，Root Bone 会瞬间失去补偿，Mesh 和武器可能明显跳回正前方。使用基于 `DeltaTime` 的插值，可以在不同帧率下保持相近的回正节奏。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:583`

### 7. 15°退出阈值与重新建立基准

当插值后的偏移满足：

```text
Abs(AO_Yaw) < 15°
```

系统结束转身，并把当前瞄准 Yaw 保存为新的 `StartingAimRotation`。

```text
TurningInPlace = ETIP_NotTurning
StartingAimRotation = CurrentAimYaw
```

因此系统使用了两个不同阈值：

```text
进入转身：Abs(AO_Yaw) > 90°
退出转身：Abs(AO_Yaw) < 15°
```

进入和退出阈值分离形成滞回区间，避免转身状态在临界角度附近频繁切换。重新记录当前方向则把本次转身后的朝向设为下一轮瞄准偏移的基准。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:594`

### 8. ADS 状态下关闭水平偏移

按住瞄准键时，当前代码进入移动/腾空/瞄准共用的重置分支：

```text
StartingAimRotation = CurrentAimYaw
AO_Yaw = 0°
TurningInPlace = ETIP_NotTurning
```

这是一项有意的视觉稳定策略。ADS 时不再积累水平 Aim Offset，也不播放左右原地转身动画，避免以下动画变化干扰准心和枪械画面：

- 上半身大角度侧扭
- Root Bone 水平补偿
- 转身步伐带来的身体摆动
- 左右手和武器姿势的横向变化

Actor 仍然通过 `bUseControllerRotationYaw` 直接跟随控制器方向，所以瞄准方向不会受到影响。

关闭的是水平 `AO_Yaw`，而不是所有瞄准偏移；`AO_Pitch` 仍会继续更新。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:504`

### 9. 垂直瞄准偏移 AO_Pitch

系统通过 `GetBaseAimRotation().Pitch` 获取垂直瞄准角度，并将其传给 Aim Offset 动画，使角色上半身和武器能够表现向上、向下瞄准。

远端角色的 Pitch 网络表示可能使用无符号角度范围，负角度会表现为 `[270°, 360°)`。因此非本地角色需要执行映射：

```text
输入：270°～360°
输出：-90°～0°
```

例如：

```text
网络表示 315° → 动画使用 -45°
```

这样本地和远端动画实例都能使用一致的上下瞄准范围。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:516`

### 10. AnimInstance 与 Animation Blueprint

`UBlasterAnimInstance::NativeUpdateAnimation()` 每帧从角色读取：

```text
AO_Yaw
AO_Pitch
TurningInPlace
```

动画蓝图使用 `AO_Yaw` 和 `AO_Pitch` 驱动 Aim Offset，使用 `TurningInPlace` 选择左转、右转或保持普通姿态，并通过 `Rotate Root Bone` 管理 Mesh 与 Actor 之间的视觉朝向差。

当角色正在重装或已经禁止操作时，`bUseAimOffsets` 会关闭，避免 Aim Offset 与蒙太奇或淘汰状态互相争夺骨骼姿势。

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:18`
- `Source/Blaster/Character/BlasterAnimInstance.cpp:54`
- `Source/Blaster/Character/BlasterAnimInstance.cpp:77`

相关 Aim Offset 资源：

- `Content/MyCharacter/Animation/FromStarter/Aim_Space_Hip.uasset`
- `Content/MyCharacter/Animation/FromStarter/Aim_Space_Ironsights.uasset`

### 11. YawOffset 和 Lean 与 AO_Yaw 的区别

动画实例还会计算 `YawOffset` 和 `Lean`，但它们与原地转身不是同一套数据。

#### YawOffset

`YawOffset` 是移动方向与瞄准方向之间的角度差：

```text
YawOffset = MovementRotation - AimRotation
```

该旋转差经过 `NormalizedDeltaRotator()` 归一化，再通过 `RInterpTo(..., 6.f)` 平滑，主要用于选择或混合前进、后退和左右横移时的持枪姿态。

#### Lean

`Lean` 来自角色当前帧与上一帧的 Actor Yaw 差，再除以 `DeltaTime` 得到每秒旋转速度：

```text
TargetLean = DeltaYaw / DeltaTime
```

结果经过 `FInterpTo(..., 6.f)` 平滑，并限制在 `[-90°, 90°]`，用于表现角色转向时的身体倾斜。

三者的职责可以概括为：

```text
AO_Yaw
→ 静止时视角相对身体的水平偏移
→ Root Bone 补偿和原地转身阈值

YawOffset
→ 移动方向相对瞄准方向的偏移
→ 前后左右持枪移动姿态

Lean
→ Actor 水平旋转速度
→ 转向时的身体倾斜
```

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:40`

## 技术亮点

### 角度归一化保证跨边界稳定

通过 `NormalizedDeltaRotator()` 把 Yaw 差规范到 `[-180°, 180°]`，系统始终使用最短、带方向的旋转差，避免镜头跨越 `±180°` 时突然得到接近 `360°` 的偏移。

### Root Bone 将游戏朝向与动画表现解耦

Character Actor 和碰撞胶囊可以立即跟随控制器朝向，而 `Rotate Root Bone` 在 Mesh Pose 中暂时抵消旋转，使脚部保持稳定，并为上半身瞄准与脚步转身留出动画空间。该补偿不影响碰撞、射线和服务器判定。

### 双阈值构成稳定的转身状态

系统在偏移超过 `90°` 时进入转身，在偏移收敛到 `15°` 内时退出。进入与退出条件分离，避免动画状态在临界角度附近反复切换。

### 插值同步视觉朝向

`FInterpTo()` 将 Root Bone 使用的偏移逐渐回收到零，使转身动画播放期间 Mesh 平滑追上 Actor 的真实朝向，避免瞬间清零导致的姿势跳变。

### ADS 优先保证瞄准稳定

按住瞄准键时主动清零水平 `AO_Yaw` 并取消原地转身，仅保留垂直 Aim Offset，减少上半身、武器和脚步动画对 ADS 画面的干扰。

### 兼容远端角色的 Pitch 表示

将非本地角色的 `[270°, 360°)` Pitch 映射回 `[-90°, 0°)`，保证网络端观察到的上下瞄准动画与本地角度语义一致。

## 截图建议

建议为个人页面准备以下游戏内画面：

1. 静止腰射并向侧面瞄准，但尚未超过 `90°`，展示上半身扭转和脚部保持方向。
2. 超过右侧阈值后播放 `IdleTurnRight` 的中间帧。
3. 超过左侧阈值后播放 `IdleTurnLeft` 的中间帧。
4. 蹲伏状态下的左转或右转动画。
5. ADS 状态下快速左右移动准心，展示角色不会播放大幅原地转身。
6. 两名玩家分别向上和向下瞄准，展示远端 `AO_Pitch` 同步效果。

推荐将图片放入当前目录下的 `images/` 子目录，并使用以下命名：

- `aim-offset-before-threshold.png`
- `turn-in-place-right.png`
- `turn-in-place-left.png`
- `crouch-turn-in-place.png`
- `ads-stable-rotation.png`
- `remote-aim-pitch.png`

## 单元总结

该功能基于控制器视角与静止起始方向之间的标准化旋转差计算 `AO_Yaw`，通过 `NormalizedDeltaRotator()` 将角度限制到 `[-180°, 180°]`，确保跨越角度边界时仍能获得正确的最短转向。Character Actor 与碰撞胶囊跟随控制器旋转，Animation Blueprint 则使用 `Rotate Root Bone` 在动画姿势中暂时抵消这部分旋转，使脚部保持稳定、上半身通过 Aim Offset 朝向准心。

当水平偏移超过 `±90°` 时，系统切换到左转或右转状态并播放对应动画，同时以 `FInterpTo()` 将 Root Bone 使用的偏移逐渐回收到零；当偏移小于 `15°` 时结束转身并重新建立瞄准基准。ADS 状态主动取消水平偏移和原地转身，仅保留垂直瞄准表现，以减少动画对枪械与镜头稳定性的影响。远端角色的网络 Pitch 还会从 `[270°, 360°)` 映射回负角度范围，保证多人环境中的上下瞄准动画一致。

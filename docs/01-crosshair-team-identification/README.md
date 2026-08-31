# 准心敌友识别

## 功能点介绍

准心会持续检测屏幕中心所指向的目标：命中敌方角色时变为红色，命中友方角色、普通场景物体或未命中目标时保持白色。检测只在本地控制角色上执行，不需要为了准心反馈额外发送网络 RPC；目标队伍来自已经复制到客户端的 `PlayerState`。

这套功能把“瞄准位置计算”和“敌友反馈”结合在同一条本地检测链路中，使玩家能够立即确认准心下的角色是否为敌人，同时为武器开火提供世界空间中的 `HitTarget`。

## 玩家可见效果

| 准心指向的对象 | 准心颜色 | 含义 |
| --- | --- | --- |
| 敌方角色 | 红色 | 当前目标可视为敌人 |
| 同队角色 | 白色 | 不标记为敌人 |
| 普通场景物体 | 白色 | 不属于可判断阵营的目标 |
| 空白区域 | 白色 | 当前没有阻挡命中 |
| 自由对战中的其他角色 | 红色 | 本地角色为 `NoTeam` 时，其他角色按敌人处理 |

当前实现以红色明确表达“敌方”，但没有为友方单独设置绿色或蓝色，因此白色同时承担友方、中立物体和无目标三种状态。

## 检测流程

```text
本地 CombatComponent Tick
        ↓
读取视口尺寸，取得屏幕中心
        ↓
DeprojectScreenToWorld：二维准心 → 世界起点与方向
        ↓
将射线起点推进到本地角色前方
        ↓
沿 ECC_Visibility 发射单次阻挡射线
        ↓
保存 ImpactPoint 为 HitTarget
        ↓
命中 Actor 是否实现 IInteractWithCrosshairsInterface？
        ├─ 否：准心保持白色
        └─ 是：通过接口读取目标队伍
                  ↓
             比较本地角色与目标队伍
                  ├─ NoTeam 或不同队：红色
                  └─ 相同队伍：白色
        ↓
将颜色、纹理和扩散值写入 FHUDPackage
        ↓
ABlasterHUD 绘制整套准心纹理
```

## 技术细节

### 1. 本地逐帧检测

`UCombatComponent::TickComponent()` 只在 `Character->IsLocallyControlled()` 成立时调用 `TraceUnderCrosshairs()`。因此每个客户端只计算自己的准心，不会为其他玩家执行无意义的瞄准查询。

检测得到的 `ImpactPoint` 会保存到 `HitTarget`。命中扫描、投射物以及散射武器在开火时都可以使用该位置，将屏幕准心所指向的世界位置传递给后续武器逻辑。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:56`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:256`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:365`

### 2. 从屏幕中心构造世界射线

系统先取得视口宽高，以 `(Width / 2, Height / 2)` 作为准心位置，再通过 `DeprojectScreenToWorld()` 得到世界空间中的起点和方向。

射线长度由 `TRACE_LENGTH` 定义为 `80000.f`，按照 UE 默认单位约为 800 米。没有产生阻挡命中时，系统把射线终点写入 `ImpactPoint`，保证瞄准空白区域时 `HitTarget` 仍然有效。

当前实现会沿射线方向推进“摄像机到角色的距离 + 100 uu”，以减少第三人称摄像机从角色背后发射射线时命中自身的概率。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:258`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:276`
- `Source/Blaster/Weapon/WeaponTypes.h:3`

### 3. 通过 Trace Channel 只检测角色身体

准心使用 `ECC_Visibility` 执行 `LineTraceSingleByChannel()`。角色胶囊体和骨骼网格体对该 Trace Channel 使用不同的碰撞响应：

```text
角色胶囊体（Pawn Profile） → Visibility: Ignore
角色骨骼网格体           → Visibility: Block
```

射线会穿过胶囊体，只有接触骨骼网格体的 Physics Asset 碰撞体时才产生阻挡命中。因此准心必须实际指向角色身体，而不是进入胶囊体的大致范围就立即变色。

这是本功能最重要的碰撞设计亮点：通过 Trace Channel 和组件级碰撞响应控制检测精度，不需要在命中后再判断命中组件是不是胶囊体。

需要注意，`ECC_SkeletalMesh` 是骨骼网格体的自定义 **Object Channel**，主要服务于投射物等碰撞规则；准心射线直接查询的是 `ECC_Visibility`。准心只检测身体的关键条件是“胶囊体忽略 Visibility、骨骼网格体阻挡 Visibility”，而不是 `ECC_SkeletalMesh` 本身。

相关实现：

- `Config/DefaultEngine.ini:139`
- `Source/Blaster/Character/BlasterCharacter.cpp:57`
- `Source/Blaster/Character/BlasterCharacter.cpp:64`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:286`

### 4. 使用接口避免依赖具体角色类型

射线命中 Actor 后，系统不会直接把目标转换成 `ABlasterCharacter`，而是先检查目标是否实现 `IInteractWithCrosshairsInterface`。接口只向准心系统暴露 `GetCrosshairTeam()`，由实现者负责提供队伍信息。

```text
HitActor
   ↓ Implements Interface?
IInteractWithCrosshairsInterface
   ↓ GetCrosshairTeam()
ETeam
```

这种做法避免了对具体角色类的强制类型转换和硬编码依赖。准心系统只依赖“目标能够提供队伍信息”这一能力；其他 C++ Actor 只要实现相同接口，也能进入同一套敌友识别流程。

当前代码仍会把 Actor 转换为接口指针，但它避免的是对 `ABlasterCharacter` 等具体业务类型的 Cast，而不是完全消除所有形式的接口转换。

相关实现：

- `Source/Blaster/Interfaces/InteractWithCrossHairsInterface.h:20`
- `Source/Blaster/Character/BlasterCharacter.h:15`
- `Source/Blaster/Character/BlasterCharacter.cpp:777`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:294`

### 5. 使用已复制的 PlayerState 队伍

`ABlasterCharacter::GetCrosshairTeam()` 从 `ABlasterPlayerState` 读取 `Team`。该属性参与网络复制，所以客户端能够直接比较本地玩家与目标玩家的队伍，不需要在每次瞄准时请求服务器。

判断规则如下：

```text
本地 Team == NoTeam       → 红色
本地 Team != 目标 Team    → 红色
本地 Team == 目标 Team    → 白色
```

相关实现：

- `Source/Blaster/PlayerState/BlasterPlayerState.h:38`
- `Source/Blaster/PlayerState/BlasterPlayerState.cpp:5`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:299`

### 6. HUD 表现与动态准心整合

敌友判断得到的颜色写入 `FHUDPackage::CrosshairsColor`。`ABlasterHUD::DrawHUD()` 在绘制中心、上下、左右五块准心纹理时统一使用该颜色，因此颜色反馈可以与移动、跳跃、瞄准和射击造成的准心扩散同时工作。

相关实现：

- `Source/Blaster/HUD/BlasterHUD.h:9`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:71`
- `Source/Blaster/HUD/BlasterHUD.cpp:92`

### 7. 人物运动与操作状态驱动准心扩散

准心除了根据敌友关系改变颜色，还会根据角色当前的运动和战斗状态动态改变上下左右四块纹理与中心点之间的距离。`UCombatComponent::SetHUDCrosshairs()` 每帧计算四个因子：

```text
CrosshairVelocityFactor → 水平移动速度
CrosshairInAirFactor    → 腾空状态
CrosshairAimFactor      → ADS 瞄准状态
CrosshairShootingFactor → 刚刚开火产生的扩张
```

最终扩散系数为：

```text
CrosshairSpread =
    0.5
    + VelocityFactor
    + InAirFactor
    - AimFactor
    + ShootingFactor
```

#### 移动速度因子

系统移除角色速度的 Z 分量，只使用水平移动速度：

```text
Velocity.Z = 0
HorizontalSpeed = Velocity.Size()
```

随后将 `[0, MaxWalkSpeed]` 映射到 `[0, 1]`，并限制结果范围：

```text
静止                 → VelocityFactor = 0
达到当前最大移动速度 → VelocityFactor = 1
```

角色移动越快，准心张得越开。映射使用当前 `CharacterMovement->MaxWalkSpeed`，因此普通移动和 ADS 降速状态都以各自当前的最大速度作为归一化基准。

#### 腾空因子

角色处于 Falling 状态时，`CrosshairInAirFactor` 以插值速度 `2.25` 逐渐接近 `2.25`：

```text
InAirFactor → FInterpTo(Target = 2.25, Speed = 2.25)
```

落地后则以更快的插值速度 `30` 回到零：

```text
InAirFactor → FInterpTo(Target = 0, Speed = 30)
```

这种不对称速度让准心在腾空过程中逐渐扩大，而落地后快速收拢，既表现空中射击的不稳定感，又减少落地后的视觉拖延。

#### ADS 收拢因子

按住瞄准键时，`CrosshairAimFactor` 以插值速度 `30` 接近 `0.58`；退出瞄准时再以相同速度回到零。

由于它在最终公式中使用减法：

```text
Spread - AimFactor
```

进入 ADS 会使准心快速收拢，退出 ADS 后恢复普通扩散范围。

#### 射击扩张因子

每次 `Fire()` 通过 `CanFire()` 检查并成功进入开火流程时，系统立即设置：

```text
CrosshairShootingFactor = 1.5
```

随后它会在每帧以插值速度 `40` 快速衰减到零。因此每次射击都会产生一次短促的准心扩张，连续射击会反复刷新该因子，形成持续抖开的视觉反馈。

#### 从系数转换为屏幕像素偏移

HUD 使用：

```text
SpreadScaled = CrosshairSpreadMax × CrosshairSpread
```

其中 `CrosshairSpreadMax` 为 `16.f`。中心纹理始终绘制在视口中心，其余四块纹理分别使用：

```text
Left   → (-SpreadScaled, 0)
Right  → ( SpreadScaled, 0)
Top    → (0, -SpreadScaled)
Bottom → (0,  SpreadScaled)
```

这使同一套准心纹理能够根据状态在屏幕中心连续伸缩，而不需要为静止、奔跑、跳跃、ADS 和开火分别准备不同 Widget。

需要准确区分的是：当前 `CrosshairSpread` 只参与 HUD 绘制，没有直接传入武器的命中或散射计算。它表达角色运动与操作状态带来的视觉准度反馈，但不直接改变子弹轨迹；启用散射的武器仍通过自己的 `TraceEndWithScatter()` 计算实际射击方向。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:71`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:97`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:337`
- `Source/Blaster/HUD/BlasterHUD.cpp:92`
- `Source/Blaster/HUD/BlasterHUD.h:64`

## 技术亮点

### Trace Channel 的差异化碰撞响应

通过让胶囊体忽略 `Visibility`、骨骼网格体阻挡 `Visibility`，准心检测精确落在角色身体上，避免胶囊体范围比人物轮廓更大而造成的错误变色。

### 接口隔离具体目标类型

准心系统通过 `IInteractWithCrosshairsInterface` 获取目标队伍，而不是直接依赖 `ABlasterCharacter`。这降低了战斗组件和角色实现之间的耦合，也为其他可识别阵营的 C++ Actor 留出了扩展入口。

### 客户端即时反馈与复制数据结合

射线和 HUD 更新完全在本地执行，保证准心反馈及时；队伍身份则复用 `PlayerState` 的复制结果，避免为纯表现逻辑增加额外网络通信。

### 瞄准与开火共享目标位置

射线结果既驱动准心颜色，也生成武器使用的 `HitTarget`，保证 UI 所表达的瞄准方向与后续开火目标来自同一条视线计算链路。

### 多因素合成动态准心

系统把水平速度、腾空、ADS 和射击反馈拆成独立因子，再通过统一公式合成为准心扩散值。速度使用归一化映射，状态切换使用基于 `DeltaTime` 的插值，使准心既能即时反映人物运动，又能避免状态变化时突然跳动。

## 讨论后的改进方案

以下内容是本单元的演进建议，不代表当前源码已经实现。

### 改进一：显式忽略自身

保留从摄像机穿过准心构造射线的方式，但使用碰撞查询参数明确忽略本地角色和当前武器，替代“把起点推进到角色前方”的经验值方案。

预期收益：

- 不再依赖摄像机距离、角色体型和额外的 `100 uu` 偏移。
- 贴墙、俯视、仰视及特殊镜头位置下的行为更稳定。
- 查询意图更加明确：射线从视角出发，但不允许命中自身。

### 改进二：使用专用 AimTrace/WeaponTrace

将准心和命中扫描武器从通用 `Visibility` 中分离，使用专门的武器瞄准 Trace Channel。场景墙体和角色骨骼阻挡该通道，胶囊体、武器附件及纯视觉对象忽略该通道。

准心与命中扫描武器应共享这条通道，避免准心能够检测目标、但实际武器射线被另一套碰撞规则阻挡的表现差异。

预期收益：

- 瞄准规则不再受通用可见性配置意外影响。
- 能够独立配置墙体、角色身体、武器附件和视觉对象。
- 碰撞矩阵更容易理解和维护。

### 改进三：拆分检测、关系判断与 HUD 表现

把当前 `TraceUnderCrosshairs()` 承担的多个职责拆分为三个阶段：

```text
TraceAimTarget
    只负责构造射线并返回 FHitResult
             ↓
ResolveTargetRelation
    只负责返回 None / Friendly / Hostile / Neutral
             ↓
UpdateCrosshairPresentation
    根据关系选择颜色并更新 HUD
```

预期收益：

- 碰撞检测、游戏规则和 UI 表现可以分别测试。
- 后续增加中立目标、不可攻击状态或友军专属颜色时，不必修改射线算法。
- HUD 不需要了解角色类和队伍数据，只消费语义化的目标关系。

## 与背包系统的边界

本功能不包含背包物品检测。背包系统虽然同样从屏幕中心发射射线，但使用独立的 `ItemTraceChannel`，并通过 `FocusedItem`、`UInvItemComponent` 和拾取 Widget 完成交互。

```text
准心敌友识别：Visibility → Crosshair Interface → Team → HUD Color
背包物品交互：ItemTrace  → FocusedItem         → Inventory Interaction
```

二者在碰撞通道、目标能力和业务结果上相互独立。

## 截图建议

建议为个人页面准备以下游戏内截图：

1. 准心指向敌方角色身体，完整展示红色准心。
2. 准心指向同队角色身体，展示白色准心。
3. 准心位于角色身体轮廓外、但仍可能处于胶囊体范围内，展示准心没有变红。
4. 敌人被墙体遮挡，展示准心不会穿过场景障碍识别敌人。
5. 角色静止和全速奔跑的准心宽度对比。
6. 跳跃过程中准心逐渐扩大、落地后快速收拢的对比。
7. 腰射与 ADS 状态下的准心宽度对比。
8. 开火瞬间与射击因子衰减后的准心对比。

推荐将图片放入当前目录下的 `images/` 子目录，并使用以下命名：

- `enemy-target-red-crosshair.png`
- `friendly-target-white-crosshair.png`
- `capsule-ignored.png`
- `occlusion-by-world.png`
- `crosshair-idle-vs-running.png`
- `crosshair-in-air.png`
- `crosshair-hip-vs-ads.png`
- `crosshair-shooting-feedback.png`

## 单元总结

该功能在本地客户端从屏幕中心构造世界射线，通过 Trace Channel 的差异化碰撞响应穿过角色胶囊体并精确命中骨骼身体；命中目标后，利用准心交互接口获取复制自 `PlayerState` 的队伍信息，避免战斗组件直接依赖具体角色类型，并实时驱动 HUD 准心颜色。整套链路没有为视觉反馈增加额外网络请求，同时复用了瞄准射线结果作为武器的目标位置。

讨论后的推荐演进方向是：使用查询参数显式忽略自身，以专用 `AimTrace/WeaponTrace` 隔离武器检测规则，并拆分目标检测、敌友关系判断和 HUD 表现三个职责，从而提高特殊镜头情况下的稳定性以及未来扩展中立目标、AI 和不同准心反馈时的可维护性。

动态准心进一步把水平移动速度、腾空、ADS 和开火反馈合成为统一扩散值：奔跑、跳跃和射击扩大准心，ADS 通过减法因子使准心收拢，所有瞬时状态均使用插值平滑过渡。HUD 最终把扩散系数换算成上下左右纹理的像素偏移，实现一套纹理适配多种人物状态的连续视觉反馈。

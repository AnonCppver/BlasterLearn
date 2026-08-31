# 武器附加与左手 FABRIK 持枪校正

## 功能点介绍

地面武器通过球形触发区域检测进入拾取范围的角色，并将当前可拾取武器只复制给该角色的拥有者客户端，用于显示拾取提示。玩家按下装备键后由服务器确认重叠目标并更新主副武器槽位；主武器附加到人物骨骼上的 `RightHandSocket`，关闭地面拾取碰撞与物理模拟，然后通过武器上的 `LeftHandSocket` 为动画蓝图提供左手目标，使用 FABRIK 逆向运动学让左手适配不同枪械的握把位置。

这套结构采用“右手固定武器、武器定位左手”的双手持枪关系：右手是枪械的主锚点，左手是运行时 IK 校正的一方。

## 完整流程

```text
Weapon AreaSphere 检测 Pawn Overlap
        ↓
Character 保存 OverlappingWeapon
        ↓
OwnerOnly 复制并显示 PickupWidget
        ↓
玩家按下 Equip
        ↓
ServerEquipButtonPressed 验证服务器重叠结果
        ↓
CombatComponent 选择主武器或副武器槽位
        ↓
WeaponState → Equipped / EquippedSecondary
        ↓
主武器附加 RightHandSocket
副武器附加 BackpackSocket
        ↓
客户端 OnRep 重建状态和附加关系
        ↓
读取 Weapon.LeftHandSocket 世界 Transform
        ↓
转换到 Character.RightHand Bone Space
        ↓
Anim Blueprint 使用 FABRIK 调整左臂
```

## 技术细节

### 1. 武器 Actor 与拾取区域

`AWeapon` 开启 Actor 和移动复制，使用 `WeaponMesh` 作为 Root Component。构造阶段关闭武器网格体和 `AreaSphere` 的碰撞，避免组件尚未初始化完成时产生拾取事件。

`BeginPlay()` 中开启 AreaSphere，将其对 `Pawn` 的响应设置为 `Overlap`，并绑定开始与结束重叠事件：

```text
OnComponentBeginOverlap → OnSphereOverlap
OnComponentEndOverlap   → OnSphereEndOverlap
```

拾取 Widget 初始保持隐藏，地面武器通过 Custom Depth 使用蓝色描边。

相关实现：

- `Source/Blaster/Weapon/Weapon.cpp:18`
- `Source/Blaster/Weapon/Weapon.cpp:44`

### 2. Overlap 设置可拾取武器

`OnSphereOverlap()` 将重叠 Actor 转换为 `ABlasterCharacter`，成功后调用：

```text
Character.SetOverlappingWeapon(this)
```

`SetOverlappingWeapon()` 先隐藏旧武器提示，再保存新武器；如果角色由本地玩家控制，就显示新武器的 Pickup Widget。结束重叠时当前武器被设置为空。

相关实现：

- `Source/Blaster/Weapon/Weapon.cpp:72`
- `Source/Blaster/Character/BlasterCharacter.cpp:604`

### 3. OwnerOnly 复制拾取目标

`OverlappingWeapon` 使用 `COND_OwnerOnly` 复制。附近的其他玩家不需要看到该角色的拾取提示，因此服务器只把结果发送给控制这个 Character 的客户端。

客户端收到变化后，`OnRep_OverlappingWeapon(LastWeapon)` 显示新武器提示并隐藏旧武器提示，避免多个 Widget 同时残留。

```text
Server OverlappingWeapon
        ↓ OwnerOnly
Owning Client
        ├─ Show New PickupWidget
        └─ Hide Last PickupWidget
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:154`
- `Source/Blaster/Character/BlasterCharacter.cpp:619`
- `Source/Blaster/Character/BlasterCharacter.h:86`

### 4. 服务器权威装备

本地玩家按下 Equip 后调用 `ServerEquipButtonPressed()`。服务器使用自己维护的 `OverlappingWeapon` 执行装备，不允许客户端直接完成武器归属变化。

`CombatComponent::EquipWeapon()` 还要求战斗状态为 `ECS_Unoccupied`，然后根据现有槽位决定：

```text
没有主武器                   → EquipPrimaryWeapon
有主武器但没有副武器         → EquipSecondaryWeapon
已经拥有主副武器             → 丢弃当前主武器并装备新武器
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:406`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:190`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:705`

### 5. WeaponState 切换碰撞与表现

武器状态包括：

```text
Initial
Equipped
EquippedSecondary
Dropped
```

进入 `EWS_Equipped` 后，武器会：

- 隐藏 Pickup Widget。
- 关闭 AreaSphere。
- 关闭物理模拟和重力。
- 关闭 WeaponMesh 碰撞。
- 关闭地面拾取描边。

进入 `EWS_EquippedSecondary` 时同样关闭拾取和物理，但使用另一种 Custom Depth 颜色表示背负武器。武器被丢弃时则与角色分离，恢复物理、重力、场景碰撞和地面描边。

相关实现：

- `Source/Blaster/Weapon/Weapon.h:10`
- `Source/Blaster/Weapon/Weapon.cpp:90`
- `Source/Blaster/Weapon/Weapon.cpp:139`
- `Source/Blaster/Weapon/Weapon.cpp:167`
- `Source/Blaster/Weapon/Weapon.cpp:217`

### 6. RightHandSocket 附加主武器

主武器通过角色 Skeletal Mesh 上的 `RightHandSocket` 完成附加：

```text
Character Mesh
    ↓
RightHand Bone
    ↓
RightHandSocket
    ↓
Weapon Actor
```

Socket 保存相对于右手骨骼的位置和旋转偏移，因此人物奔跑、跳跃、瞄准和播放开火动画时，枪械都会跟随右手骨骼运动。

当前结构不是让右手通过 IK 追踪枪械，而是让枪械成为右手 Socket 的子对象。右手是主锚点，枪械的握持位置由角色 Skeleton 上的 `RightHandSocket` 决定。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.cpp:746`
- `Content/MyCharacter/Character/Ch15_nonPBR_Skeleton.uasset`

### 7. 主副武器复制与客户端重建

`EquippedWeapon` 和 `SecondaryWeapon` 均使用 `ReplicatedUsing`。客户端收到主武器后会重新设置 `EWS_Equipped`、附加到 `RightHandSocket`、更新移动旋转模式和 HUD 弹药；收到副武器后则设置 `EWS_EquippedSecondary` 并附加到 `BackpackSocket`。

`EquippedWeapon` 与 `WeaponState` 都需要复制，但到达顺序不能保证，所以 `OnRep_EquippedWeapon()` 会主动再次设置一次武器状态。这使客户端无论先收到武器指针还是状态，都能恢复正确的碰撞和附加表现。

相关实现：

- `Source/Blaster/BlasterComponent/CombatComponent.h:104`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:208`
- `Source/Blaster/Weapon/Weapon.h:159`
- `Source/Blaster/Weapon/Weapon.cpp:112`

### 8. LeftHandSocket 定义每把枪的副手握点

不同武器的长度和前握把位置不同，因此各个武器 Skeleton 分别定义 `LeftHandSocket`：

```text
手枪       → 左手靠近主握把
步枪       → 左手位于护木
霰弹枪     → 左手位于前护木
火箭发射器 → 左手位于更靠前或靠下的位置
```

动画实例每帧读取当前武器 `LeftHandSocket` 的世界空间 Transform，使每把武器能够通过资产配置自己的左手握点，而不需要在角色代码中硬编码多套手部坐标。

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:57`
- `Content/MilitaryWeapSilver/Weapons/NewFolder/Assault_Rifle_A_Skeleton.uasset`
- `Content/MilitaryWeapSilver/Weapons/Pistols_A_Skeleton.uasset`
- `Content/MilitaryWeapSilver/Weapons/Shotgun_A_Skeleton.uasset`
- `Content/MilitaryWeapSilver/Weapons/Rocket_Launcher_A_Skeleton.uasset`
- `Content/MilitaryWeapSilver/Weapons/Sniper_Rifle_A_Skeleton.uasset`

### 9. 转换到 RightHand Bone Space

武器 Socket 返回的是世界空间 Transform，而 FABRIK 需要一个与人物动画骨骼保持稳定关系的目标。系统使用 `TransformToBoneSpace()` 将左手握点转换到角色 `RightHand` 骨骼空间：

```text
Weapon.LeftHandSocket World Transform
                    ↓
Character Mesh.TransformToBoneSpace("RightHand")
                    ↓
LeftHandTransform in RightHand Bone Space
```

选择右手作为参考空间，是因为枪械本身已经附加在右手上。角色发生世界移动、旋转或网络位置更新时，右手、枪械和左手握点仍保持同一套局部关系。

当前转换主要使用 `LeftHandSocket` 的位置；传入旋转参数的是 `FRotator::ZeroRotator`，因此运行时重点是校正左手末端位置。

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:59`

### 10. FABRIK 求解左臂

FABRIK 是一种逆向运动学算法。普通骨骼动画从父骨骼向末端计算：

```text
肩膀 → 上臂 → 前臂 → 左手
```

FABRIK 则根据最终目标反推整条骨骼链，使左手到达 `LeftHandTransform`：

```text
Weapon LeftHandSocket
        ↓
左手必须到达该位置
        ↓
FABRIK 调整上臂和前臂
        ↓
左手贴合武器握把
```

最终双手关系为：

```text
右手动画驱动枪械
        ↓
枪械定义左手握点
        ↓
FABRIK 调整人物左臂
```

动画蓝图资产中包含 FABRIK 节点并消费 `LeftHandTransform`：

- `Content/BluePrint/Character/Animation/ABP_Unequipped.uasset`

### 11. 重装时让蒙太奇接管左手

当 `CombatState` 为 `ECS_Reloading` 时，动画实例关闭 `bUseFABRIK`。这样 Reload Montage 可以自由控制左手执行取弹匣、装弹和拉栓动作，不会被 IK 持续拉回枪械握点。

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:75`

### 12. 当前没有运行时右手瞄准修正

动画实例中存在一段根据 `HitTarget` 计算 `RightHandRotation` 并以速度 `30` 插值的代码，但当前被整体注释。现有功能不能描述为“运行时调整右手朝向准心”。

当前实际生效的职责是：

```text
RightHandSocket → 调整枪械相对于人物右手的位置
LeftHandSocket  → 提供人物左手应该到达的位置
FABRIK          → 运行时调整人物左臂
```

相关实现：

- `Source/Blaster/Character/BlasterAnimInstance.cpp:66`

## 技术亮点

### 服务器权威的装备结果

客户端只发送装备请求，服务器依据自己维护的重叠武器和战斗状态决定是否装备，避免客户端直接改变武器归属。

### OwnerOnly 的拾取提示复制

`OverlappingWeapon` 只同步给角色拥有者，既支持客户端正确显示拾取提示，也避免向无关玩家复制纯本地交互状态。

### Socket 驱动的武器附加

角色 `RightHandSocket` 统一管理枪械相对于主手的位置，武器随人物骨骼动画自然运动；副武器则使用独立的 `BackpackSocket` 完成背负表现。

### 武器资产定义左手握点

每把枪通过自己的 `LeftHandSocket` 描述副手握点，使步枪、手枪、霰弹枪和火箭发射器能够复用同一套人物动画与 IK 代码。

### 跨坐标空间的 IK 目标转换

系统把武器世界空间中的握点转换到人物右手骨骼空间，在动态动画和网络移动中保持稳定的双手相对关系。

### FABRIK 适配不同枪械轮廓

右手作为枪械主锚点，FABRIK 只求解左臂，使左手在不同武器和动画姿势下持续贴合前握把；重装时关闭 IK，让蒙太奇完整接管手部动作。

## 讨论后的改进方案：多武器候选集合

当前角色只保存一个 `OverlappingWeapon`。如果拾取球同时覆盖多把武器，事件顺序可能导致目标丢失：

```text
进入武器 A → OverlappingWeapon = A
进入武器 B → OverlappingWeapon = B
离开武器 A → OverlappingWeapon = nullptr
```

虽然角色仍在武器 B 的范围内，武器 A 的 EndOverlap 仍可能清空当前选择。

推荐维护一组有效候选，而不是只依赖最后一次重叠事件：

```text
BeginOverlap
    ↓
把 Weapon 加入候选集合
    ↓
根据选择规则计算 FocusedWeapon

EndOverlap
    ↓
从候选集合移除 Weapon
    ↓
从剩余候选重新计算 FocusedWeapon
```

候选集合可以使用去重容器表达：

```text
OverlappingWeapons = { WeaponA, WeaponB, WeaponC }
```

再选择一个武器作为当前 `OverlappingWeapon`。适合当前第三人称射击交互的选择规则是：

1. 排除已装备、已销毁或不再可拾取的候选。
2. 优先选择更接近屏幕准心方向的武器。
3. 准心夹角相近时选择距离角色更近的武器。
4. 当前焦点仍有效时保留焦点，避免提示在相邻武器之间频繁跳动。

最终只把选出的 `FocusedWeapon` 作为 OwnerOnly 状态同步和 Widget 展示目标，服务器装备时也从自己的候选集合重新验证该武器仍然有效。

这样可以解决：

- 多把武器靠得很近时 EndOverlap 错误清空目标。
- 拾取提示在多个武器之间随机切换。
- 玩家无法明确选择自己准心所指向的武器。
- 客户端显示目标与服务器可装备目标缺少统一选择规则。

该方案只改变重叠目标的管理方式，不需要改变后续的装备、Socket 附加和 FABRIK 持枪链路。

## 与背包系统的边界

本单元中的 `BackpackSocket` 仅表示副武器挂在人物背部的骨骼 Socket，不属于空间背包、物品格或 `InvComponent` 系统。

```text
SecondaryWeapon + BackpackSocket
→ 人物外观上的背负武器

ItemTrace + InvComponent + SpatialInventory
→ 背包物品与 UI 交互
```

两者名称接近，但数据结构和功能完全独立。

## 截图建议

建议为个人页面准备以下游戏内截图：

1. 地面武器出现拾取 Widget 和蓝色描边。
2. 主武器附加在角色右手的正面与侧面视图。
3. 副武器附加在 `BackpackSocket` 的背面视图。
4. 步枪、霰弹枪和火箭发射器的左手握点对比。
5. 重装动画中左手离开前握把的画面。
6. 两把武器靠近时当前焦点 Widget 的选择效果，用于说明候选集合设计。

推荐图片命名：

- `weapon-pickup-widget.png`
- `weapon-right-hand-attachment.png`
- `secondary-weapon-backpack-socket.png`
- `left-hand-fabrik-weapon-comparison.png`
- `fabrik-disabled-during-reload.png`
- `multiple-weapon-focus-selection.png`

## 单元总结

该功能从武器 AreaSphere 的 Pawn Overlap 开始，通过 OwnerOnly 复制为本地玩家显示拾取提示；玩家发起装备请求后，服务器根据权威重叠结果和武器槽位更新 `EquippedWeapon`、`SecondaryWeapon` 与 `WeaponState`。主武器关闭地面物理和碰撞后附加到人物 `RightHandSocket`，右手骨骼成为枪械的主锚点；动画实例再读取每把武器自定义的 `LeftHandSocket`，将世界空间握点转换到人物 `RightHand` 骨骼空间，并由 FABRIK 调整左臂，使同一人物动画能够适配多种枪械轮廓。

针对多把武器同时进入拾取范围的情况，讨论结论是使用候选集合替代单一事件指针：BeginOverlap 加入候选、EndOverlap 移除候选，再根据准心方向和距离稳定选出当前焦点。该方案解决 EndOverlap 错误清空目标和多个拾取提示竞争的问题，同时保持后续服务器装备、Socket 附加与左手 IK 流程不变。

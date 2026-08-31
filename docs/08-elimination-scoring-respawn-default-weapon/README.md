# 角色死亡、计分、复活与默认武器

## 功能点介绍

角色生命归零后，服务器通过 `ABlasterGameMode` 统一完成规则结算，再由角色的可靠多播同步死亡表现。经过固定等待时间后，服务器销毁旧 Pawn，在出生点为原 Controller 创建新角色；新角色进入游戏时，由服务器生成并装备默认武器。

本单元从生命归零后开始，包含：

- 攻击者个人得分与得分音效。
- 受害者失败次数、最高分玩家与团队得分。
- 面向所有玩家的淘汰公告。
- 死亡角色的武器、动画、移动、输入和碰撞处理。
- 延迟复活以及 Controller、PlayerState 与 Pawn 的生命周期关系。
- 新角色如何由服务器获得默认武器。

本单元按当前源码记录实际行为，不加入重构或改进方案。

## 服务器统一处理死亡结果

生命归零后，角色将被击败角色、受害者 Controller 和攻击者 Controller 交给服务器 `ABlasterGameMode::PlayerEliminated()`：

```text
角色生命归零
    ↓
GameMode::PlayerEliminated
    ├─ 更新攻击者个人得分
    ├─ 更新最高分记录
    ├─ 向攻击者播放得分音效
    ├─ 增加受害者失败次数
    ├─ 通知角色执行死亡处理
    └─ 向每个客户端发送淘汰公告
```

`GameMode` 只存在于服务器，因此计分、角色死亡确认和复活请求都由服务器负责，客户端无法自行增加得分或主动决定复活。

当前函数首先检查攻击者和受害者 Controller 及其 PlayerState。通过检查后，分别取得 `ABlasterPlayerState` 和 `ABlasterGameState`，再执行后续结算。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:17`

## 攻击者个人得分

攻击者与受害者不是同一个 PlayerState 时，服务器为攻击者增加 1 分：

```cpp
AttackerPlayerState->AddToScore(1.f);
```

`ABlasterPlayerState::AddToScore()` 使用 UE `APlayerState` 自带的 `Score`：

```text
新分数 = GetScore() + ScoreAmount
SetScore(新分数)
```

服务器修改后立即尝试刷新本地 HUD；`Score` 的复制值到达客户端后，`OnRep_Score()` 再刷新所属玩家的分数 HUD。

PlayerState 与 Pawn 生命周期分离。死亡和复活只替换角色 Pawn，不替换 PlayerState，因此个人得分会跨越多次复活继续保留。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:27`
- `Source/Blaster/PlayerState/BlasterPlayerState.cpp:15`
- `Source/Blaster/PlayerState/BlasterPlayerState.cpp:29`

## 得分音效

只有个人得分成立时，服务器才调用攻击者 PlayerController 的可靠 Client RPC：

```text
服务器确认非自我击败
        ↓
攻击者个人得分 +1
        ↓
ClientPlayEliminationSound
        ↓
攻击者客户端 PlaySound2D
```

音效只在攻击者自己的客户端播放，不会发送给受害者或其他玩家。当前使用 `UGameplayStatics::PlaySound2D()`，音量倍率为 `8.0f`。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:31`
- `Source/Blaster/PlayerController/BlasterPlayerController.h:29`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:515`

## 失败次数

只要受害者 PlayerState 有效，服务器就调用：

```cpp
VictimPlayerState->AddToDefeats(1);
```

`Defeats` 是项目自定义的复制属性：

```cpp
UPROPERTY(ReplicatedUsing = OnRep_Defeats)
int32 Defeats;
```

服务器更新后立即刷新本地 HUD；客户端收到复制值后，通过 `OnRep_Defeats()` 更新自己的失败次数。

和个人得分一样，失败次数保存在 PlayerState 中，不会因为 Pawn 被销毁和重新生成而丢失。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:34`
- `Source/Blaster/PlayerState/BlasterPlayerState.cpp:45`
- `Source/Blaster/PlayerState/BlasterPlayerState.cpp:59`

## GameState 的最高分记录

个人得分增加后，服务器调用：

```cpp
BlasterGameState->UpdateTopScore(AttackerPlayerState);
```

`ABlasterGameState` 使用 `TopScore` 保存当前最高分数，并使用复制数组 `TopScoringPlayers` 保存所有并列最高分玩家：

```text
还没有最高分玩家
└─ 添加当前得分者，并记录 TopScore

当前得分等于 TopScore
└─ AddUnique，形成并列最高分

当前得分超过 TopScore
└─ 清空旧数组，加入新领先者并更新 TopScore
```

`TopScoringPlayers` 会复制给客户端，并在比赛结算界面中用于显示获胜玩家。

相关实现：

- `Source/Blaster/GameState/BlasterGameState.h:18`
- `Source/Blaster/GameState/BlasterGameState.cpp:15`

## 团队模式的 GameState 得分

`ATeamsGameMode::PlayerEliminated()` 首先调用父类流程，完成个人得分、失败次数、死亡处理和公告。随后根据攻击者所属队伍更新 GameState：

```text
攻击者属于蓝队
└─ BlueTeamScores()

攻击者属于红队
└─ RedTeamScores()
```

`RedTeamScore` 和 `BlueTeamScore` 都使用 `ReplicatedUsing`。服务器加分后更新本地团队 HUD；复制到客户端后，对应的 `OnRep_RedTeamScore()` 或 `OnRep_BlueTeamScore()` 更新客户端 HUD。

因此三种得分数据职责不同：

| 数据 | 所属对象 | 作用 |
| --- | --- | --- |
| `Score` | PlayerState | 单个玩家的个人得分 |
| `TopScoringPlayers` | GameState | 比赛中的最高分玩家集合 |
| `RedTeamScore` / `BlueTeamScore` | GameState | 团队比赛的公共比分 |

相关实现：

- `Source/Blaster/GameMode/TeamsGameMode.cpp:98`
- `Source/Blaster/GameState/BlasterGameState.cpp:34`
- `Source/Blaster/GameState/BlasterGameState.cpp:56`

## 全体玩家的淘汰公告

服务器遍历当前世界中的所有 PlayerController，为每个客户端调用 `ClientElimAnnouncement()`：

```text
服务器发生一次淘汰
        ↓
遍历所有 PlayerController
        ↓
每个 PlayerController 接收攻击者和受害者 PlayerState
        ↓
客户端按自己的视角生成公告文本
```

客户端根据自身 PlayerState 与双方的关系显示不同文本：

| 情况 | 公告表达 |
| --- | --- |
| 自己击败其他玩家 | `你 → 对方名字` |
| 自己被其他玩家击败 | `对方名字 → 你` |
| 自己击败自己 | `你 → 自己` |
| 其他玩家击败自己 | `玩家名字 → 自己` |
| 普通第三方事件 | `攻击者名字 → 受害者名字` |

同一次服务器事件在每个客户端上转换为符合本地玩家视角的文本，不需要服务器为每个人拼接不同字符串。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:44`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:704`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:709`

## 武器处理

GameMode 完成规则数据更新后调用角色的 `Elim(false)`。角色首先处理主武器和副武器：

```text
EquippedWeapon  → Dropped()
SecondaryWeapon → Dropped()
```

普通武器执行 `Dropped()` 后会切换为场景掉落状态、脱离角色并清空 Owner；默认武器具有 `bDestroyWeapon = true`，执行 `Dropped()` 时直接销毁。

因此死亡时：

- 玩家在场景中获得的普通武器可以掉落。
- 随角色生成的默认武器不会作为拾取物残留在地图上。
- 新角色复活后会重新生成一把新的默认武器。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:786`
- `Source/Blaster/Weapon/Weapon.cpp:217`
- `Source/Blaster/Weapon/Weapon.h:94`

## 可靠多播同步死亡表现

武器处理完成后，服务器调用可靠的 `MulticastElim()`。该函数在服务器和所有相关客户端执行：

```text
设置 bElimmed = true
播放 ElimMontage
禁用角色移动并立即停止当前速度
禁用本地输入
关闭胶囊体碰撞
关闭骨骼网格体碰撞
将武器和携带弹药 HUD 清零
必要时关闭本地狙击镜
启动 ElimDelay 计时器
```

当前 `ElimDelay` 为 `1.8f` 秒。这段时间用于完整播放死亡表现，角色不会立即从所有客户端画面中消失。

关闭胶囊体和骨骼网格体碰撞后，死亡角色不再阻挡其他角色或继续参与场景碰撞；禁用移动和输入则阻止旧 Pawn 在等待复活期间继续响应操作。

相关实现：

- `Source/Blaster/Character/BlasterCharacter.h:30`
- `Source/Blaster/Character/BlasterCharacter.h:168`
- `Source/Blaster/Character/BlasterCharacter.cpp:802`

## 延迟结束后的服务器复活

`MulticastElim()` 会在各端启动相同计时器，但 `ElimTimerFinished()` 使用 `GetAuthGameMode()` 获取服务器专属 GameMode，因此真正的复活只会由服务器执行：

```text
ElimDelay 结束
       ↓
服务器 ElimTimerFinished()
       ↓
GameMode::RequestRespawn
       ↓
Reset 旧 Character
       ↓
Destroy 旧 Character
       ↓
收集场景中的 PlayerStart
       ↓
随机选择一个出生点
       ↓
RestartPlayerAtPlayerStart
```

`RequestRespawn()` 销毁的是旧角色 Pawn，并不会销毁原有 Controller 和 PlayerState。GameMode 使用同一个 Controller 在选择的 PlayerStart 上生成并控制新 Pawn。

生命周期关系为：

```text
PlayerController ──────────────── 保留
PlayerState      ──────────────── 保留得分、失败次数和队伍

旧 Character    → Reset / Destroy
新 Character    → Spawn / Possess
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:842`
- `Source/Blaster/GameMode/BlasterGameMode.cpp:54`

## 新角色生成默认武器

新 Character 创建后重新执行 `BeginPlay()`，其中调用 `SpawDefaultWeapon()`：

```text
新 Character::BeginPlay()
        ↓
SpawDefaultWeapon()
        ↓
GetAuthGameMode()
        ↓
服务器生成 DefaultWeaponClass
        ↓
设置 bDestroyWeapon = true
        ↓
CombatComponent::EquipWeapon()
```

客户端无法取得权威 GameMode，因此不会自行生成默认武器。武器 Actor 只由服务器创建，再通过 Actor 和装备状态复制到客户端。

`CombatComponent::EquipWeapon()` 在新角色没有其他武器时，将默认武器作为主武器装备。`EquipPrimaryWeapon()` 随后执行：

- 设置 `EWS_Equipped`。
- 附加到角色骨骼的 `RightHandSocket`。
- 将 Character 设为武器 Owner。
- 更新武器弹药和当前携带弹药。
- 播放装备音效。
- 根据弹匣状态执行后续装备初始化。

默认武器的 `bDestroyWeapon` 标记形成完整生命周期：

```text
角色生成
└─ 服务器创建并装备默认武器

角色死亡
└─ 默认武器执行 Dropped()
       └─ bDestroyWeapon = true，因此直接销毁

角色复活
└─ 新 Character 再次创建一把默认武器
```

相关实现：

- `Source/Blaster/Character/BlasterCharacter.cpp:172`
- `Source/Blaster/Character/BlasterCharacter.cpp:949`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:190`
- `Source/Blaster/BlasterComponent/CombatComponent.cpp:705`

## 复活后的 HUD

PlayerController 重新控制新 Pawn 时执行 `OnPossess()`，主动读取新角色的生命和护盾并刷新 HUD。默认武器装备时，武器和 CombatComponent 再更新弹匣及携带弹药显示。

这使旧 Pawn 的死亡状态不会继续保留到新角色界面中；长期数据仍由 PlayerState 保留，而生命、护盾、武器等 Pawn 数据由新角色重新初始化。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:500`

## 技术亮点

### GameMode、GameState 与 PlayerState 分工明确

GameMode 在服务器执行规则结算和复活；GameState 保存比赛公共比分与最高分玩家；PlayerState 保存跨 Pawn 生命周期的个人得分、失败次数和队伍。

### 规则结算与表现同步分离

服务器先完成得分与失败次数更新，再通过 Client RPC 和可靠多播发送音效、公告及死亡表现。客户端只消费服务器确认的结果。

### Controller 与 PlayerState 跨复活保留

复活只替换 Pawn，不替换 Controller 和 PlayerState。角色可以恢复默认状态，同时玩家得分、失败次数和队伍归属继续保留。

### 默认武器具有独立生命周期

默认武器由服务器随角色创建，正常参与装备和复制，但在角色死亡时直接销毁，避免反复复活在地图中堆积默认武器。

## 单元总结

角色生命归零后，服务器 GameMode 统一处理个人得分、最高分、团队分数、失败次数和淘汰公告；得分成立时，只向攻击者客户端播放得分音效。随后角色丢弃普通武器、销毁默认武器，并通过可靠多播在所有端同步死亡动画、移动输入禁用、碰撞关闭和复活计时。

计时结束后，服务器销毁旧 Pawn，保留原 Controller 与 PlayerState，并在随机 PlayerStart 上创建新 Character。新角色的 `BeginPlay()` 再由服务器生成 `DefaultWeaponClass`，通过 CombatComponent 完成右手附加、Owner 设置、装备状态和弹药 HUD 更新，形成完整的死亡—复活—重新武装流程。

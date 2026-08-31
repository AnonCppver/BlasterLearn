# 主菜单、游戏设置与 Steam Session

## 功能点介绍

项目主菜单由 UMG 蓝图负责界面布局和设置交互，由 C++ `UMenu` 基类负责模式选择、Host、Join 和退出游戏，再通过自定义的 `UMultiplayerSessionsSubsystem` 封装 UE Online Session 异步接口。在线服务使用 Steam Online Subsystem 和 Steam Sockets，支持创建可发现的监听服务器、按游戏模式搜索房间、加入 Session、解析连接地址，以及从大厅无缝切换到对应比赛地图。

菜单包含以下主要功能：

- 游戏设置：窗口模式、分辨率、画质等级和垂直同步。
- 模式选择：个人射击、团队模式和 SFE。
- Host：创建 Steam Session 并进入监听大厅。
- Join：搜索相同 MatchType 的 Session 并连接主机。
- Quit：退出游戏。

## 系统结构

```text
Lvl_Init
    ↓ 创建
WBP_Menu（UMG 蓝图）
    ├─ WBP_OptionMenu：GameUserSettings
    └─ UMenu C++ Base
            ├─ 模式选择
            ├─ Host / Join / Quit
            └─ 绑定异步回调
                    ↓
UMultiplayerSessionsSubsystem
    ├─ CreateSession
    ├─ FindSessions
    ├─ JoinSession
    ├─ DestroySession
    └─ StartSession
                    ↓
IOnlineSession / OnlineSubsystemSteam
                    ↓
Steam Lobby + SteamSockets
                    ↓
Lobby → FreeShooting / TeamShooting / SFE
```

## 界面与蓝图功能

### 1. 启动地图创建主菜单

打包后的默认地图为 `Lvl_Init`。关卡蓝图创建 `WBP_Menu` 并调用 C++ 暴露的 `MenuSetup()`，传入公开连接数、MatchType 和 Lobby 路径。

相关资源：

- `Config/DefaultEngine.ini:3`
- `Content/Maps/Lvl_Init.umap`
- `Plugins/MultiplayerSessions/Content/WBP_Menu.uasset`

### 2. 主菜单 Widget

`WBP_Menu` 继承自 C++ `UMenu`，界面中包含：

```text
HostButton      → 创建 Session
JoinButton      → 搜索并加入 Session
GameModeButton  → 循环切换游戏模式
GameModeText    → 显示当前模式名称
OptionButton    → 打开设置界面
QuitButton      → 退出游戏
```

Host、Join、Quit 和 GameMode 控件通过 `BindWidget` 与 C++ 变量绑定；OptionButton 与设置界面的创建和显示主要保留在蓝图中。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/Menu.h:18`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:70`
- `Plugins/MultiplayerSessions/Content/WBP_Menu.uasset`

### 3. 菜单输入模式

`MenuSetup()` 把 Widget 加入 Viewport，并将 PlayerController 切换到 `UIOnly`：

```text
InputMode          → UIOnly
WidgetToFocus      → 当前 Menu
MouseLock          → DoNotLock
ShowMouseCursor    → true
```

菜单被销毁时，`MenuTearDown()` 执行反向操作：移除 Widget、切回 `GameOnly` 并隐藏鼠标。这让同一个 PlayerController 能够在菜单交互和游戏输入之间正确切换。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:15`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:67`

### 4. 蓝图设置界面

`WBP_OptionMenu` 主要由蓝图实现，并直接使用 UE 的 `GameUserSettings`。资产中可以确认以下设置能力：

```text
GetGameUserSettings
LoadSettings

窗口模式：
    GetFullscreenMode
    SetFullscreenMode

分辨率：
    GetScreenResolution
    SetScreenResolution

画质：
    Scalability

垂直同步：
    SetVSyncEnabled

保存应用：
    ApplySettings
```

设置项通过加减按钮循环调整，界面包含窗口模式、分辨率、画质和 VSync 的显示文本，以及 Apply 和 Exit 按钮。

将设置逻辑放在蓝图中，便于直接绑定 UMG 控件和快速调整菜单表现；Steam Session 的异步网络流程则保留在 C++ 中。

相关资源：

- `Plugins/MultiplayerSessions/Content/WBP_OptionMenu.uasset`
- `Plugins/MultiplayerSessions/Content/WBP_Menu.uasset`

## 模式选择

### 5. MatchType 与显示名称分离

`UMenu` 保存两组对应数组：

```text
内部 MatchType       菜单显示名称
FreeShooting    →    个人射击
TeamShooting    →    团队模式
SFE             →    SFE
```

点击 `GameModeButton` 后：

```text
GameModeIndex = (GameModeIndex + 1) % GameModes.Num()
MatchType = GameModes[GameModeIndex]
GameModeText = 对应显示名称
```

内部字符串用于 Session 广告、搜索过滤和地图路由，显示名称只负责本地 UI，避免中文展示文本参与网络匹配判断。

`MenuSetup()` 还会尝试根据传入的 MatchType 定位初始索引；找不到时回退到 `FreeShooting`。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/Menu.h:80`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:15`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:262`

## MultiplayerSessionsSubsystem

### 6. 使用 GameInstanceSubsystem 封装 Session

`UMultiplayerSessionsSubsystem` 继承 `UGameInstanceSubsystem`。它不属于某个关卡或 Widget，而是跟随 GameInstance 存活，因此从主菜单进入 Lobby、再切换比赛地图时可以继续保存：

- OnlineSessionInterface
- 上一次 Session Settings
- 上一次 Session Search
- 公开连接数
- 当前选择的 MatchType
- 异步委托和 DelegateHandle

菜单只调用高层接口：

```text
CreateSession
FindSessions
JoinSession
DestroySession
StartSession
```

OnlineSubsystem 的具体委托注册、句柄清理和失败广播均由 Subsystem 统一处理，避免 Widget 直接管理底层 Session 生命周期。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/MultiplayerSessionsSubsystem.h:30`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:9`

### 7. 两层异步委托

Online Session 操作不是同步返回结果，而是通过完成委托通知。Subsystem 构造时准备五种底层回调：

```text
OnCreateSessionComplete
OnFindSessionsComplete
OnJoinSessionComplete
OnDestroySessionComplete
OnStartSessionComplete
```

每次发起操作时把委托注册到 `IOnlineSession` 并保存 `FDelegateHandle`；完成后先清除对应句柄，再通过项目自己的 `MultiplayerOn...` 委托广播给菜单。

```text
Menu 发起请求
    ↓
MultiplayerSessionsSubsystem
    ↓ 注册 Online Delegate
IOnlineSession 异步执行
    ↓
Subsystem On...Complete
    ↓ 清理 DelegateHandle
MultiplayerOn... Broadcast
    ↓
Menu 回调更新按钮或执行 Travel
```

这种两层委托把平台接口与 UI 分离：Steam、NULL LAN 或其他 OnlineSubsystem 都可以复用同一套菜单回调。

## Host 流程

### 8. Host 按钮创建 Session

点击 Host 后，按钮会先被禁用，防止玩家在异步创建过程中重复提交：

```text
HostButton Disabled
        ↓
CreateSession(NumPublicConnections, MatchType)
```

Subsystem 同时保存：

```text
DesiredNumPublicConnections
DesiredMatchType
```

其中 `DesiredMatchType` 会在 Lobby 中用于选择最终比赛地图。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:222`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:34`

### 9. Session Settings

创建 Session 时设置：

```text
NumPublicConnections     → 菜单传入的公开连接数
bAllowJoinInProgress     → true
bAllowJoinViaPresence    → true
bShouldAdvertise         → true
bUsesPresence            → true
bUseLobbiesIfAvailable   → true
BuildUniqueId            → 1
```

使用 Steam 时创建 Online Lobby；使用 NULL OnlineSubsystem 时自动切换为 LAN Match。

项目还把 `MatchType` 写入 Session 自定义属性：

```text
Key   = "MatchType"
Value = FreeShooting / TeamShooting / SFE
Advertise = ViaOnlineServiceAndPing
```

这个属性使不同游戏模式能够共享同一套 Session 搜索入口，再由菜单过滤出对应房间。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:56`

### 10. 已存在 Session 时先销毁再重建

如果当前本地用户已经拥有 `NAME_GameSession`，Subsystem 不会直接再次创建，而是保存新的公开连接数和 MatchType：

```text
bCreateSessionOnDestroy = true
LastNumPublicConnections = 新值
LastMatchType = 新值
```

然后调用 `DestroySession()`。销毁成功后自动使用缓存参数再次执行 `CreateSession()`。

```text
发现已有 Session
        ↓
缓存新 Session 参数
        ↓
DestroySession
        ↓ success
CreateSession(缓存参数)
```

这避免 OnlineSubsystem 中同时存在同名 Session 导致创建失败。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:42`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:249`

### 11. 创建成功后进入监听大厅

Session 创建完成后，Subsystem 广播结果给 `UMenu::OnCreateSession()`。菜单恢复 HostButton；成功时由服务器执行：

```text
ServerTravel(PathToLobby + "?listen")
```

`?listen` 让当前客户端同时成为 Listen Server 主机，后续加入 Session 的玩家可以连接到该 Lobby 世界。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:113`

## Join 流程

### 12. 搜索 Session

点击 Join 后同样先禁用按钮，再调用：

```text
FindSessions(MaxSearchResults = 10000)
```

搜索条件包括：

```text
SEARCH_PRESENCE = true
Steam           → Online Search
NULL Subsystem  → LAN Query
```

搜索完成后，Subsystem 把结果数组广播给菜单。搜索成功但结果数为零时，对菜单表现为失败结果。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:236`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:117`

### 13. 按 MatchType 过滤并加入第一个匹配房间

菜单遍历搜索结果，读取每个 Session 广告中的：

```text
SessionSettings["MatchType"]
```

只有它与当前菜单选择的 MatchType 完全相等时才调用 `JoinSession(Result)`，并在找到第一个匹配结果后立即结束遍历。

```text
搜索结果
    ↓ 遍历
读取 MatchType
    ├─ 不同：继续查找
    └─ 相同：JoinSession 并停止
```

没有找到匹配房间时重新启用 JoinButton。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:139`

### 14. 解析连接地址并 ClientTravel

JoinSession 完成后，菜单通过 SessionInterface 获取：

```text
GetResolvedConnectString(NAME_GameSession, Address)
```

解析成功后，本地 PlayerController 执行：

```text
ClientTravel(Address, TRAVEL_Absolute)
```

Host 使用 `ServerTravel` 打开监听大厅，Join 玩家使用 `ClientTravel` 前往主机解析出的网络地址，两者职责清晰分离。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:175`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:177`

## Steam 接入

### 15. OnlineSubsystemSteam 与 SteamSockets

项目启用了：

```text
OnlineSubsystemSteam
SteamSockets
```

核心配置为：

```ini
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true
```

`SteamDevAppId=480` 是开发测试使用的 Steam App ID。网络驱动优先使用 `SteamSocketsNetDriver`，并配置 `IpNetDriver` 作为回退。

```text
GameNetDriver
    ├─ SteamSocketsNetDriver
    └─ IpNetDriver Fallback
```

Session 插件模块显式依赖 `OnlineSubsystem`、`OnlineSubsystemSteam`、UMG、Slate 和 SlateCore。

相关配置：

- `Blaster.uproject:25`
- `Config/DefaultEngine.ini:8`
- `Config/DefaultEngine.ini:20`
- `Plugins/MultiplayerSessions/MultiplayerSessions.uplugin`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/MultiplayerSessions.Build.cs`

## Lobby 与模式地图

### 16. Lobby 等待玩家

`ALobbyGameMode::PostLogin()` 在玩家进入大厅时统计 `GameState.PlayerArray`。当前实现当玩家数达到 `2` 时开始比赛，而不是等待菜单配置的全部公开连接数。

```text
Player Login
    ↓
Lobby Player Count == 2
    ↓
bUseSeamlessTravel = true
```

随后从 GameInstanceSubsystem 读取主机创建 Session 时保存的 `DesiredMatchType`。为空时回退到 `FreeShooting`。

相关实现：

- `Source/Blaster/GameMode/LobbyGameMode.cpp:8`

### 17. 按模式执行无缝 ServerTravel

Lobby 的地图路由为：

```text
FreeShooting → /Game/Maps/FreeShooting?listen
TeamShooting → /Game/Maps/TeamShooting?listen
SFE          → /Game/Maps/SFE?listen
```

`bUseSeamlessTravel = true` 允许已连接玩家随服务器一起切换地图，减少重新建立网络连接带来的状态丢失。

当前仓库和打包地图列表中可以确认 `FreeShooting` 与 `TeamShooting`；菜单和 Lobby 已预留 SFE 路由，但当前 `Content/Maps` 与 MapsToCook 中没有对应的 `SFE` 地图资产，因此它属于尚未落地的模式入口。

相关实现：

- `Source/Blaster/GameMode/LobbyGameMode.cpp:35`
- `Config/DefaultGame.ini:105`
- `Content/Maps/Lobby.umap`
- `Content/Maps/FreeShooting.umap`
- `Content/Maps/TeamShooting.umap`

## 退出与生命周期边界

### 18. Quit 与未直接使用的 StartSession

QuitButton 通过 `UKismetSystemLibrary::QuitGame()` 退出游戏。

Subsystem 完整实现了 `StartSession()` 和对应的异步回调、句柄清理及广播，但当前 Menu 和 Lobby 主流程没有直接调用它；实际比赛开始由 Lobby 的玩家数量条件和 ServerTravel 驱动。

相关实现：

- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp:251`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp:297`

## 技术亮点

### 蓝图表现与 C++ 网络职责分离

设置页和主菜单布局保留在 UMG 蓝图中，便于快速调整视觉与控件；C++ `UMenu` 处理按钮行为和 Travel，GameInstanceSubsystem 统一封装平台 Session 异步接口。

### GameInstanceSubsystem 跨地图管理在线状态

SessionInterface、搜索对象、模式选择和委托句柄跟随 GameInstance 存活，不依赖某个菜单 Widget 或关卡 Actor，适合主菜单、Lobby 和比赛地图之间的切换。

### 自定义 MatchType 贯穿完整链路

同一个内部模式标识同时用于菜单选择、Steam Session 广告、搜索结果过滤和 Lobby 地图路由，实现从用户选择到最终比赛地图的一致数据链路。

### 委托驱动异步 Session 生命周期

创建、搜索、加入、销毁和开始操作都使用完成委托，并在广播 UI 结果前清理 DelegateHandle，避免异步 OnlineSubsystem 调用阻塞游戏线程或重复触发旧回调。

### Steam Presence、Lobby 与 Socket 集成

Session 开启 Presence、广告、加入进行中和可用 Lobby，网络驱动使用 SteamSockets，同时保留 NULL LAN 查询与 IpNetDriver 回退路径。

### Host 与 Join 使用不同 Travel 语义

Host 通过 `ServerTravel(...?listen)` 建立监听服务器，Join 玩家解析 Session 地址后通过 `ClientTravel` 连接；进入 Lobby 后再使用 Seamless ServerTravel 将所有玩家带入对应模式地图。

## 截图建议

建议为个人页面准备以下截图：

1. 主菜单全貌，展示 Host、Join、模式、设置和退出入口。
2. 设置界面，展示窗口模式、分辨率、画质和 VSync。
3. 个人射击与团队模式的切换对比。
4. Host 创建 Session 后进入 Lobby 的画面。
5. 第二名玩家通过 Join 进入 Lobby 的画面。
6. 两名玩家从 Lobby 无缝进入个人射击地图。
7. 两名玩家从 Lobby 无缝进入团队模式地图。
8. Steam 好友或 Lobby 列表中的联机状态，用于证明 Steam Session 接入。

推荐图片命名：

- `main-menu.png`
- `option-menu.png`
- `game-mode-selection.png`
- `host-lobby.png`
- `join-lobby.png`
- `free-shooting-travel.png`
- `team-shooting-travel.png`
- `steam-session-presence.png`

## 单元总结

主菜单以 UMG 蓝图承载界面和 `GameUserSettings`，提供窗口模式、分辨率、画质和 VSync 设置；C++ `UMenu` 管理模式选择、Host、Join、Quit 和输入模式切换。模式内部标识与中文显示文本分离，并以 `MatchType` 自定义属性写入 Steam Session，Join 流程搜索 Presence Session 后只加入与当前选择模式一致的第一个结果。

`UMultiplayerSessionsSubsystem` 作为 GameInstanceSubsystem 封装 `IOnlineSession`，统一管理创建、搜索、加入、销毁和开始 Session 的异步委托与句柄清理。Host 成功后通过 `ServerTravel(...?listen)` 进入监听大厅，Join 玩家解析连接地址并执行 `ClientTravel`；Lobby 达到两名玩家后读取 Subsystem 中保存的 `DesiredMatchType`，再通过 Seamless ServerTravel 进入个人射击或团队模式地图。底层在线服务使用 `OnlineSubsystemSteam`、Steam Lobby Presence 和 SteamSockets，同时保留 NULL LAN 与 IP NetDriver 回退能力。

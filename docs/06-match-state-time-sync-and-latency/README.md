# 比赛状态、时间同步与延迟检测

## 功能点介绍

多人对战中的比赛阶段由服务器统一推进，客户端不直接访问只存在于服务器的 `GameMode`，而是通过自己的 `ABlasterPlayerController` 接收当前比赛状态和整条比赛时间轴。

项目没有持续复制一个每秒递减的倒计时，而是让客户端先估算服务器当前时间，再使用服务器关卡开始时间和各阶段时长自行计算 HUD 剩余时间。这使所有玩家围绕同一个服务器时间基准显示倒计时，也减少了不必要的高频网络同步。

本单元包含：

- 服务器如何推进 `WaitingToStart`、`InProgress` 和 `Cooldown`。
- 比赛状态如何同步到已连接客户端。
- 中途加入的客户端如何补齐当前状态和时间参数。
- 客户端如何利用多次请求与响应、样本过滤和平均时延估算服务器时间。
- 自定义单程延迟和 UE PlayerState Ping 的区别。
- 同步后的服务器时间如何驱动 HUD 倒计时。

## 服务器权威的比赛状态机

`ABlasterGameMode` 只存在于服务器，并保存比赛时间轴的权威数据：

| 字段 | 默认值 | 作用 |
| --- | ---: | --- |
| `WarmupTime` | 10 秒 | 比赛正式开始前的等待时长 |
| `MatchTime` | 120 秒 | 正式比赛时长 |
| `CooldownTime` | 10 秒 | 比赛结束后的结算时长 |
| `LevelStartingTime` | 运行时记录 | 服务器关卡开始时刻 |
| `MatchState` | 由 GameMode 维护 | 当前比赛阶段 |

构造函数将 `bDelayedStart` 设为 `true`，因此关卡首先处于 `MatchState::WaitingToStart`。`BeginPlay()` 使用服务器的 `GetWorld()->GetTimeSeconds()` 记录 `LevelStartingTime`。

服务器在 `Tick()` 中根据绝对时间计算各阶段的剩余时间：

```text
WaitingToStart
剩余时间 = LevelStartingTime + WarmupTime - ServerTime

InProgress
剩余时间 = LevelStartingTime + WarmupTime + MatchTime - ServerTime

Cooldown
剩余时间 = LevelStartingTime + WarmupTime + MatchTime
         + CooldownTime - ServerTime
```

状态转换流程：

```text
WaitingToStart
      │ 剩余时间归零：StartMatch()
      ▼
InProgress
      │ 剩余时间归零：SetMatchState(Cooldown)
      ▼
Cooldown
      │ 剩余时间归零：RestartGame()
      ▼
重新开始比赛
```

客户端显示的倒计时不会决定状态转换。比赛何时开始、结束和重启始终由服务器的 `GameMode` 决定。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.h:29`
- `Source/Blaster/GameMode/BlasterGameMode.cpp:88`
- `Source/Blaster/GameMode/BlasterGameMode.cpp:113`

## 比赛状态如何到达客户端

客户端没有 `GameMode`，所以服务器通过每名玩家对应的 `ABlasterPlayerController` 下发状态。

当服务器上的 `MatchState` 变化时，`ABlasterGameMode::OnMatchStateSet()` 遍历所有 PlayerController，并调用：

```cpp
BlasterPlayer->OnMatchStateSet(MatchState, bTeamsMatch);
```

PlayerController 将状态写入带有 `ReplicatedUsing` 的字段：

```cpp
UPROPERTY(ReplicatedUsing = OnRep_MatchState)
FName MatchState;
```

完整链路为：

```text
服务器 GameMode 改变 MatchState
          ↓
GameMode::OnMatchStateSet()
          ↓
服务器端 PlayerController::OnMatchStateSet()
          ↓
MatchState 属性复制给该 PlayerController 的客户端
          ↓
客户端 OnRep_MatchState()
          ↓
切换 HUD 和阶段表现
```

进入 `InProgress` 时，PlayerController 创建角色比赛 HUD 并隐藏赛前公告；进入 `Cooldown` 时，移除比赛 HUD并显示结算公告。

服务器本地玩家不依赖 `OnRep_MatchState()`。服务器端调用 `OnMatchStateSet()` 后会直接执行对应处理，因此监听服务器本地玩家和远程客户端都能更新界面。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:143`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:304`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:318`
- `Source/Blaster/PlayerController/BlasterPlayerController.h:102`

## 中途加入比赛的状态补齐

单独复制 `MatchState` 不能为新客户端提供完整倒计时，因为 `WarmupTime`、`MatchTime`、`CooldownTime` 和 `LevelStartingTime` 也参与计算。

因此 PlayerController 在 `BeginPlay()` 中调用可靠的 Server RPC：

```text
客户端 PlayerController::BeginPlay()
          ↓
ServerCheckMatchState()
          ↓
服务器从 GameMode 读取：
MatchState、WarmupTime、MatchTime、CooldownTime、LevelStartingTime
          ↓
ClientJoinMidgame(...)
          ↓
客户端保存完整时间轴并立即处理当前状态
```

`ClientJoinMidgame()` 是可靠的 Client RPC。无论玩家在等待、正式比赛还是结算阶段加入，都可以直接进入正确状态，而不必等待下一次状态转换。

客户端接收的是“当前状态＋整条比赛时间轴”，而不是某一刻的剩余秒数：

```text
当前比赛阶段
+ 三段比赛时长
+ 服务器关卡开始时刻
+ 校准后的服务器当前时间
= 客户端当前应显示的剩余时间
```

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:32`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:470`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:484`

## 客户端如何计算服务器时间

服务器与客户端的 `GetWorld()->GetTimeSeconds()` 分别来自各自进程，不能直接视为同一时钟。系统周期性执行请求—响应采样，并通过“滑动样本窗口、异常样本过滤、有效样本求平均”估算两个时钟的偏移量。

### 1. 客户端记录请求时刻

客户端在本地时间 `T0` 发出 Server RPC：

```text
T0 = TimeOfClientRequest
ServerRequestServerTime(T0)
```

### 2. 服务器记录收到请求的时刻

服务器收到请求时读取服务器世界时间：

```text
Ts = TimeServerReceivedClientRequest
```

随后通过 `ClientReportServerTime(T0, Ts)` 将客户端原始请求时间和服务器接收时间返回。

### 3. 为本次样本计算 RTT、单程延迟和时钟偏差

客户端收到回复时的本地时间记为 `T1`：

```text
RTTᵢ = T1ᵢ - T0ᵢ
OneWayᵢ = RTTᵢ × 0.5
```

这一步假定上行和下行网络耗时近似对称，因此使用 RTT 的一半估算单程延迟。它是网络回溯中的工程近似，而不是对上行、下行分别进行物理测量。

### 4. 推算回复抵达时的服务器时间

服务器在 `Ts` 收到请求，回复再返回客户端还要经过约一个单程延迟，因此：

```text
EstimatedServerTimeAtT1ᵢ = Tsᵢ + OneWayᵢ
```

本次样本对应的时钟偏差为：

```text
Deltaᵢ = EstimatedServerTimeAtT1ᵢ - T1ᵢ
```

### 5. 放入滑动窗口并过滤异常样本

客户端保存最近若干组 `{RTTᵢ, OneWayᵢ, Deltaᵢ}`。窗口满后覆盖最旧样本，避免数组持续增长。

过滤流程为：

1. 丢弃无效 RTT，例如负值、超出服务器允许回溯范围或明显超过断线阈值的样本。
2. 按 RTT 从低到高排列当前窗口中的样本。
3. 仅保留 RTT 较低的一半样本，排除可靠 RPC 排队、线程调度或瞬时拥塞造成的高延迟尖峰。
4. 对保留样本的 `OneWayᵢ` 和 `Deltaᵢ` 分别求算术平均。

```text
SingleTripTime = Average(ValidSamples.OneWay)
ClientServerDelta = Average(ValidSamples.Delta)
```

低 RTT 样本更接近数据包没有额外排队时的网络路径。对过滤后的多组结果求平均，又能避免只采用某一个最低样本所带来的偶然误差。

### 6. 使用平均偏差还原服务器时间

此后客户端不需要为每次查询重新发送 RPC：

```text
GetServerTime()
= ClientLocalTime + ClientServerDelta
```

服务器自身调用 `GetServerTime()` 时直接返回服务器世界时间。

单次采样的基础计算仍然是：

```cpp
float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
float OneWay = 0.5f * RoundTripTime;
float EstimatedServerTime = TimeServerReceivedClientRequest + OneWay;
float SampleDelta = EstimatedServerTime - GetWorld()->GetTimeSeconds();
```

`OneWay` 和 `SampleDelta` 不再直接覆盖正式结果，而是先进入样本窗口；过滤并求平均后才更新 `SingleTripTime` 与 `ClientServerDelta`。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:275`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:281`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:289`

## 首次同步与周期校准

PlayerController 的 `ReceivedPlayer()` 在本地控制器准备完成后立即请求一次服务器时间，避免客户端长期使用默认的零偏移量。

运行期间，`CheckTimeSync()` 每隔 `TimeSyncFrequency` 再次采样。当前默认频率为 5 秒：

```text
加入服务器并完成 PlayerController 初始化
                ↓
立即进行一次时间同步
                ↓
每 5 秒重新估算 RTT、SingleTripTime 和 ClientServerDelta
```

周期校准可以跟随网络状态变化，并修正客户端与服务器时钟逐渐产生的误差。每次响应先产生一个候选样本，窗口经过异常值过滤后重新计算平均单程延迟和平均时钟偏差，因此一次延迟尖峰不会直接让服务器时间和 SSR 命中时刻发生跳变。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.h:68`
- `Source/Blaster/PlayerController/BlasterPlayerController.h:70`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:295`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:458`

## HUD 倒计时

客户端每帧调用 `SetHUDTime()`，但不会用 `DeltaTime` 递减一个本地计时变量，而是始终用阶段结束时刻减去估算出的服务器当前时间。

以正式比赛为例：

```text
TimeLeft = LevelStartingTime
         + WarmupTime
         + MatchTime
         - GetServerTime()
```

随后使用 `FMath::CeilToInt(TimeLeft)` 得到要显示的整数秒。只有整数秒发生变化时才更新文本：

- `WaitingToStart` 和 `Cooldown` 更新公告界面的倒计时。
- `InProgress` 更新角色 HUD 中的比赛倒计时。
- 时间小于零时清空对应文本。

这种计算方式不需要服务器每秒广播倒计时。即使某个网络包延迟，下一帧也会根据统一时间轴重新得到正确结果，不会把一次延迟永久累积到本地计时器中。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:184`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:206`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:228`

## 两类延迟数据的区别

项目中存在两套用途不同的延迟信息。

### SingleTripTime：时间同步采样得到的单程延迟

`SingleTripTime` 来自自定义时间同步 RPC 的多次有效样本：

```text
SingleTripTime
= Average(过滤后每个自定义 RPC 样本的 RoundTripTime ÷ 2)
```

它以秒为单位，用于服务器时间估算，也会被射线、投射物和霰弹武器用于估算服务器倒带所需的命中时刻：

```text
HitTime = GetServerTime() - SingleTripTime
```

### GetCompressedPing：引擎维护的网络 Ping

高延迟提示读取 `APlayerState::GetCompressedPing()`。该数值是压缩后的 Ping，因此乘以 4 还原为毫秒：

```text
PingMilliseconds = GetCompressedPing() × 4
```

当前默认每 20 秒检查一次；高于 50 毫秒时显示高延迟图标和动画，动画持续约 5 秒。

这套 Ping 主要用于 HUD 警告，不参与 `ClientServerDelta` 的计算。`SingleTripTime` 和 `GetCompressedPing()` 虽然都反映网络延迟，但来源、单位和用途均不同。

相关实现：

- `Source/Blaster/PlayerController/BlasterPlayerController.h:32`
- `Source/Blaster/PlayerController/BlasterPlayerController.h:133`
- `Source/Blaster/PlayerController/BlasterPlayerController.cpp:523`
- `Source/Blaster/Weapon/HitScanWeapon.cpp:56`
- `Source/Blaster/Weapon/ProjectileBullet.cpp:34`
- `Source/Blaster/Weapon/Shotgun.cpp:93`

## 初始化顺序细节

源码注释记录了一个实际遇到的初始化顺序问题：`ServerCheckMatchState()` 可能先于 `ABlasterGameMode::BeginPlay()` 设置 `LevelStartingTime`，导致首次 `ClientJoinMidgame()` 收到的开始时刻为 `0`。

当前 `GameMode::BeginPlay()` 在设置 `LevelStartingTime` 后，又将该值写入索引为 0 的 PlayerController。这可以补偿监听服务器本地 PlayerController，但它不是一次面向所有已连接远程客户端的重新广播。

文档按当前源码记录这一点，不把该补偿描述成完整的全客户端时间轴重同步。

相关实现：

- `Source/Blaster/GameMode/BlasterGameMode.cpp:93`
- `Source/Blaster/GameMode/BlasterGameMode.cpp:104`

## 技术亮点

### 服务器只同步时间基准，不高频广播倒计时

服务器负责权威状态转换，客户端根据同步后的服务器时间计算 HUD。网络只需同步状态变化、阶段参数和时钟偏移采样，不需要每秒复制一份剩余时间。

### 通过 PlayerController 隔离服务器专属 GameMode

客户端不尝试访问 `GameMode`。每个 PlayerController 作为服务器与所属客户端之间的通信入口，承接状态复制、Server RPC 和 Client RPC，符合 UE 的网络对象职责。

### 支持中途加入当前比赛时间轴

`ClientJoinMidgame()` 一次传递当前状态、三段时长和关卡起始时刻，使晚加入玩家能够立即恢复当前比赛阶段及其正确倒计时。

### 将比赛逻辑与 HUD 显示分离

服务器倒计时决定真实状态转换，客户端倒计时只决定显示内容。即使客户端显示存在短暂误差，也不会改变比赛规则或提前触发下一阶段。

### 过滤并平均时间同步样本

客户端不会让最新一次 RTT 直接覆盖时间基准，而是保留一个固定容量的滑动窗口，过滤高延迟异常样本后计算平均单程延迟和平均时钟偏差。该设计降低了抖动对比赛倒计时与服务器回溯时间戳的影响。

### 区分时钟校准与高延迟提示

自定义 RTT 采样用于估算服务器时间和单程延迟，UE PlayerState Ping 用于低频高延迟警告。两种网络指标没有混用。

## 单元总结

项目以 `ABlasterGameMode` 维护服务器权威的比赛状态机，通过 `ABlasterPlayerController` 将状态变化传递给客户端，并使用可靠 RPC 为中途加入玩家补齐当前状态、阶段时长和服务器关卡起始时间。

客户端周期性收集请求—响应样本，过滤高 RTT 异常值后，以有效样本的平均单程延迟计算 `SingleTripTime`，并以平均时钟偏差更新 `ClientServerDelta`，使本地 `GetServerTime()` 能稳定地近似返回服务器当前时间。HUD 使用该时间与阶段结束时刻的差值显示倒计时，而不是维护容易漂移的本地递减计时器。

高延迟提示则独立读取 `GetCompressedPing()`，用于周期性检查和 UI 警告。由此形成了“服务器权威推进状态、客户端同步时间基准、本地计算显示、独立监控延迟”的联机时间系统。

# Demo02 — UE 5.7 物理驱动游戏机制与蓝图架构实践

## 环境

- Unreal Engine 5.7
- 插件：Modeling Tools Editor Mode
- 纯蓝图项目，无 C++ 依赖

---

## 一、项目全景

本项目围绕物理驱动的 3D 小球冒险为核心，延伸出食物收集、射击打靶、方块交互等五个玩法关卡。项目的重心不在于关卡数量，而在于每一关背后刻意练习的引擎机制：从 Chaos 物理引擎的力模式选择、向量叉乘的手动推导、到蓝图接口（BPI）的多态解耦架构、增强输入系统的生命周期管理，再到 UI 数据绑定的响应式更新——每一处都经过了从"跑通"到"理解为什么这样设计"的完整思考过程。

整个项目共编写约 40 个自定义蓝图、4 个蓝图接口、4 个 UMG 控件蓝图、16 个关卡专属静态网格体，全部从零搭建，不依赖第三方模板。

---

## 二、关卡总览

### BallAdventure_Level1Map — 小球物理冒险

这是整个项目中技术密度最高的关卡。玩家操控一个物理小球，在多层平台间滚动、跳跃、翻滚，收集水晶、开启传送门、借助火炬照明、躲避销毁触发器。关卡设计的核心挑战在于：如何在一个纯物理驱动的 Pawn 上，实现接近传统 Character 的操控手感和交互灵活性。

### EatFoodMap — 碰撞驱动的收集玩法

场景中随机分布食物道具，玩家操控球体收集食物得分，同时需要避开墙壁障碍物。这个关卡的练习重点在于 GameMode 对全局状态的集中管理，以及碰撞事件驱动游戏逻辑的完整链路。

### HitCubeMap — 物理投掷物与命中反馈

玩家发射子弹投掷物，命中场景中的目标立方体后触发反馈。核心练习点是 Projectile 的物理运动配置、碰撞通道（Collision Channel）的筛选机制，以及 OnHit 事件驱动的反馈循环。

### 02-Map — 蓝图继承与交互事件

通过与场景中立方的点击和碰撞交互，练习蓝图父子继承关系以及 Actor 级别的事件绑定。

### 01-Map — 几何体关卡原型

最基础的关卡搭建练习，使用 BSP 和静态网格体进行灰盒原型设计。

---

## 三、核心技术实现

### 3.1 物理驱动与空间矩阵变换（BallAdventure 小球 Pawn）

这是整个项目中最深入底层的一部分工作。UE 原生的 Character Movement Component 是为胶囊体人形角色设计的，无法直接用于球体。因此需要在 Pawn 类上从零搭建一套物理驱动方案。

**地面滚动的向量叉乘推导**

滚动移动的核心问题在于：如何将玩家的键盘输入方向，转换为球体上的旋转扭矩轴。这个轴的数学本质是"同时垂直于世界 Z 轴和玩家输入方向"的向量——只有绕这个轴旋转，球体才会朝着玩家想去的方向滚动而不产生侧滑。

具体实现中，先从 PlayerController 获取 Control Rotation，使用 Break Rotator 只提取 Yaw（Z 轴）分量，再通过 Make Rotator 重建一个仅包含水平朝向的参照系。这步很关键：如果不剥离 Pitch 分量，相机俯仰角会导致前向向量指向地面，球体会被"按"进地板里。然后用世界 Z 轴向量 (0,0,1) 与处理后的输入方向做 Cross Product，严格执行 A×B 的顺序（左手坐标系下，Z 轴叉乘方向向量得到垂直于滑行方向的扭矩轴）。最后通过 Add Torque (In Radians) 节点，开启 Accel Change 标志（忽略质点质量惯性），将扭矩直接施加于球体。

**空中双端驱动与 Tick 优化**

跳跃后的空中控制是物理驱动游戏的一个经典难题——如果只用 Add Force，球体在空中会像失重一样难以操控；如果完全不做空中控制，玩家又会觉得手感迟钝。

解决方案是在 Event Tick 中维护一个地面/悬空状态机：通过 Line Trace By Channel 向下做射线检测判定是否着地。地面分支只施加扭矩（靠摩擦力滚动）；空中分支同时调用 Add Force（搭配 Accel Change，修改绝对空间坐标提供方向控制）和 Add Torque（维持视觉旋转惯性）。这种双端驱动的方式在保留物理真实感的同时，给了玩家足够的空中调整空间。

性能方面，将 Tick Interval 设置为 0.033 秒（约 30FPS），相比默认的每帧执行，性能开销减半，而操控手感几乎无感知差异。

**防眩晕摄像机架构**

物理驱动的球体会不断翻滚，如果摄像机直接挂在球体上，画面会像滚筒洗衣机一样旋转。解决方案是在 SpringArm 组件中开启 Use Pawn Control Rotation——摄像机的旋转仅由 PlayerController 管理，球体的物理旋转只作用于 Static Mesh 自身的视觉表现，二者彻底解耦。这背后的架构认知是：控制旋转属于"灵魂"层（PlayerController），物理旋转属于"肉体"层（Pawn），不应互相污染。

**Teleport 安全机制**

传送门（BP_Portal）的实现中，瞬移位置不从球体中心读取（因为球体在滚动中中心位置不稳定），而是从场景中预先放置的 Arrow Component 提取绝对 Transform。调用 Set Actor Location And Rotation 时严格勾选 Teleport 标志位——这个 flag 会通知 Chaos 物理引擎清空当前帧的残余速度和惯性向量，避免大距离跃迁导致的物理爆炸（球体飞出场景）和摄像机抖动。

**工业级假死与复活流水线**

死亡处理遵循严格的四步序列：Disable Input（拔除输入控制权，防止死亡后还能操控）→ Set Simulate Physics = False（冻结物理模拟，消除重力拉扯）→ Set Collision Enabled = No Collision（幽灵化，防止与其他物体叠加阻滞）→ Set Actor Hidden In Game = True（视觉消失）。复活时反过来处理输入模式安全切换：Set Input Mode Game Only → Remove From Parent 清理死亡 UI → 焦点重获取。这个序列的每一步都有明确的顺序依赖，跳步或乱序都会导致诡异的 bug（比如复活后无法操控、UI 残留、物理状态不一致等）。

### 3.2 蓝图接口体系 — 多态解耦的核心

Demo02 是四个关卡中接口设计最完整的。项目中编写了四套蓝图接口，用面向对象的思维方式来做蓝图间的通信：

**BPI_Interact（通用交互）** — 定义 Interact() 函数签名。BP_Door01 实现该接口后执行开门/关门 Timeline 动画，BP_Portal 实现该接口后执行传送逻辑。调用侧（玩家触发器）只需要调用 BPI_Interact.Interact()，完全不需要知道对面是门还是传送门。这就是接口隔离原则在蓝图中的直接体现。

**BPI_Deliver（交付接口）** — 定义 Deliver(Item) 函数。水晶（BP_Crystal）被收集完成后，通过此接口通知传送门"有水晶送达了"。水晶不需要知道传送门收到水晶后会做什么（可能是计数、可能是激活特效），只需要把 Item 引用通过接口传过去即可。

**BPI_ControllerCommands（控制器命令）** — 定义 Command(CommandType) 函数。各个交互蓝图通过此接口向 PlayerController 发送指令，而不是直接 Cast To 具体控制器类型。

**BPI_PlayerState（玩家状态）** — 定义 GetScore() 和 AddScore() 函数。任何需要读写分数的蓝图都通过此接口操作，避免直接访问 PlayerState 内部变量。

这四套接口的组合使用，构建了一个低耦合的交互网络。在工业级项目中，这种设计带来的直接收益是：新增交互物不影响已有代码、修改某个交互物的内部逻辑不影响调用方、不同程序员可以并行开发不同的接口实现者。

关于蓝图通信方式的选择，我在项目中形成了明确的判断标准：常驻框架层（如 GameMode 引用）用 Cast To（因为游戏不运行 GM 也不存在，硬引用是安全的）；物理接触交互（开门、传送、伤害）用 BPI（弱耦合，自带类型安全检查，调用不存在的接口不会崩溃，只是静默跳过）；全局状态同步（血量变化通知 UI 刷新）用 Event Dispatcher（一对多广播，发布者不需要知道有哪些订阅者）。

### 3.3 增强输入系统 — 输入与实体的彻底分离

在 BP_BallAdventurePlayerController 的 Event OnPossess 中，通过 Enhanced Input Local Player Subsystem 注入 Input Mapping Context。这意味着 IMC 挂载在 Controller 上而非 Pawn 上——当 Pawn 被销毁或切换时，输入绑定不会丢失。

项目中编写了 4 种 Input Action：跳跃（Bool 类型，只响应 Started 首帧触发，防止按住连跳）、视角控制（Axis2D 类型，每帧 Triggered）、翻滚（Bool 类型）、交互触发（Bool 类型，绑定 F 键）。这些 IA 通过 IMC_BallAdventure 统一映射到键盘按键。

这里有一个容易被忽视的细节：Input Action 的 Trigger Event 有三种生命周期——Started（按下首帧触发一次，适用于跳跃这种不可重复的动作）、Triggered（持续按住每帧触发，适用于移动和蓄力）、Completed（松开时触发，适用于需要"松手响应"的操作）。选错 Trigger Event 类型会导致手感问题，比如用 Triggered 做跳跃会让角色每帧都在跳。

### 3.4 碰撞检测与事件驱动

整个项目的游戏逻辑几乎全部由碰撞事件驱动，这体现了 UE 物理系统作为游戏逻辑载体的设计思想：

- **BP_Crystal**：OnComponentBeginOverlap → 通过 BPI_PlayerState 增加分数 → 通过 BPI_Deliver 通知传送门 → DestroyActor 并生成拾取特效
- **BP_Bullet**：设置 Projectile 碰撞通道，OnHit → 判定命中对象 → 触发反馈 → 销毁自身
- **BP_HitCube**：OnHit 接收端 → 材质切换/颜色变化/销毁反馈 → 通过 BPI 更新计分
- **BP_Wall**：OnComponentBeginOverlap → 碰撞墙体判定失败/扣分

在碰撞系统中，获取目标 Actor 的唯一正确方式是通过事件参数中的 Other Actor——绝不可以用 Get Player Pawn 这种全局查询。全局查询在单玩家场景看不出问题，但在分屏或多人环境下会返回错误的 Pawn 实例，导致跨玩家 Bug。

### 3.5 UI 数据绑定

四个 UMG Widget Blueprint 覆盖了游戏 UI 的常见需求：

- **WBP_BallHealthBar**：Progress Bar 控件绑定玩家血量变量，通过 Bind Widget 实现数据驱动的响应式更新
- **WBP_BallMainHDU**：Canvas Panel 布局的 HUD 覆盖层，得分和提示文本实时绑定
- **WBP_BallScoreBoard**：Vertical Box 列表展示收集进度
- **WBP_DeathScreen**：全屏覆盖的死亡界面，按钮事件绑定到重新开始/返回主菜单（Open Level 节点跳转）

每个 Widget 都在 Event Construct 中完成初始绑定，使用 IsValid 节点做空指针保护——这是一个在工业级 UI 开发中很容易被忽视但至关重要的防御性编程习惯。

### 3.6 GameMode 状态管理

BP_BallAdventureMode 作为游戏规则的中央管理者，负责指定 Default Pawn、PlayerController、HUD 类，同时管理收集进度判定和游戏结束条件。将状态逻辑集中在 GameMode 而非散布在各个 Actor 中，使得游戏规则可预测、可调试——这是 UE 框架设计的核心意图，GameMode 就是服务端权威状态的唯一持有者。

### 3.7 EatFood 关卡 — GameMode 为中心的规则管理

EatFood 关卡将 GameMode 的角色发挥得更充分：BP_GameMode 不仅管理得分累计，还负责食物生成管理（随机位置刷新）、游戏结束判定（时间到/全部收集）。BP_Food 的 OnBeginOverlap 触发得分增加后立即 DestroyActor，同时通过 BPI_BallInteraction 接口统一食物拾取与碰撞响应的协议。

### 3.8 HitCube 关卡 — 投掷物与碰撞通道

BP_Bullet 通过 AddImpulse 或 SetPhysicsLinearVelocity 设置初始速度，使用 Projectile 碰撞通道预设进行命中筛选。碰撞通道的选择不是随意的——Projectile 通道默认对 WorldStatic 和 WorldDynamic 产生 Hit 事件，对 Pawn 产生 Overlap 事件，这个预设本身就是 Epic 对射击游戏最佳实践的总结。

### 3.9 方块交互关卡 — 蓝图继承

Cube4_Blueprint 作为父蓝图定义了基础的 OnClicked 和 OnBeginOverlap 事件响应。Cube4_Blueprint_Blueprint_zi 作为子蓝图继承并扩展了这些行为。蓝图继承的实践价值在于：当多个 Actor 共享 80% 的行为逻辑时，修改父蓝图的实现会自动传播到所有子蓝图，不用逐个修改。

---

## 四、自定义资源清单

### 静态网格体（16 个，全部从零建模）
SM_AirWall_01/02（空气墙）、SM_Floor_01（地板）、SM_GoldenDisc01-04（金色圆盘装饰）、SM_InvisibleFloor_01/02（隐形地板）、SM_SingleLogBridge（独木桥）、SM_Torch（火炬）、SM_Tube（管道）、SM_Wall_01/02（墙壁）、SM_WoodenTrack01/02/03（木质轨道）

### 自定义材质
M_Crystal — 水晶专用材质，控制颜色、发光和透明度参数

---

## 五、目录结构

```
Content/
├── MyMaps/                              # 主要游戏内容
│   ├── BluePrint/
│   │   ├── BP_BallAdventure_Level1/     # 核心项目：完整蓝图接口示例
│   │   │   ├── BP_BallAdventureMode     # GameMode
│   │   │   ├── BP_BallAdventurePlayerController  # PlayerController
│   │   │   ├── BP_BallAdventurePlayerPawn        # 小球 Pawn（核心物理逻辑）
│   │   │   ├── BP_Crystal              # 收集品
│   │   │   ├── BP_Door01               # 交互门
│   │   │   ├── BP_Portal               # 传送门
│   │   │   ├── BP_Torch                # 火炬
│   │   │   ├── BP_VanityDestruction    # 销毁触发器
│   │   │   ├── BPI_Interact            # 交互接口
│   │   │   ├── BPI_ControllerCommands  # 控制器命令接口
│   │   │   ├── BPI_PlayerState         # 玩家状态接口
│   │   │   ├── BPI_Deliver             # 交付接口
│   │   │   ├── E_PlayerAttributes      # 玩家属性枚举
│   │   │   ├── Input/                  # 增强输入动作（4 IA + 1 IMC）
│   │   │   └── UI/                     # 4 个 Widget Blueprint
│   │   ├── BP_EatFood/                 # 食物收集子系统
│   │   ├── BP_HitCubeMap/              # 射击打靶子系统
│   │   └── BP_Map02/                   # 方块交互子系统
│   ├── Maps/                           # 关卡文件（5 umaps）
│   ├── Materials/                      # M_Crystal 材质
│   └── Mesh/BallAdventure_Level1Mesh/  # 16 个自定义静态网格体
├── FirstPerson/                         # UE 第一人称模板
├── ImportContent/                       # 导入的第三方资源（角色/车辆模型）
├── LevelPrototyping/                    # 关卡原型素材
└── StarterContent/                      # UE 初学者内容包
```

---

## 六、运行方式

1. 安装 Unreal Engine 5.7
2. 双击 `Demo02.uproject`
3. Content Browser → `Content/MyMaps/Maps/` → 打开任意关卡

建议从 **BallAdventure_Level1Map** 开始，该关卡集中展示了物理驱动角色控制、蓝图接口解耦、增强输入系统和 UI 数据绑定的完整技术栈。

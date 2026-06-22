<h1 align="center">UE5 物理驱动游戏机制实践</h1>

<p align="center">
  <samp>
    Unreal Engine 5.7 · 蓝图 + C++ · 26 个蓝图 · Chaos 物理 · BPI 多态解耦
  </samp>
</p>

---

## 项目概述

围绕纯物理驱动的 3D 小球冒险构建的游戏机制练习项目，延伸出食物收集、射击打靶、方块交互、载具驾驶共 5 个玩法关卡。核心目标不是关卡数量，而是每一关背后刻意练习的引擎机制——**Chaos 物理引擎力模式、向量叉乘扭矩计算、Blueprint Interface 多态解耦、Enhanced Input 生命周期、PlayerController 复活握手协议**。

个人独立完成，共编写 26 个自定义蓝图、5 套 BPI 接口、4 个 UMG 控件蓝图、16 个关卡专属静态网格体，全部从零搭建。同步记录了 18 篇技术笔记，形成从坐标系到网络同步的完整知识链。

**核心命题**：在纯物理驱动的 Pawn 上实现接近传统 Character 的操控手感，通过向量叉乘、双端驱动、BPI 多态解耦等底层机制构建完整的 3D 平台跳跃玩法系统。

---

## 场景设计

共搭建 5 张关卡地图：

| 关卡 | 玩法 | 核心技术 |
|------|------|---------|
| **BallAdventure_Level1Map** | 小球物理冒险 — 收集水晶、开启传送门、火炬照明、躲避销毁触发器 | 向量叉乘扭矩计算、Chaos 双端驱动、增强输入解耦、BPI 多态交互、Teleport 跃迁、假死复活流水线 |
| **EatFoodMap** | 食物收集 — 场景中收集食物，碰撞墙壁判定 | 碰撞驱动得分、GameMode 集中规则管理、随机位置刷新 |
| **HitCubeMap** | 射击打靶 — 发射投掷物命中立方体 | Projectile 碰撞通道预设、物理投掷物、OnHit 事件反馈链 |
| **02-Map** | 方块交互 — 与场景立方体触发逻辑 | 蓝图父子继承、OnClicked/OnBeginOverlap 事件绑定 |
| **01-Map** | 基础搭建 — 几何体关卡原型 | BSP/静态网格体灰盒原型设计 |

---

## MVC 架构设计

严格遵循 **Controller - Pawn - GameMode** 三位一体分层：

### PlayerController — 系统级命令唯一受理者

`BP_BallAdventurePlayerController` 实现 `BPI_ControllerCommands` 全部 4 个函数，承担所有跨系统通信：

- `ChangePawn(NewPawn)` — 调用 `Possess` 切换附身目标（载具上下车热插拔）
- `NotifyPlayerDeath()` — 受理死亡上报，触发假死序列
- `RequestPawnRespawn()` — 受理 UI 复活请求
- `NotifyPlayerRespawnReady()` — 执行 `Start Camera Fade` 亮屏回调

通过 `Event OnPossess` 注入 Enhanced Input Local Player Subsystem，管理 `Set Input Mode Game Only` / `Set Input Mode UI Only` 双模式切换。SpringArm 通过 `Use Pawn Control Rotation` 实现防眩晕解耦。

**设计哲学**：所有系统级命令集中到 Controller 实现接口隔离原则（ISP）——交互蓝图只调用接口，不关心谁处理。新增载具时只需在载具里调用 `ChangePawn`，Controller 逻辑零修改。

### Pawn — 只负责物理响应

`BP_BallAdventurePlayerPawn`（227 节点 / 175 连线，项目最大蓝图）只承担物理计算：接收扭矩、受力移动、射线检测地面。不持有任何 UI 引用，不直接调用 Destroy，通过 BPI 上报 Controller。

### GameMode — 规则判定与状态管理

`BP_BallAdventureMode` 指定 Default Pawn/Controller/HUD，收集进度全局判定，游戏结束条件管理。

---

## 物理驱动详解

### 地面滚动：向量叉乘扭矩

```
键盘 WASD 输入 → Cross Product 计算旋转扭矩轴 → Add Torque(In Radians) + Accel Change
```

将二维键盘输入通过向量叉乘映射为球体三维旋转扭矩，使球体按玩家意图方向滚动。这是手动推导的物理公式，而非使用 CharacterMovement 组件预设。

### 地面判定：Line Trace 状态机

```
Line Trace By Channel → 射线长度 = 球体半径 + 10cm 容差 → 地面/空中状态切换
```

10cm 容差防止地形起伏导致的判定抖动——射线来回切换地面/空中状态会引发物理抖动。

### 空中双端驱动

```
Add Force + Accel Change（空中水平推力）+ Add Torque（保持旋转惯性）
```

空中既要有操控力让玩家能调整落点，又要让物理引擎保持惯性旋转——在物理真实感和操控手感之间取平衡。

### 性能优化

Tick Interval 锁定 0.033s（约 30 FPS），相比默认每帧执行，物理计算开销减半。

---

## BPI 接口隔离（5 套）

所有跨蓝图通信通过 Blueprint Interface 实现，杜绝 Cast To 硬引用：

| 接口 | 函数数 | 实现者 | 职责 |
| :--- | :----- | :----- | :----- |
| `BPI_Interact` | 2 | BP_Door01 / BP_Portal / BP_Vehicle_Car01 | 通用交互（开门/传送/上车） |
| `BPI_Deliver` | 1 | BP_Portal | 收集交付通知（水晶→传送门） |
| `BPI_ControllerCommands` | 4 | BP_BallAdventurePlayerController | 系统级命令申报（Pawn切换/死亡/复活） |
| `BPI_PlayerState` | 2 | BP_BallAdventurePlayerPawn | 玩家属性读写（ModifyAttribute 泛化属性修改） |
| `BPI_BallInteraction` | 3 | BP_Food | 食物拾取三函数协议 |

**BPI 按能力分类原则**：一个接口只定义一组相关操作。不把所有函数塞进一个 BPI，保证每个实现者只需要关注自己需要的接口。

---

## 蓝图总览

### BallAdventure 关卡 — 角色系统

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| BP_BallAdventurePlayerPawn | `Pawn` | 小球 Pawn — 物理移动/跳跃/翻滚（227 节点 / 175 连线） |
| BP_BallAdventurePlayerController | `PlayerController` | BPI_ControllerCommands 四函数全集 — 增强输入/死亡UI/复活握手 |
| BP_BallAdventureMode | `GameModeBase` | 游戏模式 — 规则与状态管理 |
| E_PlayerAttributes | `Enum` | 玩家属性枚举（血量/分数/收集进度） |

### 交互机制

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| BP_Crystal | `Actor` | 可收集水晶 — VLerp 吸收动画 + Timeline 防抖 |
| BP_Door01 | `Actor` | 可开关门 — Timeline 旋转动画 |
| BP_Portal | `Actor` | 传送门 — Teleport 空间跃迁（双重接口 BPI_Interact + BPI_Deliver） |
| BP_Torch | `Actor` | 火炬照明 — Point Light 组件 + 动态材质 |
| BP_VanityDestruction | `Actor` | Controller 中介式死亡触发墙 |
| BP_Vehicle_Car01 | `Pawn` | 可驾驶载具 — ChangePawn 热插拔 Pawn 切换 |

### 增强输入系统

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| IA_Jump / IA_Look / IA_Roll / IA_Interact | `Input Action` | 4 个独立输入动作 |
| IMC_Ball | `Input Mapping Context` | 统一映射 4 个 IA 到物理按键 |

### UMG 控件蓝图

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| WBP_BallHealthBar | `UserWidget` | 局部 Pawn 专属血条 UI（绑定 Pawn 属性广播） |
| WBP_BallMainHUD | `UserWidget` | 全局 HUD — 容器布局 |
| WBP_BallScoreBoard | `UserWidget` | 收集进度展示 |
| WBP_DeathScreen | `UserWidget` | 全局死亡 UI — Controller 托管显示/隐藏 |

### EatFood 关卡子系统

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| BP_Player | `Pawn` | 玩家角色 — 物理驱动移动 |
| BP_MyController | `PlayerController` | 输入绑定与视角管理 |
| BP_Food | `StaticMeshActor` | 食物物品 — 碰撞拾取加分 |
| BP_Wall | `StaticMeshActor` | 墙壁障碍 — 边界划定 |
| BP_GameMode | `GameModeBase` | 得分累计 + 食物生成 + 结束判定 |
| BPI_BallInteraction | `Interface` | 食物拾取三函数协议 |

### HitCube 关卡 — 射击系统

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| BP_Bullet | `StaticMeshActor` | 子弹投掷物 — Projectile 碰撞通道 |
| BP_HitCube | `StaticMeshActor` | 目标立方体 — OnHit 反馈链 |

### 02-Map 关卡 — 方块交互

| 蓝图 | 父类 | 功能 |
| :--- | :--- | :--- |
| Cube4_Blueprint | `StaticMeshActor` | 父级可交互立方体 |
| Cube4_Blueprint_Blueprint_zi | `Cube4_Blueprint_C` | 子级继承变体 |

---

## 死亡复活握手协议

死亡不是简单 Destroy，而是一套完整的双向握手流程：

```
碰撞触发 → BP_VanityDestruction 受理
          → BPI_ControllerCommands::NotifyPlayerDeath(DeathPawn)
          → Controller: 播放 Camera Fade 黑屏
          → 生成 WBP_DeathScreen（Set Input Mode UI Only）
          → 玩家点击复活按钮
          → UI → BPI_ControllerCommands::RequestPawnRespawn
          → Controller: 验证状态 → 重置 Pawn 位置/速度/碰撞
          → BPI_ControllerCommands::NotifyPlayerRespawnReady
          → Start Camera Fade 亮屏 → Set Input Mode Game Only
          → 世界恢复
```

实现了完整的「谁来谁带去」（RAII 原则）——Controller 持有 UI 引用并在复活完成后干净清理。

---

## Content 目录结构

```
Demo02 5.7/Content/
├── MyMaps/
│   ├── BluePrint/
│   │   ├── BP_BallAdventure_Level1/       ← 核心关卡：完整物理驱动 + BPI 解耦
│   │   │   ├── Player/                    ← Pawn/Controller/GameMode/BPI/Enum
│   │   │   ├── Crystal/                   ← BP_Crystal（子目录）
│   │   │   ├── OpenDoor/                  ← BP_Door01（子目录）
│   │   │   ├── Portal/                    ← BP_Portal（子目录）
│   │   │   ├── Input/                     ← 4 IA + 1 IMC 增强输入
│   │   │   └── UI/                        ← 4 个 UMG 控件蓝图
│   │   ├── BP_EatFood/                    ← 食物收集子系统
│   │   ├── BP_HitCubeMap/                 ← 射击打靶子系统
│   │   └── BP_Map02/                      ← 方块交互子系统
│   ├── Maps/                              ← 5 个 .umap 关卡文件
│   ├── Materials/                         ← M_Crystal 自定义材质
│   └── Mesh/BallAdventure_Level1Mesh/     ← 16 个自定义静态网格体
├── FirstPerson/                            ← UE 第一人称模板（未使用）
├── ImportContent/                          ← 第三方 FBX 导入（GIRL/Low_Poly_Cars）
├── Vehicles/                               ← 载具模板（OffroadCar/SportsCar）
└── StarterContent/                         ← UE 启动内容
```

---

## 蓝图统计

| 指标 | 数值 |
|------|------|
| 自定义蓝图 | 26（Blueprint 22 + Widget 4） |
| BPI 接口 | 5 套（共 12 个函数） |
| 最大蓝图 | `BP_BallAdventurePlayerPawn` — 227 节点 / 175 连线 / 43 函数 / 10 事件 / 15 变量 |
| 关卡地图 | 5 张 |
| 自定义静态网格体 | 16 个 |
| C++ 插件 | BlueprintTopologyExporter（13 种 K2Node 类型穿透） |
| 开发笔记 | 18 篇（坐标系 → Gameplay 框架 → 物理控制 → BPI → 网络同步） |

---

## 开发笔记知识链路

18 篇笔记随项目进度同步记录，形成从基础到架构的完整知识链：

```text
01 坐标系与物理基石
  ↓
02 Gameplay 框架（MVC 分层）
  ↓
03 物理控制 + 摄像机底层架构
  ↓
04 蓝图通信（Cast vs BPI 选择原则）
  ↓
05 射线检测（Line Trace By Channel 地面判定）
  ↓
06 向量叉乘 + 空中双端驱动
  ↓
07 BPI 开门（接口通信模式）
  ↓
08 传送门（Teleport + 双重接口）
  ↓
09 Timeline + Lerp 角度调控
  ↓
10-11 死亡动画 UI + 复活完整流水线
  ↓
12 BPI 终极分类（ISP 接口隔离原则）
  ↓
13 IA 命名规范 + Enum 设计
  ↓
14 函数复用 → 宏 vs 函数 → Triggered vs Started
  ↓
15 UI 血条广播绑定 → 碰撞矩阵
  ↓
16 Function vs Event vs Macro → 局部UI vs 全局UI
  ↓
17 Terminal/git → 网络同步 → UObject 类族谱
  ↓
18 PlayerController 生命周期 → RAII → 双向握手复活
```

---

**引擎**: Unreal Engine 5.7 · **开发方式**: 蓝图 + C++ 插件 · **周期**: 约 4 个月（个人独立开发）

---

## 程序员日志

### 2026.03 — 物理引擎启蒙
项目起源于一个简单的问题：「UE5 里的 Character 移动太丝滑了，能不能自己用纯物理实现？」于是把 Pawn 改成一个球，关掉所有预设的 Movement Component，从零开始写物理逻辑。第一个坑：`Add Force` 和 `Add Impulse` 的区别——前者是持续力（类似火箭推进），后者是瞬间冲量（类似一拳打飞）。搞混了这两个，小球要么推不动，要么飞得太远。最终选择 `Add Torque(In Radians)` 驱动滚动旋转，因为扭矩只改变角速度，视觉上最自然。

### 2026.04 — BPI 解耦觉醒
当项目有 10 个以上的蓝图后，Cast To 调用的数量开始失控。一个 Door 蓝图需要 Cast To PlayerPawn、Cast To GameMode、Cast To Controller——每次 Cast 都是一条硬引用链。查了 UE5 源码发现每次 Cast 都会触发 Object 查找，在 Tick 里用 Cast 等于是每帧做一次数据库查询。于是全面重构为 Blueprint Interface 通信：交互蓝图只调用 `BPI_Interact::ExecuteInteract(Instigator)`，具体谁处理、怎么处理，完全由实现者决定。这就是接口隔离原则。

### 2026.05 — 死亡复活流水线
「Player Dead」看起来很简单——播放动画、显示 UI、重新开始。但实际写起来发现这是个分布式状态机：Pawn 需要停止物理响应、Controller 需要切换 Input Mode、UI 需要等待动画播放完毕才能显示复活按钮。三者之间的时序如果没处理好，会导致「黑屏后按键还能操控角色」这种诡异的并发 Bug。最终设计了一套双向握手协议：每个状态转换都必须由接收方主动确认，发送方不得假设操作已完成。

### 2026.06 — C++ 插件与自动化
纯蓝图的局限开始显现——我无法知道一个蓝图里到底有多少个节点、哪些函数被调用了多少次。于是动手写了一个 C++ 插件，通过 `UEdGraph` → `UEdGraphNode` → `UEdGraphPin` → `LinkedTo[]` 的反射链路，把蓝图节点图完整导出为结构化数据。配合 Python 脚本和 Claude Code Skill，实现了每次修改蓝图后的自动文档更新。从「手动维护设计文档」到「一条命令全自动」，这是这个项目最大的工程化收获。

**最大的收获**：物理驱动的游戏不是"更难做"，而是"更需要思考"。每一条物理规则都是你手动施加的，没有 CharacterMovement 帮你兜底。这迫使你真正理解引擎的工作原理，而不是依赖预设行为。
